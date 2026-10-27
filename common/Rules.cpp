#include "Rules.h"

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
