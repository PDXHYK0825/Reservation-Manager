#pragma once

#include "../common/Models.h"

class JsonStore;

class StatisticsManager {
public:
    explicit StatisticsManager(const JsonStore& store);

    OperationResult summarize(const User& admin, const QDate& from,
                             const QDate& to,
                             StatisticsSummary& summary) const;

private:
    const JsonStore& m_store;
};
