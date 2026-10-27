#pragma once

// 成员 D：JSON 持久化与统计。
#include "common/Models.h"
#include <QtWidgets>

class JsonStore : public QObject
{
    Q_OBJECT

public:
    explicit JsonStore(QObject *parent = nullptr);
    OperationResult load();
    const AppData &data() const;
    OperationResult commit(const AppData &updatedData);

signals:
    void dataChanged();

private:
    AppData m_data;
};

class StatisticsManager {
public:
    explicit StatisticsManager(const JsonStore& store);

    OperationResult summarize(const User& admin, const QDate& from,
                             const QDate& to,
                             StatisticsSummary& summary) const;

private:
    const JsonStore& m_store;
};

class StatisticsPage : public QWidget
{
public:
    StatisticsPage(StatisticsManager &manager, const User &user, QWidget *parent = nullptr);
    void refreshStatistics();
private:
    StatisticsManager &manager;
    User user;
    QDateEdit *from, *to;
    QLabel *message;
    QLabel *counts[4];
};
