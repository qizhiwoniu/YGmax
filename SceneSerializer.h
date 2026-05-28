#pragma once
// ═══════════════════════════════════════════════════════════════
//  SceneSerializer.h
//
//  .YGX  ─ 二进制场景文件（Little-Endian）
//  .LVL  ─ UTF-8 纯文本 Lua 脚本文件
//
//  ┌─────────────────────── YGX 文件布局 ──────────────────────┐
//  │ Magic      u8[4]  = {'Y','G','X', 0x01}                   │
//  │ Version    u32    = 1                                      │
//  │ ObjectCnt  u32    = N                                      │
//  │ CamYaw/Pitch/Dist  f32×3                                   │
//  │ CamTargX/Y/Z       f32×3                                   │
//  │ ─── 每个对象 ────────────────────────────────────────────  │
//  │   NameLen  u32                                             │
//  │   Name     u8[NameLen]  (UTF-8，无 null)                  │
//  │   Kind     u32   0=Mesh  1=Light  2=Lua                   │
//  │   PosX/Y/Z f32×3                                          │
//  │   RotX/Y/Z f32×3  (欧拉角，度)                            │
//  │   SclX/Y/Z f32×3                                          │
//  │   ColorR/G/B  u8×3                                        │
//  │   Visible  u8     0 or 1                                   │
//  │   Layer    i32                                             │
//  │   VtxCnt   u32                                             │
//  │   Vertices f32[VtxCnt]                                     │
//  │   IdxCnt   u32                                             │
//  │   Indices  u32[IdxCnt]                                     │
//  └────────────────────────────────────────────────────────────┘
// ═══════════════════════════════════════════════════════════════

#include <QString>
#include <QVector3D>
#include <QColor>
#include <vector>

// 前向声明（避免把整个 Viewport3D.h 拖入序列化层）
enum class ObjectKind;

// ───────────────────────────────────────────────────────────────
//  SceneObjectData  ─  纯数据快照，不含任何 OpenGL 资源
// ───────────────────────────────────────────────────────────────
struct SceneObjectData
{
    QString    name;
    ObjectKind kind;

    QVector3D  position { 0.f, 0.f, 0.f };
    QVector3D  rotation { 0.f, 0.f, 0.f };
    QVector3D  scale    { 1.f, 1.f, 1.f };
    QColor     color    { 180, 140, 80  };
    bool       visible  = true;
    int        layer    = 0;

    std::vector<float>    vertices;   // x,y,z,nx,ny,nz ...
    std::vector<uint32_t> indices;
};

// ───────────────────────────────────────────────────────────────
//  SceneSnapshot  ─  可完整序列化的场景状态
// ───────────────────────────────────────────────────────────────
struct SceneSnapshot
{
    float     camYaw   =  35.f;
    float     camPitch =  25.f;
    float     camDist  =  12.f;
    QVector3D camTarget{ 0.f, 0.f, 0.f };

    std::vector<SceneObjectData> objects;
};

// ───────────────────────────────────────────────────────────────
//  错误码
// ───────────────────────────────────────────────────────────────
enum class SerializeError
{
    None,
    FileOpenFailed,
    InvalidMagic,
    UnsupportedVersion,
    ReadError,
    WriteError
};

// ───────────────────────────────────────────────────────────────
//  SceneSerializer  ─  纯静态工具类
// ───────────────────────────────────────────────────────────────
class SceneSerializer
{
public:
    static SerializeError saveScene(const QString& path,
                                    const SceneSnapshot& snapshot);

    static SerializeError loadScene(const QString& path,
                                    SceneSnapshot& outSnapshot);

    static SerializeError saveLua(const QString& path,
                                  const QString& luaSource);

    static SerializeError loadLua(const QString& path,
                                  QString& outSource);

    static QString errorString(SerializeError err);

private:
    SceneSerializer() = delete;

    static constexpr quint8  kMagic[4] = { 'Y', 'G', 'X', 0x01 };
    static constexpr quint32 kVersion  = 1;
};
