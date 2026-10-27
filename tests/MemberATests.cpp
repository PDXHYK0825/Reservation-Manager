#include "../common/DemoData.h"
#include "../managers/SeatManager.h"
#include "../storage/JsonStore.h"
#include "../ui/LoginDialog.h"
#include "../ui/MainWindow.h"
#include "../ui/SeatPage.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTimer>
#include <QtTest>

namespace
{
const QDate testDate(2026, 10, 24);

QDateTime at(int hour, int minute = 0)
{
    return QDateTime(testDate, QTime(hour, minute));
}

Reservation booking()
{
    Reservation result;
    result.id = QStringLiteral("test-reservation");
    result.studentAccount = QStringLiteral("student01");
    result.seatId = 1;
    result.startTime = at(11);
    result.endTime = at(12);
    return result;
}
}

class MemberATests : public QObject
{
    Q_OBJECT

private slots:
    void fixedData()
    {
        const QVector<User> users = DemoData::users();
        QCOMPARE(users.size(), 3);
        QCOMPARE(users[0].role, UserRole::Admin);
        QCOMPARE(users[1].role, UserRole::Student);
        QCOMPARE(users[2].role, UserRole::Student);
        JsonStore store;
        SeatManager manager(store);
        const QVector<Seat> seats = manager.listSeats();
        QCOMPARE(seats.size(), 20);
        for (int i = 0; i < seats.size(); i++)
        {
            QCOMPARE(seats[i].id, i + 1);
            QCOMPARE(seats[i].name, QStringLiteral("%1号").arg(i + 1, 2, 10, QLatin1Char('0')));
        }
    }

    void login_data()
    {
        QTest::addColumn<QString>("account");
        QTest::addColumn<QString>("password");
        QTest::addColumn<bool>("success");
        QTest::newRow("admin") << QStringLiteral("admin") << QStringLiteral("123456") << true;
        QTest::newRow("student01") << QStringLiteral("student01") << QStringLiteral("123456") << true;
        QTest::newRow("student02") << QStringLiteral("student02") << QStringLiteral("123456") << true;
        QTest::newRow("wrong-password") << QStringLiteral("admin") << QStringLiteral("bad") << false;
        QTest::newRow("unknown-account") << QStringLiteral("unknown") << QStringLiteral("123456") << false;
        QTest::newRow("empty") << QString() << QString() << false;
    }

    void login()
    {
        QFETCH(QString, account);
        QFETCH(QString, password);
        QFETCH(bool, success);
        LoginDialog dialog;
        QLineEdit *passwordEdit = dialog.findChild<QLineEdit *>(QStringLiteral("passwordEdit"));
        QCOMPARE(passwordEdit->echoMode(), QLineEdit::Password);
        dialog.findChild<QLineEdit *>(QStringLiteral("accountEdit"))->setText(account);
        passwordEdit->setText(password);
        if (account == QStringLiteral("admin") && success)
        {
            QVERIFY(dialog.grab().save(QCoreApplication::applicationDirPath()
                                      + QStringLiteral("/login.png")));
        }
        dialog.findChild<QPushButton *>(QStringLiteral("loginButton"))->click();
        QCOMPARE(dialog.result() == QDialog::Accepted, success);
        QVERIFY(passwordEdit->text().isEmpty());
        if (success)
        {
            const User loggedIn = dialog.currentUser();
            QCOMPARE(loggedIn.account, account);
            const QVector<User> users = DemoData::users();
            for (const User &user : users)
            {
                if (user.account == account)
                {
                    QCOMPARE(loggedIn.name, user.name);
                    QCOMPARE(loggedIn.role, user.role);
                    QCOMPARE(loggedIn.password, user.password);
                }
            }
        }
        else
        {
            QVERIFY(dialog.currentUser().account.isEmpty());
            QVERIFY(!dialog.findChild<QLabel *>(QStringLiteral("messageLabel"))->text().isEmpty());
        }
    }

    void seatStates_data()
    {
        QTest::addColumn<int>("flag");
        QTest::addColumn<bool>("checkedIn");
        QTest::addColumn<QDateTime>("now");
        QTest::addColumn<QDateTime>("start");
        QTest::addColumn<QDateTime>("end");
        QTest::addColumn<int>("currentState");
        QTest::addColumn<bool>("available");
        const int active = static_cast<int>(ReservationFlag::Active);
        const int free = static_cast<int>(SeatCurrentState::Free);
        const int reserved = static_cast<int>(SeatCurrentState::Reserved);
        const int inUse = static_cast<int>(SeatCurrentState::InUse);
        QTest::newRow("free-now-future-conflict") << active << false << at(10) << at(11, 30) << at(12) << free << false;
        QTest::newRow("start-boundary") << active << false << at(11) << at(11) << at(11, 30) << reserved << false;
        QTest::newRow("waiting") << active << false << at(11, 7) << at(11, 30) << at(12) << reserved << false;
        QTest::newRow("in-use") << active << true << at(11, 7) << at(11, 30) << at(12) << inUse << false;
        QTest::newRow("end-boundary") << active << true << at(12) << at(12) << at(12, 30) << free << true;
        QTest::newRow("adjacent-before") << active << false << at(9) << at(10, 30) << at(11) << free << true;
        QTest::newRow("contains-booking") << active << false << at(9) << at(10, 30) << at(12, 30) << free << false;
        QTest::newRow("cancelled") << static_cast<int>(ReservationFlag::Cancelled) << true << at(11, 7) << at(11, 30) << at(12) << free << true;
        QTest::newRow("terminated") << static_cast<int>(ReservationFlag::Terminated) << true << at(11, 7) << at(11, 30) << at(12) << free << true;
        QTest::newRow("last-slot") << active << false << at(22) << at(22, 30) << at(23) << free << true;
    }

    void seatStates()
    {
        QFETCH(int, flag);
        QFETCH(bool, checkedIn);
        QFETCH(QDateTime, now);
        QFETCH(QDateTime, start);
        QFETCH(QDateTime, end);
        QFETCH(int, currentState);
        QFETCH(bool, available);
        AppData data;
        Reservation reservation = booking();
        reservation.flag = static_cast<ReservationFlag>(flag);
        if (checkedIn)
            reservation.checkedInAt = at(11, 7);
        data.reservations.append(reservation);
        JsonStore store;
        store.commit(data);
        QSignalSpy writes(&store, &JsonStore::dataChanged);
        const QVector<SeatStateView> states = SeatManager(store).getSeatStates(start, end, now);
        QCOMPARE(states.size(), 20);
        QCOMPARE(static_cast<int>(states[0].currentState), currentState);
        QCOMPARE(states[0].availableForSelectedPeriod, available);
        for (int i = 1; i < states.size(); i++)
        {
            QCOMPARE(states[i].currentState, SeatCurrentState::Free);
            QVERIFY(states[i].availableForSelectedPeriod);
        }
        QCOMPARE(writes.count(), 0);
        QCOMPARE(store.data().reservations.size(), 1);
        QCOMPARE(store.data().violations.size(), 0);
    }

    void invalidPeriods_data()
    {
        QTest::addColumn<QDateTime>("start");
        QTest::addColumn<QDateTime>("end");
        QTest::addColumn<QDateTime>("now");
        QTest::newRow("invalid-start") << QDateTime() << at(12) << at(10);
        QTest::newRow("invalid-end") << at(11) << QDateTime() << at(10);
        QTest::newRow("invalid-now") << at(11) << at(12) << QDateTime();
        QTest::newRow("reversed") << at(12) << at(11) << at(10);
        QTest::newRow("equal") << at(11) << at(11) << at(10);
        QTest::newRow("quarter-hour") << at(11) << at(11, 15) << at(10);
        QTest::newRow("seconds") << at(11).addSecs(1) << at(12) << at(10);
        QTest::newRow("milliseconds") << at(11) << at(12).addMSecs(1) << at(10);
        QTest::newRow("before-open") << at(7, 30) << at(8, 30) << at(7);
        QTest::newRow("after-close") << at(22, 30) << at(23, 30) << at(10);
        QTest::newRow("tomorrow") << at(11).addDays(1) << at(12).addDays(1) << at(10);
        QTest::newRow("cross-day") << at(22, 30) << at(8).addDays(1) << at(10);
    }

    void invalidPeriods()
    {
        QFETCH(QDateTime, start);
        QFETCH(QDateTime, end);
        QFETCH(QDateTime, now);
        JsonStore store;
        QVERIFY(SeatManager(store).getSeatStates(start, end, now).isEmpty());
    }

    void seatPageSelectionAndRefresh()
    {
        JsonStore store;
        SeatManager manager(store);
        SeatPage page(manager);
        page.refresh(at(10));
        QGridLayout *grid = page.findChild<QGridLayout *>(QStringLiteral("seatLayout"));
        QCOMPARE(grid->rowCount(), 4);
        QCOMPARE(grid->columnCount(), 5);
        QCOMPARE(grid->count(), 20);
        QComboBox *start = page.findChild<QComboBox *>(QStringLiteral("startCombo"));
        QComboBox *end = page.findChild<QComboBox *>(QStringLiteral("endCombo"));
        QCOMPARE(start->count(), 30);
        for (int i = 0; i < start->count(); i++)
            QCOMPARE(start->itemData(i).toTime().minute() % 30, 0);
        start->setCurrentIndex(start->findData(QTime(11, 30)));
        end->setCurrentIndex(end->findData(QTime(12, 0)));
        QSignalSpy selection(&page, &SeatPage::selectionChanged);
        QPushButton *button = page.findChild<QPushButton *>(QStringLiteral("seatButton1"));
        button->click();
        QCOMPARE(selection.last()[0].toInt(), 1);
        QCOMPARE(selection.last()[1].toDateTime(), at(11, 30));
        QCOMPARE(selection.last()[2].toDateTime(), at(12));
        QVERIFY(button->text().contains(QStringLiteral("可预约")));
        AppData data;
        data.reservations.append(booking());
        store.commit(data);
        page.refresh(at(10));
        QVERIFY(button->text().contains(QStringLiteral("当前：空闲")));
        QVERIFY(button->text().contains(QStringLiteral("已有预约")));
        page.resize(900, 550);
        QVERIFY(page.grab().save(QCoreApplication::applicationDirPath()
                                + QStringLiteral("/seat-page.png")));
        page.refresh(at(11, 7));
        QVERIFY(button->text().contains(QStringLiteral("当前：已预约")));
        page.refresh(at(12));
        QVERIFY(button->text().contains(QStringLiteral("当前：空闲")));
        QVERIFY(page.findChild<QLabel *>(QStringLiteral("messageLabel"))->text()
                .contains(QStringLiteral("开始时间已过")));
        page.refresh(at(23, 1));
        QVERIFY(page.findChild<QLabel *>(QStringLiteral("messageLabel"))->text()
                .contains(QStringLiteral("今日可预约时段已结束")));
        page.refresh(at(10).addDays(1));
        QCOMPARE(page.findChild<QLabel *>(QStringLiteral("dateLabel"))->text(),
                 testDate.addDays(1).toString(QStringLiteral("yyyy-MM-dd")));
        for (int i = 0; i < end->count(); i++)
            QVERIFY(end->itemData(i).toTime() > start->currentData().toTime());
    }

    void windowRefreshAndRoleSwitch()
    {
        JsonStore store;
        const QVector<User> users = DemoData::users();
        for (const User &user : users)
        {
            MainWindow window(user, store);
            QLabel *label = window.findChild<QLabel *>(QStringLiteral("userLabel"));
            QVERIFY(label->text().contains(user.account));
            QVERIFY(label->text().contains(user.role == UserRole::Admin
                ? QStringLiteral("管理员") : QStringLiteral("学生")));
            QVERIFY(window.grab().save(QCoreApplication::applicationDirPath()
                + QStringLiteral("/main-%1.png").arg(user.account)));
            QSignalSpy refresh(&window, &MainWindow::refreshRequested);
            QSignalSpy writes(&store, &JsonStore::dataChanged);
            AppData data = store.data();
            data.reservations = {booking()};
            store.commit(data);
            QCOMPARE(refresh.count(), 1);
            QCOMPARE(writes.count(), 1);
            QTimer *timer = window.findChild<QTimer *>();
            QCOMPARE(timer->interval(), 30000);
            QVERIFY(timer->isActive());
            QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
            QCOMPARE(refresh.count(), 2);
            QCOMPARE(writes.count(), 1);
            window.addPage(QStringLiteral("测试页面"), new QWidget);
            QCOMPARE(window.findChild<QTabWidget *>()->count(), 2);
            QSignalSpy logout(&window, &MainWindow::logoutRequested);
            window.findChild<QPushButton *>(QStringLiteral("logoutButton"))->click();
            QCOMPARE(logout.count(), 1);
            QCOMPARE(store.data().reservations.size(), 1);
        }
    }
};

QTEST_MAIN(MemberATests)
#include "MemberATests.moc"
