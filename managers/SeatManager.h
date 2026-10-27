#ifndef SEATMANAGER_H
#define SEATMANAGER_H

#include "../common/Models.h"

class JsonStore;

class SeatManager
{
public:
    explicit SeatManager(const JsonStore &store);
    QVector<Seat> listSeats() const;
    QVector<SeatStateView> getSeatStates(
        const QDateTime &start, const QDateTime &end,
        const QDateTime &now) const;

private:
    const JsonStore &dataStore;
};

#endif // SEATMANAGER_H
