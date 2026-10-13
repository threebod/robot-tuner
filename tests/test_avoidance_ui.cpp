#include <QApplication>
#include <QComboBox>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QFontDatabase>
#include <QFont>
#include <iostream>
#include "device/MecanumJogClient.h"
#include "pages/FieldPositionPage.h"

static bool require(bool condition,const char *message) {
    if(!condition)std::cerr<<message<<'\n';
    return condition;
}
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    if(!qEnvironmentVariable("AV_SCREENSHOT_PATH").isEmpty()) {
        const int fontId=QFontDatabase::addApplicationFont(QStringLiteral("C:/Windows/Fonts/msyh.ttc"));
        const auto families=QFontDatabase::applicationFontFamilies(fontId);
        if(!families.isEmpty())app.setFont(QFont(families.first(),10));
    }
    MecanumJogClient client;
    FieldPositionPage page;
    QList<QByteArray> tx;
    QVector<QPointF> displayed;
    int snapshots=0;
    QObject::connect(&client,&MecanumJogClient::bytesReady,[&](QByteArray bytes){tx.append(bytes);});
    QObject::connect(&client,&MecanumJogClient::avoidanceStateReceived,&page,&FieldPositionPage::setAvoidanceStatus);
    QObject::connect(&client,&MecanumJogClient::avoidanceRunningChanged,&page,&FieldPositionPage::setAvoidanceRunning);
    QObject::connect(&client,&MecanumJogClient::avoidanceError,&page,&FieldPositionPage::setAvoidanceError);
    QObject::connect(&client,&MecanumJogClient::avoidancePathReceived,&page,&FieldPositionPage::setAvoidancePath);
    QObject::connect(&client,&MecanumJogClient::avoidancePathReceived,[&](QVector<QPointF> points){displayed=points;++snapshots;});
    page.setNavigationMode(true);page.setNavigationConnected(true);client.setConnected(true);
    auto *rpm=page.findChild<QSpinBox*>("avoidanceRpmSpin");
    auto *start=page.findChild<QPushButton*>("avoidanceStartButton");
    auto *clear=page.findChild<QPushButton*>("avoidanceClearButton");
    if(!require(rpm && rpm->minimum()==10 && rpm->maximum()==250 && rpm->value()==30,"avoidance RPM controls incorrect"))return 1;
    if(!require(!client.startAvoidanceRun(1,9) && !client.startAvoidanceRun(1,251),"speed bounds not enforced"))return 1;
    if(!require(client.startAvoidanceRun(1,250) && tx.back()==QByteArray("arm\r\n"),"avoidance authorization missing"))return 1;
    client.ingestBytes("ARMED for one enable or motion command\r\n");
    if(!require(tx.back()==QByteArray("avoid start 1 250\r\n"),"avoidance start command incorrect"))return 1;
    const QByteArray state="AV STATE state=MOVE active=1 round=1 target=RAW_1 region=3 blocked=4 replans=1 camera=1 x=2100 y=2080 yaw_cdeg=9000 reason=NONE\r\n";
    client.ingestBytes(QByteArrayView(state).first(20));client.ingestBytes(QByteArrayView(state).sliced(20));
    if(!require(client.avoidanceRunning() && !start->isEnabled() && !clear->isEnabled(),"running controls not locked"))return 1;
    if(!require(!client.startFullRoute(1,30) && !client.beginRouteExecution() && !client.clearAvoidanceMemory(),"motion mutual exclusion failed"))return 1;
    if(!require(!start->isEnabled() && !clear->isEnabled(),"rejected command unlocked active avoidance controls"))return 1;
    client.ingestBytes("ERR AV: clear requires stopped vehicle\r\n");
    if(!require(client.avoidanceRunning() && !clear->isEnabled(),"nonfatal rejection cancelled running state"))return 1;
    client.ingestBytes("AV PATH_BEGIN version=2 count=2\r\nAV PATH_POINT version=2 index=0 x=2100 y=2080\r\n");
    if(!require(snapshots==0,"partial snapshot was displayed"))return 1;
    client.ingestBytes("AV PATH_POINT version=2 index=1 x=1200 y=2080\r\nAV PATH_END version=2\r\n");
    if(!require(snapshots==1 && displayed.size()==2,"complete path missing"))return 1;
    client.ingestBytes("AV PATH_BEGIN version=3 count=2\r\nAV PATH_POINT version=3 index=1 x=300 y=300\r\nAV PATH_END version=3\r\n");
    if(!require(snapshots==1,"out of order path replaced valid map"))return 1;
    if(!require(client.stopAvoidanceRun() && tx.back()==QByteArray("avoid stop\r\n"),"stop command failed"))return 1;
    client.ingestBytes("AV STATE state=IDLE active=0 round=1 target=RAW_1 region=3 blocked=4 replans=1 camera=0 x=2100 y=2080 yaw_cdeg=9000 reason=STOPPED\r\n");
    if(!require(start->isEnabled() && clear->isEnabled() && client.clearAvoidanceMemory(),"idle memory clear unavailable"))return 1;
    if(!require(client.requestAvoidanceStatus() && tx.back()==QByteArray("avoid status\r\n"),"snapshot query unavailable"))return 1;
    client.setConnected(false);page.setNavigationConnected(false);
    if(!require(!start->isEnabled() && !client.avoidanceRunning(),"disconnect left controls active"))return 1;
    client.setConnected(true);page.setNavigationConnected(true);client.requestAvoidanceStatus();
    client.ingestBytes(state);client.ingestBytes("AV PATH_BEGIN version=4 count=2\r\nAV PATH_POINT version=4 index=0 x=2100 y=2080\r\nAV PATH_POINT version=4 index=1 x=1200 y=2080\r\nAV PATH_END version=4\r\n");
    page.resize(1500,1050);page.show();app.processEvents();
    for(auto *scroll:page.findChildren<QScrollArea*>())scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    app.processEvents();
    const QString imagePath=qEnvironmentVariable("AV_SCREENSHOT_PATH");
    if(!imagePath.isEmpty() && !page.grab().save(imagePath))return 1;
    client.emergencyStop();
    if(!require(!client.avoidanceRunning() && tx.back()==QByteArray("!"),"emergency stop failed"))return 1;
    std::cout<<"PASS: avoidance UI, authorization, limits, memory, snapshots, disconnect and mutual exclusion\n";
    return 0;
}
