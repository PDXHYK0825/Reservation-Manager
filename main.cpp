#include "storage/JsonStore.h"
#include "ui/LoginDialog.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QObject::connect(&app, &QApplication::lastWindowClosed, &app, &QApplication::quit);

    JsonStore store;
    const OperationResult loaded = store.load();
    if (!loaded.ok)
    {
        QMessageBox::critical(nullptr, QStringLiteral("数据加载失败"), loaded.message);
        return 1;
    }

    while (true)
    {
        LoginDialog login;
        if (login.exec() != QDialog::Accepted)
        {
            break;
        }

        MainWindow window(login.currentUser(), store);
        bool logout = false;
        QObject::connect(&window, &MainWindow::logoutRequested, &app, [&]() {
            logout = true;
            app.quit();
        });
        window.show();
        app.exec();
        if (!logout)
        {
            break;
        }
    }
    return 0;
}
