#include "stdafx.h"
#include "SceneSerializer.h"
#include "Viewport3D.h"   // ObjectKind 完整定义

#include <QFile>
#include <QSaveFile>
#include <QDataStream>
#include <QTextStream>

// ─────────────────────────────────────────────────────────────
//  ObjectKind ↔ quint32
// ─────────────────────────────────────────────────────────────
static quint32 kindToInt(ObjectKind k)
{
    switch (k) {
    case ObjectKind::Light: return 1;
    case ObjectKind::Lua:   return 2;
    default:                return 0;   // Mesh
    }
}

static ObjectKind intToKind(quint32 v)
{
    switch (v) {
    case 1:  return ObjectKind::Light;
    case 2:  return ObjectKind::Lua;
    default: return ObjectKind::Mesh;
    }
}

// ─────────────────────────────────────────────────────────────
//  saveScene  ─  写入 .YGX（原子写：先落临时文件再 commit）
// ─────────────────────────────────────────────────────────────
SerializeError SceneSerializer::saveScene(const QString& path,
                                          const SceneSnapshot& snap)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return SerializeError::FileOpenFailed;

    QDataStream ds(&file);
    ds.setVersion(QDataStream::Qt_6_0);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);

    // ── 文件头 ─────────────────────────────────────────────
    ds.writeRawData(reinterpret_cast<const char*>(kMagic), 4);
    ds << kVersion;
    ds << static_cast<quint32>(snap.objects.size());

    // ── 摄像机 ─────────────────────────────────────────────
    ds << snap.camYaw << snap.camPitch << snap.camDist;
    ds << snap.camTarget.x() << snap.camTarget.y() << snap.camTarget.z();

    // ── 对象列表 ───────────────────────────────────────────
    for (const SceneObjectData& obj : snap.objects)
    {
        QByteArray nameUtf8 = obj.name.toUtf8();
        ds << static_cast<quint32>(nameUtf8.size());
        ds.writeRawData(nameUtf8.constData(), nameUtf8.size());

        ds << kindToInt(obj.kind);

        ds << obj.position.x() << obj.position.y() << obj.position.z();
        ds << obj.rotation.x() << obj.rotation.y() << obj.rotation.z();
        ds << obj.scale.x()    << obj.scale.y()    << obj.scale.z();

        ds << static_cast<quint8>(obj.color.red())
           << static_cast<quint8>(obj.color.green())
           << static_cast<quint8>(obj.color.blue());

        ds << static_cast<quint8>(obj.visible ? 1u : 0u);
        ds << static_cast<qint32>(obj.layer);

        ds << static_cast<quint32>(obj.vertices.size());
        for (float f : obj.vertices) ds << f;

        ds << static_cast<quint32>(obj.indices.size());
        for (quint32 idx : obj.indices) ds << idx;
    }

    if (ds.status() != QDataStream::Ok)
        return SerializeError::WriteError;

    return file.commit() ? SerializeError::None
                         : SerializeError::WriteError;
}

// ─────────────────────────────────────────────────────────────
//  loadScene  ─  读取 .YGX
// ─────────────────────────────────────────────────────────────
SerializeError SceneSerializer::loadScene(const QString& path,
                                          SceneSnapshot& out)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return SerializeError::FileOpenFailed;

    QDataStream ds(&file);
    ds.setVersion(QDataStream::Qt_6_0);
    ds.setByteOrder(QDataStream::LittleEndian);
    ds.setFloatingPointPrecision(QDataStream::SinglePrecision);

    // ── 魔数 ──────────────────────────────────────────────
    char magic[4];
    ds.readRawData(magic, 4);
    if (magic[0] != kMagic[0] || magic[1] != kMagic[1] ||
        magic[2] != kMagic[2] || magic[3] != kMagic[3])
        return SerializeError::InvalidMagic;

    quint32 version = 0;
    ds >> version;
    if (version > kVersion)
        return SerializeError::UnsupportedVersion;

    quint32 objCount = 0;
    ds >> objCount;

    // ── 摄像机 ────────────────────────────────────────────
    float ctX = 0, ctY = 0, ctZ = 0;
    ds >> out.camYaw >> out.camPitch >> out.camDist
       >> ctX >> ctY >> ctZ;
    out.camTarget = { ctX, ctY, ctZ };

    // ── 对象列表 ──────────────────────────────────────────
    out.objects.clear();
    out.objects.reserve(objCount);

    for (quint32 i = 0; i < objCount; ++i)
    {
        SceneObjectData obj;

        quint32 nameLen = 0;
        ds >> nameLen;
        QByteArray nameBytes(static_cast<int>(nameLen), '\0');
        ds.readRawData(nameBytes.data(), static_cast<int>(nameLen));
        obj.name = QString::fromUtf8(nameBytes);

        quint32 kindInt = 0;
        ds >> kindInt;
        obj.kind = intToKind(kindInt);

        float px, py, pz, rx, ry, rz, sx, sy, sz;
        ds >> px >> py >> pz;   obj.position = { px, py, pz };
        ds >> rx >> ry >> rz;   obj.rotation = { rx, ry, rz };
        ds >> sx >> sy >> sz;   obj.scale    = { sx, sy, sz };

        quint8 r8, g8, b8;
        ds >> r8 >> g8 >> b8;
        obj.color = QColor(r8, g8, b8);

        quint8 vis8 = 1;
        ds >> vis8;
        obj.visible = (vis8 != 0);

        qint32 layer = 0;
        ds >> layer;
        obj.layer = layer;

        quint32 vtxCnt = 0;
        ds >> vtxCnt;
        obj.vertices.resize(vtxCnt);
        for (quint32 j = 0; j < vtxCnt; ++j) ds >> obj.vertices[j];

        quint32 idxCnt = 0;
        ds >> idxCnt;
        obj.indices.resize(idxCnt);
        for (quint32 j = 0; j < idxCnt; ++j) ds >> obj.indices[j];

        if (ds.status() != QDataStream::Ok)
            return SerializeError::ReadError;

        out.objects.push_back(std::move(obj));
    }

    return SerializeError::None;
}

// ─────────────────────────────────────────────────────────────
//  saveLua  ─  写入 .LVL（UTF-8 纯文本，原子写）
// ─────────────────────────────────────────────────────────────
SerializeError SceneSerializer::saveLua(const QString& path,
                                        const QString& luaSource)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return SerializeError::FileOpenFailed;

    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << luaSource;

    if (ts.status() != QTextStream::Ok)
        return SerializeError::WriteError;

    return file.commit() ? SerializeError::None
                         : SerializeError::WriteError;
}

// ─────────────────────────────────────────────────────────────
//  loadLua  ─  读取 .LVL
// ─────────────────────────────────────────────────────────────
SerializeError SceneSerializer::loadLua(const QString& path,
                                        QString& outSource)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return SerializeError::FileOpenFailed;

    QTextStream ts(&file);
    ts.setEncoding(QStringConverter::Utf8);
    outSource = ts.readAll();

    return (ts.status() == QTextStream::Ok) ? SerializeError::None
                                            : SerializeError::ReadError;
}

// ─────────────────────────────────────────────────────────────
//  errorString
// ─────────────────────────────────────────────────────────────
QString SceneSerializer::errorString(SerializeError err)
{
    switch (err) {
    case SerializeError::None:               return {};
    case SerializeError::FileOpenFailed:     return QObject::tr("无法打开文件");
    case SerializeError::InvalidMagic:       return QObject::tr("不是有效的 YGX 场景文件");
    case SerializeError::UnsupportedVersion: return QObject::tr("文件版本过高，请更新软件");
    case SerializeError::ReadError:          return QObject::tr("读取数据时出错，文件可能已损坏");
    case SerializeError::WriteError:         return QObject::tr("写入文件时出错");
    }
    return QObject::tr("未知错误");
}
