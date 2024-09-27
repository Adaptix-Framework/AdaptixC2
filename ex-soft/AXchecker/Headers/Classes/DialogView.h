#ifndef AXCHECKER_DIALOGVIEW_H
#define AXCHECKER_DIALOGVIEW_H

#include <main.h>
#include <Classes/WidgetBuilder.h>

class DialogView : public QDialog
{
    WidgetBuilder*    widgetBuilder = nullptr;
    QVBoxLayout*      layout        = nullptr;
    QDialogButtonBox* buttonBox     = nullptr;

public:
    explicit DialogView(WidgetBuilder* builder);
    ~DialogView() override;

    QString GetData();
};

#endif //AXCHECKER_DIALOGVIEW_H