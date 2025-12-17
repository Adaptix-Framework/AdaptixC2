#ifndef AXUIFACTORY_H
#define AXUIFACTORY_H

#include <QObject>
#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QPlainTextEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QGroupBox>
#include <QSplitter>
#include <QScrollArea>
#include <QStackedWidget>
#include <QDialog>
#include <QFrame>
#include <QBoxLayout>
#include <QGridLayout>
#include <Utils/CustomElements.h>

class AxUiFactory : public QObject {
    Q_OBJECT

    QWidget* parentWidget;

public:
    explicit AxUiFactory(QObject* parent = nullptr);
    ~AxUiFactory() override;

    QWidget* getParentWidget() const { return parentWidget; }

public Q_SLOTS:
    QWidget* createWidget();
    QFrame* createLine(int shape);
    QLabel* createLabel(const QString& text);
    QLineEdit* createLineEdit(const QString& text);
    QComboBox* createComboBox();
    QCheckBox* createCheckBox(const QString& label);
    QSpinBox* createSpinBox();
    QDateEdit* createDateEdit();
    QTimeEdit* createTimeEdit();
    QPushButton* createPushButton(const QString& text);
    QPlainTextEdit* createPlainTextEdit(const QString& text);
    QListWidget* createListWidget();
    QTableWidget* createTableWidget();
    QTabWidget* createTabWidget();
    QGroupBox* createGroupBox(const QString& title);
    QSplitter* createSplitter(int orientation);
    QScrollArea* createScrollArea();
    QStackedWidget* createStackedWidget();
    QDialog* createDialog(const QString& title);
    FileSelector* createFileSelector();
};

#endif
