#include <Classes/DialogView.h>

DialogView::DialogView(WidgetBuilder *builder) {
    layout = new QVBoxLayout(this);

    if(builder && builder->valid) {
        widgetBuilder = builder;
        layout->addWidget(widgetBuilder->GetWidget());
    }

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    layout->addWidget(buttonBox);
    this->setLayout(layout);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

DialogView::~DialogView()= default;

QString DialogView::GetData() {
    if(widgetBuilder && widgetBuilder->valid) {
        return widgetBuilder->CollectData();
    }
    return {""};
}
