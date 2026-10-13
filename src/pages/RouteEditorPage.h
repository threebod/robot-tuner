#pragma once

#include <QWidget>
#include "device/RouteSequenceModel.h"

class RouteSequenceRunner;
class QComboBox;
class QSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QTableWidget;
class QStackedWidget;

class RouteEditorPage : public QWidget {
    Q_OBJECT
public:
    explicit RouteEditorPage(RouteSequenceRunner *runner, QWidget *parent = nullptr);
    void setConnected(bool connected);
    void showError(const QString &error);
    void setSequence(const RouteSequence &sequence);
    RouteSequence sequence() const;
    bool loadFile(const QString &path, QString *error = nullptr);
    bool saveFile(const QString &path, QString *error = nullptr) const;

signals:
    void executionRequested(RouteSequence sequence, int from, bool selectedOnly);

private:
    RouteStep editedStep() const;
    void loadSelected();
    void refresh(int selected);
    void updateControls();
    void requestRun(int from, bool selectedOnly);

    RouteSequenceRunner *runner_;
    RouteSequence sequence_;
    MechanismSequence importedAction_;
    bool connected_{};
    QWidget *editControls_{};
    QLineEdit *name_{};
    QTableWidget *table_{};
    QComboBox *type_{};
    QStackedWidget *parameters_{};
    QComboBox *moveDirection_{};
    QSpinBox *distance_{};
    QSpinBox *rpm_{};
    QComboBox *turnDirection_{};
    QSpinBox *degrees_{};
    QComboBox *visionMode_{};
    QSpinBox *target_{};
    QSpinBox *forward_{};
    QSpinBox *right_{};
    QLabel *actionLabel_{};
    QSpinBox *waitMs_{};
    QLineEdit *prompt_{};
    QPushButton *selected_{};
    QPushButton *from_{};
    QPushButton *all_{};
    QPushButton *stop_{};
    QPushButton *confirm_{};
    QLabel *status_{};
};
