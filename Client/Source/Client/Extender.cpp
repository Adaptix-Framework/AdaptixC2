#include <Client/Extender.h>

Extender::Extender()
{
    dialogExtender = new DialogExtender(this);
}

Extender::~Extender() = default;

void Extender::LoadFromFile(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QString content = QString(file.readAll());
    file.close();
}

void Extender::NewExtension(ExtensionFile extFile)
{

}

void Extender::EnableExtension(QString path)
{

}

void Extender::DisableExtension(QString path)
{

}

void Extender::DeleteExtension(QString path)
{

}