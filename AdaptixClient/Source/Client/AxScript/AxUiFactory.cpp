#include <Client/AxScript/AxUiFactory.h>

AxUiFactory::AxUiFactory(QObject* parent) : QObject(parent)
{
    parentWidget = new QWidget();
}

AxUiFactory::~AxUiFactory()
{
    delete parentWidget;
}

QWidget* AxUiFactory::createWidget()
{
    auto* w = new QWidget(parentWidget);
    w->setProperty("Main", "base");
    return w;
}

QFrame* AxUiFactory::createLine(int shape)
{
    auto* line = new QFrame(parentWidget);
    line->setFrameShape(static_cast<QFrame::Shape>(shape));
    if (shape == QFrame::VLine)
        line->setMinimumHeight(25);
    else
        line->setMinimumWidth(25);
    return line;
}

QLabel* AxUiFactory::createLabel(const QString& text)
{
    return new QLabel(text, parentWidget);
}

QLineEdit* AxUiFactory::createLineEdit(const QString& text)
{
    return new QLineEdit(text, parentWidget);
}

QComboBox* AxUiFactory::createComboBox()
{
    return new QComboBox(parentWidget);
}

QCheckBox* AxUiFactory::createCheckBox(const QString& label)
{
    return new QCheckBox(label, parentWidget);
}

QSpinBox* AxUiFactory::createSpinBox()
{
    return new QSpinBox(parentWidget);
}

QDateEdit* AxUiFactory::createDateEdit()
{
    return new QDateEdit(parentWidget);
}

QTimeEdit* AxUiFactory::createTimeEdit()
{
    return new QTimeEdit(parentWidget);
}

QPushButton* AxUiFactory::createPushButton(const QString& text)
{
    return new QPushButton(text, parentWidget);
}

QPlainTextEdit* AxUiFactory::createPlainTextEdit(const QString& text)
{
    return new QPlainTextEdit(text, parentWidget);
}

QListWidget* AxUiFactory::createListWidget()
{
    return new QListWidget(parentWidget);
}

QTableWidget* AxUiFactory::createTableWidget()
{
    return new QTableWidget(parentWidget);
}

QTabWidget* AxUiFactory::createTabWidget()
{
    return new QTabWidget(parentWidget);
}

QGroupBox* AxUiFactory::createGroupBox(const QString& title)
{
    return new QGroupBox(title, parentWidget);
}

QSplitter* AxUiFactory::createSplitter(int orientation)
{
    return new QSplitter(static_cast<Qt::Orientation>(orientation), parentWidget);
}

QScrollArea* AxUiFactory::createScrollArea()
{
    return new QScrollArea(parentWidget);
}

QStackedWidget* AxUiFactory::createStackedWidget()
{
    return new QStackedWidget(parentWidget);
}

QDialog* AxUiFactory::createDialog(const QString& title)
{
    auto* dialog = new QDialog(parentWidget);
    dialog->setWindowTitle(title);
    dialog->setProperty("Main", "base");
    return dialog;
}

FileSelector* AxUiFactory::createFileSelector()
{
    return new FileSelector(parentWidget);
}
