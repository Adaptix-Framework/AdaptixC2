#include <Utils/FileSystem.h>

QString ReadFileString(const QString &filePath, bool* result)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        *result = false;
        return QString();
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    *result = true;
    return content;
}

QString GetRootPathWindows(const QString& path)
{
    if (path.startsWith("\\\\")) {
        int secondSlash = path.indexOf('\\', 2);
        if (secondSlash != -1) {
            return path.left(secondSlash);
        }
    }

    int firstSlash = path.indexOf('\\');
    if (firstSlash != -1) {
        return path.left(firstSlash);
    }

    return path;
}