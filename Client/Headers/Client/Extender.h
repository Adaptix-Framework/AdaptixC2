#ifndef ADAPTIXCLIENT_EXTENDER_H
#define ADAPTIXCLIENT_EXTENDER_H

#include <main.h>
#include <UI/Dialogs/DialogExtender.h>

class Extender
{

public:
    Extender();
    ~Extender();

    DialogExtender* dialogExtender = nullptr;

    void LoadExtention(QString path);
};

#endif //ADAPTIXCLIENT_EXTENDER_H
