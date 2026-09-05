#ifndef DIALOGTARGET_H
#define DIALOGTARGET_H

#include <main.h>

#include <oclero/qlementine/widgets/Switch.hpp>

class DialogTarget : public QDialog
{
Q_OBJECT
    QVBoxLayout* mainLayout     = nullptr;
    QGroupBox*   hostGroup      = nullptr;
    QGridLayout* hostGrid       = nullptr;
    QGroupBox*   systemGroup    = nullptr;
    QGridLayout* systemGrid     = nullptr;
    QHBoxLayout* buttonLayout   = nullptr;
    QPushButton* createButton   = nullptr;
    QPushButton* cancelButton   = nullptr;
    QLabel*      errorLabel     = nullptr;

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
    void fillFields(const TargetData &targetData);

public:
    explicit DialogTarget(QWidget* parent = nullptr);
    ~DialogTarget() override;

    void StartDialog();
    void SetEditmode(const TargetData &targetData);
    void SetTemplate(const TargetData &targetData);
    bool IsValid() const;
    QString    GetMessage() const;
    TargetData GetTargetData() const;

Q_SIGNALS:
    void submitted(const TargetData &data);

protected Q_SLOTS:
    void onButtonCreate();
    void onButtonCancel();
};

#endif
