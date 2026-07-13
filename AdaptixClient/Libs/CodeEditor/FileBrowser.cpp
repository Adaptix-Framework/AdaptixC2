#include "FileBrowser.h"

#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QDir>
#include <QFont>
#include <QTimer>

FileBrowser::FileBrowser(QWidget* parent) : QWidget(parent), m_header(new QLabel(this)), m_treeView(new QTreeView(this)), m_model(new QFileSystemModel(this))
{
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    QFont headerFont;
    headerFont.setBold(true);
    headerFont.setPointSize(10);
    m_header->setFont(headerFont);
    m_header->setAlignment(Qt::AlignCenter);
    m_header->setText("No project open");
    layout->addWidget(m_header);

    m_treeView->setVisible(false);
    layout->addWidget(m_treeView);

    QTimer::singleShot(0, this, [this]() {
        m_model->setParent(this);
        m_model->setNameFilterDisables(false);
        m_model->setNameFilters({"*.c", "*.cpp", "*.h", "*.hpp", "*.cc", "*.hh",
                                  "*.axs", "*.js", "*.mjs", "*.ts",
                                  "*.xml", "*.json", "*.txt", "*.cmake", "*.py",
                                  "*.java", "*.qml", "*.qml", "*.md", "*.sh"});

        m_treeView->setModel(m_model);
        m_treeView->setAnimated(true);
        m_treeView->setHeaderHidden(true);
        m_treeView->setColumnHidden(1, true);
        m_treeView->setColumnHidden(2, true);
        m_treeView->setColumnHidden(3, true);
        m_treeView->header()->setStretchLastSection(false);
        m_treeView->header()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_treeView->setIndentation(16);
        m_treeView->setExpandsOnDoubleClick(true);

        connect(m_treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
            if (index.isValid() && !m_model->isDir(index))
                Q_EMIT fileSelected(m_model->filePath(index));
        });
    });
}

void FileBrowser::setRootPath(const QString& path)
{
    if (path.isEmpty()) {
        m_treeView->setVisible(false);
        m_header->setText("No project open");
        return;
    }

    if (!m_treeView->model()) {
        QTimer::singleShot(100, this, [this, path]() {
            setRootPath(path);
        });
        return;
    }

    auto index = m_model->setRootPath(path);
    m_treeView->setRootIndex(index);
    m_treeView->setVisible(true);

    QString dirName = QDir(path).dirName();
    if (dirName.isEmpty())
        dirName = path;
    m_header->setText(dirName);
}

QString FileBrowser::rootPath() const
{
    return m_model->rootPath();
}
