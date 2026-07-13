#pragma once

#include <QWidget>
#include <QVector>

class QLineEdit;
class QCheckBox;
class QTableWidget;
class QLabel;

class BuildPanel : public QWidget
{
Q_OBJECT
public:
    struct Param
    {
        QString name;
        QString type;
        QString value;
    };

    explicit BuildPanel(QWidget* parent = nullptr);
    QSize sizeHint() const override;

    QString defines() const;
    QString buildCommand() const;
    QString runCommand() const;
    QString formattedRunCommand() const;
    bool useMainEngine() const;
    QVector<Param> parameters() const;

    void setDefines(const QString& v);
    void setBuildCommand(const QString& v);
    void setRunCommand(const QString& v);
    void setMainEngineChecked(bool checked);
    void setParameters(const QVector<Param>& params);

    void setBuildRowVisible(bool visible);
    void setRunRowVisible(bool visible);
    void setDefinesRowVisible(bool visible);
    void setParamsVisible(bool visible);
    void setMainEngineVisible(bool visible);

Q_SIGNALS:
    void configurationChanged();

private:
    QWidget* m_buildRow       = nullptr;
    QWidget* m_runRow         = nullptr;
    QWidget* m_definesRow     = nullptr;
    QWidget* m_paramsHeader   = nullptr;
    QWidget* m_paramsTableWidget = nullptr;

    QLineEdit* m_buildEdit    = nullptr;
    QLineEdit* m_runEdit      = nullptr;
    QLineEdit* m_definesEdit  = nullptr;
    QCheckBox* m_mainEngineCheck = nullptr;
    QTableWidget* m_paramsTable  = nullptr;

    void addParameterRow(const QString& name = QString(), const QString& type = QString("str"), const QString& value = QString());
    void setupContextMenu();
    static QLabel* makePrefixLabel(const QString& text);
    static QWidget* makeIconLabel(const char* svg, const QString& text);
};
