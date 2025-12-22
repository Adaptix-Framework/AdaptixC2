#ifndef ADAPTIXCLIENT_DIALOGCONNECT_H
#define ADAPTIXCLIENT_DIALOGCONNECT_H

#include <main.h>
#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyledItemDelegate>

class QStyleOptionViewItem;
class QModelIndex;

class AuthProfile;

class ProfileListDelegate : public QStyledItemDelegate
{
public:
    explicit ProfileListDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    QFont getFont(const QWidget *widget) const;
};

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
    QLabel*       label_ProjectDir    = nullptr;
    QLabel*       label_Host          = nullptr;
    QLabel*       label_Port          = nullptr;
    QLabel*       label_Endpoint      = nullptr;
    QLineEdit*    lineEdit_User       = nullptr;
    QLineEdit*    lineEdit_Password   = nullptr;
    QLineEdit*    lineEdit_Project    = nullptr;
    QLineEdit*    lineEdit_ProjectDir = nullptr;
    QLineEdit*    lineEdit_Host       = nullptr;
    QLineEdit*    lineEdit_Port       = nullptr;
    QLineEdit*    lineEdit_Endpoint   = nullptr;
    QPushButton*  ButtonConnect       = nullptr;
    QPushButton*  ButtonClear         = nullptr;
    QListWidget*  listWidget          = nullptr;
    QPushButton*  ButtonNewProfile    = nullptr;
    QPushButton*  ButtonLoad         = nullptr;
    QPushButton*  ButtonSave         = nullptr;
    QLabel*       label_Profiles      = nullptr;
    QMenu*        menuContex          = nullptr;

    bool projectDirTouched = false;

    void createUI();
    bool checkValidInput() const;
    void loadProjects();
    void clearFields();

public:
    QVector<AuthProfile> listProjects;

    explicit DialogConnect();
    ~DialogConnect() override;

    AuthProfile* StartDialog();

private Q_SLOTS:
    void onButton_Connect();
    void onButton_Clear();
    void handleContextMenu( const QPoint &pos ) const;
    void itemSelected();
    void itemRemove();
    void onProjectNameChanged(const QString &text);
    void onProjectDirEdited(const QString &text);
    void onSelectProjectDir();
    void onButton_NewProfile();
    void onButton_Load();
    void onButton_Save();
};

#endif
