#include <UI/Dialogs/DialogImportTargets.h>
#include <Utils/NonBlockingDialogs.h>

#include <QFile>
#include <QPointer>

DialogImportTargets::DialogImportTargets(QWidget* parent) : QDialog(parent)
{
    createUI();
    connect(loadFileButton, &QPushButton::clicked, this, &DialogImportTargets::onLoadFile);
    connect(importButton,   &QPushButton::clicked, this, &DialogImportTargets::onImport);
    connect(cancelButton,   &QPushButton::clicked, this, &DialogImportTargets::onCancel);
    connect(textEdit,       &QPlainTextEdit::textChanged, this, &DialogImportTargets::onTextChanged);
    connect(aliveCheck,     &QCheckBox::toggled, this, [this](bool) { updateStatus(); });
}

DialogImportTargets::~DialogImportTargets() = default;

void DialogImportTargets::createUI()
{
    resize(740, 540);
    setWindowTitle("Import Targets");
    setProperty("Main", "base");

    helpLabel = new QLabel(this);
    helpLabel->setWordWrap(true);
    helpLabel->setText( QStringLiteral(
            "Paste scan output or load a file. Supported formats (auto-detected):\n"
            "• Nmap XML (-oX)\n"
            "• Nmap greppable (-oG) — Host: lines\n"
            "• Nmap normal output — “Nmap scan report for …”\n"
            "• NetExec / CrackMapExec — e.g.  SMB  10.0.0.5  445  DC01  [*] Windows … (domain:CORP)\n"));

    textEdit = new QPlainTextEdit(this);
    textEdit->setPlaceholderText( QStringLiteral(
            "Host: 10.0.0.5 (dc01.corp.local)\tStatus: Up\n"
            "Nmap scan report for dc01.corp.local (10.0.0.5)\n"
            "SMB         10.0.0.5     445    DC01    [*] Windows Server 2019 (name:DC01) (domain:CORP)"));

    tagLabel = new QLabel("Default tag:", this);
    tagInput = new QLineEdit(this);
    tagInput->setPlaceholderText("scan");
    tagInput->setMaximumWidth(180);

    aliveLabel = new QLabel("Mark alive:", this);
    aliveCheck = new QCheckBox(this);
    aliveCheck->setChecked(true);

    optionsLayout = new QHBoxLayout();
    optionsLayout->addWidget(tagLabel);
    optionsLayout->addWidget(tagInput);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(aliveLabel);
    optionsLayout->addWidget(aliveCheck);
    optionsLayout->addStretch();

    statusLabel = new QLabel(this);
    statusLabel->setText("0 targets");

    loadFileButton = new QPushButton("Load file", this);
    importButton   = new QPushButton("Import", this);
    importButton->setDefault(true);
    importButton->setEnabled(false);
    cancelButton   = new QPushButton("Cancel", this);

    buttonLayout = new QHBoxLayout();
    buttonLayout->addWidget(loadFileButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(importButton);

    mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(helpLabel);
    mainLayout->addWidget(textEdit, 1);
    mainLayout->addLayout(optionsLayout);
    mainLayout->addWidget(statusLabel);
    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
}

void DialogImportTargets::StartDialog()
{
    m_valid = false;
    m_data.clear();
    exec();
}

void DialogImportTargets::updateStatus()
{
    const auto parsed = parseTargetsImport(textEdit->toPlainText(), tagInput->text().trimmed(), aliveCheck->isChecked());

    if (parsed.items.isEmpty()) {
        statusLabel->setText(QStringLiteral("0 targets"));
        importButton->setEnabled(false);
    } else {
        QString extra;
        if (!parsed.detectedFormat.isEmpty())
            extra += QStringLiteral(" · %1").arg(parsed.detectedFormat);
        if (parsed.skipped > 0)
            extra += QStringLiteral(" · %1 skipped").arg(parsed.skipped);
        statusLabel->setText(QStringLiteral("%1 targets %2").arg(parsed.items.size()).arg(extra));
        importButton->setEnabled(true);
    }
}

void DialogImportTargets::onTextChanged()
{
    updateStatus();
}

void DialogImportTargets::onLoadFile()
{
    QPointer<DialogImportTargets> self = this;
    NonBlockingDialogs::getOpenFileName( this, "Load targets / scan file", QString(), "Scan Files (*.xml *.gnmap *.nmap *.txt *.log *.out);;All Files (*)", [self](const QString& path) {
        if (!self || path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            MessageError(QStringLiteral("Failed to open file"));
            return;
        }
        const QByteArray data = f.read(30 * 1024 * 1024);
        self->textEdit->setPlainText(QString::fromUtf8(data));
    });
}

void DialogImportTargets::onImport()
{
    const auto parsed = parseTargetsImport(textEdit->toPlainText(), tagInput->text().trimmed(), aliveCheck->isChecked());

    if (parsed.items.isEmpty()) {
        MessageError(QStringLiteral("No targets parsed from input"));
        return;
    }

    m_data = parsed.items;
    m_valid = true;
    accept();
}

void DialogImportTargets::onCancel()
{
    m_valid = false;
    m_data.clear();
    reject();
}
