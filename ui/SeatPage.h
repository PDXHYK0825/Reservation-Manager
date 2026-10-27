#ifndef SEATPAGE_H
#define SEATPAGE_H

#include "../common/Models.h"
#include <QWidget>

class QButtonGroup;
class SeatManager;
namespace Ui { class SeatPage; }

class SeatPage : public QWidget
{
    Q_OBJECT

public:
    explicit SeatPage(const SeatManager &manager, QWidget *parent = nullptr);
    ~SeatPage();
    void refresh(const QDateTime &now);

signals:
    void selectionChanged(int seatId, const QDateTime &start, const QDateTime &end);

private:
    Ui::SeatPage *ui;
    const SeatManager &seatManager;
    QButtonGroup *seatButtons;
    QDate selectedDate;
    QDateTime currentTime;

    void updateEndTimes();
    void refreshSelection();
    void notifySelection();
};

#endif // SEATPAGE_H
