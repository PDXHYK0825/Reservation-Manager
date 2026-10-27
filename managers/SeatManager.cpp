#include "SeatManager.h"

#include "../common/DemoData.h"
#include "../common/Rules.h"
#include "../storage/JsonStore.h"

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
