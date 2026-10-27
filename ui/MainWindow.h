#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "../common/Models.h"
#include "../managers/SeatManager.h"
#include <QMainWindow>

class JsonStore;
class SeatPage;
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const User &user, JsonStore &store, QWidget *parent = nullptr);
    ~MainWindow();
    void addPage(const QString &title, QWidget *page);
    void refreshPages();

signals:
    void logoutRequested();
    void refreshRequested(const QDateTime &now);
    void seatSelectionChanged(int seatId, const QDateTime &start, const QDateTime &end);

private:
    Ui::MainWindow *ui;
    User currentUser;
    SeatManager seatManager;
    SeatPage *seatsPage;
};

#endif // MAINWINDOW_H
