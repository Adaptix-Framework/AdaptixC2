#ifndef ADAPTIXCLIENT_DIALOGCONNECT_H
#define ADAPTIXCLIENT_DIALOGCONNECT_H

#include <main.h>
#include <Client/AuthProfile.h>

class DialogConnect : public QDialog
{
    bool toConnect    = false;
    bool isNewProject = true;

    QGridLayout*  gridLayout          = nullptr;
    QLabel*       label_UserInfo      = nullptr;
    QLabel*       label_User          = nullptr;
    QLabel*       label_Password      = nullptr;
    QLabel*       label_ServerDetails = nullptr;
    QLabel*       label_Project       = nullptr;
    QLabel*       label_Host          = nullptr;
    QLabel*       label_Port          = nullptr;
    QLabel*       label_Endpoint      = nullptr;
    QLineEdit*    lineEdit_User       = nullptr;
    QLineEdit*    lineEdit_Password   = nullptr;
    QLineEdit*    lineEdit_Project    = nullptr;
    QLineEdit*    lineEdit_Host       = nullptr;
    QLineEdit*    lineEdit_Port       = nullptr;
    QLineEdit*    lineEdit_Endpoint   = nullptr;
    QPushButton*  ButtonConnect       = nullptr;
    QTableWidget* tableWidget         = nullptr;
    QMenu*        menuContex          = nullptr;

    void createUI();
    bool checkValidInput() const;
    void loadProjects();

public:
    QVector<AuthProfile> listProjects;

    explicit DialogConnect();
    ~DialogConnect() override;

    AuthProfile* StartDialog();

private slots:
    void onButton_Connect();
    void handleContextMenu( const QPoint &pos ) const;
    void itemSelected();
    void itemRemove() const;

};

#endif //ADAPTIXCLIENT_DIALOGCONNECT_H
