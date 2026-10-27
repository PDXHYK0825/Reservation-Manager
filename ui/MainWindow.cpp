#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "SeatPage.h"
#include "../common/Rules.h"
#include "../storage/JsonStore.h"
#include <QTimer>

MainWindow::MainWindow(const User &user, JsonStore &store, QWidget *parent) :
    QMainWindow(parent), ui(new Ui::MainWindow), currentUser(user),
    seatManager(store), seatsPage(new SeatPage(seatManager, this))
{
    ui->setupUi(this);
    const QString role = currentUser.role == UserRole::Admin
        ? QStringLiteral("管理员") : QStringLiteral("学生");
    ui->userLabel->setText(QStringLiteral("%1（%2） - %3")
                          .arg(currentUser.name, currentUser.account, role));
    addPage(QStringLiteral("座位查询"), seatsPage);

    connect(this, &MainWindow::refreshRequested, seatsPage, &SeatPage::refresh);
    connect(seatsPage, &SeatPage::selectionChanged,
            this, &MainWindow::seatSelectionChanged);
    connect(&store, &JsonStore::dataChanged, this, &MainWindow::refreshPages);
    connect(ui->refreshButton, &QPushButton::clicked, this, &MainWindow::refreshPages);
    connect(ui->logoutButton, &QPushButton::clicked, this, [this]() {
        hide();
        emit logoutRequested();
    });

    QTimer *timer = new QTimer(this);
    timer->setInterval(30000);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshPages);
    timer->start();
    refreshPages();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::addPage(const QString &title, QWidget *page)
{
    ui->tabWidget->addTab(page, title);
}

void MainWindow::refreshPages()
{
    const QDateTime now = toMinute(QDateTime::currentDateTime());
    ui->timeLabel->setText(QStringLiteral("当前时间：%1")
        .arg(now.toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    emit refreshRequested(now);
}
