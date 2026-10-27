#pragma once
#include <QDateTime>
#include <QString>
#include <QVector>

enum class UserRole { Admin, Student };
enum class ReservationFlag { Active, Cancelled, Terminated };
enum class SeatCurrentState { Free, Reserved, InUse };
enum class ReservationViewState {
    Upcoming, WaitingCheckIn, InUse, Finished, Cancelled, Terminated
};

struct User {
    QString account;
    QString name;
    QString password;
    UserRole role = UserRole::Student;
};

struct Seat {
    int id = 0;
    QString name;
};

struct Reservation {
    QString id;
    QString studentAccount;
    int seatId = 0;
    QDateTime startTime;
    QDateTime endTime;
    ReservationFlag flag = ReservationFlag::Active;
    QDateTime checkedInAt;
    QDateTime closedAt;
};

struct Violation {
    QString id;
    QString reservationId;
    QDateTime recordedAt;
    bool revoked = false;
};

struct AppData {
    QVector<Reservation> reservations;
    QVector<Violation> violations;
};

struct OperationResult {
    bool ok = false;
    QString message;
};

struct SeatStateView {
    Seat seat;
    SeatCurrentState currentState = SeatCurrentState::Free;
    bool availableForSelectedPeriod = false;
};

struct StatisticsSummary {
    int reservationCount = 0;
    int cancelledCount = 0;
    int checkedInCount = 0;
    int activeViolationCount = 0;
};
