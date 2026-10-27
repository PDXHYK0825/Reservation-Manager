#pragma once

// 成员 A：登录、主窗口、演示账号及座位查询。
#include "common/Models.h"
#include <QtWidgets>

class JsonStore;

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

class SeatManager
{
public:
    explicit SeatManager(const JsonStore &store);
    QVector<Seat> listSeats() const;
    QVector<SeatStateView> getSeatStates(
        const QDateTime &start, const QDateTime &end,
        const QDateTime &now) const;

private:
    const JsonStore &dataStore;
};

class SeatPage : public QWidget
{
    Q_OBJECT
public:
    explicit SeatPage(SeatManager &manager, QWidget *parent = nullptr);
    void refresh(const QDateTime &now);
signals:
    void selectionChanged(int seatId, const QDateTime &start, const QDateTime &end);
private:
    SeatManager &manager;
    QComboBox *startCombo, *endCombo;
    QButtonGroup *buttons;
    QLabel *message;
    QDateTime currentTime;
    void updateEndTimes();
    void refreshSelection();
};
