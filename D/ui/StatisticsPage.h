#pragma once

#include "../common/Models.h"

#include <QWidget>

class QDateEdit;
class QLabel;
class StatisticsManager;

class StatisticsPage : public QWidget {
    Q_OBJECT

public:
    StatisticsPage(StatisticsManager& manager, const User& currentUser,
                   QWidget* parent = nullptr);

public slots:
    void refreshStatistics();

private slots:
    void applyFilter();

private:
    void buildUi();

    StatisticsManager& m_manager;
    User m_user;
    QDateEdit* m_fromDate = nullptr;
    QDateEdit* m_toDate = nullptr;
    QLabel* m_reservationCount = nullptr;
    QLabel* m_cancelledCount = nullptr;
    QLabel* m_checkedInCount = nullptr;
    QLabel* m_activeViolationCount = nullptr;
    QLabel* m_message = nullptr;
};
