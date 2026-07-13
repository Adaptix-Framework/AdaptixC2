#include <Language.h>
#include <QIODevice>
#include <QXmlStreamReader>

Language::Language(QIODevice* device, QObject* parent) : QObject(parent), m_loaded(false), m_list()
{
    load(device);
}

bool Language::load(QIODevice* device)
{
    if (device == nullptr)
        return false;

    QXmlStreamReader reader(device);

    QString name;
    QStringList list;
    bool readText = false;

    while (!reader.atEnd() && !reader.hasError()) {
        auto type = reader.readNext();

        if (type == QXmlStreamReader::TokenType::StartElement) {
            if (reader.name().toString() == "section") {
                if (!list.empty()) {
                    m_list[name] = list;
                    list.clear();
                }
                name = reader.attributes().value("name").toString();
            }
            else if (reader.name().toString() == "name") {
                readText = true;
            }
        }
        else if (type == QXmlStreamReader::TokenType::Characters && readText) {
            list << reader.text().toString();
            readText = false;
        }
    }

    if (!list.empty())
        m_list[name] = list;

    m_loaded = !reader.hasError();
    return m_loaded;
}

QStringList Language::keys()
{
    return m_list.keys();
}

QStringList Language::names(const QString& key)
{
    return m_list[key];
}

bool Language::isLoaded() const
{
    return m_loaded;
}
