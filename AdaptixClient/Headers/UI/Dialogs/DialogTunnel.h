#ifndef DIALOGTUNNEL_H
#define DIALOGTUNNEL_H

#include <main.h>
#include <oclero/qlementine/widgets/Switch.hpp>
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

class DialogTunnel : public QDialog
{
     QVBoxLayout*    mainLayout           = nullptr;
     QHBoxLayout*    segLayout            = nullptr;
     QLabel*         descLabel            = nullptr;
     QLineEdit*      descInput            = nullptr;
     QGroupBox*      configGroup          = nullptr;
     QGridLayout*    configGrid           = nullptr;
     QStackedWidget* stackWidget          = nullptr;
     QHBoxLayout*    buttonLayout         = nullptr;
     QPushButton*    buttonCancel         = nullptr;
     QPushButton*    buttonCreate         = nullptr;

     oclero::qlementine::SegmentedControl* typeSegment     = nullptr;
     oclero::qlementine::SegmentedControl* endpointSegment = nullptr;

     QWidget*        socks5Widget         = nullptr;
     QLineEdit*      socks5AddrInput      = nullptr;
     QSpinBox*       socks5PortSpin       = nullptr;
     oclero::qlementine::Switch* socks5UseAuth = nullptr;
     QLineEdit*      socks5UserInput      = nullptr;
     QLineEdit*      socks5PassInput      = nullptr;

     QWidget*        socks4Widget         = nullptr;
     QLineEdit*      socks4AddrInput      = nullptr;
     QSpinBox*       socks4PortSpin       = nullptr;

     QWidget*        lpfWidget            = nullptr;
     QLineEdit*      lpfAddrInput         = nullptr;
     QSpinBox*       lpfPortSpin          = nullptr;
     QLineEdit*      lpfTargetAddrInput   = nullptr;
     QSpinBox*       lpfTargetPortSpin    = nullptr;

     QWidget*        rpfWidget            = nullptr;
     QSpinBox*       rpfPortSpin          = nullptr;
     QLineEdit*      rpfTargetAddrInput   = nullptr;
     QSpinBox*       rpfTargetPortSpin    = nullptr;

     bool       valid      = false;
     QString    message    = "";
     QString    tunnelType = "";
     QByteArray jsonData;

     qint64 AgentId = 0;
     QStringList typeNames;

     void createUI();

public:
     explicit DialogTunnel(qint64 agentId, bool s4, bool s5, bool lpf, bool rpf);
     ~DialogTunnel() override;

     void StartDialog();
     bool IsValid() const;
     QString GetMessage() const;
     QString GetTunnelType() const;
     QString GetEndpoint() const;
     QByteArray GetTunnelData() const;

protected Q_SLOTS:
     void changeType(int index) const;
     void onSocks5AuthCheckChange() const;
     void onButtonCreate();
     void onButtonCancel();
};

#endif
