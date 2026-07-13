#ifndef ADAPTIXCLIENT_AXSCRIPTCOMPLETER_H
#define ADAPTIXCLIENT_AXSCRIPTCOMPLETER_H

#include <QCompleter>

class AxScriptCompleter : public QCompleter
{
Q_OBJECT

public:
    explicit AxScriptCompleter(QObject* parent = nullptr);
};

#endif
