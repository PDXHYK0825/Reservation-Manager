// 成员 B：时间规则与预约管理。
#include "managers/ReservationManager.h"
#include "ui/MainWindow.h"
#include "storage/JsonStore.h"
#include <algorithm>

QDateTime toMinute(const QDateTime& value)
{
    if (!value.isValid())
        return {};

    QDateTime local = value.toLocalTime();
    local.setTime(QTime(local.time().hour(), local.time().minute()));
    return local;
}

bool isHalfHourAligned(const QDateTime& value)
{
    if (!value.isValid())
        return false;

    const QTime time = value.time();
    return (time.minute() == 0 || time.minute() == 30)
        && time.second() == 0 && time.msec() == 0;
}

bool overlaps(const QDateTime& startA, const QDateTime& endA,
              const QDateTime& startB, const QDateTime& endB)
{
    return startA < endB && endA > startB;
}

ReservationViewState stateOf(const Reservation& reservation,
                             const QDateTime& now)
{
    const QDateTime minuteNow = toMinute(now);
    if (reservation.flag == ReservationFlag::Cancelled)
        return ReservationViewState::Cancelled;
    if (reservation.flag == ReservationFlag::Terminated)
        return ReservationViewState::Terminated;
    if (!reservation.endTime.isValid() || minuteNow >= reservation.endTime)
        return ReservationViewState::Finished;
    if (minuteNow < reservation.startTime)
        return ReservationViewState::Upcoming;
    if (reservation.checkedInAt.isValid())
        return ReservationViewState::InUse;
    return ReservationViewState::WaitingCheckIn;
}

namespace {

OperationResult failure(const QString& message)
{
    return {false, message};
}

OperationResult success(const QString& message)
{
    return {true, message};
}

bool isActive(const Reservation& reservation)
{
    return reservation.flag == ReservationFlag::Active;
}
}

ReservationManager::ReservationManager(JsonStore& store)
    : m_store(store)
{
}

OperationResult ReservationManager::createReservation(
    const User& user, int seatId, const QDateTime& start,
    const QDateTime& end, const QDateTime& now)
{
    if (user.role != UserRole::Student || user.account.trimmed().isEmpty())
        return failure(QStringLiteral("只有已登录学生可以创建预约。"));
    if (seatId < 1 || seatId > DemoData::seatCount)
        return failure(QStringLiteral("座位编号无效。"));

    const QDateTime minuteNow = toMinute(now);
    const QDateTime minuteStart = toMinute(start);
    const QDateTime minuteEnd = toMinute(end);
    if (!minuteNow.isValid() || !minuteStart.isValid() || !minuteEnd.isValid())
        return failure(QStringLiteral("预约时间无效。"));
    if (!isHalfHourAligned(start) || !isHalfHourAligned(end))
        return failure(QStringLiteral("预约时间必须按整点或半点选择。"));
    if (minuteStart.date() != minuteNow.date()
        || minuteEnd.date() != minuteNow.date())
        return failure(QStringLiteral("只能预约当天时段。"));
    if (minuteStart < minuteNow)
        return failure(QStringLiteral("预约开始时间不能早于当前时间。"));
    if (minuteStart.time() < DemoData::openingTime()
        || minuteEnd.time() > DemoData::closingTime())
        return failure(QStringLiteral("开放时间为每天08:00至23:00。"));
    if (minuteEnd <= minuteStart)
        return failure(QStringLiteral("结束时间必须晚于开始时间，至少预约30分钟。"));

    const AppData& current = m_store.data();
    for (const Reservation& existing : current.reservations) {
        if (!isActive(existing))
            continue;

        if (existing.seatId == seatId
            && overlaps(minuteStart, minuteEnd,
                        existing.startTime, existing.endTime)) {
            return failure(QStringLiteral("该座位在所选时段已被预约。"));
        }
        if (existing.studentAccount == user.account
            && overlaps(minuteStart, minuteEnd,
                        existing.startTime, existing.endTime)) {
            return failure(QStringLiteral("你在所选时段已有其他预约。"));
        }
    }

    AppData updated = current;
    Reservation reservation;
    reservation.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    reservation.studentAccount = user.account;
    reservation.seatId = seatId;
    reservation.startTime = minuteStart;
    reservation.endTime = minuteEnd;
    reservation.flag = ReservationFlag::Active;
    updated.reservations.push_back(reservation);

    const OperationResult committed = m_store.commit(updated);
    if (!committed.ok)
        return committed;
    return success(QStringLiteral("预约成功。"));
}

OperationResult ReservationManager::cancelReservation(
    const User& user, const QString& reservationId, const QDateTime& now)
{
    if (user.role != UserRole::Student || user.account.trimmed().isEmpty())
        return failure(QStringLiteral("只有预约本人可以取消预约。"));
    if (reservationId.trimmed().isEmpty())
        return failure(QStringLiteral("预约编号无效。"));

    const QDateTime minuteNow = toMinute(now);
    if (!minuteNow.isValid())
        return failure(QStringLiteral("操作时间无效。"));

    AppData updated = m_store.data();
    auto reservation = std::find_if(updated.reservations.begin(),
                                    updated.reservations.end(),
        [&reservationId](const Reservation& item) {
            return item.id == reservationId;
        });
    if (reservation == updated.reservations.end())
        return failure(QStringLiteral("未找到该预约。"));
    if (reservation->studentAccount != user.account)
        return failure(QStringLiteral("不能取消其他学生的预约。"));
    if (!isActive(*reservation))
        return failure(QStringLiteral("该预约已关闭，不能再次取消。"));
    if (!reservation->endTime.isValid() || minuteNow >= reservation->endTime)
        return failure(QStringLiteral("已结束的预约不能取消。"));

    reservation->flag = ReservationFlag::Cancelled;
    reservation->closedAt = minuteNow;
    const OperationResult committed = m_store.commit(updated);
    if (!committed.ok)
        return committed;
    return success(QStringLiteral("预约已取消。"));
}

QVector<Reservation> ReservationManager::listReservations(const User& user) const
{
    QVector<Reservation> result;
    const QVector<Reservation>& all = m_store.data().reservations;
    result.reserve(all.size());
    for (const Reservation& reservation : all) {
        if (user.role == UserRole::Admin
            || (user.role == UserRole::Student
                && reservation.studentAccount == user.account)) {
            result.push_back(reservation);
        }
    }
    std::sort(result.begin(), result.end(),
        [](const Reservation& left, const Reservation& right) {
            if (left.startTime != right.startTime)
                return left.startTime < right.startTime;
            return left.id < right.id;
        });
    return result;
}

QString displayTime(const QDateTime &value)
{
    return value.isValid() ? value.toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QStringLiteral("—");
}

QString reservationStateText(const Reservation &reservation, const QDateTime &now)
{
    const QStringList names = {QStringLiteral("待开始"), QStringLiteral("待签到"), QStringLiteral("使用中"),
        QStringLiteral("已结束"), QStringLiteral("已取消"), QStringLiteral("违约终止")};
    return names[int(stateOf(reservation, now))];
}

ReservationPage::ReservationPage(ReservationManager &manager, const User &user, QWidget *parent,
                               bool historyOnly)
    : QWidget(parent), manager(manager), user(user)
{
    auto *root = new QVBoxLayout(this);
    controlsPanel = new QWidget;
    auto *panelLayout = new QVBoxLayout(controlsPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    auto *controls = new QHBoxLayout;
    if (user.role == UserRole::Student && !historyOnly) {
        create = new QPushButton(QStringLiteral("预约所选座位"));
        controls->addWidget(create);
        auto *cancel = new QPushButton(QStringLiteral("取消所选预约"));
        controls->addWidget(cancel);
        connect(create, &QPushButton::clicked, this, [this] {
            message->setText(this->manager.createReservation(this->user, seatId, start, end,
                QDateTime::currentDateTime()).message);
        });
        connect(cancel, &QPushButton::clicked, this, [this] {
            const int row = table->currentRow();
            if (row < 0) { message->setText(QStringLiteral("请先选择一条预约。")); return; }
            message->setText(this->manager.cancelReservation(this->user,
                table->item(row, 0)->data(Qt::UserRole).toString(), QDateTime::currentDateTime()).message);
        });
        setSelection(-1, {}, {});
    } else if (user.role == UserRole::Admin) {
        from = new QDateEdit(QDate::currentDate());
        to = new QDateEdit(QDate::currentDate());
        for (QDateEdit *edit : {from, to}) {
            edit->setCalendarPopup(true);
            edit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
        }
        account = new QComboBox;
        account->addItem(QStringLiteral("全部学生"), QString());
        for (const User &candidate : DemoData::users())
            if (candidate.role == UserRole::Student)
                account->addItem(candidate.account, candidate.account);
        controls->addWidget(new QLabel(QStringLiteral("预约日期")));
        controls->addWidget(from);
        controls->addWidget(new QLabel(QStringLiteral("至")));
        controls->addWidget(to);
        controls->addWidget(account);
        auto *query = new QPushButton(QStringLiteral("查询"));
        controls->addWidget(query);
        connect(query, &QPushButton::clicked, this, &ReservationPage::refresh);
    }
    controls->addStretch();
    panelLayout->addLayout(controls);
    message = new QLabel;
    panelLayout->addWidget(message);
    root->addWidget(controlsPanel);
    controlsPanel->setVisible(!historyOnly);
    table = new QTableWidget(0, 6);
    table->setHorizontalHeaderLabels({QStringLiteral("学生账号"), QStringLiteral("座位"),
        QStringLiteral("开始时间"), QStringLiteral("结束时间"), QStringLiteral("状态"), QStringLiteral("签到时间")});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(table);
}

void ReservationPage::setSelection(int id, const QDateTime &begin, const QDateTime &finish)
{
    seatId = id; start = begin; end = finish;
    if (create)
        create->setEnabled(id > 0 && begin >= toMinute(QDateTime::currentDateTime()));
}

void ReservationPage::refresh()
{
    QString selected;
    if (table->currentRow() >= 0)
        selected = table->item(table->currentRow(), 0)->data(Qt::UserRole).toString();
    table->setSortingEnabled(false);
    table->setRowCount(0);
    if (from && from->date() > to->date()) {
        message->setText(QStringLiteral("起始日期不能晚于结束日期。"));
        table->setSortingEnabled(true);
        return;
    }
    const QDateTime now = QDateTime::currentDateTime();
    for (const Reservation &r : manager.listReservations(user)) {
        if (from && (r.startTime.date() < from->date() || r.startTime.date() > to->date()
            || (!account->currentData().toString().isEmpty() && r.studentAccount != account->currentData().toString())))
            continue;
        const int row = table->rowCount();
        table->insertRow(row);
        const QStringList values = {r.studentAccount, QStringLiteral("%1号").arg(r.seatId, 2, 10, QLatin1Char('0')),
            displayTime(r.startTime), displayTime(r.endTime), reservationStateText(r, now), displayTime(r.checkedInAt)};
        for (int column = 0; column < values.size(); ++column) {
            auto *item = new QTableWidgetItem(values[column]);
            item->setData(Qt::UserRole, r.id);
            table->setItem(row, column, item);
        }
    }
    table->setSortingEnabled(true);
    for (int row = 0; row < table->rowCount(); ++row)
        if (table->item(row, 0)->data(Qt::UserRole).toString() == selected) {
            table->selectRow(row);
            break;
        }
}
