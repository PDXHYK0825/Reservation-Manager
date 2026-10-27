#include "StatisticsPage.h"

#include "../managers/StatisticsManager.h"

#include <QDateEdit>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

StatisticsPage::StatisticsPage(StatisticsManager& manager,
                               const User& currentUser, QWidget* parent)
    : QWidget(parent), m_manager(manager), m_user(currentUser)
{
    buildUi();
    refreshStatistics();
}

void StatisticsPage::buildUi()
{
    auto* root = new QVBoxLayout(this);
    auto* filters = new QHBoxLayout;

    m_fromDate = new QDateEdit(QDate::currentDate(), this);
    m_toDate = new QDateEdit(QDate::currentDate(), this);
    m_fromDate->setCalendarPopup(true);
    m_toDate->setCalendarPopup(true);

    auto* queryButton = new QPushButton(QStringLiteral("查询"), this);
    connect(queryButton, &QPushButton::clicked,
            this, &StatisticsPage::applyFilter);

    filters->addWidget(new QLabel(QStringLiteral("开始日期"), this));
    filters->addWidget(m_fromDate);
    filters->addWidget(new QLabel(QStringLiteral("结束日期"), this));
    filters->addWidget(m_toDate);
    filters->addWidget(queryButton);
    filters->addStretch();
    root->addLayout(filters);

    m_message = new QLabel(this);
    root->addWidget(m_message);

    auto* summary = new QFormLayout;
    m_reservationCount = new QLabel(QStringLiteral("0"), this);
    m_cancelledCount = new QLabel(QStringLiteral("0"), this);
    m_checkedInCount = new QLabel(QStringLiteral("0"), this);
    m_activeViolationCount = new QLabel(QStringLiteral("0"), this);
    summary->addRow(QStringLiteral("预约总数"), m_reservationCount);
    summary->addRow(QStringLiteral("取消数"), m_cancelledCount);
    summary->addRow(QStringLiteral("签到数"), m_checkedInCount);
    summary->addRow(QStringLiteral("有效违约数"), m_activeViolationCount);
    root->addLayout(summary);
    root->addStretch();
}

void StatisticsPage::applyFilter()
{
    refreshStatistics();
}

void StatisticsPage::refreshStatistics()
{
    StatisticsSummary summary;
    const OperationResult result = m_manager.summarize(
        m_user, m_fromDate->date(), m_toDate->date(), summary);

    m_message->setText(result.message);
    if (!result.ok) {
        m_reservationCount->setText(QStringLiteral("0"));
        m_cancelledCount->setText(QStringLiteral("0"));
        m_checkedInCount->setText(QStringLiteral("0"));
        m_activeViolationCount->setText(QStringLiteral("0"));
        return;
    }

    m_reservationCount->setText(QString::number(summary.reservationCount));
    m_cancelledCount->setText(QString::number(summary.cancelledCount));
    m_checkedInCount->setText(QString::number(summary.checkedInCount));
    m_activeViolationCount->setText(QString::number(summary.activeViolationCount));
}
