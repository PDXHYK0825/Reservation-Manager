#include "JsonStore.h"

#include "../common/Rules.h"

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace {
const QString kDateTimeFormat = QStringLiteral("yyyy-MM-dd'T'HH:mm");

OperationResult failure(const QString& message)
{
    return {false, message};
}

OperationResult success(const QString& message)
{
    return {true, message};
}

QString dataFilePath()
{
    return QCoreApplication::applicationDirPath()
        + QStringLiteral("/study_room_data.json");
}

bool isUuid(const QString& value)
{
    return !QUuid::fromString(value).isNull();
}

bool readRequiredString(const QJsonObject& object, const QString& key,
                        QString& output)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
        return false;
    output = value.toString();
    return true;
}

bool readDateTime(const QJsonObject& object, const QString& key,
                  QDateTime& output)
{
    const QJsonValue value = object.value(key);
    if (value.isNull()) {
        output = {};
        return true;
    }
    if (!value.isString())
        return false;

    const QString text = value.toString();
    const QDateTime parsed = QDateTime::fromString(text, kDateTimeFormat);
    if (!parsed.isValid() || parsed.toString(kDateTimeFormat) != text)
        return false;

    output = parsed;
    return true;
}

QJsonValue dateTimeJson(const QDateTime& value)
{
    if (!value.isValid())
        return QJsonValue(QJsonValue::Null);
    return QJsonValue(toMinute(value).toString(kDateTimeFormat));
}

bool parseReservation(const QJsonValue& value, Reservation& reservation,
                      QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("预约记录格式无效。");
        return false;
    }

    const QJsonObject object = value.toObject();
    QString flag;
    if (!readRequiredString(object, QStringLiteral("id"), reservation.id)
        || !readRequiredString(object, QStringLiteral("studentAccount"),
                               reservation.studentAccount)
        || !readRequiredString(object, QStringLiteral("flag"), flag)
        || !object.value(QStringLiteral("seatId")).isDouble()
        || !readDateTime(object, QStringLiteral("startTime"), reservation.startTime)
        || !readDateTime(object, QStringLiteral("endTime"), reservation.endTime)
        || !readDateTime(object, QStringLiteral("checkedInAt"), reservation.checkedInAt)
        || !readDateTime(object, QStringLiteral("closedAt"), reservation.closedAt)) {
        error = QStringLiteral("预约记录缺少字段或字段类型错误。");
        return false;
    }

    const double seatId = object.value(QStringLiteral("seatId")).toDouble();
    if (seatId != static_cast<int>(seatId)) {
        error = QStringLiteral("预约记录的座位编号无效。");
        return false;
    }
    reservation.seatId = static_cast<int>(seatId);

    if (flag == QStringLiteral("active"))
        reservation.flag = ReservationFlag::Active;
    else if (flag == QStringLiteral("cancelled"))
        reservation.flag = ReservationFlag::Cancelled;
    else if (flag == QStringLiteral("terminated"))
        reservation.flag = ReservationFlag::Terminated;
    else {
        error = QStringLiteral("预约记录的状态值无效。");
        return false;
    }

    if (!isUuid(reservation.id) || reservation.studentAccount.trimmed().isEmpty()
        || reservation.seatId < 1 || reservation.seatId > 20
        || !reservation.startTime.isValid() || !reservation.endTime.isValid()
        || reservation.endTime <= reservation.startTime
        || !isHalfHourAligned(reservation.startTime)
        || !isHalfHourAligned(reservation.endTime)) {
        error = QStringLiteral("预约记录包含无效编号、时间或座位信息。");
        return false;
    }

    if (reservation.checkedInAt.isValid()
        && (reservation.checkedInAt < reservation.startTime
            || reservation.checkedInAt >= reservation.endTime)) {
        error = QStringLiteral("预约记录中的签到时间不在预约时段内。");
        return false;
    }

    const bool isClosed = reservation.flag != ReservationFlag::Active;
    if (isClosed != reservation.closedAt.isValid()) {
        error = QStringLiteral("预约记录的关闭时间与状态不一致。");
        return false;
    }
    if (reservation.closedAt.isValid()
        && (reservation.closedAt < reservation.startTime
            || reservation.closedAt >= reservation.endTime)) {
        error = QStringLiteral("预约记录中的关闭时间无效。");
        return false;
    }

    return true;
}

bool parseViolation(const QJsonValue& value, Violation& violation,
                    QString& error)
{
    if (!value.isObject()) {
        error = QStringLiteral("违约记录格式无效。");
        return false;
    }

    const QJsonObject object = value.toObject();
    if (!readRequiredString(object, QStringLiteral("id"), violation.id)
        || !readRequiredString(object, QStringLiteral("reservationId"),
                               violation.reservationId)
        || !readDateTime(object, QStringLiteral("recordedAt"), violation.recordedAt)
        || !object.value(QStringLiteral("revoked")).isBool()) {
        error = QStringLiteral("违约记录缺少字段或字段类型错误。");
        return false;
    }

    violation.revoked = object.value(QStringLiteral("revoked")).toBool();
    if (!isUuid(violation.id) || !isUuid(violation.reservationId)
        || !violation.recordedAt.isValid()) {
        error = QStringLiteral("违约记录包含无效编号或登记时间。");
        return false;
    }
    return true;
}

bool validateData(const AppData& data, QString& error)
{
    QSet<QString> reservationIds;
    for (const Reservation& reservation : data.reservations) {
        const bool validFlag = reservation.flag == ReservationFlag::Active
            || reservation.flag == ReservationFlag::Cancelled
            || reservation.flag == ReservationFlag::Terminated;
        if (!isUuid(reservation.id) || reservationIds.contains(reservation.id)
            || reservation.studentAccount.trimmed().isEmpty()
            || !validFlag
            || reservation.seatId < 1 || reservation.seatId > 20
            || !reservation.startTime.isValid() || !reservation.endTime.isValid()
            || reservation.endTime <= reservation.startTime
            || !isHalfHourAligned(reservation.startTime)
            || !isHalfHourAligned(reservation.endTime)) {
            error = QStringLiteral("预约数据包含重复编号或无效记录。");
            return false;
        }
        reservationIds.insert(reservation.id);

        if (reservation.checkedInAt.isValid()
            && (reservation.checkedInAt < reservation.startTime
                || reservation.checkedInAt >= reservation.endTime)) {
            error = QStringLiteral("签到时间必须位于预约时段内。");
            return false;
        }

        const bool isClosed = reservation.flag != ReservationFlag::Active;
        if (isClosed != reservation.closedAt.isValid()
            || (reservation.closedAt.isValid()
                && (reservation.closedAt < reservation.startTime
                    || reservation.closedAt >= reservation.endTime))) {
            error = QStringLiteral("预约的状态与关闭时间不一致。");
            return false;
        }
    }

    QSet<QString> violationIds;
    QSet<QString> activeViolationReservations;
    for (const Violation& violation : data.violations) {
        if (!isUuid(violation.id) || violationIds.contains(violation.id)
            || !isUuid(violation.reservationId)
            || !reservationIds.contains(violation.reservationId)
            || !violation.recordedAt.isValid()) {
            error = QStringLiteral("违约数据包含重复编号、无效编号或不存在的预约关联。");
            return false;
        }
        violationIds.insert(violation.id);
        if (!violation.revoked) {
            if (activeViolationReservations.contains(violation.reservationId)) {
                error = QStringLiteral("同一预约存在多条未撤销的违约记录。");
                return false;
            }
            activeViolationReservations.insert(violation.reservationId);
        }
    }

    return true;
}

QJsonObject reservationJson(const Reservation& reservation)
{
    QString flag;
    switch (reservation.flag) {
    case ReservationFlag::Active: flag = QStringLiteral("active"); break;
    case ReservationFlag::Cancelled: flag = QStringLiteral("cancelled"); break;
    case ReservationFlag::Terminated: flag = QStringLiteral("terminated"); break;
    }

    QJsonObject object;
    object.insert(QStringLiteral("id"), reservation.id);
    object.insert(QStringLiteral("studentAccount"), reservation.studentAccount);
    object.insert(QStringLiteral("seatId"), reservation.seatId);
    object.insert(QStringLiteral("startTime"), dateTimeJson(reservation.startTime));
    object.insert(QStringLiteral("endTime"), dateTimeJson(reservation.endTime));
    object.insert(QStringLiteral("flag"), flag);
    object.insert(QStringLiteral("checkedInAt"), dateTimeJson(reservation.checkedInAt));
    object.insert(QStringLiteral("closedAt"), dateTimeJson(reservation.closedAt));
    return object;
}

QJsonObject violationJson(const Violation& violation)
{
    QJsonObject object;
    object.insert(QStringLiteral("id"), violation.id);
    object.insert(QStringLiteral("reservationId"), violation.reservationId);
    object.insert(QStringLiteral("recordedAt"), dateTimeJson(violation.recordedAt));
    object.insert(QStringLiteral("revoked"), violation.revoked);
    return object;
}

QJsonDocument toJsonDocument(const AppData& data)
{
    QJsonArray reservations;
    for (const Reservation& reservation : data.reservations)
        reservations.append(reservationJson(reservation));

    QJsonArray violations;
    for (const Violation& violation : data.violations)
        violations.append(violationJson(violation));

    QJsonObject root;
    root.insert(QStringLiteral("reservations"), reservations);
    root.insert(QStringLiteral("violations"), violations);
    return QJsonDocument(root);
}
}

JsonStore::JsonStore(QObject *parent)
    : QObject(parent)
{
}

OperationResult JsonStore::load()
{
    const QString path = dataFilePath();
    if (!QFile::exists(path)) {
        m_data = {};
        return success(QStringLiteral("已初始化空数据。"));
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return failure(QStringLiteral("无法读取数据文件：%1").arg(file.errorString()));

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return failure(QStringLiteral("数据文件格式错误：%1").arg(parseError.errorString()));

    const QJsonObject root = document.object();
    if (root.size() != 2
        || !root.value(QStringLiteral("reservations")).isArray()
        || !root.value(QStringLiteral("violations")).isArray()) {
        return failure(QStringLiteral("数据文件必须包含预约和违约两个数组。"));
    }

    AppData loaded;
    QString error;
    const QJsonArray reservations = root.value(QStringLiteral("reservations")).toArray();
    loaded.reservations.reserve(reservations.size());
    for (const QJsonValue& value : reservations) {
        Reservation reservation;
        if (!parseReservation(value, reservation, error))
            return failure(error);
        loaded.reservations.push_back(reservation);
    }

    const QJsonArray violations = root.value(QStringLiteral("violations")).toArray();
    loaded.violations.reserve(violations.size());
    for (const QJsonValue& value : violations) {
        Violation violation;
        if (!parseViolation(value, violation, error))
            return failure(error);
        loaded.violations.push_back(violation);
    }

    if (!validateData(loaded, error))
        return failure(error);

    m_data = loaded;
    emit dataChanged();
    return success(QStringLiteral("数据加载成功。"));
}

const AppData& JsonStore::data() const
{
    return m_data;
}

OperationResult JsonStore::commit(const AppData& updatedData)
{
    QString error;
    if (!validateData(updatedData, error))
        return failure(error);

    QSaveFile file(dataFilePath());
    if (!file.open(QIODevice::WriteOnly))
        return failure(QStringLiteral("无法保存数据文件：%1").arg(file.errorString()));

    const QByteArray bytes = toJsonDocument(updatedData).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return failure(QStringLiteral("写入数据文件失败：%1").arg(file.errorString()));
    }
    if (!file.commit())
        return failure(QStringLiteral("提交数据文件失败：%1").arg(file.errorString()));

    m_data = updatedData;
    emit dataChanged();
    return success(QStringLiteral("数据保存成功。"));
}
