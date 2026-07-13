#include <AxScriptCompleter.h>
#include <Language.h>
#include <QFile>
#include <QStringListModel>

AxScriptCompleter::AxScriptCompleter(QObject* parent) : QCompleter(parent)
{
    Q_INIT_RESOURCE(codeeditor_resources);
    QFile fl(":/languages/axscript.xml");

    if (!fl.open(QIODevice::ReadOnly))
        return;

    Language language(&fl);

    if (!language.isLoaded())
        return;

    QStringList allNames;
    auto keys = language.keys();
    for (auto&& key : keys) {
        allNames.append(language.names(key));
    }

    allNames.append({
        "var", "let", "const", "function", "return",
        "if", "else", "for", "while", "do", "switch", "case", "default",
        "break", "continue", "throw", "try", "catch", "finally",
        "new", "typeof", "instanceof", "void", "delete",
        "true", "false", "null", "undefined",
        "console.log", "console.error",
        "JSON.stringify", "JSON.parse",
        "parseInt", "parseFloat", "isNaN", "isFinite",
        "Array", "Object", "String", "Number", "Boolean", "RegExp", "Date", "Error",
        "setTimeout", "setInterval", "clearTimeout", "clearInterval",
        "require", "module", "exports",
    });

    allNames.removeDuplicates();
    allNames.sort(Qt::CaseInsensitive);

    auto* model = new QStringListModel(allNames, this);
    setModel(model);
    setCompletionMode(QCompleter::CompletionMode::PopupCompletion);
    setCaseSensitivity(Qt::CaseSensitive);
    setWrapAround(true);
}
