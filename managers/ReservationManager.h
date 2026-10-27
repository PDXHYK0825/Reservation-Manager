#pragma once

#include "../common/Models.h"

class JsonStore;

class ReservationManager {
public:
    explicit ReservationManager(JsonStore& store);

    OperationResult createReservation(const User& user, int seatId,
                                      const QDateTime& start,
                                      const QDateTime& end,
                                      const QDateTime& now);
    OperationResult cancelReservation(const User& user,
                                      const QString& reservationId,
                                      const QDateTime& now);
    QVector<Reservation> listReservations(const User& user) const;

private:
    JsonStore& m_store;
};
