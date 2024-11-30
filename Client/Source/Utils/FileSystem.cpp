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

QString GetBasenameWindows(const QString& path)
{
    QStringList pathParts = path.split("\\", Qt::SkipEmptyParts);
    return  pathParts[pathParts.size()-1];
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

QString GetParentPathWindows(const QString& path)
{
    if (path.length() == 2 && path[1] == ':')
        return path;

    if (path.startsWith("\\\\") && !path.contains('\\'))
        return path;

    QString parentPath = path;
    if (!parentPath.endsWith('\\'))
        parentPath += '\\';

    int lastBackslashIndex = parentPath.lastIndexOf('\\');
    int secondLastBackslashIndex = parentPath.lastIndexOf('\\', lastBackslashIndex - 1);

    if (secondLastBackslashIndex != -1) {
        return parentPath.left(secondLastBackslashIndex);
    }

    return path;
}
