#include "DemoData.h"

QVector<User> DemoData::users()
{
    return {
        {QStringLiteral("admin"), QStringLiteral("管理员"),
         QStringLiteral("123456"), UserRole::Admin},
        {QStringLiteral("student01"), QStringLiteral("学生一"),
         QStringLiteral("123456"), UserRole::Student},
        {QStringLiteral("student02"), QStringLiteral("学生二"),
         QStringLiteral("123456"), UserRole::Student}
    };
}

QVector<Seat> DemoData::seats()
{
    QVector<Seat> result;
    for (int id = 1; id <= seatCount; id++)
    {
        result.append({id, QStringLiteral("%1号").arg(id, 2, 10, QLatin1Char('0'))});
    }
    return result;
}

QTime DemoData::openingTime()
{
    return QTime(8, 0);
}

QTime DemoData::closingTime()
{
    return QTime(23, 0);
}
