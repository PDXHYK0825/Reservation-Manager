#include "SeatPage.h"
#include "ui_SeatPage.h"

#include "../common/DemoData.h"
#include "../common/Rules.h"
#include "../managers/SeatManager.h"
#include <QButtonGroup>
#include <QPushButton>
#include <QSignalBlocker>

SeatPage::SeatPage(const SeatManager &manager, QWidget *parent) :
    QWidget(parent), ui(new Ui::SeatPage), seatManager(manager),
    seatButtons(new QButtonGroup(this))
{
    ui->setupUi(this);
    const QVector<Seat> seats = seatManager.listSeats();
    for (int i = 0; i < seats.size(); i++)
    {
        QPushButton *button = new QPushButton(seats[i].name, this);
        button->setObjectName(QStringLiteral("seatButton%1").arg(seats[i].id));
        button->setCheckable(true);
        button->setMinimumSize(125, 85);
        seatButtons->addButton(button, seats[i].id);
        ui->seatLayout->addWidget(button, i / DemoData::seatColumns,
                                 i % DemoData::seatColumns);
    }

    for (QTime time = DemoData::openingTime(); time < DemoData::closingTime();
         time = time.addSecs(DemoData::reservationMinutes * 60))
    {
        ui->startCombo->addItem(time.toString(QStringLiteral("HH:mm")), time);
    }
    connect(ui->startCombo, &QComboBox::currentIndexChanged, this, [this]() {
        updateEndTimes();
        refreshSelection();
    });
    connect(ui->endCombo, &QComboBox::currentIndexChanged,
            this, &SeatPage::refreshSelection);
    connect(seatButtons, &QButtonGroup::idClicked,
            this, &SeatPage::notifySelection);
    refresh(QDateTime::currentDateTime());
}

SeatPage::~SeatPage()
{
    delete ui;
}

void SeatPage::updateEndTimes()
{
    const QSignalBlocker blocker(ui->endCombo);
    const QTime previousEnd = ui->endCombo->currentData().toTime();
    const QTime start = ui->startCombo->currentData().toTime();
    ui->endCombo->clear();
    for (QTime time = start.addSecs(DemoData::reservationMinutes * 60);
         time <= DemoData::closingTime();
         time = time.addSecs(DemoData::reservationMinutes * 60))
    {
        ui->endCombo->addItem(time.toString(QStringLiteral("HH:mm")), time);
    }
    const int previousIndex = ui->endCombo->findData(previousEnd);
    if (previousIndex >= 0)
    {
        ui->endCombo->setCurrentIndex(previousIndex);
    }
}

void SeatPage::refresh(const QDateTime &now)
{
    currentTime = toMinute(now);
    if (selectedDate != currentTime.date())
    {
        selectedDate = currentTime.date();
        ui->dateLabel->setText(selectedDate.toString(QStringLiteral("yyyy-MM-dd")));
        int startIndex = 0;
        while (startIndex + 1 < ui->startCombo->count()
               && ui->startCombo->itemData(startIndex).toTime() < currentTime.time())
        {
            startIndex++;
        }
        const QSignalBlocker blocker(ui->startCombo);
        ui->startCombo->setCurrentIndex(startIndex);
        updateEndTimes();
    }
    refreshSelection();
}

void SeatPage::refreshSelection()
{
    const QDateTime start(selectedDate, ui->startCombo->currentData().toTime());
    const QDateTime end(selectedDate, ui->endCombo->currentData().toTime());
    const QVector<SeatStateView> states = seatManager.getSeatStates(start, end, currentTime);
    if (states.isEmpty())
    {
        ui->messageLabel->setText(QStringLiteral("时段无效，请重新选择"));
        for (QAbstractButton *button : seatButtons->buttons())
        {
            button->setEnabled(false);
            button->setText(QStringLiteral("%1号\n当前：—\n所选时段：—")
                            .arg(seatButtons->id(button), 2, 10, QLatin1Char('0')));
        }
        notifySelection();
        return;
    }

    const bool pastStart = start < currentTime;
    if (currentTime.time() > DemoData::closingTime()
                            .addSecs(-DemoData::reservationMinutes * 60))
    {
        ui->messageLabel->setText(QStringLiteral("今日可预约时段已结束。"));
    }
    else
    {
        ui->messageLabel->setText(pastStart
            ? QStringLiteral("所选开始时间已过，请选择后续时段。")
            : QStringLiteral("请选择座位；当前状态与所选时段是否占用分别显示。"));
    }
    for (const SeatStateView &view : states)
    {
        QString currentState = QStringLiteral("空闲");
        if (view.currentState == SeatCurrentState::Reserved)
        {
            currentState = QStringLiteral("已预约");
        }
        else if (view.currentState == SeatCurrentState::InUse)
        {
            currentState = QStringLiteral("使用中");
        }
        const QString periodState = !view.availableForSelectedPeriod
            ? QStringLiteral("已有预约")
            : (pastStart ? QStringLiteral("已过开始时间") : QStringLiteral("可预约"));
        QAbstractButton *button = seatButtons->button(view.seat.id);
        button->setEnabled(true);
        button->setText(QStringLiteral("%1\n当前：%2\n所选时段：%3")
                        .arg(view.seat.name, currentState, periodState));
    }
    notifySelection();
}

void SeatPage::notifySelection()
{
    const int seatId = seatButtons->checkedId();
    ui->selectionLabel->setText(seatId < 0 ? QStringLiteral("尚未选择座位")
        : QStringLiteral("已选择：%1号").arg(seatId, 2, 10, QLatin1Char('0')));
    emit selectionChanged(seatId,
        QDateTime(selectedDate, ui->startCombo->currentData().toTime()),
        QDateTime(selectedDate, ui->endCombo->currentData().toTime()));
}
