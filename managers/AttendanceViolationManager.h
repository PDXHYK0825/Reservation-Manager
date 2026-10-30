#pragma once

// 成员 C：签到与违约管理。
#include "common/Models.h"
#include <QtWidgets>

class JsonStore;

class AttendanceViolationManager
{
public:
    explicit AttendanceViolationManager(JsonStore &store);

    OperationResult checkIn(const User &user, const QDateTime &now);

    OperationResult checkIn(const User &user,
                            const QString &reservationId,
                            const QDateTime &now);

    OperationResult addViolation(const User &admin,
                                 const QString &reservationId,
                                 const QDateTime &now);

    OperationResult revokeViolation(const User &admin,
                                    const QString &violationId);

    OperationResult recordExpiredReservations(const QDateTime &now);

    QVector<Violation> listViolations(const User &user) const;

private:
    JsonStore &dataStore;

    int findReservation(const AppData &data, const QString &reservationId) const;
};

class AttendanceViolationPage : public QWidget
{
public:
    AttendanceViolationPage(AttendanceViolationManager &manager, const JsonStore &store,
                            const User &user, QWidget *parent = nullptr);
    void refresh();
    QWidget *checkInControls() const { return controlsPanel; }
private:
    AttendanceViolationManager &manager;
    const JsonStore &store;
    User user;
    QComboBox *reservations = nullptr;
    QTableWidget *violations = nullptr;
    QLabel *message;
    QWidget *controlsPanel;
};
