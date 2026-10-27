#include "ReservationPage.h"

#include "../managers/ReservationManager.h"
#include "../common/Rules.h"

#include <QComboBox>
#include <QAbstractItemView>
#include <QDateEdit>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTime>
#include <QVBoxLayout>
#include <QVariant>

namespace {
constexpr int kReservationIdRole = Qt::UserRole + 1;

QDateTime dateTimeForToday(const QString& timeText, const QDate& date)
{
    return QDateTime(date, QTime::fromString(timeText, QStringLiteral("HH:mm")));
}

QString displayTime(const QDateTime& value)
{
    return value.isValid()
        ? value.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
        : QStringLiteral("—");
}
}

ReservationPage::ReservationPage(ReservationManager& manager,
                                 const User& currentUser, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_user(currentUser)
{
    buildUi();
    refreshReservations();
}

void ReservationPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    auto* controls = new QHBoxLayout;

    if (m_user.role == UserRole::Student) {
        auto* seatLabel = new QLabel(QStringLiteral("座位"), this);
        m_seatCombo = new QComboBox(this);
        for (int id = 1; id <= 20; ++id)
            m_seatCombo->addItem(QStringLiteral("%1号座位").arg(id, 2, 10, QLatin1Char('0')), id);

        m_startCombo = new QComboBox(this);
        m_endCombo = new QComboBox(this);
        for (int minutes = ReservationRules::OpeningHour * 60;
             minutes <= ReservationRules::ClosingHour * 60;
             minutes += ReservationRules::SlotMinutes) {
            const QString value = QTime(minutes / 60, minutes % 60)
                                      .toString(QStringLiteral("HH:mm"));
            m_startCombo->addItem(value, value);
            m_endCombo->addItem(value, value);
        }
        m_endCombo->setCurrentIndex(1);
        connect(m_startCombo, qOverload<int>(&QComboBox::currentIndexChanged),
                this, [this](int startIndex) {
                    if (m_endCombo->currentIndex() <= startIndex
                        && startIndex + 1 < m_endCombo->count())
                        m_endCombo->setCurrentIndex(startIndex + 1);
                });
        auto* createButton = new QPushButton(QStringLiteral("预约"), this);
        m_cancelButton = new QPushButton(QStringLiteral("取消所选预约"), this);
        connect(createButton, &QPushButton::clicked,
                this, &ReservationPage::createReservation);
        connect(m_cancelButton, &QPushButton::clicked,
                this, &ReservationPage::cancelSelectedReservation);

        controls->addWidget(seatLabel);
        controls->addWidget(m_seatCombo);
        controls->addWidget(new QLabel(QStringLiteral("开始"), this));
        controls->addWidget(m_startCombo);
        controls->addWidget(new QLabel(QStringLiteral("结束"), this));
        controls->addWidget(m_endCombo);
        controls->addWidget(createButton);
        controls->addWidget(m_cancelButton);
    } else {
        m_fromDate = new QDateEdit(QDate::currentDate(), this);
        m_toDate = new QDateEdit(QDate::currentDate(), this);
        m_fromDate->setCalendarPopup(true);
        m_toDate->setCalendarPopup(true);
        m_accountFilter = new QComboBox(this);
        m_accountFilter->addItem(QStringLiteral("全部学生"), QString());
        for (const Reservation& reservation : m_manager.listReservations(m_user)) {
            if (m_accountFilter->findData(reservation.studentAccount) < 0)
                m_accountFilter->addItem(reservation.studentAccount,
                                         reservation.studentAccount);
        }
        auto* filterButton = new QPushButton(QStringLiteral("查询"), this);
        connect(filterButton, &QPushButton::clicked,
                this, &ReservationPage::applyAdminFilter);
        controls->addWidget(new QLabel(QStringLiteral("预约日期"), this));
        controls->addWidget(m_fromDate);
        controls->addWidget(new QLabel(QStringLiteral("至"), this));
        controls->addWidget(m_toDate);
        controls->addWidget(m_accountFilter);
        controls->addWidget(filterButton);
        controls->addStretch();
    }
    root->addLayout(controls);

    m_message = new QLabel(this);
    root->addWidget(m_message);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(m_user.role == UserRole::Admin ? 6 : 5);
    QStringList headers;
    if (m_user.role == UserRole::Admin)
        headers << QStringLiteral("学生账号");
    headers << QStringLiteral("座位") << QStringLiteral("开始时间")
            << QStringLiteral("结束时间") << QStringLiteral("状态")
            << QStringLiteral("签到时间");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    root->addWidget(m_table);
}

void ReservationPage::refreshReservations()
{
    populateTable(m_manager.listReservations(m_user));
    if (m_accountFilter) {
        const QString selected = m_accountFilter->currentData().toString();
        m_accountFilter->clear();
        m_accountFilter->addItem(QStringLiteral("全部学生"), QString());
        for (const Reservation& reservation : m_manager.listReservations(m_user)) {
            if (m_accountFilter->findData(reservation.studentAccount) < 0)
                m_accountFilter->addItem(reservation.studentAccount,
                                         reservation.studentAccount);
        }
        const int index = m_accountFilter->findData(selected);
        m_accountFilter->setCurrentIndex(index >= 0 ? index : 0);
    }
}

void ReservationPage::createReservation()
{
    const QDate today = QDate::currentDate();
    const QDateTime start = dateTimeForToday(m_startCombo->currentData().toString(), today);
    const QDateTime end = dateTimeForToday(m_endCombo->currentData().toString(), today);
    const OperationResult result = m_manager.createReservation(
        m_user, m_seatCombo->currentData().toInt(), start, end, QDateTime::currentDateTime());
    m_message->setText(result.message);
    if (result.ok)
        refreshReservations();
}

void ReservationPage::cancelSelectedReservation()
{
    const int row = m_table->currentRow();
    if (row < 0) {
        m_message->setText(QStringLiteral("请先选择一条预约。"));
        return;
    }
    const QString id = m_table->item(row, 0)->data(kReservationIdRole).toString();
    const OperationResult result = m_manager.cancelReservation(
        m_user, id, QDateTime::currentDateTime());
    m_message->setText(result.message);
    if (result.ok)
        refreshReservations();
}

void ReservationPage::applyAdminFilter()
{
    const QDate from = m_fromDate->date();
    const QDate to = m_toDate->date();
    if (from > to) {
        m_message->setText(QStringLiteral("起始日期不能晚于结束日期。"));
        return;
    }

    QVector<Reservation> filtered;
    const QString account = m_accountFilter->currentData().toString();
    for (const Reservation& reservation : m_manager.listReservations(m_user)) {
        const QDate date = reservation.startTime.date();
        if (date >= from && date <= to
            && (account.isEmpty() || reservation.studentAccount == account))
            filtered.push_back(reservation);
    }
    populateTable(filtered);
    m_message->clear();
}

QString ReservationPage::stateText(const Reservation& reservation,
                                   const QDateTime& now) const
{
    switch (stateOf(reservation, now)) {
    case ReservationViewState::Upcoming: return QStringLiteral("待开始");
    case ReservationViewState::WaitingCheckIn: return QStringLiteral("待签到");
    case ReservationViewState::InUse: return QStringLiteral("使用中");
    case ReservationViewState::Finished: return QStringLiteral("已结束");
    case ReservationViewState::Cancelled: return QStringLiteral("已取消");
    case ReservationViewState::Terminated: return QStringLiteral("违约终止");
    }
    return {};
}

void ReservationPage::populateTable(const QVector<Reservation>& reservations)
{
    m_table->setRowCount(reservations.size());
    const QDateTime now = toMinute(QDateTime::currentDateTime());
    for (int row = 0; row < reservations.size(); ++row) {
        const Reservation& reservation = reservations.at(row);
        int column = 0;
        if (m_user.role == UserRole::Admin)
            m_table->setItem(row, column++, new QTableWidgetItem(reservation.studentAccount));
        auto* seat = new QTableWidgetItem(QStringLiteral("%1号座位")
                                              .arg(reservation.seatId, 2, 10, QLatin1Char('0')));
        seat->setData(kReservationIdRole, reservation.id);
        m_table->setItem(row, column++, seat);
        m_table->setItem(row, column++, new QTableWidgetItem(displayTime(reservation.startTime)));
        m_table->setItem(row, column++, new QTableWidgetItem(displayTime(reservation.endTime)));
        m_table->setItem(row, column++, new QTableWidgetItem(stateText(reservation, now)));
        m_table->setItem(row, column++, new QTableWidgetItem(displayTime(reservation.checkedInAt)));
    }
}
