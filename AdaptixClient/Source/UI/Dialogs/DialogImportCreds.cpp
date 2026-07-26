#include <UI/Dialogs/DialogImportCreds.h>
#include <Utils/NonBlockingDialogs.h>

#include <QFile>
#include <QPointer>

DialogImportCreds::DialogImportCreds(QWidget* parent) : QDialog(parent)
{
    createUI();
    connect(loadFileButton, &QPushButton::clicked, this, &DialogImportCreds::onLoadFile);
    connect(importButton,   &QPushButton::clicked, this, &DialogImportCreds::onImport);
    connect(cancelButton,   &QPushButton::clicked, this, &DialogImportCreds::onCancel);
    connect(textEdit,       &QPlainTextEdit::textChanged, this, &DialogImportCreds::onTextChanged);
}

DialogImportCreds::~DialogImportCreds() = default;

void DialogImportCreds::createUI()
{
    resize(720, 520);
    setWindowTitle("Import Credentials");
    setProperty("Main", "base");

    helpLabel = new QLabel(this);
    helpLabel->setWordWrap(true);
    helpLabel->setText( QStringLiteral(
            "Paste text or load a file. Supported line formats (auto-detected):\n"
            "• secretsdump/NTDS:  DOMAIN\\user:rid:lmhash:nthash:::  or  DOMAIN/user:rid:lm:nt:::\n"
            "• domain\\user:secret\n"
            "• domain/user:secret  (impacket)\n"
            "• user:secret\n"));

    textEdit = new QPlainTextEdit(this);
    textEdit->setPlaceholderText( QStringLiteral(
            "CORP\\admin:P@ssw0rd\n"
            "CORP/svc_sql:NThash...\n"
            "admin:Password1\n"
            "CORP\\alice:1001:aad3...:31d6cfe0d16ae931b73c59d7e0c089c0:::"));

    tagLabel = new QLabel("Tag:", this);
    tagInput = new QLineEdit(this);
    tagInput->setPlaceholderText("imported");
    tagInput->setMaximumWidth(180);

    storageLabel = new QLabel("Storage:", this);
    storageCombo = new QComboBox(this);
    storageCombo->setEditable(true);
    storageCombo->addItems(QStringList() << "manual" << "ntds" << "lsass" << "sam" << "browser" << "dpapi" << "database");
    storageCombo->setCurrentText("manual");
    storageCombo->setMaximumWidth(140);

    optionsLayout = new QHBoxLayout();
    optionsLayout->addWidget(tagLabel);
    optionsLayout->addWidget(tagInput);
    optionsLayout->addSpacing(12);
    optionsLayout->addWidget(storageLabel);
    optionsLayout->addWidget(storageCombo);
    optionsLayout->addStretch();

    statusLabel = new QLabel(this);
    statusLabel->setText("0 credentials");

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

void DialogImportCreds::StartDialog()
{
    m_valid = false;
    m_data.clear();
    exec();
}

void DialogImportCreds::updateStatus()
{
    auto storage = storageCombo->currentText().trimmed().isEmpty() ? QStringLiteral("manual") : storageCombo->currentText().trimmed();
    const auto parsed = parseCredentialsImport( textEdit->toPlainText(), tagInput->text().trimmed(), storage );
    if (parsed.items.isEmpty()) {
        statusLabel->setText(parsed.skipped > 0 ? QStringLiteral("0 credentials (%1 lines skipped)").arg(parsed.skipped) : QStringLiteral("0 credentials"));
        importButton->setEnabled(false);
    } else {
        statusLabel->setText(QStringLiteral("%1 credentials %2").arg(parsed.items.size()).arg(parsed.skipped > 0 ? QStringLiteral(" (%1 skipped/duplicates)").arg(parsed.skipped) : QString()));
        importButton->setEnabled(true);
    }
}

void DialogImportCreds::onTextChanged()
{
    updateStatus();
}

void DialogImportCreds::onLoadFile()
{
    QPointer<DialogImportCreds> self = this;
    NonBlockingDialogs::getOpenFileName(this, "Load credentials file", QString(), "Text Files (*.txt *.csv *.ntds *.out *.log);;All Files (*)", [self](const QString& path) {
        if (!self || path.isEmpty())
            return;
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            MessageError(QStringLiteral("Failed to open file"));
            return;
        }
        const QByteArray data = f.read(20 * 1024 * 1024);
        self->textEdit->setPlainText(QString::fromUtf8(data));
    });
}

void DialogImportCreds::onImport()
{
    const QString storage = storageCombo->currentText().trimmed().isEmpty() ? QStringLiteral("manual") : storageCombo->currentText().trimmed();

    const auto parsed = parseCredentialsImport( textEdit->toPlainText(), tagInput->text().trimmed(), storage);
    if (parsed.items.isEmpty()) {
        MessageError(QStringLiteral("No credentials parsed from input"));
        return;
    }

    m_data = parsed.items;
    m_valid = true;
    accept();
}

void DialogImportCreds::onCancel()
{
    m_valid = false;
    m_data.clear();
    reject();
}
