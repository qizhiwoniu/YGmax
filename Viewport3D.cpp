#include "stdafx.h"
#include "Viewport3D.h"
#include <QtMath>
#include <QApplication>

Viewport3D::Viewport3D(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(300, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // 整体深色背景
    setAutoFillBackground(false);
    setAttribute(Qt::WA_OpaquePaintEvent);

    buildOverlayToolBar();
}

// ─────────────────────────────────────────────────────────────
//  Overlay 工具栏（对标截图中 Persp / Full Render / View 按钮）
// ─────────────────────────────────────────────────────────────
void Viewport3D::buildOverlayToolBar()
{
    m_overlayBar = new QWidget(this);
    m_overlayBar->setFixedHeight(28);
    m_overlayBar->setStyleSheet(R"(
        QWidget {
            background: rgba(30,30,30,210);
        }
        QComboBox {
            background: transparent;
            border: none;
            color: #cccccc;
            font-size: 12px;
            padding: 0 4px;
        }
        QComboBox::drop-down { border: none; }
        QComboBox QAbstractItemView {
            background: #2d2d2d;
            color: #cccccc;
            selection-background-color: #094771;
        }
        QToolButton {
            background: transparent;
            border: none;
            color: #cccccc;
            font-size: 12px;
            padding: 2px 8px;
        }
        QToolButton:hover {
            background: rgba(255,255,255,20);
            border-radius: 2px;
        }
        QToolButton:checked {
            background: rgba(0,120,212,120);
            border-radius: 2px;
        }
    )");

    auto* bar = new QHBoxLayout(m_overlayBar);
    bar->setContentsMargins(6, 0, 6, 0);
    bar->setSpacing(2);

    // ▶ Play 按钮
    auto* playBtn = new QToolButton(m_overlayBar);
    playBtn->setText("▶");
    playBtn->setToolTip(tr("Play in Viewport"));
    bar->addWidget(playBtn);

    bar->addSpacing(6);

    // Persp / Top / Front / …
    m_viewCombo = new QComboBox(m_overlayBar);
    m_viewCombo->addItems({ tr("Persp"), tr("Top"), tr("Front"), tr("Right"), tr("Left"), tr("Bottom") });
    m_viewCombo->setFixedWidth(72);
    bar->addWidget(m_viewCombo);

    // Full Render / Wireframe / …
    m_renderCombo = new QComboBox(m_overlayBar);
    m_renderCombo->addItems({ tr("Full Render"), tr("Wireframe"), tr("Solid") });
    m_renderCombo->setFixedWidth(100);
    bar->addWidget(m_renderCombo);

    // View 按钮（弹出摄像机设置等，TODO）
    auto* viewBtn = new QToolButton(m_overlayBar);
    viewBtn->setText(tr("View"));
    bar->addWidget(viewBtn);

    bar->addStretch(1);

    // 全屏按钮
    auto* fullBtn = new QToolButton(m_overlayBar);
    fullBtn->setText("⛶");
    fullBtn->setToolTip(tr("Expand Viewport"));
    bar->addWidget(fullBtn);
}

// ─────────────────────────────────────────────────────────────
//  Resize：工具栏跟随宽度
// ─────────────────────────────────────────────────────────────
// (handled in paintEvent layout update — simpler to do here)

// ─────────────────────────────────────────────────────────────
//  paintEvent — 占位渲染（地面网格 + 坐标轴 + 天空渐变）
// ─────────────────────────────────────────────────────────────
void Viewport3D::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect vp = rect();

    // ── 1. 天空/背景渐变 ──────────────────────────────────
    QLinearGradient sky(0, 0, 0, vp.height());
    sky.setColorAt(0.0, QColor("#5b7ea8"));   // 天蓝
    sky.setColorAt(0.5, QColor("#c8d8e8"));   // 地平线淡蓝白
    sky.setColorAt(0.5, QColor("#b0b8be"));   // 地面灰（硬过渡）
    sky.setColorAt(1.0, QColor("#9aa0a6"));
    p.fillRect(vp, sky);

    // ── 2. 地面网格（透视投影简化版）──────────────────────
    p.setRenderHint(QPainter::Antialiasing, false);
    drawGrid(p);

    // ── 3. 坐标轴 ─────────────────────────────────────────
    drawAxes(p);

    // ── 4. Overlay 工具栏定位（每次 paint 重定位）─────────
    m_overlayBar->setGeometry(0, 0, vp.width(), 28);
    m_overlayBar->raise();
}

void Viewport3D::drawGrid(QPainter& p)
{
    const QRect vp = rect();
    // 地平线 Y（视口一半偏下一点，模拟摄像机俯视）
    const int horizY = static_cast<int>(vp.height() * 0.52f);
    // 透视消失点 X
    const int vanishX = vp.width() / 2;

    // 地面底色
    QLinearGradient ground(0, horizY, 0, vp.height());
    ground.setColorAt(0, QColor("#a8abb0"));
    ground.setColorAt(1, QColor("#888c92"));
    p.fillRect(QRect(0, horizY, vp.width(), vp.height() - horizY), ground);

    // 网格线
    p.setPen(QPen(QColor(255, 255, 255, 60), 1));

    const int lines = 16;
    const float spread = vp.width() * 0.9f;

    // 纵向线（向消失点收束）
    for (int i = 0; i <= lines; ++i) {
        float t = static_cast<float>(i) / lines;
        float bx = vp.left() + (vp.width() * t);   // 底部 X
        p.drawLine(QPointF(bx, vp.height()), QPointF(vanishX, horizY));
    }

    // 横向线（越靠近地平线越密）
    for (int row = 1; row <= 10; ++row) {
        float ratio = static_cast<float>(row) / 10.f;
        float fy = vp.height() - (vp.height() - horizY) * ratio * ratio;
        // 计算当前行左右截断（收束效果）
        float lx = vanishX - spread * 0.5f * (1.f - ratio);
        float rx = vanishX + spread * 0.5f * (1.f - ratio);
        p.drawLine(QPointF(lx, fy), QPointF(rx, fy));
    }
}

void Viewport3D::drawAxes(QPainter& p)
{
    const QRect vp = rect();
    const int horizY = static_cast<int>(vp.height() * 0.52f);
    const int originX = vp.width() / 2;
    const int originY = horizY;

    // 轴线长度（简化投影）
    const int len = 60;

    // X 轴（红，右）
    p.setPen(QPen(QColor("#e05050"), 2));
    p.drawLine(originX, originY, originX + len, originY + 12);
    p.drawText(originX + len + 2, originY + 14, "X");

    // Y 轴（绿，上）
    p.setPen(QPen(QColor("#50c050"), 2));
    p.drawLine(originX, originY, originX, originY - len);
    p.drawText(originX + 2, originY - len - 4, "Y");

    // Z 轴（蓝，左前）
    p.setPen(QPen(QColor("#5088e0"), 2));
    p.drawLine(originX, originY, originX - len, originY + 12);
    p.drawText(originX - len - 12, originY + 14, "Z");

    // 原点十字
    p.setPen(QPen(Qt::white, 2));
    p.drawEllipse(QPoint(originX, originY), 3, 3);
}

// ─────────────────────────────────────────────────────────────
//  鼠标 / 滚轮（简易轨道控制骨架，后续接真 3D 后端）
// ─────────────────────────────────────────────────────────────
void Viewport3D::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton ||
        event->button() == Qt::RightButton)
    {
        m_lastMousePos = event->pos();
        event->accept();
    }
}

void Viewport3D::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & (Qt::MiddleButton | Qt::RightButton)) {
        const QPoint delta = event->pos() - m_lastMousePos;
        m_orbitYaw   += delta.x() * 0.5f;
        m_orbitPitch += delta.y() * 0.5f;
        m_orbitPitch  = qBound(-89.f, m_orbitPitch, 89.f);
        m_lastMousePos = event->pos();
        update();   // 触发 repaint（占位器重画坐标轴方向）
        event->accept();
    }
}

void Viewport3D::wheelEvent(QWheelEvent* event)
{
    const float delta = event->angleDelta().y() / 120.f;
    m_zoomDist = qMax(1.f, m_zoomDist - delta);
    update();
    event->accept();
}
