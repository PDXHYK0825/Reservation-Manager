#ifndef DEMODATA_H
#define DEMODATA_H

#include "Models.h"
#include <QTime>

namespace DemoData
{
constexpr int seatCount = 20;
constexpr int seatRows = 4;
constexpr int seatColumns = 5;
constexpr int reservationMinutes = 30;

QVector<User> users();
QVector<Seat> seats();
QTime openingTime();
QTime closingTime();
}

#endif // DEMODATA_H
