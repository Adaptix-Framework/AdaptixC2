#include <UI/Widgets/TaskOutputWidget.h>
#include <Client/ConsoleTheme.h>
#include <Client/Settings.h>
#include <Utils/FontManager.h>
#include <MainAdaptix.h>

TaskOutputWidget::TaskOutputWidget()
{
    this->createUI();

    outputTextEdit->viewport()->installEventFilter(this);
    inputMessage->installEventFilter(this);

    connect(&ConsoleThemeManager::instance(), &ConsoleThemeManager::themeChanged, this, &TaskOutputWidget::applyTheme);
    connect(&FontManager::instance(), &FontManager::typographyChanged, this, [this]() {
        if (inputMessage)
            inputMessage->setFont(FontManager::instance().appMonoFont());
        if (outputTextEdit)
            outputTextEdit->setFont(FontManager::instance().appMonoFont());
    });
    applyTheme();
}

TaskOutputWidget::~TaskOutputWidget() = default;

void TaskOutputWidget::createUI()
{
    inputMessage = new QLineEdit(this);
    inputMessage->setReadOnly(true);
    inputMessage->setFont( FontManager::instance().getFont("Hack") );

    outputTextEdit = new QTextEdit(this);
    outputTextEdit->setReadOnly(true);
    outputTextEdit->setWordWrapMode(QTextOption::WrapAnywhere);
    outputTextEdit->setFont( FontManager::instance().getFont("Hack") );

    mainGridLayout = new QGridLayout(this );
    mainGridLayout->setVerticalSpacing(4 );
    mainGridLayout->setContentsMargins(0, 0, 0, 4 );
    mainGridLayout->addWidget( inputMessage, 0, 0, 1, 1 );
    mainGridLayout->addWidget( outputTextEdit, 1, 0, 1, 1 );

    this->setLayout(mainGridLayout);
}

void TaskOutputWidget::applyTheme()
{
    forceThemeColors();
}

ConsoleThemeData TaskOutputWidget::getActiveTheme() const
{
    if (GlobalClient->settings->data.ConsoleUseAppTheme)
        return ConsoleThemeManager::buildFromQlementine( GlobalClient->settings->data.MainTheme, GlobalClient->settings->data.ConsoleBgImagePath, GlobalClient->settings->data.ConsoleBgDimming);
    return ConsoleThemeManager::instance().theme();
}

void TaskOutputWidget::forceThemeColors()
{
    if (m_suppressPaletteGuard)
        return;
    m_suppressPaletteGuard = true;

    const auto theme = getActiveTheme();
    const QString bgName = theme.background.color.name();
    const QString fgName = theme.textColor.name();

    inputMessage->setStyleSheet(QStringLiteral("QLineEdit { background-color: %1; color: %2; border: 1px solid #2A2A2A; padding: 4px; border-radius: 4px; }").arg(bgName, fgName));
    outputTextEdit->setStyleSheet(QStringLiteral("QTextEdit { background-color: %1; color: %2; border: 1px solid #2A2A2A; border-radius: 4px; }").arg(bgName, fgName));

    auto applyVp = [&](QWidget* w) {
        QPalette p = w->palette();
        for (auto group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
            p.setColor(group, QPalette::Base, theme.background.color);
            p.setColor(group, QPalette::Text, theme.textColor);
        }
        w->setPalette(p);
    };
    applyVp(outputTextEdit->viewport());
    applyVp(inputMessage);

    m_suppressPaletteGuard = false;
}

bool TaskOutputWidget::eventFilter(QObject* obj, QEvent* event)
{
    if ((obj == outputTextEdit->viewport() || obj == inputMessage) && (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange))
        forceThemeColors();

    return QWidget::eventFilter(obj, event);
}

void TaskOutputWidget::SetConten(const QString &message, const QString &text) const
{
    if( message.isEmpty() )
        inputMessage->clear();
    else
        inputMessage->setText(TrimmedEnds(message).toHtmlEscaped());

    if ( text.isEmpty() )
        outputTextEdit->clear();
    else
        outputTextEdit->setText( TrimmedEnds(text) );
}
