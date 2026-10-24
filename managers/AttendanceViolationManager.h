#ifndef ATTENDANCEVIOLATIONMANAGER_H
#define ATTENDANCEVIOLATIONMANAGER_H

#include "../common/Models.h"

class JsonStore;

class AttendanceViolationManager
{
public:
    explicit AttendanceViolationManager(JsonStore &store);

    OperationResult checkIn(const User &user,
                            const QString &reservationId,
                            const QDateTime &now);

    OperationResult addViolation(const User &admin,
                                 const QString &reservationId,
                                 const QDateTime &now);

    OperationResult revokeViolation(const User &admin,
                                    const QString &violationId);

    QVector<Violation> listViolations(const User &user) const;

private:
    JsonStore &dataStore;

    int findReservation(const AppData &data, const QString &reservationId) const;
};

#endif // ATTENDANCEVIOLATIONMANAGER_H
