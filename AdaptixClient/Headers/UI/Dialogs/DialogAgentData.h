#ifndef ADAPTIXCLIENT_DIALOGAGENTDATA_H
#define ADAPTIXCLIENT_DIALOGAGENTDATA_H

#include <main.h>
#include <Client/AuthProfile.h>

#include <oclero/qlementine/widgets/Switch.hpp>

class DialogAgentData : public QDialog
{
    QVBoxLayout* mainLayout        = nullptr;
    QHBoxLayout* columnsLayout     = nullptr;
    QHBoxLayout* buttonLayout      = nullptr;
    QPushButton* buttonUpdate      = nullptr;
    QPushButton* buttonCancel      = nullptr;

    QGroupBox*   groupIdentity     = nullptr;
    QGridLayout* layoutIdentity    = nullptr;
    QLineEdit*   inputDomain       = nullptr;
    QLineEdit*   inputComputer     = nullptr;
    QLineEdit*   inputUsername     = nullptr;
    QLineEdit*   inputImpersonated = nullptr;

    QGroupBox*   groupProcess      = nullptr;
    QGridLayout* layoutProcess     = nullptr;
    QLineEdit*   inputProcess      = nullptr;
    QComboBox*   inputArch         = nullptr;
    QSpinBox*    inputPid          = nullptr;
    QSpinBox*    inputTid          = nullptr;
    oclero::qlementine::Switch* inputElevated = nullptr;

    QGroupBox*   groupNetwork      = nullptr;
    QGridLayout* layoutNetwork     = nullptr;
    QLineEdit*   inputInternalIP   = nullptr;
    QLineEdit*   inputExternalIP   = nullptr;

    QGroupBox*   groupOS           = nullptr;
    QGridLayout* layoutOS          = nullptr;
    QComboBox*   inputOs           = nullptr;
    QLineEdit*   inputOsDesc       = nullptr;
    QSpinBox*    inputGmtOffset    = nullptr;
    QSpinBox*    inputACP          = nullptr;
    QSpinBox*    inputOemCP        = nullptr;

    AuthProfile authProfile;
    qint64      agentId = 0;

    QString     originalInternalIP;
    QString     originalExternalIP;
    int         originalGmtOffset;
    int         originalACP;
    int         originalOemCP;
    int         originalPid;
    int         originalTid;
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
