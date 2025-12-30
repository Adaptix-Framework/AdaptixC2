#ifndef ADAPTIXCLIENT_DIALOGAGENTDATA_H
#define ADAPTIXCLIENT_DIALOGAGENTDATA_H

#include <main.h>
#include <Client/AuthProfile.h>

class DialogAgentData : public QDialog
{

    QVBoxLayout* mainLayout        = nullptr;
    QHBoxLayout* hLayoutBottom     = nullptr;
    QSpacerItem* horizontalSpacer  = nullptr;

    QGroupBox*   groupNetwork      = nullptr;
    QGridLayout* layoutNetwork     = nullptr;
    QLabel*      labelInternalIP   = nullptr;
    QLineEdit*   inputInternalIP   = nullptr;
    QLabel*      labelExternalIP   = nullptr;
    QLineEdit*   inputExternalIP   = nullptr;

    QGroupBox*   groupCoding       = nullptr;
    QGridLayout* layoutCoding      = nullptr;
    QLabel*      labelACP          = nullptr;
    QSpinBox*    inputACP          = nullptr;
    QLabel*      labelOemCP        = nullptr;
    QSpinBox*    inputOemCP        = nullptr;

    QGroupBox*   groupProcess      = nullptr;
    QGridLayout* layoutProcess     = nullptr;
    QLabel*      labelProcess      = nullptr;
    QLineEdit*   inputProcess      = nullptr;
    QComboBox*   inputArch         = nullptr;
    QCheckBox*   inputElevated     = nullptr;
    QLabel*      labelPid          = nullptr;
    QLineEdit*   inputPid          = nullptr;
    QLabel*      labelTid          = nullptr;
    QLineEdit*   inputTid          = nullptr;

    QGroupBox*   groupOS           = nullptr;
    QGridLayout* layoutOS          = nullptr;
    QLabel*      labelOs           = nullptr;
    QComboBox*   inputOs           = nullptr;
    QLineEdit*   inputOsDesc       = nullptr;
    QLabel*      labelGmtOffset    = nullptr;
    QSpinBox*    inputGmtOffset    = nullptr;

    QGroupBox*   groupContext      = nullptr;
    QGridLayout* layoutContext     = nullptr;
    QLabel*      labelDomain       = nullptr;
    QLineEdit*   inputDomain       = nullptr;
    QLabel*      labelComputer     = nullptr;
    QLineEdit*   inputComputer     = nullptr;
    QLabel*      labelUsername     = nullptr;
    QLineEdit*   inputUsername     = nullptr;
    QLabel*      labelImpersonated = nullptr;
    QLineEdit*   inputImpersonated = nullptr;

    QPushButton* buttonUpdate      = nullptr;
    QPushButton* buttonCancel      = nullptr;

    AuthProfile authProfile;
    QString     agentId;

    QString     originalInternalIP;
    QString     originalExternalIP;
    int         originalGmtOffset;
    int         originalACP;
    int         originalOemCP;
    QString     originalPid;
    QString     originalTid;
    QString     originalArch;
    bool        originalElevated;
    QString     originalProcess;
    int         originalOs;
    QString     originalOsDesc;
    QString     originalDomain;
    QString     originalComputer;
    QString     originalUsername;
    QString     originalImpersonated;

    void createUI();

public:
    explicit DialogAgentData(QWidget* parent = nullptr);
    ~DialogAgentData() override;

    void SetProfile(const AuthProfile &profile);
    void SetAgentData(const AgentData &data);
    void Start();

protected Q_SLOTS:
    void onButtonUpdate();
    void onButtonCancel();
};

#endif
