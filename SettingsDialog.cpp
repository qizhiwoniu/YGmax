#include "stdafx.h"
#include "SettingsDialog.h"
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("设置"));
    setFixedSize(520, 420);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 12);
    mainLayout->setSpacing(12);

    // ── Tab 页 ────────────────────────────────────────────────
    auto* tabs = new QTabWidget(this);
    buildGeneralTab(tabs);
    buildRenderTab(tabs);
    buildShortcutTab(tabs);
    buildGamepadTab(tabs);
    mainLayout->addWidget(tabs, 1);

    // ── 底部按钮 ──────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_okBtn     = new QPushButton(tr("确定"),   this);
    m_cancelBtn = new QPushButton(tr("取消"),   this);
    m_applyBtn  = new QPushButton(tr("应用"),   this);
    m_okBtn->setFixedWidth(80);
    m_cancelBtn->setFixedWidth(80);
    m_applyBtn->setFixedWidth(80);
    m_okBtn->setDefault(true);
    btnRow->addWidget(m_okBtn);
    btnRow->addWidget(m_cancelBtn);
    btnRow->addWidget(m_applyBtn);
    mainLayout->addLayout(btnRow);

    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_okBtn,     &QPushButton::clicked, this, [this]() {
        // TODO: 在此读取各控件值并持久化到 QSettings
        accept();
    });
    connect(m_applyBtn, &QPushButton::clicked, this, [this]() {
        // TODO: 同上，但不关闭对话框
    });

    applyStyle();
}

// ─────────────────────────────────────────────────────────────
//  通用页
// ─────────────────────────────────────────────────────────────
void SettingsDialog::buildGeneralTab(QTabWidget* tabs)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(16);

    // ── 界面 ──────────────────────────────────────────────────
    auto* uiGroup = new QGroupBox(tr("界面"), w);
    auto* uiForm  = new QFormLayout(uiGroup);
    uiForm->setLabelAlignment(Qt::AlignRight);
    uiForm->setSpacing(10);

    m_langCombo = new QComboBox;
    m_langCombo->addItems({ tr("简体中文"), tr("English") });
    uiForm->addRow(tr("语言："), m_langCombo);

    m_restoreLayout = new QCheckBox(tr("启动时恢复上次布局"));
    m_restoreLayout->setChecked(true);
    uiForm->addRow(QString(), m_restoreLayout);

    lay->addWidget(uiGroup);

    // ── 自动保存 ──────────────────────────────────────────────
    auto* saveGroup = new QGroupBox(tr("自动保存"), w);
    auto* saveForm  = new QFormLayout(saveGroup);
    saveForm->setLabelAlignment(Qt::AlignRight);
    saveForm->setSpacing(10);

    m_autoSaveCheck = new QCheckBox(tr("启用自动保存"));
    m_autoSaveCheck->setChecked(true);
    saveForm->addRow(QString(), m_autoSaveCheck);

    m_autoSaveSpin = new QSpinBox;
    m_autoSaveSpin->setRange(1, 60);
    m_autoSaveSpin->setValue(5);
    m_autoSaveSpin->setSuffix(tr(" 分钟"));
    m_autoSaveSpin->setFixedWidth(100);
    saveForm->addRow(tr("间隔："), m_autoSaveSpin);

    // 自动保存开关联动间隔控件
    connect(m_autoSaveCheck, &QCheckBox::toggled,
            m_autoSaveSpin,  &QSpinBox::setEnabled);

    lay->addWidget(saveGroup);
    lay->addStretch();

    tabs->addTab(w, tr("通用"));
}

// ─────────────────────────────────────────────────────────────
//  渲染页
// ─────────────────────────────────────────────────────────────
void SettingsDialog::buildRenderTab(QTabWidget* tabs)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(16);

    // ── 质量 ──────────────────────────────────────────────────
    auto* qualGroup = new QGroupBox(tr("质量"), w);
    auto* qualForm  = new QFormLayout(qualGroup);
    qualForm->setLabelAlignment(Qt::AlignRight);
    qualForm->setSpacing(10);

    m_msaaCombo = new QComboBox;
    m_msaaCombo->addItems({ tr("关闭"), "2×", "4×", "8×" });
    m_msaaCombo->setCurrentIndex(2);  // 默认 4×
    qualForm->addRow(tr("多重采样（MSAA）："), m_msaaCombo);

    m_vsyncCheck = new QCheckBox(tr("垂直同步（V-Sync）"));
    m_vsyncCheck->setChecked(true);
    qualForm->addRow(QString(), m_vsyncCheck);

    lay->addWidget(qualGroup);

    // ── 视口 ──────────────────────────────────────────────────
    auto* vpGroup = new QGroupBox(tr("视口"), w);
    auto* vpForm  = new QFormLayout(vpGroup);
    vpForm->setLabelAlignment(Qt::AlignRight);
    vpForm->setSpacing(10);

    // FOV 滑块
    auto* fovRow = new QHBoxLayout;
    m_fovSlider = new QSlider(Qt::Horizontal);
    m_fovSlider->setRange(30, 120);
    m_fovSlider->setValue(45);
    m_fovLabel  = new QLabel("45°");
    m_fovLabel->setFixedWidth(36);
    fovRow->addWidget(m_fovSlider);
    fovRow->addWidget(m_fovLabel);
    connect(m_fovSlider, &QSlider::valueChanged, this, [this](int v) {
        m_fovLabel->setText(QString("%1°").arg(v));
    });
    vpForm->addRow(tr("视角（FOV）："), fovRow);

    m_gridCheck = new QCheckBox(tr("显示网格"));
    m_gridCheck->setChecked(true);
    vpForm->addRow(QString(), m_gridCheck);

    m_axisCheck = new QCheckBox(tr("显示坐标轴"));
    m_axisCheck->setChecked(true);
    vpForm->addRow(QString(), m_axisCheck);

    lay->addWidget(vpGroup);
    lay->addStretch();

    tabs->addTab(w, tr("渲染"));
}

// ─────────────────────────────────────────────────────────────
//  快捷键页
// ─────────────────────────────────────────────────────────────
void SettingsDialog::buildShortcutTab(QTabWidget* tabs)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(10);

    struct ShortcutRow { QString label; QString defaultKey; };
    const ShortcutRow rows[] = {
        { tr("撤销"),       "Ctrl+Z"  },
        { tr("重做"),       "Ctrl+Y"  },
        { tr("保存"),       "Ctrl+S"  },
        { tr("聚焦选中"),   "F"       },
        { tr("删除选中"),   "Delete"  },
        { tr("运行场景"),   "F5"      },
    };

    auto* group = new QGroupBox(tr("快捷键绑定"), w);
    auto* form  = new QFormLayout(group);
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(8);

    for (auto& row : rows) {
        auto* edit = new QKeySequenceEdit(QKeySequence(row.defaultKey));
        edit->setFixedWidth(160);
        form->addRow(row.label + "：", edit);
    }

    lay->addWidget(group);
    lay->addStretch();

    tabs->addTab(w, tr("快捷键"));
}

// ─────────────────────────────────────────────────────────────
//  手柄映射页
// ─────────────────────────────────────────────────────────────
void SettingsDialog::buildGamepadTab(QTabWidget* tabs)
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 8);
    lay->setSpacing(16);

    // ── 设备 ──────────────────────────────────────────────────
    auto* devGroup = new QGroupBox(tr("设备"), w);
    auto* devForm  = new QFormLayout(devGroup);
    devForm->setLabelAlignment(Qt::AlignRight);
    devForm->setSpacing(10);

    m_gamepadCombo = new QComboBox;
    m_gamepadCombo->addItem(tr("（未检测到手柄）"));
    devForm->addRow(tr("当前手柄："), m_gamepadCombo);

    m_gamepadEnable = new QCheckBox(tr("启用手柄输入"));
    m_gamepadEnable->setChecked(true);
    devForm->addRow(QString(), m_gamepadEnable);

    m_gamepadVibrate = new QCheckBox(tr("启用震动反馈"));
    m_gamepadVibrate->setChecked(true);
    devForm->addRow(QString(), m_gamepadVibrate);

    // 启用开关联动其他控件
    connect(m_gamepadEnable, &QCheckBox::toggled, m_gamepadVibrate, &QCheckBox::setEnabled);

    lay->addWidget(devGroup);

    // ── 摇杆 ──────────────────────────────────────────────────
    auto* stickGroup = new QGroupBox(tr("摇杆"), w);
    auto* stickForm  = new QFormLayout(stickGroup);
    stickForm->setLabelAlignment(Qt::AlignRight);
    stickForm->setSpacing(10);

    // 死区滑块
    auto* dzRow = new QHBoxLayout;
    m_deadZoneSlider = new QSlider(Qt::Horizontal);
    m_deadZoneSlider->setRange(0, 50);
    m_deadZoneSlider->setValue(10);
    m_deadZoneLabel = new QLabel("10%");
    m_deadZoneLabel->setFixedWidth(36);
    dzRow->addWidget(m_deadZoneSlider);
    dzRow->addWidget(m_deadZoneLabel);
    connect(m_deadZoneSlider, &QSlider::valueChanged, this, [this](int v) {
        m_deadZoneLabel->setText(QString("%1%").arg(v));
    });
    stickForm->addRow(tr("死区："), dzRow);

    lay->addWidget(stickGroup);

    // ── 按键绑定 ──────────────────────────────────────────────
    auto* btnGroup = new QGroupBox(tr("按键绑定"), w);
    auto* btnForm  = new QFormLayout(btnGroup);
    btnForm->setLabelAlignment(Qt::AlignRight);
    btnForm->setSpacing(8);

    struct GamepadRow { QString action; QString defaultBtn; };
    const GamepadRow rows[] = {
        { tr("确认"),         "A / Cross"        },
        { tr("取消"),         "B / Circle"       },
        { tr("摄像机重置"),   "右摇杆按下 (R3)"  },
        { tr("聚焦选中"),     "左摇杆按下 (L3)"  },
        { tr("开始运行"),     "Start / Options"  },
    };
    for (auto& row : rows) {
        auto* edit = new QLineEdit(row.defaultBtn);
        edit->setFixedWidth(160);
        edit->setReadOnly(true);   // 后续可扩展为捕获输入
        edit->setPlaceholderText(tr("点击后按手柄键…"));
        btnForm->addRow(row.action + "：", edit);
    }

    lay->addWidget(btnGroup);
    lay->addStretch();

    tabs->addTab(w, tr("手柄映射"));
}

// ─────────────────────────────────────────────────────────────
//  样式
// ─────────────────────────────────────────────────────────────
void SettingsDialog::applyStyle()
{
    setStyleSheet(R"(
        QDialog {
            background: #1e1e1e;
            color: #d0d0d0;
        }
        QTabWidget::pane {
            border: 1px solid #3f3f46;
            background: #252526;
            border-radius: 4px;
        }
        QTabBar::tab {
            background: #2d2d2d;
            color: #aaaaaa;
            padding: 6px 18px;
            border: 1px solid #3f3f46;
            border-bottom: none;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background: #252526;
            color: #ffffff;
            border-bottom: 1px solid #252526;
        }
        QTabBar::tab:hover:!selected {
            background: #3f3f46;
        }
        QGroupBox {
            color: #cccccc;
            border: 1px solid #3f3f46;
            border-radius: 4px;
            margin-top: 8px;
            padding-top: 8px;
            font-size: 12px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 0 6px;
            left: 10px;
        }
        QLabel {
            color: #cccccc;
            font-size: 12px;
        }
        QComboBox, QSpinBox, QKeySequenceEdit {
            background: #3c3c3c;
            color: #d0d0d0;
            border: 1px solid #555;
            border-radius: 3px;
            padding: 3px 6px;
            font-size: 12px;
        }
        QComboBox::drop-down { border: none; width: 20px; }
        QComboBox QAbstractItemView {
            background: #2a2a2e;
            color: #d0d0d0;
            selection-background-color: #0e639c;
            border: 1px solid #555;
        }
        QCheckBox {
            color: #cccccc;
            font-size: 12px;
            spacing: 6px;
        }
        QCheckBox::indicator {
            width: 14px; height: 14px;
            border: 1px solid #666;
            border-radius: 2px;
            background: #3c3c3c;
        }
        QCheckBox::indicator:checked {
            background: #0078d4;
            border-color: #0078d4;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #3f3f46;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 14px; height: 14px;
            margin: -5px 0;
            background: #0078d4;
            border-radius: 7px;
        }
        QSlider::sub-page:horizontal {
            background: #0078d4;
            border-radius: 2px;
        }
        QPushButton {
            background: #3c3c3c;
            color: #d0d0d0;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 5px 12px;
            font-size: 12px;
        }
        QPushButton:hover  { background: #4a4a4a; }
        QPushButton:pressed { background: #0078d4; border-color: #0078d4; }
        QPushButton:default {
            background: #0078d4;
            border-color: #0078d4;
            color: #ffffff;
        }
        QPushButton:default:hover  { background: #1a8fe3; }
    )");
}
