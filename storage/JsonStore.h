#ifndef JSONSTORE_H
#define JSONSTORE_H

#include "../common/Models.h"
#include <QObject>

class JsonStore : public QObject
{
    Q_OBJECT

public:
    explicit JsonStore(QObject *parent = nullptr);
    OperationResult load();
    const AppData &data() const;
    OperationResult commit(const AppData &updatedData);

signals:
    void dataChanged();

private:
    AppData storedData;
};

#endif // JSONSTORE_H
