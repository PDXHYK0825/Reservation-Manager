// 成员 C：签到与违约管理。
#include "managers/AttendanceViolationManager.h"
#include "managers/ReservationManager.h"
#include "storage/JsonStore.h"

AttendanceViolationManager::AttendanceViolationManager(JsonStore &store) :
    dataStore(store)
{
}

int AttendanceViolationManager::findReservation(
    const AppData &data, const QString &reservationId) const
{
    for (int i = 0; i < data.reservations.size(); i++)
    {
        if (data.reservations[i].id == reservationId)
        {
            return i;
        }
    }

    return -1;
}

OperationResult AttendanceViolationManager::checkIn(
    const User &user, const QString &reservationId, const QDateTime &now)
{
    if (user.role != UserRole::Student || user.account.isEmpty())
    {
        return {false, QStringLiteral("仅学生可以签到")};
    }

    QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
    {
        return {false, QStringLiteral("操作时间无效")};
    }

    AppData data = dataStore.data();
    int index = findReservation(data, reservationId);
    if (index == -1)
    {
        return {false, QStringLiteral("预约不存在")};
    }

    Reservation &reservation = data.reservations[index];
    if (reservation.studentAccount != user.account)
    {
        return {false, QStringLiteral("只能对本人的预约签到")};
    }
    if (reservation.flag != ReservationFlag::Active)
    {
        return {false, QStringLiteral("已取消或已终止的预约不能签到")};
    }
    if (!reservation.startTime.isValid() || !reservation.endTime.isValid()
        || reservation.startTime >= reservation.endTime)
    {
        return {false, QStringLiteral("预约时间无效")};
    }
    if (currentTime < reservation.startTime || currentTime >= reservation.endTime)
    {
        return {false, QStringLiteral("请在预约时段内签到")};
    }
    if (reservation.checkedInAt.isValid())
    {
        return {false, QStringLiteral("该预约已签到")};
    }

    reservation.checkedInAt = currentTime;
    OperationResult result = dataStore.commit(data);
    if (result.ok)
    {
        result.message = QStringLiteral("签到成功");
    }
    return result;
}

OperationResult AttendanceViolationManager::addViolation(
    const User &admin, const QString &reservationId, const QDateTime &now)
{
    if (admin.role != UserRole::Admin || admin.account.isEmpty())
    {
        return {false, QStringLiteral("仅管理员可以登记违约")};
    }

    QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
    {
        return {false, QStringLiteral("操作时间无效")};
    }

    AppData data = dataStore.data();
    int index = findReservation(data, reservationId);
    if (index == -1)
    {
        return {false, QStringLiteral("预约不存在")};
    }

    Reservation &reservation = data.reservations[index];
    if (!reservation.startTime.isValid() || !reservation.endTime.isValid()
        || reservation.startTime >= reservation.endTime)
    {
        return {false, QStringLiteral("预约时间无效")};
    }
    if (currentTime < reservation.startTime)
    {
        return {false, QStringLiteral("预约开始后才能登记违约")};
    }

    for (int i = 0; i < data.violations.size(); i++)
    {
        if (data.violations[i].reservationId == reservationId
            && !data.violations[i].revoked)
        {
            return {false, QStringLiteral("该预约已有未撤销的违约记录")};
        }
    }

    Violation violation;
    violation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    violation.reservationId = reservationId;
    violation.recordedAt = currentTime;
    violation.revoked = false;
    data.violations.append(violation);

    // 同时登记违约并终止仍有效的预约。
    if (reservation.flag == ReservationFlag::Active
        && currentTime < reservation.endTime)
    {
        reservation.flag = ReservationFlag::Terminated;
        reservation.closedAt = currentTime;
    }

    OperationResult result = dataStore.commit(data);
    if (result.ok)
    {
        result.message = QStringLiteral("违约登记成功");
    }
    return result;
}

OperationResult AttendanceViolationManager::revokeViolation(
    const User &admin, const QString &violationId)
{
    if (admin.role != UserRole::Admin || admin.account.isEmpty())
    {
        return {false, QStringLiteral("仅管理员可以撤销违约")};
    }

    AppData data = dataStore.data();
    for (int i = 0; i < data.violations.size(); i++)
    {
        Violation &violation = data.violations[i];
        if (violation.id == violationId)
        {
            if (violation.revoked)
            {
                return {false, QStringLiteral("该违约记录已撤销")};
            }

            // 保留记录，不恢复原预约。
            violation.revoked = true;
            OperationResult result = dataStore.commit(data);
            if (result.ok)
            {
                result.message = QStringLiteral("违约已撤销");
            }
            return result;
        }
    }

    return {false, QStringLiteral("违约记录不存在")};
}

QVector<Violation> AttendanceViolationManager::listViolations(
    const User &user) const
{
    QVector<Violation> records;
    if (user.account.isEmpty())
    {
        return records;
    }

    const AppData &data = dataStore.data();
    if (user.role == UserRole::Admin)
    {
        return data.violations;
    }
    if (user.role != UserRole::Student)
    {
        return records;
    }

    for (int i = 0; i < data.violations.size(); i++)
    {
        const Violation &violation = data.violations[i];
        for (int j = 0; j < data.reservations.size(); j++)
        {
            const Reservation &reservation = data.reservations[j];
            if (violation.reservationId == reservation.id
                && reservation.studentAccount == user.account)
            {
                records.append(violation);
                break;
            }
        }
    }
    return records;
}

AttendanceViolationPage::AttendanceViolationPage(AttendanceViolationManager &manager,
    const JsonStore &store, const User &user, QWidget *parent)
    : QWidget(parent), manager(manager), store(store), user(user)
{
    auto *root = new QVBoxLayout(this);
    controlsPanel = new QWidget;
    auto *panelLayout = new QVBoxLayout(controlsPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(QStringLiteral("选择预约")));
    reservations = new QComboBox;
    reservations->setMinimumContentsLength(45);
    controls->addWidget(reservations, 1);
    const bool admin = user.role == UserRole::Admin;
    auto *action = new QPushButton(admin ? QStringLiteral("登记违约") : QStringLiteral("签到"));
    controls->addWidget(action);
    panelLayout->addLayout(controls);
    message = new QLabel;
    message->setWordWrap(true);
    panelLayout->addWidget(message);
    root->addWidget(controlsPanel);
    connect(action, &QPushButton::clicked, this, [this, admin] {
        const QString id = reservations->currentData().toString();
        if (id.isEmpty()) { message->setText(QStringLiteral("请先选择一条预约。")); return; }
        const QDateTime now = QDateTime::currentDateTime();
        const OperationResult result = admin ? this->manager.addViolation(this->user, id, now)
                                            : this->manager.checkIn(this->user, id, now);
        message->setText(result.message);
    });
    if (admin) {
        auto *revoke = new QPushButton(QStringLiteral("撤销所选违约"));
        root->addWidget(revoke, 0, Qt::AlignLeft);
        connect(revoke, &QPushButton::clicked, this, [this] {
            const int row = violations->currentRow();
            if (row < 0) { message->setText(QStringLiteral("请先选择一条违约记录。")); return; }
            message->setText(this->manager.revokeViolation(this->user,
                violations->item(row, 0)->data(Qt::UserRole).toString()).message);
        });
    }
    violations = new QTableWidget(0, 5);
    violations->setHorizontalHeaderLabels({QStringLiteral("学生账号"), QStringLiteral("座位"),
        QStringLiteral("预约开始时间"), QStringLiteral("登记时间"), QStringLiteral("状态")});
    violations->setEditTriggers(QAbstractItemView::NoEditTriggers);
    violations->setSelectionBehavior(QAbstractItemView::SelectRows);
    violations->setSelectionMode(QAbstractItemView::SingleSelection);
    violations->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(violations);
}

void AttendanceViolationPage::refresh()
{
    const QString reservationId = reservations->currentData().toString();
    reservations->clear();
    const QDateTime now = QDateTime::currentDateTime();
    for (const Reservation &r : store.data().reservations) {
        if (user.role != UserRole::Admin && r.studentAccount != user.account)
            continue;
        reservations->addItem(QStringLiteral("%1　%2号　%3—%4　%5")
            .arg(r.studentAccount).arg(r.seatId, 2, 10, QLatin1Char('0'))
            .arg(displayTime(r.startTime), r.endTime.time().toString(QStringLiteral("HH:mm")),
                 reservationStateText(r, now)), r.id);
    }
    const int index = reservations->findData(reservationId);
    if (index >= 0)
        reservations->setCurrentIndex(index);
    QString selected;
    if (violations->currentRow() >= 0)
        selected = violations->item(violations->currentRow(), 0)->data(Qt::UserRole).toString();
    violations->setSortingEnabled(false);
    violations->setRowCount(0);
    for (const Violation &v : manager.listViolations(user))
        for (const Reservation &r : store.data().reservations)
            if (r.id == v.reservationId) {
                const int row = violations->rowCount();
                violations->insertRow(row);
                const QStringList values = {r.studentAccount, QStringLiteral("%1号").arg(r.seatId, 2, 10, QLatin1Char('0')),
                    displayTime(r.startTime), displayTime(v.recordedAt), v.revoked ? QStringLiteral("已撤销") : QStringLiteral("有效违约")};
                for (int column = 0; column < values.size(); ++column) {
                    auto *item = new QTableWidgetItem(values[column]);
                    item->setData(Qt::UserRole, v.id);
                    violations->setItem(row, column, item);
                }
                break;
            }
    violations->setSortingEnabled(true);
    for (int row = 0; row < violations->rowCount(); ++row)
        if (violations->item(row, 0)->data(Qt::UserRole).toString() == selected) {
            violations->selectRow(row);
            break;
        }
}
