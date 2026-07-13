#pragma once

#include <QWidget>

class QTreeView;
class QFileSystemModel;
class QLabel;

class FileBrowser : public QWidget
{
Q_OBJECT
    QLabel* m_header;
    QTreeView* m_treeView;
    QFileSystemModel* m_model;

public:
    explicit FileBrowser(QWidget* parent = nullptr);

    void setRootPath(const QString& path);
    QString rootPath() const;

Q_SIGNALS:
    void fileSelected(const QString& filePath);
};
