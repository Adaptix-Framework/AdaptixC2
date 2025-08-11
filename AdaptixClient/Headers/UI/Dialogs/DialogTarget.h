#ifndef DIALOGTARGET_H
#define DIALOGTARGET_H

#include <main.h>

class DialogTarget : public QDialog
{
    QGridLayout* mainGridLayout = nullptr;
    QLabel*      computerLabel  = nullptr;
    QLineEdit*   computerInput  = nullptr;
    QLabel*      domainLabel    = nullptr;
    QLineEdit*   domainInput    = nullptr;
    QLabel*      addressLabel   = nullptr;
    QLineEdit*   addressInput   = nullptr;
    QCheckBox*   aliveCheck     = nullptr;
    QLabel*      osLabel        = nullptr;
    QComboBox*   osCombo        = nullptr;
    QLabel*      osDescLabel    = nullptr;
    QLineEdit*   osDescInput    = nullptr;
    QLabel*      tagLabel       = nullptr;
    QLineEdit*   tagInput       = nullptr;
    QLabel*      infoLabel      = nullptr;
    QLineEdit*   infoInput      = nullptr;
    QHBoxLayout* hLayoutBottom  = nullptr;
    QSpacerItem* spacer_1       = nullptr;
    QSpacerItem* spacer_2       = nullptr;
    QPushButton* createButton   = nullptr;
    QPushButton* cancelButton   = nullptr;

    bool       valid    = false;
    QString    message  = "";
    QString    targetId = "";
    TargetData data = {};

    void createUI();

public:
    explicit DialogTarget();
    ~DialogTarget() override;

    void StartDialog();
    void SetEditmode(const TargetData &targetData);
    bool IsValid() const;
    QString    GetMessage() const;
    TargetData GetTargetData() const;

protected slots:
    void onButtonCreate();
    void onButtonCancel();
};

#endif //DIALOGTARGET_H
