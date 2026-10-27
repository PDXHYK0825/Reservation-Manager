#include "LoginDialog.h"
#include "ui_LoginDialog.h"

#include "../common/DemoData.h"
#include <QPushButton>

LoginDialog::LoginDialog(QWidget *parent) :
    QDialog(parent), ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    ui->loginButton->setDefault(true);
    connect(ui->loginButton, &QPushButton::clicked, this, &LoginDialog::login);
    connect(ui->exitButton, &QPushButton::clicked, this, &QDialog::reject);
    ui->accountEdit->setFocus();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

User LoginDialog::currentUser() const
{
    return loggedInUser;
}

void LoginDialog::login()
{
    const QString account = ui->accountEdit->text().trimmed();
    const QString password = ui->passwordEdit->text();
    const QVector<User> users = DemoData::users();
    for (const User &user : users)
    {
        if (user.account == account && user.password == password)
        {
            loggedInUser = user;
            ui->passwordEdit->clear();
            accept();
            return;
        }
    }

    ui->messageLabel->setText(QStringLiteral("账号或密码错误"));
    ui->passwordEdit->clear();
    ui->passwordEdit->setFocus();
}
