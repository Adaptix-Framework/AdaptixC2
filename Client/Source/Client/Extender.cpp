#include <Client/Extender.h>

Extender::Extender()
{
    dialogExtender = new DialogExtender();
}

Extender::~Extender() = default;

void Extender::LoadExtention(QString path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    QString content = QString(file.readAll());
    file.close();
}
