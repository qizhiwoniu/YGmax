#pragma once
#include <QDialog>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSlider>
#include <QPushButton>
#include <QGroupBox>
#include <QKeySequenceEdit>
#include <QLineEdit>

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

private:
    void buildGeneralTab(QTabWidget* tabs);
    void buildRenderTab(QTabWidget* tabs);
    void buildShortcutTab(QTabWidget* tabs);
    void buildGamepadTab(QTabWidget* tabs);
    void applyStyle();

    // ── 通用 ──────────────────────────────────────────────────
    QComboBox* m_langCombo      = nullptr;
    QCheckBox* m_autoSaveCheck  = nullptr;
    QSpinBox*  m_autoSaveSpin   = nullptr;
    QCheckBox* m_restoreLayout  = nullptr;

    // ── 渲染 ──────────────────────────────────────────────────
    QComboBox* m_msaaCombo      = nullptr;
    QCheckBox* m_vsyncCheck     = nullptr;
    QSlider*   m_fovSlider      = nullptr;
    QLabel*    m_fovLabel       = nullptr;
    QCheckBox* m_gridCheck      = nullptr;
    QCheckBox* m_axisCheck      = nullptr;

    // ── 底部按钮 ──────────────────────────────────────────────
    QPushButton* m_okBtn        = nullptr;
    QPushButton* m_cancelBtn    = nullptr;
    QPushButton* m_applyBtn     = nullptr;

    // ── 手柄映射 ──────────────────────────────────────────────
    QComboBox*   m_gamepadCombo   = nullptr;   // 手柄设备选择
    QCheckBox*   m_gamepadEnable  = nullptr;   // 启用手柄
    QCheckBox*   m_gamepadVibrate = nullptr;   // 震动反馈
    QSlider*     m_deadZoneSlider = nullptr;   // 死区
    QLabel*      m_deadZoneLabel  = nullptr;
};
