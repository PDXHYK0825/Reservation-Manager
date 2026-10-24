#include "AttendanceViolationManager.h"

#include "../common/Rules.h"
#include "../storage/JsonStore.h"

#include <QUuid>

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
    OperationResult result;
    result.ok = false;

    if (user.role != UserRole::Student || user.account.isEmpty())
    {
        result.message = QStringLiteral("仅学生可以签到");
        return result;
    }

    QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
    {
        result.message = QStringLiteral("操作时间无效");
        return result;
    }

    AppData data = dataStore.data();
    int index = findReservation(data, reservationId);
    if (index == -1)
    {
        result.message = QStringLiteral("预约不存在");
        return result;
    }

    Reservation &reservation = data.reservations[index];
    if (reservation.studentAccount != user.account)
    {
        result.message = QStringLiteral("只能对本人的预约签到");
        return result;
    }
    if (reservation.flag != ReservationFlag::Active)
    {
        result.message = QStringLiteral("已取消或已终止的预约不能签到");
        return result;
    }
    if (!reservation.startTime.isValid() || !reservation.endTime.isValid()
        || reservation.startTime >= reservation.endTime)
    {
        result.message = QStringLiteral("预约时间无效");
        return result;
    }
    if (currentTime < reservation.startTime || currentTime >= reservation.endTime)
    {
        result.message = QStringLiteral("请在预约时段内签到");
        return result;
    }
    if (reservation.checkedInAt.isValid())
    {
        result.message = QStringLiteral("该预约已签到");
        return result;
    }

    reservation.checkedInAt = currentTime;
    result = dataStore.commit(data);
    if (result.ok)
    {
        result.message = QStringLiteral("签到成功");
    }
    return result;
}

OperationResult AttendanceViolationManager::addViolation(
    const User &admin, const QString &reservationId, const QDateTime &now)
{
    OperationResult result;
    result.ok = false;

    if (admin.role != UserRole::Admin || admin.account.isEmpty())
    {
        result.message = QStringLiteral("仅管理员可以登记违约");
        return result;
    }

    QDateTime currentTime = toMinute(now);
    if (!currentTime.isValid())
    {
        result.message = QStringLiteral("操作时间无效");
        return result;
    }

    AppData data = dataStore.data();
    int index = findReservation(data, reservationId);
    if (index == -1)
    {
        result.message = QStringLiteral("预约不存在");
        return result;
    }

    Reservation &reservation = data.reservations[index];
    if (!reservation.startTime.isValid() || !reservation.endTime.isValid()
        || reservation.startTime >= reservation.endTime)
    {
        result.message = QStringLiteral("预约时间无效");
        return result;
    }
    if (currentTime < reservation.startTime)
    {
        result.message = QStringLiteral("预约开始后才能登记违约");
        return result;
    }

    for (int i = 0; i < data.violations.size(); i++)
    {
        if (data.violations[i].reservationId == reservationId
            && !data.violations[i].revoked)
        {
            result.message = QStringLiteral("该预约已有未撤销的违约记录");
            return result;
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

    result = dataStore.commit(data);
    if (result.ok)
    {
        result.message = QStringLiteral("违约登记成功");
    }
    return result;
}

OperationResult AttendanceViolationManager::revokeViolation(
    const User &admin, const QString &violationId)
{
    OperationResult result;
    result.ok = false;

    if (admin.role != UserRole::Admin || admin.account.isEmpty())
    {
        result.message = QStringLiteral("仅管理员可以撤销违约");
        return result;
    }

    AppData data = dataStore.data();
    for (int i = 0; i < data.violations.size(); i++)
    {
        Violation &violation = data.violations[i];
        if (violation.id == violationId)
        {
            if (violation.revoked)
            {
                result.message = QStringLiteral("该违约记录已撤销");
                return result;
            }

            // 保留记录，不恢复原预约。
            violation.revoked = true;
            result = dataStore.commit(data);
            if (result.ok)
            {
                result.message = QStringLiteral("违约已撤销");
            }
            return result;
        }
    }

    result.message = QStringLiteral("违约记录不存在");
    return result;
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
