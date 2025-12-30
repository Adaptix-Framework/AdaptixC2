#ifndef ADAPTIXCLIENT_DIALOGCONNECT_H
#define ADAPTIXCLIENT_DIALOGCONNECT_H

#include <main.h>
#include <Utils/CustomElements.h>

class AuthProfile;

class DialogConnect : public QDialog
{
Q_OBJECT

    bool toConnect    = false;
    bool isNewProject = true;

    QGridLayout*  gridLayout          = nullptr;
    QGroupBox*    groupUserInfo       = nullptr;
    QGroupBox*    groupServerDetails  = nullptr;
    QGroupBox*    groupProject        = nullptr;
    QLabel*       label_User          = nullptr;
    QLabel*       label_Password      = nullptr;
    QLabel*       label_Project       = nullptr;
    QLabel*       label_ProjectDir    = nullptr;
    QLabel*       label_Url           = nullptr;
    QLineEdit*    lineEdit_User       = nullptr;
    QLineEdit*    lineEdit_Password   = nullptr;
    QLineEdit*    lineEdit_Project    = nullptr;
    QLineEdit*    lineEdit_ProjectDir = nullptr;
    QLineEdit*    lineEdit_Url        = nullptr;
    QPushButton*  buttonConnect       = nullptr;
    QPushButton*  buttonNewProfile    = nullptr;
    QPushButton*  buttonLoad          = nullptr;
    QPushButton*  buttonSave          = nullptr;
    QLabel*       label_Profiles      = nullptr;
    QMenu*        menuContext         = nullptr;
    CardListWidget* cardWidget     = nullptr;

    bool parseUrl(QString &host, QString &port, QString &endpoint) const;
    QString buildUrl(const QString &host, const QString &port, const QString &endpoint) const;

    void createUI();
    bool checkValidInput() const;
    void loadProjects();
    void clearFields();

    QVector<AuthProfile> listProjects;
    bool projectDirTouched = false;

public:
    explicit DialogConnect();
    ~DialogConnect() override;

    AuthProfile* StartDialog();

private Q_SLOTS:
    void onButton_Connect();
    void handleContextMenu( const QPoint &pos );
    void onProfileSelected();
    void itemRemove();
    void onProjectNameChanged(const QString &text);
    void onProjectDirEdited(const QString &text);
    void onSelectProjectDir();
    void onButton_NewProfile();
    void onButton_Load();
    void onButton_Save();
};

#endif