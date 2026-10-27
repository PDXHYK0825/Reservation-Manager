#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "../common/Models.h"
#include <QDialog>

namespace Ui { class LoginDialog; }

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();
    User currentUser() const;

private:
    Ui::LoginDialog *ui;
    User loggedInUser;

    void login();
};

#endif // LOGINDIALOG_H
