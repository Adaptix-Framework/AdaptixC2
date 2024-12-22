#ifndef CMDCHECKER_MAINCMD_H
#define CMDCHECKER_MAINCMD_H

#include <main.h>

#include <Classes/Utils.h>
#include <Classes/Commander.h>

class MainCmd : public QWidget
{
    QGridLayout* mainLayout      = nullptr;
    QTextEdit*   consoleTextEdit = nullptr;
    QPushButton* selectRegButton = nullptr;
    QPushButton* selectExtButton = nullptr;
    QPushButton* loadRegButton   = nullptr;
    QPushButton* loadExtButton   = nullptr;
    QPushButton* execButton      = nullptr;
    QLineEdit*   pathRegInput    = nullptr;
    QLineEdit*   pathExtInput    = nullptr;
    QLineEdit*   commandInput    = nullptr;
    QCompleter*  completer       = nullptr;

    Commander* commander = nullptr;

    void createUI();
    void setStyle();

public:
    explicit MainCmd();
    ~MainCmd() override;

    void Start();

public slots:
    void SelectRegFile();
    void SelectExtFile();
    void LoadRegFile();
    void LoadExtFile();
    void ExecuteCommand();
};

#endif //CMDCHECKER_MAINCMD_H
