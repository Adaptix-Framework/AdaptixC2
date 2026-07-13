#pragma once

#include <QCompleter>

class CXXCompleter : public QCompleter
{
Q_OBJECT

public:
    explicit CXXCompleter(QObject* parent = nullptr);
};
