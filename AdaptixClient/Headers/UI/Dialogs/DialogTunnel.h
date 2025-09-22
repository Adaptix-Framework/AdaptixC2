#ifndef DIALOGTUNNEL_H
#define DIALOGTUNNEL_H

#include <main.h>

class DialogTunnel : public QDialog
{
     QGridLayout*    mainGridLayout       = nullptr;
     QLabel*         tunnelTypeLabel      = nullptr;
     QComboBox*      tunnelTypeCombo      = nullptr;
     QLabel*         tunnelDescLabel      = nullptr;
     QLineEdit*      tunnelDescInput      = nullptr;
     QLabel*         tunnelEndpointLabel  = nullptr;
     QComboBox*      tunnelEndpointCombo  = nullptr;
     QGroupBox*      tunnelConfigGroupbox = nullptr;
     QStackedWidget* tunnelStackWidget    = nullptr;
     QGridLayout*    stackGridLayout      = nullptr;
     QHBoxLayout*    hLayoutBottom        = nullptr;
     QPushButton*    buttonCancel         = nullptr;
     QPushButton*    buttonCreate         = nullptr;
     QSpacerItem*    horizontalSpacer_1   = nullptr;
     QSpacerItem*    horizontalSpacer_2   = nullptr;

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

     QWidget*        socks4Widget         = nullptr;
     QGridLayout*    socks4GridLayout     = nullptr;
     QLabel*         socks4LocalAddrLabel = nullptr;
     QLineEdit*      socks4LocalAddrInput = nullptr;
     QSpinBox*       socks4LocalPortSpin  = nullptr;

     QWidget*        lpfWidget          = nullptr;
     QGridLayout*    lpfGridLayout      = nullptr;
     QLabel*         lpfLocalAddrLabel  = nullptr;
     QLineEdit*      lpfLocalAddrInput  = nullptr;
     QSpinBox*       lpfLocalPortSpin   = nullptr;
     QLabel*         lpfTargetAddrLabel = nullptr;
     QLineEdit*      lpfTargetAddrInput = nullptr;
     QSpinBox*       lpfTargetPortSpin  = nullptr;

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
     explicit DialogTunnel(const QString &agentId, bool s4, bool s5, bool lpf, bool rpf);
     ~DialogTunnel() override;

     void StartDialog();
     bool IsValid() const;
     QString GetMessage() const;
     QString GetTunnelType() const;
     QString GetEndpoint() const;
     QByteArray GetTunnelData() const;

protected Q_SLOTS:
     void changeType(const QString &type) const;
     void onSocks5AuthCheckChange() const;
     void onButtonCreate();
     void onButtonCancel();
};

#endif
