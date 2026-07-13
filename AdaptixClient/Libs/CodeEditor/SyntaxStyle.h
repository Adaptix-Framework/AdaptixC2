#pragma once

#include <QObject>
#include <QTextCharFormat>
#include <QMap>

class SyntaxStyle : public QObject
{
Q_OBJECT
    QString m_name;
    QMap<QString, QTextCharFormat> m_data;
    bool m_loaded;

public:
    explicit SyntaxStyle(QObject* parent = nullptr);

    bool load(const QString& xmlContent);
    bool loadFromFile(const QString& filePath);

    QString name() const;
    bool isLoaded() const;
    QTextCharFormat getFormat(const QString& name) const;

    static SyntaxStyle* defaultStyle();
};
