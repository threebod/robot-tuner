#include "pages/TerminalPage.h"

#include <QVBoxLayout>

#include "widgets/SerialDebugPanel.h"

TerminalPage::TerminalPage(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    panel_ = new SerialDebugPanel(QStringLiteral("terminal"), this);
    layout->addWidget(panel_);
    connect(panel_, &SerialDebugPanel::rawSendRequested, this,
            &TerminalPage::rawSendRequested);
}

void TerminalPage::setConnected(bool connected) {
    panel_->setConnected(connected);
}

void TerminalPage::setTextStreamMode(bool enabled) {
    panel_->setTextStreamMode(enabled);
}

void TerminalPage::showSendError(const QString &error) {
    panel_->showSendError(error);
}

void TerminalPage::appendTx(const QByteArray &bytes) {
    panel_->appendTx(bytes);
}

void TerminalPage::appendRx(const QByteArray &bytes) {
    panel_->appendRx(bytes);
}

void TerminalPage::appendDecodedFrame(const protocol::Frame &frame) {
    panel_->appendDecodedFrame(frame);
}
