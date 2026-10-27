#ifndef RULES_H
#define RULES_H

#include "Models.h"

QDateTime toMinute(const QDateTime &value);
bool isHalfHourAligned(const QDateTime &value);
bool overlaps(const QDateTime &startA, const QDateTime &endA,
              const QDateTime &startB, const QDateTime &endB);
ReservationViewState stateOf(const Reservation &reservation, const QDateTime &now);

#endif // RULES_H
