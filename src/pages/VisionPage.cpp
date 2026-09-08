#include "pages/VisionPage.h"

#include <QLabel>
#include <QVBoxLayout>

VisionPage::VisionPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    auto *placeholder = new QLabel(
        QStringLiteral("K230 / MaixCAM 视觉调参将在后续版本实现"), this);
    placeholder->setObjectName(QStringLiteral("visionPlaceholderLabel"));
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    auto *usart1 = new QLabel(
        QStringLiteral("USART1 接口：保留用于 K230 / MaixCAM 视觉通信；当前版本不提供调参控件。"),
        this);
    usart1->setObjectName(QStringLiteral("visionUsart1Label"));
    usart1->setAlignment(Qt::AlignCenter);
    usart1->setWordWrap(true);
    layout->addWidget(usart1);
    layout->addStretch();
}
