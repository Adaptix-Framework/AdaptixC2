#include <UI/Dialogs/DialogPayload.h>
#include <UI/Widgets/AdaptixWidget.h>
#include <Client/Requestor.h>
#include <Client/AxScript/AxScriptManager.h>
#include <Client/AxScript/AxElementWrappers.h>
#include <Utils/FontManager.h>
#include <Utils/Logs.h>

#include <oclero/qlementine/widgets/Switch.hpp>

#include <QJSEngine>
#include <QJSValue>
#include <QPointer>
#include <QScrollArea>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QButtonGroup>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDateTime>
#include <QSet>

DialogPayload::DialogPayload(AdaptixWidget* w, qint64 id, QWidget* parent) : QDialog(parent), adaptixWidget(w), payloadId(id)
{
    if (w && w->GetProfile())
        authProfile = *w->GetProfile();
    createUI();
}

DialogPayload::~DialogPayload() = default;

QString DialogPayload::formatSize(qint64 bytes)
{
    if (bytes < 1024)
        return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024)
        return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024LL * 1024 * 1024)
        return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}

QString DialogPayload::prettyConfig(const QString& raw)
{
    if (raw.isEmpty())
        return QStringLiteral("(no build config stored)");
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8(), &pe);
    if (pe.error == QJsonParseError::NoError)
        return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    return raw;
}

void DialogPayload::createUI()
{
    this->setWindowTitle(QStringLiteral("Payload #%1").arg(payloadId));
    this->setProperty("Main", "base");
    this->resize(720, 560);
    this->setMinimumSize(640, 480);

    const int ctrlH = FontManager::instance().typography().controlHeight;

    auto* viewToggleGroup = new QFrame(this);
    viewToggleGroup->setObjectName(QStringLiteral("PayloadViewSegment"));
    auto* toggleLay = new QHBoxLayout(viewToggleGroup);
    toggleLay->setContentsMargins(3, 3, 3, 3);
    toggleLay->setSpacing(2);
    viewToggleGroup->setStyleSheet(QStringLiteral(
        "QFrame#PayloadViewSegment {"
        "  background: palette(mid);"
        "  border: 1px solid palette(dark);"
        "  border-radius: 6px;"
        "}"
    ));

    auto makeViewBtn = [this, ctrlH](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setCheckable(true);
        btn->setAutoExclusive(true);
        btn->setFocusPolicy(Qt::NoFocus);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(qMax(26, ctrlH - 4));
        btn->setMinimumWidth(100);
        btn->setObjectName(QStringLiteral("PayloadViewSegBtn"));
        return btn;
    };
    infoViewBtn   = makeViewBtn(QStringLiteral("Information"));
    configViewBtn = makeViewBtn(QStringLiteral("Config"));
    infoViewBtn->setChecked(true);
    const QString segBtnCss = QStringLiteral(
        "QPushButton#PayloadViewSegBtn {"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 4px;"
        "  color: palette(text);"
        "  font-weight: 600;"
        "  padding: 2px 14px;"
        "}"
        "QPushButton#PayloadViewSegBtn:hover {"
        "  background: palette(alternate-base);"
        "}"
        "QPushButton#PayloadViewSegBtn:checked {"
        "  background: palette(base);"
        "  color: palette(highlight);"
        "}"
    );
    infoViewBtn->setStyleSheet(segBtnCss);
    configViewBtn->setStyleSheet(segBtnCss);
    toggleLay->addWidget(infoViewBtn);
    toggleLay->addWidget(configViewBtn);

    viewButtonGroup = new QButtonGroup(this);
    viewButtonGroup->setExclusive(true);
    viewButtonGroup->addButton(infoViewBtn, 0);
    viewButtonGroup->addButton(configViewBtn, 1);
    connect(viewButtonGroup, &QButtonGroup::idClicked, this, &DialogPayload::onViewChanged);

    infoPage = new QWidget(this);
    auto* infoLay = new QVBoxLayout(infoPage);
    infoLay->setContentsMargins(8, 8, 8, 8);
    infoLay->setSpacing(12);

    auto setupForm = [](QFormLayout* form) {
        form->setContentsMargins(14, 14, 14, 14);
        form->setHorizontalSpacing(16);
        form->setVerticalSpacing(10);
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    };

    auto makeInlineLabel = [](QWidget* parent, const QString& text) {
        auto* lab = new QLabel(text, parent);
        lab->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        return lab;
    };
    auto makePairRow = [](QWidget* parent, QWidget* left, QWidget* midLabel, QWidget* right, int leftStretch = 1, int rightStretch = 1) {
        auto* row = new QWidget(parent);
        auto* lay = new QHBoxLayout(row);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(10);
        lay->addWidget(left, leftStretch);
        if (midLabel)
            lay->addWidget(midLabel, 0, Qt::AlignVCenter);
        lay->addWidget(right, rightStretch);
        return row;
    };

    auto* editGroup = new QGroupBox(QStringLiteral("Editable"), infoPage);
    auto* editForm = new QFormLayout(editGroup);
    setupForm(editForm);

    nameInput = new QLineEdit(editGroup);
    nameInput->setFixedHeight(ctrlH);
    nameInput->setPlaceholderText(QStringLiteral("Display name"));

    descriptionInput = new QLineEdit(editGroup);
    descriptionInput->setFixedHeight(ctrlH);
    descriptionInput->setPlaceholderText(QStringLiteral("Optional description / notes"));

    artifactInput = new QLineEdit(editGroup);
    artifactInput->setFixedHeight(ctrlH);
    artifactInput->setPlaceholderText(QStringLiteral("exe, dll, bin…"));

    archInput = new QLineEdit(editGroup);
    archInput->setFixedHeight(ctrlH);
    archInput->setMaximumWidth(140);
    archInput->setPlaceholderText(QStringLiteral("x64, x86…"));

    hiddenSwitch = new oclero::qlementine::Switch(editGroup);
    hiddenSwitch->setText(QStringLiteral("Hidden"));
    hiddenSwitch->setToolTip(QStringLiteral("Hide this payload from the default Payload Store list"));

    auto* artArchRow = makePairRow(editGroup, artifactInput, makeInlineLabel(editGroup, QStringLiteral("Arch")), archInput, 2, 0);

    editForm->addRow(QStringLiteral("Name"), nameInput);
    editForm->addRow(QStringLiteral("Description"), descriptionInput);
    editForm->addRow(QStringLiteral("Artifact"), artArchRow);
    editForm->addRow(QStringLiteral("Visibility"), hiddenSwitch);

    auto* metaGroup = new QGroupBox(QStringLiteral("Details"), infoPage);
    auto* metaForm = new QFormLayout(metaGroup);
    setupForm(metaForm);

    auto makeRoField = [metaGroup, ctrlH](bool mono = false) {
        auto* edit = new QLineEdit(metaGroup);
        edit->setFixedHeight(ctrlH);
        edit->setReadOnly(true);
        edit->setFocusPolicy(Qt::ClickFocus);
        if (mono)
            edit->setFont(FontManager::instance().appMonoFont());
        return edit;
    };

    idValue = makeRoField();
    idValue->setMaximumWidth(96);
    idValue->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    uidValue = makeRoField(true);
    uidValue->setPlaceholderText(QStringLiteral("—"));

    typeValue      = makeRoField();
    listenersValue = makeRoField();
    sizeValue      = makeRoField();
    sizeValue->setMaximumWidth(140);
    sizeValue->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    filenameValue  = makeRoField(true);
    creatorValue   = makeRoField();
    createdValue   = makeRoField();
    md5Value       = makeRoField(true);
    sha1Value      = makeRoField(true);
    sha256Value    = makeRoField(true);

    auto* idUidRow = makePairRow(metaGroup, idValue, makeInlineLabel(metaGroup, QStringLiteral("UID")), uidValue, 0, 1);
    auto* typeSizeRow = makePairRow(metaGroup, typeValue, makeInlineLabel(metaGroup, QStringLiteral("Size")), sizeValue, 1, 0);
    auto* creatorCreatedRow = makePairRow(metaGroup, creatorValue, makeInlineLabel(metaGroup, QStringLiteral("Created")), createdValue, 1, 1);

    metaForm->addRow(QStringLiteral("ID"), idUidRow);
    metaForm->addRow(QStringLiteral("Type"), typeSizeRow);
    metaForm->addRow(QStringLiteral("Listener(s)"), listenersValue);
    metaForm->addRow(QStringLiteral("Filename"), filenameValue);
    metaForm->addRow(QStringLiteral("Creator"), creatorCreatedRow);
    metaForm->addRow(QStringLiteral("MD5"), md5Value);
    metaForm->addRow(QStringLiteral("SHA1"), sha1Value);
    metaForm->addRow(QStringLiteral("SHA256"), sha256Value);

    auto* scroll = new QScrollArea(infoPage);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* scrollInner = new QWidget(scroll);
    auto* scrollLay = new QVBoxLayout(scrollInner);
    scrollLay->setContentsMargins(0, 0, 0, 0);
    scrollLay->setSpacing(12);
    scrollLay->addWidget(editGroup);
    scrollLay->addWidget(metaGroup);
    scrollLay->addStretch(1);
    scroll->setWidget(scrollInner);
    infoLay->addWidget(scroll, 1);

    configPage = new QWidget(this);
    auto* configLay = new QVBoxLayout(configPage);
    configLay->setContentsMargins(8, 8, 8, 8);
    configLay->setSpacing(0);

    configStack = new QStackedWidget(configPage);

    agentConfigHost = new QWidget(configStack);
    auto* agentHostLay = new QVBoxLayout(agentConfigHost);
    agentHostLay->setContentsMargins(0, 0, 0, 0);
    agentHostLay->setSpacing(0);

    agentConfigScroll = new QScrollArea(agentConfigHost);
    agentConfigScroll->setWidgetResizable(true);
    agentConfigScroll->setFrameShape(QFrame::NoFrame);
    agentHostLay->addWidget(agentConfigScroll, 1);

    agentConfigScroll->setWidget(new QWidget(agentConfigScroll));

    configText = new QTextEdit(configStack);
    configText->setReadOnly(true);
    configText->setFont(FontManager::instance().appMonoFont());
    configText->setFrameShape(QFrame::NoFrame);
    {
        QPalette pal = configText->palette();
        pal.setColor(QPalette::Base, pal.color(QPalette::Base).darker(115));
        configText->setPalette(pal);
        configText->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  background: palette(base);"
            "  color: palette(text);"
            "  border: 1px solid palette(mid);"
            "  border-radius: 4px;"
            "  padding: 6px;"
            "}"
        ));
    }

    configStack->addWidget(agentConfigHost); // 0
    configStack->addWidget(configText);      // 1
    configStack->setCurrentIndex(1);
    configLay->addWidget(configStack, 1);

    contentStack = new QStackedWidget(this);
    contentStack->addWidget(infoPage);   // 0
    contentStack->addWidget(configPage); // 1
    contentStack->setCurrentIndex(0);

    cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
    cancelButton->setFixedHeight(30);
    cancelButton->setFixedWidth(120);
    saveButton = new QPushButton(QStringLiteral("Save"), this);
    saveButton->setDefault(true);
    saveButton->setFixedHeight(30);
    saveButton->setFixedWidth(120);

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveButton, &QPushButton::clicked, this, &DialogPayload::onSave);

    auto* footer = new QWidget(this);
    footer->setObjectName(QStringLiteral("DialogFooter"));
    footer->setFixedHeight(50);
    footer->setStyleSheet(QStringLiteral("QWidget#DialogFooter { border-top: 1px solid palette(mid); }"));
    auto* footerLay = new QHBoxLayout(footer);
    footerLay->setContentsMargins(12, 0, 10, 0);
    footerLay->setSpacing(8);
    footerLay->addWidget(viewToggleGroup, 0, Qt::AlignVCenter);
    footerLay->addStretch(1);
    footerLay->addWidget(cancelButton);
    footerLay->addWidget(saveButton);

    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 0);
    mainLay->setSpacing(0);
    mainLay->addWidget(contentStack, 1);
    mainLay->addWidget(footer, 0);
    this->setLayout(mainLay);
}

void DialogPayload::openOnConfigTab(bool onConfig)
{
    if (onConfig)
        showConfigView();
    else
        showInfoView();
}

void DialogPayload::showInfoView()
{
    if (infoViewBtn) {
        QSignalBlocker b(infoViewBtn);
        infoViewBtn->setChecked(true);
    }
    if (configViewBtn) {
        QSignalBlocker b(configViewBtn);
        configViewBtn->setChecked(false);
    }
    if (contentStack)
        contentStack->setCurrentIndex(0);
}

void DialogPayload::showConfigView()
{
    if (configViewBtn) {
        QSignalBlocker b(configViewBtn);
        configViewBtn->setChecked(true);
    }
    if (infoViewBtn) {
        QSignalBlocker b(infoViewBtn);
        infoViewBtn->setChecked(false);
    }
    if (contentStack)
        contentStack->setCurrentIndex(1);
    if (!agentUiLoaded)
        loadAgentConfigUi();
}

void DialogPayload::onViewChanged(int id)
{
    if (contentStack)
        contentStack->setCurrentIndex(id);
    if (id == 1 && !agentUiLoaded)
        loadAgentConfigUi();
}

void DialogPayload::applyDataToForm()
{
    setWindowTitle(QStringLiteral("Payload #%1 — %2").arg(payloadId).arg(data.Name));

    nameInput->setText(data.Name);
    descriptionInput->setText(data.Description);
    artifactInput->setText(data.Artifact);
    archInput->setText(data.Arch);
    hiddenSwitch->setChecked(data.Hidden);

    idValue->setText(QStringLiteral("#%1").arg(data.PayloadId));
    uidValue->setText(data.Uid);
    typeValue->setText(data.AgentType.isEmpty() ? QStringLiteral("—") : data.AgentType);
    listenersValue->setText(data.Listeners.isEmpty() ? QStringLiteral("—") : data.Listeners.join(QStringLiteral(", ")));
    sizeValue->setText(formatSize(data.Size));
    filenameValue->setText(data.Filename.isEmpty() ? QStringLiteral("—") : data.Filename);
    creatorValue->setText(data.Creator.isEmpty() ? QStringLiteral("—") : data.Creator);
    createdValue->setText(data.Created > 0 ? QDateTime::fromSecsSinceEpoch(data.Created).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"));
    md5Value->setText(data.Md5.isEmpty() ? QStringLiteral("—") : data.Md5);
    sha1Value->setText(data.Sha1.isEmpty() ? QStringLiteral("—") : data.Sha1);
    sha256Value->setText(data.Sha256.isEmpty() ? QStringLiteral("—") : data.Sha256);

    configText->setPlainText(prettyConfig(rawConfigJson));
}

void DialogPayload::loadAgentConfigUi()
{
    agentUiLoaded = true;

    if (!adaptixWidget || !adaptixWidget->ScriptManager || data.AgentType.isEmpty()) {
        configStack->setCurrentIndex(1);
        return;
    }

    auto* engine = adaptixWidget->ScriptManager->AgentScriptEngine(data.AgentType);
    if (!engine) {
        configStack->setCurrentIndex(1);
        return;
    }

    QJSValue func = engine->globalObject().property(QStringLiteral("GenerateUI"));
    if (!func.isCallable()) {
        configStack->setCurrentIndex(1);
        return;
    }

    QStringList listenerTypes = data.Listeners;
    if (listenerTypes.isEmpty() && adaptixWidget->AgentTypes.contains(data.AgentType))
        listenerTypes = adaptixWidget->AgentTypes.value(data.AgentType).listenerTypes;

    QStringList regTypes;
    QSet<QString> seen;
    for (const QString& name : listenerTypes) {
        bool mapped = false;
        for (const auto& l : adaptixWidget->Listeners) {
            if (l.Name == name && !seen.contains(l.ListenerRegName)) {
                seen.insert(l.ListenerRegName);
                regTypes.append(l.ListenerRegName);
                mapped = true;
                break;
            }
        }
        if (!mapped && !seen.contains(name)) {
            seen.insert(name);
            regTypes.append(name);
        }
    }
    if (regTypes.isEmpty() && adaptixWidget->AgentTypes.contains(data.AgentType))
        regTypes = adaptixWidget->AgentTypes.value(data.AgentType).listenerTypes;

    QJSValue jsListeners = engine->newArray(regTypes.size());
    for (int i = 0; i < regTypes.size(); ++i)
        jsListeners.setProperty(i, regTypes[i]);

    QJSValue result = func.call(QJSValueList{jsListeners});
    if (result.isError() || !result.isObject()) {
        configStack->setCurrentIndex(1);
        return;
    }

    QJSValue ui_container = result.property(QStringLiteral("ui_container"));
    QJSValue ui_panel     = result.property(QStringLiteral("ui_panel"));
    if (ui_container.isUndefined() || !ui_container.isObject() || ui_panel.isUndefined() || !ui_panel.isQObject()) {
        configStack->setCurrentIndex(1);
        return;
    }

    auto* formElement = dynamic_cast<AxPanelWrapper*>(ui_panel.toQObject());
    auto* container = dynamic_cast<AxContainerWrapper*>(ui_container.toQObject());
    if (!formElement || !container) {
        configStack->setCurrentIndex(1);
        return;
    }

    agentContainer = container;
    agentFormWidget = formElement->widget();
    if (!agentFormWidget) {
        configStack->setCurrentIndex(1);
        return;
    }

    if (!rawConfigJson.isEmpty())
        container->fromJson(rawConfigJson);

    agentFormWidget->setEnabled(false);
    agentFormWidget->setParent(nullptr);

    auto* group = new QGroupBox(QStringLiteral("Agent config"), agentConfigHost);
    group->setAlignment(Qt::AlignHCenter);
    auto* gLay = new QVBoxLayout(group);
    gLay->setContentsMargins(8, 12, 8, 8);
    gLay->addWidget(agentFormWidget);
    if (agentConfigScroll)
        agentConfigScroll->setWidget(group);

    configStack->setCurrentIndex(0);
}

void DialogPayload::loadAndShow()
{
    if (!authProfile.GetHost().isEmpty()) {
        // profile valid
    } else if (adaptixWidget && adaptixWidget->GetProfile()) {
        authProfile = *adaptixWidget->GetProfile();
    }

    QPointer<DialogPayload> safeThis = this;
    const qint64 id = payloadId;
    HttpReqPayloadGetAsync(id, authProfile,
        [safeThis, id](bool success, const QString& message, const QJsonObject& response) {
            if (!safeThis)
                return;
            if (!success) {
                MessageError(message.isEmpty() ? QStringLiteral("Failed to load payload") : message);
                safeThis->reject();
                return;
            }
            const QJsonObject o = response.value(QStringLiteral("payload")).toObject();
            PayloadData p;
            p.PayloadId = static_cast<qint64>(o.value(QStringLiteral("p_id")).toDouble());
            if (p.PayloadId <= 0)
                p.PayloadId = id;
            p.Name = o.value(QStringLiteral("p_name")).toString();
            p.AgentType = o.value(QStringLiteral("p_type")).toString();
            p.Artifact = o.value(QStringLiteral("p_artifact")).toString();
            p.Arch = o.value(QStringLiteral("p_arch")).toString();
            if (o.value(QStringLiteral("p_listeners")).isArray()) {
                for (const QJsonValue& lv : o.value(QStringLiteral("p_listeners")).toArray())
                    p.Listeners << lv.toString();
            }
            p.Size = static_cast<qint64>(o.value(QStringLiteral("p_size")).toDouble());
            p.Sha1 = o.value(QStringLiteral("p_sha1")).toString();
            p.Sha256 = o.value(QStringLiteral("p_sha256")).toString();
            p.Md5 = o.value(QStringLiteral("p_md5")).toString();
            p.Creator = o.value(QStringLiteral("p_creator")).toString();
            p.Created = static_cast<qint64>(o.value(QStringLiteral("p_date")).toDouble());
            p.Hidden = o.value(QStringLiteral("p_hidden")).toBool();
            p.Filename = o.value(QStringLiteral("p_filename")).toString();
            p.BuildId = o.value(QStringLiteral("p_build_id")).toString();
            p.Watermark = o.value(QStringLiteral("p_watermark")).toString();
            p.Description = o.value(QStringLiteral("p_notes")).toString();
            p.Tag = o.value(QStringLiteral("p_tag")).toString();
            p.Uid = o.value(QStringLiteral("p_uid")).toString();
            p.Missing = o.value(QStringLiteral("p_missing")).toBool();
            p.ConfigJson = o.value(QStringLiteral("p_config")).toString();

            safeThis->data = p;
            safeThis->rawConfigJson = p.ConfigJson;
            safeThis->applyDataToForm();
            safeThis->show();
            safeThis->raise();
            safeThis->activateWindow();
        });
}

void DialogPayload::onSave()
{
    const QString name = nameInput->text().trimmed();
    if (name.isEmpty()) {
        MessageError(QStringLiteral("Name is required"));
        return;
    }
    if (authProfile.GetHost().isEmpty() && adaptixWidget && adaptixWidget->GetProfile())
        authProfile = *adaptixWidget->GetProfile();

    QPointer<DialogPayload> safeThis = this;
    HttpReqPayloadUpdateAsync(payloadId, name, descriptionInput->text().trimmed(), artifactInput->text().trimmed(), archInput->text().trimmed(), hiddenSwitch->isChecked(), authProfile,
        [safeThis](bool success, const QString& message, const QJsonObject&) {
            if (!safeThis)
                return;
            if (!success) {
                MessageError(message.isEmpty() ? QStringLiteral("Update failed") : message);
                return;
            }
            MessageSuccess(QStringLiteral("Payload updated"));
            safeThis->accept();
        });
}
