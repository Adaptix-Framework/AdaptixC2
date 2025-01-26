#include <UI/Dialogs/DialogSettings.h>

DialogSettings::DialogSettings(QWidget* w )
{
    mainWidget = w;

    this->createUI();

    connect(listSettings, &QListWidget::currentRowChanged, this, &DialogSettings::onStackChange);
}

void DialogSettings::createUI()
{
    this->setWindowTitle("Adaptix Settings");
    this->resize(1200, 600);

    listSettings = new QListWidget(this);
    listSettings->setFixedWidth(150);
    listSettings->setAlternatingRowColors(true);

    labelHeader = new QLabel(this);
    labelHeader->setStyleSheet("font-size: 16px; font-weight: bold; margin-bottom: 10px;");
    labelHeader->setText("Extensions");

    lineFrame = new QFrame(this);
    lineFrame->setFrameShape(QFrame::HLine);
    lineFrame->setFrameShadow(QFrame::Plain);

    headerLayout = new QVBoxLayout();
    headerLayout->addWidget(labelHeader);
    headerLayout->addWidget(lineFrame);
    headerLayout->setSpacing(0);

    hSpacer      = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    buttonOk     = new QPushButton("Apply ", this);
    buttonCancel = new QPushButton("Cancel", this);

    stackSettings = new QStackedWidget(this);

    layoutMain = new QGridLayout(this);
    layoutMain->setContentsMargins(4, 4, 4, 4);
    layoutMain->addWidget(listSettings, 0, 0, 2, 1);
    layoutMain->addLayout(headerLayout, 0, 1, 1, 3);
    layoutMain->addWidget(stackSettings, 1, 1, 1, 3);
    layoutMain->addItem(hSpacer, 2, 1, 1, 1);
    layoutMain->addWidget(buttonOk, 2, 2, 1, 1);
    layoutMain->addWidget(buttonCancel, 2, 3, 1, 1);

    this->setLayout(layoutMain);
}

void DialogSettings::onStackChange(int index)
{
    QString text = listSettings->item(index)->text();
    labelHeader->setText(text);
    stackSettings->setCurrentIndex(index);
}
