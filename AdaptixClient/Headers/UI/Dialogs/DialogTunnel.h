#ifndef DIALOGTUNNEL_H
#define DIALOGTUNNEL_H

#include <main.h>
#include <Client/WidgetBuilder.h>
#include <Client/AuthProfile.h>

class DialogTunnel : public QDialog
{
     QGridLayout*    mainGridLayout       = nullptr;
     QLabel*         tunnelTypeLabel      = nullptr;
     QComboBox*      tunnelTypeCombo      = nullptr;
     QLabel*         tunnelDescLabel      = nullptr;
     QLineEdit*      tunnelDescInput      = nullptr;
     QGroupBox*      tunnelConfigGroupbox = nullptr;
     QStackedWidget* tunnelStackWidget    = nullptr;
     QGridLayout*    stackGridLayout      = nullptr;
     QHBoxLayout*    hLayoutBottom        = nullptr;
     QPushButton*    buttonCancel         = nullptr;
     QPushButton*    buttonCreate         = nullptr;
     QSpacerItem*    horizontalSpacer_1   = nullptr;
     QSpacerItem*    horizontalSpacer_2   = nullptr;
     //Socks5
     QWidget*        socks5Widget         = nullptr;
     QGridLayout*    socks5GridLayout     = nullptr;
     QLabel*         socks5LocalAddrLabel = nullptr;
     QLineEdit*      socks5LocalAddrInput = nullptr;
     QSpinBox*       socks5LocalPortSpin  = nullptr;
     QCheckBox*      socks5UseAuth        = nullptr;
     QLabel*         socks5AuthUserLabel  = nullptr;
     QLineEdit*      socks5AuthUserInput  = nullptr;
     QLabel*         socks5AuthPassLabel  = nullptr;
     QLineEdit*      socks5AuthPassInput  = nullptr;
     //Socks4
     QWidget*        socks4Widget         = nullptr;
     QGridLayout*    socks4GridLayout     = nullptr;
     QLabel*         socks4LocalAddrLabel = nullptr;
     QLineEdit*      socks4LocalAddrInput = nullptr;
     QSpinBox*       socks4LocalPortSpin  = nullptr;
     //LPF
     QWidget*        lpfWidget          = nullptr;
     QGridLayout*    lpfGridLayout      = nullptr;
     QLabel*         lpfLocalAddrLabel  = nullptr;
     QLineEdit*      lpfLocalAddrInput  = nullptr;
     QSpinBox*       lpfLocalPortSpin   = nullptr;
     QLabel*         lpfTargetAddrLabel = nullptr;
     QLineEdit*      lpfTargetAddrInput = nullptr;
     QSpinBox*       lpfTargetPortSpin  = nullptr;
     //RPF
     QWidget*        rpfWidget          = nullptr;
     QGridLayout*    rpfGridLayout      = nullptr;
     QLabel*         rpfPortLabel       = nullptr;
     QSpinBox*       rpfPortSpin        = nullptr;
     QLabel*         rpfTargetAddrLabel = nullptr;
     QLineEdit*      rpfTargetAddrInput = nullptr;
     QSpinBox*       rpfTargetPortSpin  = nullptr;

     bool       valid      = false;
     QString    message    = "";
     QString    tunnelType = "";
     QByteArray jsonData;

     QString AgentId = "";

     void createUI();

public:
     explicit DialogTunnel();
     ~DialogTunnel() override;

     void SetSettings(const QString &agentId, bool s5, bool s4, bool lpf, bool rpf);
     void StartDialog();
     bool IsValid() const;
     QString GetMessage() const;
     QString GetTunnelType() const;
     QByteArray GetTunnelData() const;

protected slots:
     void changeType(const QString &type) const;
     void onSocks5AuthCheckChange() const;
     void onButtonCreate();
     void onButtonCancel();
};

#endif //DIALOGTUNNEL_H
