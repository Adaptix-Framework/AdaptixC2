#ifndef ADAPTIXCLIENT_DIALOGPAYLOAD_H
#define ADAPTIXCLIENT_DIALOGPAYLOAD_H

#include <main.h>
#include <Client/AuthProfile.h>

#include <QButtonGroup>
#include <QScrollArea>

class AdaptixWidget;
class AxContainerWrapper;

namespace oclero::qlementine { class Switch; }

class DialogPayload : public QDialog
{
Q_OBJECT
    AdaptixWidget* adaptixWidget = nullptr;
    AuthProfile authProfile;
    qint64      payloadId = 0;
    PayloadData data;
    QString     rawConfigJson;

    QPushButton*   infoViewBtn     = nullptr;
    QPushButton*   configViewBtn   = nullptr;
    QButtonGroup*  viewButtonGroup = nullptr;
    QStackedWidget* contentStack   = nullptr;

    QWidget*   infoPage         = nullptr;
    QLineEdit* nameInput        = nullptr;
    QLineEdit* descriptionInput = nullptr;
    QLineEdit* artifactInput    = nullptr;
    QLineEdit* archInput        = nullptr;
    oclero::qlementine::Switch* hiddenSwitch = nullptr;

    QLineEdit* idValue        = nullptr;
    QLineEdit* uidValue       = nullptr;
    QLineEdit* typeValue      = nullptr;
    QLineEdit* listenersValue = nullptr;
    QLineEdit* sizeValue      = nullptr;
    QLineEdit* filenameValue  = nullptr;
    QLineEdit* creatorValue   = nullptr;
    QLineEdit* createdValue   = nullptr;
    QLineEdit* md5Value       = nullptr;
    QLineEdit* sha1Value      = nullptr;
    QLineEdit* sha256Value    = nullptr;

    QWidget*        configPage         = nullptr;
    QStackedWidget* configStack        = nullptr; // 0 = agent UI, 1 = plain JSON fallback
    QWidget*        agentConfigHost    = nullptr;
    QScrollArea*    agentConfigScroll  = nullptr;
    QTextEdit*      configText         = nullptr;
    AxContainerWrapper* agentContainer = nullptr;
    QWidget*        agentFormWidget    = nullptr;
    bool            agentUiLoaded      = false;

    QPushButton* saveButton   = nullptr;
    QPushButton* cancelButton = nullptr;

    void createUI();
    void applyDataToForm();
    void loadAgentConfigUi();
    void showInfoView();
    void showConfigView();
    static QString formatSize(qint64 bytes);
    static QString prettyConfig(const QString& raw);

public:
    explicit DialogPayload(AdaptixWidget* w, qint64 payloadId, QWidget* parent = nullptr);
    ~DialogPayload() override;

    void openOnConfigTab(bool onConfig);
    void loadAndShow();

protected Q_SLOTS:
    void onSave();
    void onViewChanged(int id);
};

#endif
