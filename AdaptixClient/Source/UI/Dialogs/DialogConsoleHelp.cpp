#include <UI/Dialogs/DialogConsoleHelp.h>
#include <Agent/Commander.h>
#include <Utils/FontManager.h>
#include <Client/ConsoleTheme.h>
#include <MainAdaptix.h>
#include <Client/Settings.h>
#include <Client/AuthProfile.h>

#include <QAbstractItemView>
#include <QLineEdit>
#include <QMouseEvent>
#include <QTextCursor>

#include <oclero/qlementine/style/QlementineStyle.hpp>
#include <oclero/qlementine/style/Theme.hpp>

DialogConsoleHelp::DialogConsoleHelp(Commander* c, qint64 id, QWidget* parent) : QDialog(parent), commander(c), agentId(id)
{
    setWindowTitle(tr("Command Help"));
    setAttribute(Qt::WA_DeleteOnClose);
    setModal(false);
    createUI();
    applyTheme();
    reloadCatalog();
    refreshHelp();
}

DialogConsoleHelp::~DialogConsoleHelp() = default;

void DialogConsoleHelp::createUI()
{
    commandCombo = new QComboBox(this);
    commandCombo->setEditable(true);
    commandCombo->setInsertPolicy(QComboBox::NoInsert);
    commandCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    commandCombo->setToolTip(tr("Command or subcommand. Help updates as you type."));
    if (auto* completer = commandCombo->completer()) {
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        completer->setCompletionMode(QCompleter::PopupCompletion);
    }

    helpView = new QPlainTextEdit(this);
    helpView->setReadOnly(true);
    helpView->setLineWrapMode(QPlainTextEdit::NoWrap);
    helpView->setFont(FontManager::instance().getFont("Hack"));
    helpView->setPlaceholderText(tr("Help text"));

    closeButton = new QPushButton(tr("Close"), this);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(commandCombo);
    layout->addWidget(helpView, 1);
    layout->addWidget(closeButton, 0, Qt::AlignRight);

    if (commandCombo->lineEdit())
        commandCombo->lineEdit()->installEventFilter(this);
    if (commandCombo->view() && commandCombo->view()->viewport())
        commandCombo->view()->viewport()->installEventFilter(this);
    if (auto* completer = commandCombo->completer()) {
        if (completer->popup() && completer->popup()->viewport())
            completer->popup()->viewport()->installEventFilter(this);
    }

    connect(commandCombo, &QComboBox::editTextChanged, this, &DialogConsoleHelp::refreshHelp);
    connect(commandCombo, &QComboBox::currentIndexChanged, this, [this](int) { refreshHelp(); });
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    if (commander)
        connect(commander, &Commander::commandsUpdated, this, &DialogConsoleHelp::reloadCatalog);

    resize(780, 520);
}

void DialogConsoleHelp::applyTheme()
{
    helpView->setFont(FontManager::instance().getFont("Hack"));
    auto* qs = qobject_cast<oclero::qlementine::QlementineStyle*>(qApp ? qApp->style() : nullptr);
    const auto& at = qs ? qs->theme() : oclero::qlementine::Theme();
    ConsoleThemeData theme;
    if (GlobalClient && GlobalClient->settings->data.ConsoleUseAppTheme)
        theme = ConsoleThemeManager::buildFromQlementine(GlobalClient->settings->data.MainTheme, GlobalClient->settings->data.ConsoleBgImagePath, GlobalClient->settings->data.ConsoleBgDimming);
    else
        theme = ConsoleThemeManager::instance().theme();
    helpView->setStyleSheet(
        QStringLiteral("QPlainTextEdit { background-color: %1; color: %2; border: 1px solid %3; border-radius: 4px; }")
            .arg(theme.background.color.name(), theme.textColor.name(), at.borderColor.name()));
}

QString DialogConsoleHelp::stripHelpPrefix(QString text)
{
    text = text.trimmed();
    if (text.compare(QStringLiteral("help"), Qt::CaseInsensitive) == 0)
        return QString();
    if (text.startsWith(QStringLiteral("help "), Qt::CaseInsensitive) ||
        text.startsWith(QStringLiteral("help\t"), Qt::CaseInsensitive))
        return text.mid(5).trimmed();
    return text;
}

void DialogConsoleHelp::setQuery(const QString& text)
{
    const QString seed = stripHelpPrefix(text);
    refreshing = true;
    commandCombo->setEditText(seed);
    refreshing = false;
    refreshHelp();
    if (commandCombo->lineEdit()) {
        commandCombo->lineEdit()->setFocus();
        commandCombo->lineEdit()->end(false);
    }
}

void DialogConsoleHelp::reloadCatalog()
{
    if (!commander)
        return;
    const QString current = commandCombo->currentText();
    refreshing = true;
    commandCombo->clear();
    commandCombo->addItems(commander->GetHelpCatalog());
    commandCombo->setEditText(current);
    refreshing = false;
    if (auto* completer = commandCombo->completer()) {
        completer->setCaseSensitivity(Qt::CaseInsensitive);
        completer->setFilterMode(Qt::MatchContains);
        if (completer->popup() && completer->popup()->viewport())
            completer->popup()->viewport()->installEventFilter(this);
    }
}

void DialogConsoleHelp::refreshHelp()
{
    if (refreshing || !commander)
        return;

    const QString query = stripHelpPrefix(commandCombo->currentText());
    const QString cmdline = query.isEmpty() ? QStringLiteral("help") : (QStringLiteral("help ") + query);
    const CommanderResult result = commander->ProcessInput(agentId, cmdline);

    helpView->clear();
    QString body = result.message;
    if (result.styledHelp && !result.error) {
        const auto theme = ConsoleThemeManager::instance().theme();
        QTextCharFormat dim;
        dim.setForeground(theme.debug.color);
        QTextCharFormat plain;
        plain.setForeground(QColor(helpView->palette().color(QPalette::Text)));
        QTextCursor cur(helpView->document());
        const QStringList lines = body.split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines[i];
            const bool inactive = line.startsWith(kHelpInactiveMarker);
            if (inactive)
                line.remove(0, 1);
            cur.setCharFormat(inactive ? dim : plain);
            cur.insertText(line);
            if (i + 1 < lines.size())
                cur.insertText(QStringLiteral("\n"));
        }
    } else {
        helpView->setPlainText(body);
    }

    QTextCursor top(helpView->document());
    top.movePosition(QTextCursor::Start);
    helpView->setTextCursor(top);
}

bool DialogConsoleHelp::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonDblClick) {
        QString command;
        QAbstractItemView* view = nullptr;
        if (commandCombo->view() && watched == commandCombo->view()->viewport())
            view = commandCombo->view();
        else if (commandCombo->completer() && commandCombo->completer()->popup()
                 && watched == commandCombo->completer()->popup()->viewport())
            view = commandCombo->completer()->popup();

        if (view) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            const QModelIndex idx = view->indexAt(mouse->pos());
            if (idx.isValid())
                command = idx.data().toString();
        } else if (commandCombo->lineEdit() && watched == commandCombo->lineEdit()) {
            command = commandCombo->currentText().trimmed();
        }
        if (!command.isEmpty()) {
            Q_EMIT insertCommand(command);
            close();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}
