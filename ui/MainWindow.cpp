// 成员 A：登录、主窗口、演示账号及座位查询。
#include "ui/MainWindow.h"
#include "managers/ReservationManager.h"
#include "managers/AttendanceViolationManager.h"
#include "storage/JsonStore.h"

QVector<User> DemoData::users()
{
    return {
        {QStringLiteral("admin"), QStringLiteral("管理员"),
         QStringLiteral("123456"), UserRole::Admin},
        {QStringLiteral("student01"), QStringLiteral("学生一"),
         QStringLiteral("123456"), UserRole::Student},
        {QStringLiteral("student02"), QStringLiteral("学生二"),
         QStringLiteral("123456"), UserRole::Student}
    };
}

QVector<Seat> DemoData::seats()
{
    QVector<Seat> result;
    for (int id = 1; id <= seatCount; id++)
    {
        result.append({id, QStringLiteral("%1号").arg(id, 2, 10, QLatin1Char('0'))});
    }
    return result;
}

SeatPage::SeatPage(SeatManager &manager, QWidget *parent)
    : QWidget(parent), manager(manager)
{
    auto *root = new QVBoxLayout(this);
    auto *controls = new QHBoxLayout;
    startCombo = new QComboBox;
    endCombo = new QComboBox;
    for (QTime time = DemoData::openingTime(); time < DemoData::closingTime();
         time = time.addSecs(DemoData::reservationMinutes * 60))
        startCombo->addItem(time.toString(QStringLiteral("HH:mm")), time);
    controls->addWidget(new QLabel(QStringLiteral("当天时段　开始")));
    controls->addWidget(startCombo);
    controls->addWidget(new QLabel(QStringLiteral("结束")));
    controls->addWidget(endCombo);
    controls->addStretch();
    root->addLayout(controls);
    auto *grid = new QGridLayout;
    buttons = new QButtonGroup(this);
    for (const Seat &seat : manager.listSeats()) {
        auto *button = new QPushButton;
        button->setCheckable(true);
        button->setMinimumHeight(80);
        buttons->addButton(button, seat.id);
        grid->addWidget(button, (seat.id - 1) / DemoData::seatColumns,
                        (seat.id - 1) % DemoData::seatColumns);
    }
    root->addLayout(grid);
    message = new QLabel;
    message->setWordWrap(true);
    root->addWidget(message);
    connect(startCombo, &QComboBox::currentIndexChanged, this, [this] {
        updateEndTimes();
        refreshSelection();
    });
    connect(endCombo, &QComboBox::currentIndexChanged, this, &SeatPage::refreshSelection);
    connect(buttons, &QButtonGroup::idClicked, this, &SeatPage::refreshSelection);
}

void SeatPage::updateEndTimes()
{
    const QTime oldEnd = endCombo->currentData().toTime();
    const QSignalBlocker blocker(endCombo);
    endCombo->clear();
    for (QTime time = startCombo->currentData().toTime().addSecs(DemoData::reservationMinutes * 60);
         time <= DemoData::closingTime(); time = time.addSecs(DemoData::reservationMinutes * 60))
        endCombo->addItem(time.toString(QStringLiteral("HH:mm")), time);
    const int index = endCombo->findData(oldEnd);
    if (index >= 0)
        endCombo->setCurrentIndex(index);
}

void SeatPage::refresh(const QDateTime &now)
{
    const bool newDate = currentTime.date() != now.date();
    currentTime = toMinute(now);
    if (newDate) {
        int index = 0;
        while (index + 1 < startCombo->count()
               && startCombo->itemData(index).toTime() < currentTime.time())
            ++index;
        const QSignalBlocker blocker(startCombo);
        startCombo->setCurrentIndex(index);
        updateEndTimes();
    }
    refreshSelection();
}

void SeatPage::refreshSelection()
{
    const QDateTime start(currentTime.date(), startCombo->currentData().toTime());
    const QDateTime end(currentTime.date(), endCombo->currentData().toTime());
    for (const SeatStateView &view : manager.getSeatStates(start, end, currentTime)) {
        const QString state = view.currentState == SeatCurrentState::InUse ? QStringLiteral("使用中")
            : view.currentState == SeatCurrentState::Reserved ? QStringLiteral("已预约") : QStringLiteral("空闲");
        buttons->button(view.seat.id)->setText(QStringLiteral("%1\n当前：%2\n所选时段：%3")
            .arg(view.seat.name, state, !view.availableForSelectedPeriod ? QStringLiteral("已有预约")
                : start < currentTime ? QStringLiteral("已过开始时间") : QStringLiteral("可预约")));
    }
    const int id = buttons->checkedId();
    message->setText(id < 0 ? QStringLiteral("请选择时段及座位；当前状态与所选时段是否占用分别显示。")
        : QStringLiteral("已选择：%1号　%2—%3")
            .arg(id, 2, 10, QLatin1Char('0')).arg(start.time().toString("HH:mm"), end.time().toString("HH:mm")));
    emit selectionChanged(id, start, end);
}

bool showLogin(User &user)
{
    QDialog dialog;
    dialog.setWindowTitle(QStringLiteral("自习室预约系统 - 登录"));
    dialog.setMinimumWidth(360);
    auto *root = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *account = new QLineEdit;
    auto *password = new QLineEdit;
    password->setEchoMode(QLineEdit::Password);
    form->addRow(QStringLiteral("账号"), account);
    form->addRow(QStringLiteral("密码"), password);
    root->addLayout(form);
    auto *message = new QLabel;
    root->addWidget(message);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("登录"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("退出"));
    root->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        for (const User &candidate : DemoData::users()) {
            if (candidate.account == account->text().trimmed()
                && candidate.password == password->text()) {
                user = candidate;
                password->clear();
                dialog.accept();
                return;
            }
        }
        message->setText(QStringLiteral("账号或密码错误"));
        password->clear();
        password->setFocus();
    });
    return dialog.exec() == QDialog::Accepted;
}

class MainWindow : public QMainWindow
{
public:
    MainWindow(const User &user, JsonStore &store)
        : seats(store), reservations(store), attendance(store), statistics(store)
    {
        setWindowTitle(QStringLiteral("校园自习室预约管理系统"));
        resize(1060, user.role == UserRole::Admin ? 720 : 900);
        auto *central = new QWidget;
        setCentralWidget(central);
        auto *root = new QVBoxLayout(central);
        auto *header = new QHBoxLayout;
        const bool admin = user.role == UserRole::Admin;
        header->addWidget(new QLabel(QStringLiteral("%1（%2） - %3").arg(user.name, user.account,
            admin ? QStringLiteral("管理员") : QStringLiteral("学生"))));
        header->addStretch();
        auto *clock = new QLabel;
        header->addWidget(clock);
        auto *refreshButton = new QPushButton(QStringLiteral("刷新"));
        auto *logoutButton = new QPushButton(QStringLiteral("退出登录"));
        header->addWidget(refreshButton);
        header->addWidget(logoutButton);
        root->addLayout(header);
        auto *tabs = new QTabWidget;
        root->addWidget(tabs);
        auto *seatPage = new SeatPage(seats);
        auto *reservationPage = new ReservationPage(reservations, user);
        auto *attendancePage = new AttendanceViolationPage(attendance, store, user);
        auto *statisticsPage = admin ? new StatisticsPage(statistics, user) : nullptr;
        ReservationPage *historyReservations = nullptr;
        if (admin) {
            tabs->addTab(seatPage, QStringLiteral("座位查询"));
            tabs->addTab(reservationPage, QStringLiteral("预约与签到查询"));
            tabs->addTab(attendancePage, QStringLiteral("违约管理"));
            tabs->addTab(statisticsPage, QStringLiteral("统计"));
        } else {
            auto *bookingPage = new QWidget;
            auto *bookingLayout = new QVBoxLayout(bookingPage);
            bookingLayout->addWidget(seatPage);
            bookingLayout->addWidget(reservationPage->bookingControls());
            bookingLayout->addWidget(attendancePage->checkInControls());
            bookingLayout->addWidget(new QLabel(QStringLiteral("预约记录")));
            bookingLayout->addWidget(reservationPage, 1);
            tabs->addTab(bookingPage, QStringLiteral("座位预约"));

            auto *historyPage = new QWidget;
            auto *historyLayout = new QVBoxLayout(historyPage);
            historyReservations = new ReservationPage(reservations, user, historyPage, true);
            historyLayout->addWidget(new QLabel(QStringLiteral("预约与签到记录")));
            historyLayout->addWidget(historyReservations, 1);
            historyLayout->addWidget(new QLabel(QStringLiteral("违约记录")));
            historyLayout->addWidget(attendancePage, 1);
            tabs->addTab(historyPage, QStringLiteral("预约记录"));
        }
        connect(seatPage, &SeatPage::selectionChanged, reservationPage, &ReservationPage::setSelection);
        auto refresh = [=] {
            const QDateTime now = toMinute(QDateTime::currentDateTime());
            clock->setText(QStringLiteral("当前时间：%1").arg(displayTime(now)));
            seatPage->refresh(now);
            reservationPage->refresh();
            attendancePage->refresh();
            if (historyReservations)
                historyReservations->refresh();
            if (statisticsPage)
                statisticsPage->refreshStatistics();
        };
        connect(&store, &JsonStore::dataChanged, this, refresh);
        connect(refreshButton, &QPushButton::clicked, this, refresh);
        connect(logoutButton, &QPushButton::clicked, this, [this] { loggedOut = true; close(); });
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, refresh);
        timer->start(30000);
        refresh();
    }
    bool loggedOut = false;
private:
    SeatManager seats;
    ReservationManager reservations;
    AttendanceViolationManager attendance;
    StatisticsManager statistics;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, &QApplication::quit);
    JsonStore store;
    const OperationResult loaded = store.load();
    if (!loaded.ok) {
        QMessageBox::critical(nullptr, QStringLiteral("数据加载失败"), loaded.message);
        return 1;
    }
    User user;
    while (showLogin(user)) {
        MainWindow window(user, store);
        window.show();
        app.exec();
        if (!window.loggedOut)
            break;
    }
    return 0;
}

QTime DemoData::openingTime()
{
    return QTime(8, 0);
}

QTime DemoData::closingTime()
{
    return QTime(23, 0);
}

SeatManager::SeatManager(const JsonStore &store) :
    dataStore(store)
{
}

QVector<Seat> SeatManager::listSeats() const
{
    return DemoData::seats();
}

QVector<SeatStateView> SeatManager::getSeatStates(
    const QDateTime &start, const QDateTime &end,
    const QDateTime &now) const
{
    const QDateTime currentTime = toMinute(now);
    const QDateTime localStart = start.toLocalTime();
    const QDateTime localEnd = end.toLocalTime();
    if (!currentTime.isValid() || !isHalfHourAligned(localStart)
        || !isHalfHourAligned(localEnd) || localStart >= localEnd
        || localStart.date() != currentTime.date()
        || localEnd.date() != currentTime.date()
        || localStart.time() < DemoData::openingTime()
        || localEnd.time() > DemoData::closingTime())
    {
        return {};
    }

    QVector<SeatStateView> result;
    const AppData &data = dataStore.data();
    const QVector<Seat> seats = listSeats();
    for (const Seat &seat : seats)
    {
        SeatStateView view;
        view.seat = seat;
        view.availableForSelectedPeriod = true;
        for (const Reservation &reservation : data.reservations)
        {
            if (reservation.seatId != seat.id
                || reservation.flag != ReservationFlag::Active)
            {
                continue;
            }

            if (overlaps(localStart, localEnd,
                         reservation.startTime, reservation.endTime))
            {
                view.availableForSelectedPeriod = false;
            }

            const ReservationViewState state = stateOf(reservation, currentTime);
            if (state == ReservationViewState::InUse)
            {
                view.currentState = SeatCurrentState::InUse;
            }
            else if (state == ReservationViewState::WaitingCheckIn
                     && view.currentState != SeatCurrentState::InUse)
            {
                view.currentState = SeatCurrentState::Reserved;
            }
        }
        result.append(view);
    }
    return result;
}
