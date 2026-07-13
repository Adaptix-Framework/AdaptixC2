#pragma once

#include <QObject>
#include <QStringList>
#include <QMap>

class QIODevice;

class Language : public QObject
{
Q_OBJECT
    bool m_loaded;
    QMap<QString, QStringList> m_list;

    bool load(QIODevice* device);

public:
    explicit Language(QIODevice* device, QObject* parent = nullptr);

    bool isLoaded() const;
    QStringList keys();
    QStringList names(const QString& key);
};
