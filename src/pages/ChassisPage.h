#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "device/ParameterCatalog.h"
#include "device/TelemetryTypes.h"

class QAbstractSpinBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QPushButton;

class ChassisPage : public QWidget {
    Q_OBJECT

public:
    explicit ChassisPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setValues(const QVector<ParameterValue> &values);

signals:
    void readRequested(quint8 group);
    void writeRequested(quint8 group, QVector<ParameterValue> values);

private slots:
    void handlePidProfileChanged(int index);
    void readPid();
    void writePid();
    void readChassis();
    void writeChassis();
    void readImu();
    void writeImu();
    void restoreInitial();

private:
    static constexpr quint8 kPidGroup = 0x10;
    static constexpr quint8 kChassisGroup = 0x20;
    static constexpr quint8 kImuGroup = 0x40;

    bool ownsGroup(quint8 group) const;
    quint16 activePidId(int fieldOffset) const;
    QAbstractSpinBox *createSpinBox(const ParameterSpec &spec,
                                    const QString &objectName,
                                    QWidget *parent);
    void addFieldRow(QFormLayout *layout, const ParameterSpec &spec,
                     QAbstractSpinBox *control);
    void capturePidControls(int profile = -1);
    QVariant controlValue(QAbstractSpinBox *control) const;
    void applyValue(const ParameterValue &value);
    void refreshPidControls();
    void refreshRestoreButton();
    QVector<ParameterValue> collectGroup(quint8 group);
    void setStatus(const QString &message);

    const ParameterCatalog &catalog_;
    bool connected_{};
    int activePidProfile_{};
    QComboBox *pidProfileCombo_{};
    QVector<QAbstractSpinBox *> pidControls_;
    QVector<const ParameterSpec *> pidSpecs_;
    QHash<quint16, QAbstractSpinBox *> controls_;
    QHash<quint16, ParameterValue> values_;
    QHash<quint16, ParameterValue> connectionInitialValues_;
    QSet<quint8> connectionInitialGroups_;
    QSet<quint8> pendingReadGroups_;
    QSet<quint8> pendingWriteGroups_;
    QVector<QWidget *> interactiveWidgets_;
    QPushButton *pidReadButton_{};
    QPushButton *pidWriteButton_{};
    QPushButton *chassisReadButton_{};
    QPushButton *chassisWriteButton_{};
    QPushButton *imuReadButton_{};
    QPushButton *imuWriteButton_{};
    QPushButton *restoreInitialButton_{};
    QLabel *statusLabel_{};
};
