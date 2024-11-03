#ifndef CMDCHECKER_MAINCMD_H
#define CMDCHECKER_MAINCMD_H

#include <main.h>

#include <Classes/Utils.h>
#include <Classes/Commander.h>

class MainCmd : public QWidget
{
    QGridLayout* mainLayout     = nullptr;
    QTextEdit*   consoleTextEdit = nullptr;
    QPushButton* selectButton   = nullptr;
    QPushButton* loadButton     = nullptr;
    QPushButton* execButton     = nullptr;
    QLineEdit*   pathInput      = nullptr;
    QLineEdit*   commandInput   = nullptr;
    QCompleter*  completer      = nullptr;

    Commander* commander = nullptr;

    void createUI();
    void setStyle();

public:
    explicit MainCmd();
    ~MainCmd() override;

    void Start();

public slots:
    void SelectFile();
    void LoadFile();
    void ExecuteCommand();
};

#endif //CMDCHECKER_MAINCMD_H
