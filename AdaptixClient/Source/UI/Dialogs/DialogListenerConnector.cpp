#include <UI/Dialogs/DialogListenerConnector.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxElementWrappers.h>

void DialogListenerConnector::createUI()
{
    this->setWindowTitle("Create Connector");
    this->setProperty("Main", "base");

    labelListener     = new QLabel("Listener:", this);
    inputListenerName = new QLineEdit(this);
    inputListenerName->setReadOnly(true);

    connectorGroupbox = new QGroupBox("Connector config", this);
    connectorGroupbox->setAlignment(Qt::AlignHCenter);

    buttonConnect = new QPushButton("Connect", this);
    buttonConnect->setDefault(true);
    buttonConnect->setFixedWidth(140);

    buttonCancel = new QPushButton("Cancel", this);
    buttonCancel->setFixedWidth(140);

    auto headerLayout = new QGridLayout();
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setHorizontalSpacing(8);
    headerLayout->setVerticalSpacing(6);
    headerLayout->addWidget(labelListener,     0, 0);
    headerLayout->addWidget(inputListenerName, 0, 1);
    headerLayout->setColumnStretch(0, 0);
    headerLayout->setColumnStretch(1, 1);

    auto buttonsLayout = new QHBoxLayout();
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->addStretch();
    buttonsLayout->addWidget(buttonConnect);
    buttonsLayout->addWidget(buttonCancel);
    buttonsLayout->addStretch();

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);
    mainLayout->addLayout(headerLayout);
    mainLayout->addWidget(connectorGroupbox, 1);
    mainLayout->addLayout(buttonsLayout);

    this->setLayout(mainLayout);
}

DialogListenerConnector::DialogListenerConnector(AdaptixWidget* adaptixWidget, const QString &listenerName, const QString &listenerType)
{
    this->adaptixWidget = adaptixWidget;
    this->listenerName  = listenerName;
    this->listenerType  = listenerType;

    createUI();

    inputListenerName->setText(listenerName);

    connect(buttonConnect, &QPushButton::clicked, this, &DialogListenerConnector::onButtonConnect);
    connect(buttonCancel,  &QPushButton::clicked, this, &QDialog::reject);
}

DialogListenerConnector::~DialogListenerConnector() = default;

void DialogListenerConnector::SetProfile(const AuthProfile &profile)
{
    this->authProfile = profile;
}

void DialogListenerConnector::SetConnectorUI(AxContainerWrapper* container, QWidget* panel, int height, int width)
{
    this->container = container;
    this->panel     = panel;

    if (panel) {
        auto* layout = new QVBoxLayout();
        layout->setContentsMargins(5, 5, 5, 5);
        layout->addWidget(panel);
        connectorGroupbox->setLayout(layout);
    }
    if (height > 0 && width > 0)
        this->resize(width, height);
}

void DialogListenerConnector::Start()
{
    this->setModal(true);
    this->show();
}

void DialogListenerConnector::onButtonConnect()
{
    if (!container) {
        MessageError("Connector UI is not initialized");
        return;
    }

    QString configData = container->toJson();

    buttonConnect->setEnabled(false);
    buttonCancel->setEnabled(false);

    QPointer<DialogListenerConnector> safeThis = this;
    HttpReqListenerConnectorAsync(listenerName, configData, authProfile,
        [safeThis](bool success, const QString &message, const QJsonObject&) {
            if (!safeThis)
                return;
            safeThis->buttonConnect->setEnabled(true);
            safeThis->buttonCancel->setEnabled(true);

            if (!success) {
                MessageError(message);
                return;
            }
            safeThis->accept();
        });
}
