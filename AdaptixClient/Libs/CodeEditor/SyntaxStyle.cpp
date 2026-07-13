#include <SyntaxStyle.h>
#include <QDebug>
#include <QXmlStreamReader>
#include <QFile>

SyntaxStyle::SyntaxStyle(QObject* parent) : QObject(parent), m_name(), m_data(), m_loaded(false) {}

bool SyntaxStyle::load(const QString& xmlContent)
{
    QXmlStreamReader reader(xmlContent);

    while (!reader.atEnd() && !reader.hasError()) {
        auto token = reader.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (reader.name().toString() == "style-scheme") {
                if (reader.attributes().hasAttribute("name"))
                    m_name = reader.attributes().value("name").toString();
            }
            else if (reader.name().toString() == "style") {
                auto attributes = reader.attributes();
                auto name = attributes.value("name");

                QTextCharFormat format;
                if (attributes.hasAttribute("background"))
                    format.setBackground(QColor(attributes.value("background").toString()));
                if (attributes.hasAttribute("foreground"))
                    format.setForeground(QColor(attributes.value("foreground").toString()));
                if (attributes.hasAttribute("bold") && attributes.value("bold").toString() == "true")
                    format.setFontWeight(QFont::Weight::Bold);
                if (attributes.hasAttribute("italic") && attributes.value("italic").toString() == "true")
                    format.setFontItalic(true);

                if (attributes.hasAttribute("underlineStyle")) {
                    auto underline = attributes.value("underlineStyle").toString();
                    auto s = QTextCharFormat::UnderlineStyle::NoUnderline;

                    if (underline == "SingleUnderline")
                        s = QTextCharFormat::UnderlineStyle::SingleUnderline;
                    else if (underline == "DashUnderline")
                        s = QTextCharFormat::UnderlineStyle::DashUnderline;
                    else if (underline == "DotLine")
                        s = QTextCharFormat::UnderlineStyle::DotLine;
                    else if (underline == "DashDotLine")
                        s = QTextCharFormat::DashDotLine;
                    else if (underline == "DashDotDotLine")
                        s = QTextCharFormat::DashDotDotLine;
                    else if (underline == "WaveUnderline")
                        s = QTextCharFormat::WaveUnderline;

                    format.setUnderlineStyle(s);
                }

                if (attributes.hasAttribute("underlineColor"))
                    format.setUnderlineColor(QColor(attributes.value("underlineColor").toString()));

                m_data[name.toString()] = format;
            }
        }
    }

    m_loaded = !reader.hasError();
    return m_loaded;
}

bool SyntaxStyle::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    return load(file.readAll());
}

QString SyntaxStyle::name() const
{
    return m_name;
}

QTextCharFormat SyntaxStyle::getFormat(const QString& name) const
{
    auto result = m_data.find(name);
    if (result == m_data.end())
        return QTextCharFormat();
    return result.value();
}

bool SyntaxStyle::isLoaded() const
{
    return m_loaded;
}

SyntaxStyle* SyntaxStyle::defaultStyle()
{
    static SyntaxStyle style;
    if (!style.isLoaded()) {
        Q_INIT_RESOURCE(codeeditor_resources);
        QFile file(":/default_style.xml");
        if (!file.open(QIODevice::ReadOnly))
            return &style;
        if (!style.load(file.readAll()))
            qWarning() << "CC-Editor: can't load default style";
    }
    return &style;
}
