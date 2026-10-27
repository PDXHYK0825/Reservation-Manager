#include "StatisticsManager.h"

#include "../storage/JsonStore.h"

namespace {
OperationResult failure(const QString& message)
{
    return {false, message};
}

OperationResult success(const QString& message)
{
    return {true, message};
}
}

StatisticsManager::StatisticsManager(const JsonStore& store)
    : m_store(store)
{
}

OperationResult StatisticsManager::summarize(
    const User& admin, const QDate& from, const QDate& to,
    StatisticsSummary& summary) const
{
    summary = {};

    if (admin.role != UserRole::Admin || admin.account.trimmed().isEmpty())
        return failure(QStringLiteral("仅管理员可以查看统计数据。"));
    if (!from.isValid() || !to.isValid() || from > to)
        return failure(QStringLiteral("统计日期范围无效。"));

    const AppData& data = m_store.data();
    for (const Reservation& reservation : data.reservations) {
        if (!reservation.startTime.isValid())
            continue;
        const QDate reservationDate = reservation.startTime.date();
        if (reservationDate < from || reservationDate > to)
            continue;

        ++summary.reservationCount;
        if (reservation.flag == ReservationFlag::Cancelled)
            ++summary.cancelledCount;
        if (reservation.checkedInAt.isValid())
            ++summary.checkedInCount;
    }

    for (const Violation& violation : data.violations) {
        if (violation.revoked)
            continue;

        for (const Reservation& reservation : data.reservations) {
            if (reservation.id != violation.reservationId
                || !reservation.startTime.isValid())
                continue;

            const QDate reservationDate = reservation.startTime.date();
            if (reservationDate >= from && reservationDate <= to)
                ++summary.activeViolationCount;
            break;
        }
    }

    return success(QStringLiteral("统计完成。"));
}
