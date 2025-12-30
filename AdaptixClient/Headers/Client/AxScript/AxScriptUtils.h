#ifndef AXSCRIPTUTILS_H
#define AXSCRIPTUTILS_H

#include <QJSValue>
#include <QJSEngine>
#include <QStringList>
#include <QSet>

class AxScriptUtils {
public:
    static QStringList jsArrayToStringList(const QJSValue& array)
    {
        QStringList result;
        if (!array.isArray())
            return result;

        const int length = array.property("length").toInt();
        result.reserve(length);
        for (int i = 0; i < length; ++i)
            result << array.property(i).toString();

        return result;
    }

    static QSet<QString> jsArrayToStringSet(const QJSValue& array)
    {
        QSet<QString> result;
        if (!array.isArray())
            return result;

        const int length = array.property("length").toInt();
        result.reserve(length);
        for (int i = 0; i < length; ++i)
            result.insert(array.property(i).toString());

        return result;
    }

    static QSet<int> parseOsSet(const QJSValue& os)
    {
        QSet<int> result;
        if (!os.isArray())
            return result;

        const int length = os.property("length").toInt();
        for (int i = 0; i < length; ++i) {
            QString val = os.property(i).toString();
            if (val == "windows")    result.insert(1);
            else if (val == "linux") result.insert(2);
            else if (val == "macos") result.insert(3);
        }
        return result;
    }

    static QList<int> parseOsList(const QJSValue& os)
    {
        QList<int> result;
        if (!os.isArray())
            return result;

        const int length = os.property("length").toInt();
        for (int i = 0; i < length; ++i) {
            QString val = os.property(i).toString();
            if (val == "windows")    result.append(1);
            else if (val == "linux") result.append(2);
            else if (val == "macos") result.append(3);
        }
        return result;
    }

    static bool isValidArray(const QJSValue& value)
    {
        return !value.isUndefined() && !value.isNull() && value.isArray();
    }

    static bool isValidNonEmptyArray(const QJSValue& value)
    {
        return isValidArray(value) && value.property("length").toInt() > 0;
    }

    static bool isOptionalValidArray(const QJSValue& value)
    {
        if (value.isUndefined() || value.isNull())
            return true;
        return value.isArray();
    }

    static QJSValue stringListToJsArray(QJSEngine* engine, const QStringList& list)
    {
        QJSValue array = engine->newArray(list.size());
        for (int i = 0; i < list.size(); ++i)
            array.setProperty(i, list[i]);
        return array;
    }

    static QJSValue variantListToJsArray(QJSEngine* engine, const QVariantList& list)
    {
        return engine->toScriptValue(list);
    }
};

#endif
