#include "stdafx.h"
#include "AboutDialog.h"
#include "version.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    // 无边框 + 透明背景（和主窗口风格一致）
    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    setAttribute(Qt::WA_TranslucentBackground);
    setFixedSize(360, 220);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(8);

    // 产品名
    QLabel* nameLabel = new QLabel(VER_PRODUCT_NAME, this);
    nameLabel->setStyleSheet("color: #ffffff; font-size: 20px; font-weight: bold;");

    // 版本号
    QLabel* verLabel = new QLabel(QString("版本  %1").arg(VERSION_STR), this);
    verLabel->setStyleSheet("color: #aaaaaa; font-size: 13px;");

    // 描述
    QLabel* descLabel = new QLabel(VER_FILE_DESC, this);
    descLabel->setStyleSheet("color: #aaaaaa; font-size: 13px;");

    // 版权
    QLabel* copyLabel = new QLabel(VER_COPYRIGHT, this);
    copyLabel->setStyleSheet("color: #666666; font-size: 12px;");

    // 分隔线
    QFrame* line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #333333;");

    // 关闭按钮
    QPushButton* closeBtn = new QPushButton("关闭", this);
    closeBtn->setFixedSize(80, 30);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background: #2d2d2d;
            color: #cccccc;
            border: 1px solid #444444;
            border-radius: 4px;
            font-size: 13px;
        }
        QPushButton:hover {
            background: #3a3a3a;
        }
        QPushButton:pressed {
            background: #222222;
        }
    )");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);

    mainLayout->addWidget(nameLabel);
    mainLayout->addWidget(verLabel);
    mainLayout->addWidget(descLabel);
    mainLayout->addWidget(copyLabel);
    mainLayout->addStretch();
    mainLayout->addWidget(line);
    mainLayout->addSpacing(4);
    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}

void AboutDialog::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
        QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(rect(), 10, 10);

    // 背景色和主窗口一致
    painter.fillPath(path, QColor("#1b1b1c"));

    // 描边
    painter.setPen(QPen(QColor("#333333"), 1));
    painter.drawPath(path);
}