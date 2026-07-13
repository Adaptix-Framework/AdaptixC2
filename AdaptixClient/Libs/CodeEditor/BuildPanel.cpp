#include "BuildPanel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QCheckBox>
#include <QHeaderView>
#include <QComboBox>
#include <QFontDatabase>
#include <QSvgRenderer>
#include <QPainter>
#include <QMenu>

namespace {

const QStringList kParamTypes{
    "str", "cstr", "int", "u32", "i32", "u64", "i64", "ptr", "bool"
};

const char* kBuildIconSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24"
     fill="none" stroke="#808080" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
  <path d="M14.5 4.5l5 5"/>
  <path d="M3 21l8.5-8.5"/>
  <path d="M13 6l5 5 2-2a2.83 2.83 0 0 0-4-4l-3 1z" transform="translate(2.5 -2.5)"/>
  <path d="M9.5 9.5L4 15l-1 4 4-1 5.5-5.5"/>
  <path d="M14.5 4.5L9.5 9.5"/>
</svg>
)SVG";

const char* kRunIconSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16" viewBox="0 0 24 24"
     fill="#808080" stroke="#808080" stroke-width="1.5" stroke-linejoin="round">
  <polygon points="6 4 20 12 6 20 6 4"/>
</svg>
)SVG";

QPixmap renderSvg(const char* svg, int size)
{
    QSvgRenderer renderer(QByteArray::fromRawData(svg, int(qstrlen(svg))));
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    renderer.render(&painter);
    return pm;
}

QFont monoFont()
{
    QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    f.setPointSize(qMax(8, f.pointSize() - 1));
    return f;
}

}

BuildPanel::BuildPanel(QWidget* parent) : QWidget(parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 5, 8, 5);
    root->setSpacing(5);

    const QFont cmdFont = monoFont();

    m_buildRow = new QWidget(this);
    {
        auto* rowLayout = new QHBoxLayout(m_buildRow);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        m_buildEdit = new QLineEdit(m_buildRow);
        m_buildEdit->setFont(cmdFont);
        m_buildEdit->setPlaceholderText("build command, e.g. gcc -O2 -o out src/main.c");
        rowLayout->addWidget(makeIconLabel(kBuildIconSvg, QStringLiteral("Build")));
        rowLayout->addWidget(m_buildEdit, 1);
    }
    root->addWidget(m_buildRow);

    m_runRow = new QWidget(this);
    {
        auto* rowLayout = new QHBoxLayout(m_runRow);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        m_runEdit = new QLineEdit(m_runRow);
        m_runEdit->setFont(cmdFont);
        m_runEdit->setPlaceholderText("run command, e.g. ./out");
        rowLayout->addWidget(makeIconLabel(kRunIconSvg, QStringLiteral("Run")));
        rowLayout->addWidget(m_runEdit, 1);
    }
    root->addWidget(m_runRow);

    m_definesRow = new QWidget(this);
    {
        auto* rowLayout = new QHBoxLayout(m_definesRow);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(6);
        rowLayout->addWidget(makePrefixLabel(QStringLiteral("# Defines")));
        m_definesEdit = new QLineEdit(m_definesRow);
        m_definesEdit->setFont(cmdFont);
        m_definesEdit->setPlaceholderText("FOO=1; BAR=2  (semicolon-separated)");
        rowLayout->addWidget(m_definesEdit, 1);
    }
    root->addWidget(m_definesRow);

    m_mainEngineCheck = new QCheckBox("Execute in Main engine (REPL mode)", this);
    m_mainEngineCheck->setToolTip("Execute in the global Main AxScript engine. Unchecked = isolated Sandbox engine.");
    m_mainEngineCheck->setVisible(false);
    root->addWidget(m_mainEngineCheck);

    m_paramsHeader = new QWidget(this);
    {
        auto* headerLayout = new QHBoxLayout(m_paramsHeader);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(6);
        auto* paramsLabel = new QLabel("Parameters", m_paramsHeader);
        QFont labelFont = paramsLabel->font();
        labelFont.setBold(true);
        labelFont.setPointSize(qMax(8, labelFont.pointSize() - 1));
        paramsLabel->setFont(labelFont);
        paramsLabel->setStyleSheet("color: #c8c8c8;");
        headerLayout->addWidget(paramsLabel);
        headerLayout->addStretch();
    }
    root->addWidget(m_paramsHeader);

    m_paramsTableWidget = new QWidget(this);
    {
        auto* tableLayout = new QVBoxLayout(m_paramsTableWidget);
        tableLayout->setContentsMargins(0, 0, 0, 0);
        tableLayout->setSpacing(0);

        m_paramsTable = new QTableWidget(0, 3, m_paramsTableWidget);
        m_paramsTable->setHorizontalHeaderLabels({"Name", "Type", "Value"});
        m_paramsTable->setFont(cmdFont);
        m_paramsTable->horizontalHeader()->setStretchLastSection(true);
        m_paramsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_paramsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
        m_paramsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_paramsTable->verticalHeader()->setVisible(false);
        m_paramsTable->setMaximumHeight(120);
        m_paramsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_paramsTable->setAlternatingRowColors(true);
        m_paramsTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);
        tableLayout->addWidget(m_paramsTable);

        setupContextMenu();
    }
    root->addWidget(m_paramsTableWidget);

    connect(m_buildEdit, &QLineEdit::textChanged, this, &BuildPanel::configurationChanged);
    connect(m_runEdit, &QLineEdit::textChanged, this, &BuildPanel::configurationChanged);
    connect(m_definesEdit, &QLineEdit::textChanged, this, &BuildPanel::configurationChanged);
}

void BuildPanel::setupContextMenu()
{
    m_paramsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_paramsTable, &QTableWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QMenu menu;

        menu.addAction("Add parameter", this, [this]() {
            addParameterRow();
        });

        auto* removeAction = menu.addAction("Remove parameter", this, [this]() {
            auto row = m_paramsTable->currentRow();
            if (row >= 0) {
                m_paramsTable->removeRow(row);
                Q_EMIT configurationChanged();
            }
        });
        removeAction->setEnabled(m_paramsTable->currentRow() >= 0);

        menu.exec(m_paramsTable->viewport()->mapToGlobal(pos));
    });
}

QLabel* BuildPanel::makePrefixLabel(const QString& text)
{
    auto* label = new QLabel(text);
    label->setFont(monoFont());
    label->setStyleSheet(QStringLiteral("color: #808080;"));
    label->setMinimumWidth(72);
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setTextInteractionFlags(Qt::NoTextInteraction);
    return label;
}

QWidget* BuildPanel::makeIconLabel(const char* svg, const QString& text)
{
    auto* row = new QWidget;
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    auto* icon = new QLabel;
    icon->setPixmap(renderSvg(svg, 14));

    auto* label = new QLabel(text);
    label->setFont(monoFont());
    label->setStyleSheet(QStringLiteral("color: #808080;"));

    layout->addWidget(icon);
    layout->addWidget(label);
    layout->addStretch();

    row->setMinimumWidth(72);
    return row;
}

void BuildPanel::addParameterRow(const QString& name, const QString& type, const QString& value)
{
    int row = m_paramsTable->rowCount();
    m_paramsTable->insertRow(row);

    const auto editableFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;

    auto* nameItem = new QTableWidgetItem(name);
    nameItem->setFlags(editableFlags);
    m_paramsTable->setItem(row, 0, nameItem);

    auto* typeCombo = new QComboBox(this);
    typeCombo->addItems(kParamTypes);
    typeCombo->setCurrentText(type.isEmpty() ? QStringLiteral("str") : type);
    m_paramsTable->setCellWidget(row, 1, typeCombo);

    auto* valueItem = new QTableWidgetItem(value);
    valueItem->setFlags(editableFlags);
    m_paramsTable->setItem(row, 2, valueItem);

    m_paramsTable->setRowHeight(row, m_paramsTable->rowHeight(row));
    m_paramsTable->setCurrentCell(row, 0);
    m_paramsTable->editItem(nameItem);

    Q_EMIT configurationChanged();
}

QString BuildPanel::defines() const { return m_definesEdit->text(); }
QString BuildPanel::buildCommand() const { return m_buildEdit->text(); }
QString BuildPanel::runCommand() const { return m_runEdit->text(); }
bool BuildPanel::useMainEngine() const { return m_mainEngineCheck && m_mainEngineCheck->isChecked(); }

QVector<BuildPanel::Param> BuildPanel::parameters() const
{
    QVector<Param> params;
    for (int i = 0; i < m_paramsTable->rowCount(); ++i)
    {
        Param p;
        auto* nameItem = m_paramsTable->item(i, 0);
        auto* typeCombo = qobject_cast<QComboBox*>(m_paramsTable->cellWidget(i, 1));
        auto* valueItem = m_paramsTable->item(i, 2);

        p.name = nameItem ? nameItem->text() : QString();
        p.type = typeCombo ? typeCombo->currentText() : "str";
        p.value = valueItem ? valueItem->text() : QString();

        if (!p.name.isEmpty())
            params.append(p);
    }
    return params;
}

QString BuildPanel::formattedRunCommand() const
{
    QString cmd = runCommand();
    auto params = parameters();

    if (params.isEmpty())
        return cmd;

    QStringList args;
    for (const auto& p : params)
    {
        if (p.type == "str" || p.type == "cstr")
            args.append(QString("\"%1\"").arg(p.value));
        else
            args.append(p.value);
    }

    if (cmd.isEmpty())
        return args.join(" ");

    return cmd + " " + args.join(" ");
}

void BuildPanel::setDefines(const QString& v) { m_definesEdit->setText(v); }
void BuildPanel::setBuildCommand(const QString& v) { m_buildEdit->setText(v); }
void BuildPanel::setRunCommand(const QString& v) { m_runEdit->setText(v); }

void BuildPanel::setMainEngineChecked(bool checked)
{
    if (m_mainEngineCheck)
        m_mainEngineCheck->setChecked(checked);
}

void BuildPanel::setParameters(const QVector<Param>& params)
{
    m_paramsTable->setRowCount(0);
    for (const auto& p : params)
        addParameterRow(p.name, p.type, p.value);
}

QSize BuildPanel::sizeHint() const
{
    int h = 0;
    const auto margins = layout()->contentsMargins();
    const int spacing = layout()->spacing();
    h += margins.top() + margins.bottom();

    auto rowH = [](QWidget* w) -> int {
        return w && w->isVisible() ? w->sizeHint().height() : 0;
    };

    bool first = true;
    for (auto* row : { m_buildRow, m_runRow, m_definesRow, m_paramsHeader, m_paramsTableWidget }) {
        int rh = rowH(row);
        if (rh > 0) {
            if (!first)
                h += spacing;
            h += rh;
            first = false;
        }
    }
    return QSize(QWidget::sizeHint().width(), h);
}

void BuildPanel::setBuildRowVisible(bool visible)     { if (m_buildRow)      m_buildRow->setVisible(visible); }
void BuildPanel::setRunRowVisible(bool visible)       { if (m_runRow)        m_runRow->setVisible(visible); }
void BuildPanel::setDefinesRowVisible(bool visible)   { if (m_definesRow)    m_definesRow->setVisible(visible); }
void BuildPanel::setParamsVisible(bool visible)
{
    if (m_paramsHeader)      m_paramsHeader->setVisible(visible);
    if (m_paramsTableWidget) m_paramsTableWidget->setVisible(visible);
}
void BuildPanel::setMainEngineVisible(bool visible)   { if (m_mainEngineCheck) m_mainEngineCheck->setVisible(visible); }
