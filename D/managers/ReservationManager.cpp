#include "ReservationManager.h"

#include "../common/Rules.h"
#include "../storage/JsonStore.h"

#include <algorithm>
#include <QUuid>

namespace {
constexpr int kFirstSeatId = 1;
constexpr int kLastSeatId = 20;

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
    if (seatId < kFirstSeatId || seatId > kLastSeatId)
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
    if (minuteStart.time() < QTime(ReservationRules::OpeningHour, 0)
        || minuteEnd.time() > QTime(ReservationRules::ClosingHour, 0))
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
