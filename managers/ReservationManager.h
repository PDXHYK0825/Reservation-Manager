#pragma once

// 成员 B：时间规则与预约管理。
#include "common/Models.h"
#include <QtWidgets>

class JsonStore;

QDateTime toMinute(const QDateTime &value);
bool isHalfHourAligned(const QDateTime &value);
bool overlaps(const QDateTime &startA, const QDateTime &endA,
              const QDateTime &startB, const QDateTime &endB);
ReservationViewState stateOf(const Reservation &reservation, const QDateTime &now);

class ReservationManager {
public:
    explicit ReservationManager(JsonStore& store);

    OperationResult createReservation(const User& user, int seatId,
                                      const QDateTime& start,
                                      const QDateTime& end,
                                      const QDateTime& now);
    OperationResult cancelReservation(const User& user,
                                      const QString& reservationId,
                                      const QDateTime& now);
    QVector<Reservation> listReservations(const User& user) const;

private:
    JsonStore& m_store;
};

QString displayTime(const QDateTime &value);
QString reservationStateText(const Reservation &reservation, const QDateTime &now,
                             const QVector<Violation> &violations = {});

class ReservationPage : public QWidget
{
public:
    ReservationPage(ReservationManager &manager, const JsonStore &store,
                    const User &user, QWidget *parent = nullptr,
                    bool historyOnly = false);
    void refresh();
    void setSelection(int seatId, const QDateTime &start, const QDateTime &end);
    QWidget *bookingControls() const { return controlsPanel; }
private:
    ReservationManager &manager;
    const JsonStore &store;
    User user;
    QTableWidget *table = nullptr;
    QLabel *message;
    QWidget *controlsPanel;
    QPushButton *create = nullptr;
    QDateEdit *from = nullptr, *to = nullptr;
    QComboBox *account = nullptr;
    int seatId = -1;
    QDateTime start, end;
};
