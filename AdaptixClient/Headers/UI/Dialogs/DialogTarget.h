#ifndef DIALOGTARGET_H
#define DIALOGTARGET_H

#include <main.h>

#include <oclero/qlementine/widgets/Switch.hpp>

class DialogTarget : public QDialog
{
    QVBoxLayout* mainLayout     = nullptr;
    QGroupBox*   hostGroup      = nullptr;
    QGridLayout* hostGrid       = nullptr;
    QGroupBox*   systemGroup    = nullptr;
    QGridLayout* systemGrid     = nullptr;
    QHBoxLayout* buttonLayout   = nullptr;
    QPushButton* createButton   = nullptr;
    QPushButton* cancelButton   = nullptr;

    QLineEdit*   computerInput  = nullptr;
    QLineEdit*   domainInput    = nullptr;
    QLineEdit*   addressInput   = nullptr;
    oclero::qlementine::Switch* aliveSwitch = nullptr;
    QComboBox*   osCombo        = nullptr;
    QLineEdit*   osDescInput    = nullptr;
    QLineEdit*   tagInput       = nullptr;
    QLineEdit*   infoInput      = nullptr;

    bool       valid    = false;
    QString    message  = "";
    qint64     targetId = 0;
    TargetData data = {};

    bool editMode = false;

    void createUI();

public:
    explicit DialogTarget();
    ~DialogTarget() override;

    void StartDialog();
    void SetEditmode(const TargetData &targetData);
    bool IsValid() const;
    QString    GetMessage() const;
    TargetData GetTargetData() const;

protected Q_SLOTS:
    void onButtonCreate();
    void onButtonCancel();
};

#endif
