#pragma once

#include "../common/Models.h"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class ReservationManager;

// Student mode provides booking and cancellation; administrator mode provides
// read-only reservation search. The page is injected with the logged-in user.
class ReservationPage : public QWidget {
    Q_OBJECT
public:
    ReservationPage(ReservationManager& manager, const User& currentUser,
                    QWidget* parent = nullptr);

public slots:
    void refreshReservations();

private slots:
    void createReservation();
    void cancelSelectedReservation();
    void applyAdminFilter();

private:
    void buildUi();
    void populateTable(const QVector<Reservation>& reservations);
    QString stateText(const Reservation& reservation,
                      const QDateTime& now) const;

    ReservationManager& m_manager;
    User m_user;
    QComboBox* m_seatCombo = nullptr;
    QComboBox* m_startCombo = nullptr;
    QComboBox* m_endCombo = nullptr;
    QDateEdit* m_fromDate = nullptr;
    QDateEdit* m_toDate = nullptr;
    QComboBox* m_accountFilter = nullptr;
    QTableWidget* m_table = nullptr;
    QPushButton* m_cancelButton = nullptr;
    QLabel* m_message = nullptr;
};
