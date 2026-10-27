// Only linked by MemberATests.vcxproj. No business JSON is read or written.
#include "../common/Rules.h"
#include "../common/DemoData.h"
#include "../storage/JsonStore.h"
#include "../ui/LoginDialog.h"
#include "../ui/MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

namespace
{
JsonStore *previewStore = nullptr;

// Exercise the real main.cpp only when explicitly requested by the test command.
void startLoginCycleTest()
{
    if (!qEnvironmentVariableIsSet("MEMBER_A_LOGIN_CYCLE"))
        return;

    QTimer *timer = new QTimer(QCoreApplication::instance());
    QObject::connect(timer, &QTimer::timeout, timer,
        [session = 0, triedWrongPassword = false,
         firstStore = static_cast<JsonStore *>(nullptr)]() mutable {
        const QVector<User> users = DemoData::users();
        const QWidgetList windows = QApplication::topLevelWidgets();
        for (QWidget *widget : windows)
        {
            if (!widget->isVisible())
                continue;
            LoginDialog *login = qobject_cast<LoginDialog *>(widget);
            if (login)
            {
                if (session == users.size())
                {
                    QFile log(QCoreApplication::applicationDirPath()
                              + QStringLiteral("/login-cycle.txt"));
                    if (!log.open(QIODevice::WriteOnly | QIODevice::Truncate)
                        || log.write("PASS: wrong password; admin -> student01 -> student02; same store; exit\n") < 0)
                        qFatal("Cannot write login-cycle result");
                    login->reject();
                    return;
                }
                login->findChild<QLineEdit *>(QStringLiteral("accountEdit"))
                    ->setText(users[session].account);
                login->findChild<QLineEdit *>(QStringLiteral("passwordEdit"))
                    ->setText(triedWrongPassword ? users[session].password : QStringLiteral("wrong"));
                login->findChild<QPushButton *>(QStringLiteral("loginButton"))->click();
                if (!triedWrongPassword)
                {
                    if (login->result() == QDialog::Accepted)
                        qFatal("Wrong password was accepted");
                    triedWrongPassword = true;
                }
                return;
            }
            MainWindow *window = qobject_cast<MainWindow *>(widget);
            if (window)
            {
                if (session >= users.size()
                    || !window->findChild<QLabel *>(QStringLiteral("userLabel"))
                        ->text().contains(users[session].account))
                    qFatal("Wrong account after login");
                if (session == 0)
                    firstStore = previewStore;
                if (previewStore != firstStore)
                    qFatal("JsonStore changed across sessions");
                session++;
                window->findChild<QPushButton *>(QStringLiteral("logoutButton"))->click();
                return;
            }
        }
    });
    timer->start(50);
    QTimer::singleShot(10000, QCoreApplication::instance(), []() {
        qFatal("Login cycle timed out");
    });
}
}

Q_COREAPP_STARTUP_FUNCTION(startLoginCycleTest)

JsonStore::JsonStore(QObject *parent) : QObject(parent)
{
    previewStore = this;
}

OperationResult JsonStore::load()
{
    return {true, {}};
}

const AppData &JsonStore::data() const
{
    return m_data;
}

OperationResult JsonStore::commit(const AppData &updatedData)
{
    m_data = updatedData;
    emit dataChanged();
    return {true, {}};
}

QDateTime toMinute(const QDateTime &value)
{
    if (!value.isValid())
    {
        return {};
    }
    QDateTime result = value.toLocalTime();
    result.setTime(QTime(result.time().hour(), result.time().minute()));
    return result;
}

bool isHalfHourAligned(const QDateTime &value)
{
    return value.isValid() && value.time().minute() % 30 == 0
        && value.time().second() == 0 && value.time().msec() == 0;
}

bool overlaps(const QDateTime &startA, const QDateTime &endA,
              const QDateTime &startB, const QDateTime &endB)
{
    return startA < endB && endA > startB;
}

ReservationViewState stateOf(const Reservation &reservation, const QDateTime &now)
{
    if (reservation.flag == ReservationFlag::Cancelled)
        return ReservationViewState::Cancelled;
    if (reservation.flag == ReservationFlag::Terminated)
        return ReservationViewState::Terminated;
    if (now >= reservation.endTime)
        return ReservationViewState::Finished;
    if (now < reservation.startTime)
        return ReservationViewState::Upcoming;
    if (reservation.checkedInAt.isValid())
        return ReservationViewState::InUse;
    return ReservationViewState::WaitingCheckIn;
}
