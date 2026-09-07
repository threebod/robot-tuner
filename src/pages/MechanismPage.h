#pragma once

#include <QHash>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "device/ParameterCatalog.h"
#include "device/TelemetryTypes.h"

class QAbstractSpinBox;
class QFormLayout;
class QLabel;
class QPushButton;

class MechanismPage : public QWidget {
    Q_OBJECT

public:
    explicit MechanismPage(QWidget *parent = nullptr);

    void setConnected(bool connected);
    void setValues(const QVector<ParameterValue> &values);

signals:
    void readRequested(quint8 group);
    void writeRequested(quint8 group, QVector<ParameterValue> values);

private slots:
    void readMechanism();
    void writeMechanism();
    void restoreInitial();

private:
    static constexpr quint8 kMechanismGroup = 0x30;

    QAbstractSpinBox *createSpinBox(const ParameterSpec &spec,
                                    const QString &objectName,
                                    QWidget *parent);
    void addFieldRow(QFormLayout *layout, const ParameterSpec &spec,
                     QAbstractSpinBox *control);
    QVariant controlValue(QAbstractSpinBox *control) const;
    void applyValue(const ParameterValue &value);
    QVector<ParameterValue> collectValues();
    void refreshRestoreButton();
    void setStatus(const QString &message);

    const ParameterCatalog &catalog_;
    bool connected_{};
    QHash<quint16, QAbstractSpinBox *> controls_;
    QHash<quint16, ParameterValue> values_;
    QHash<quint16, ParameterValue> connectionInitialValues_;
    QSet<quint8> connectionInitialGroups_;
    QSet<quint8> pendingReadGroups_;
    QSet<quint8> pendingWriteGroups_;
    QVector<QWidget *> interactiveWidgets_;
    QPushButton *readButton_{};
    QPushButton *writeButton_{};
    QPushButton *restoreInitialButton_{};
    QLabel *statusLabel_{};
};
