#include "window.hpp"
#include <QPainter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QScrollArea>
#include <QSettings>
#include <QShortcut>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QApplication>
#include <QLineEdit>
#include <QAbstractSpinBox>
#include <iostream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSaveFile>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QStackedWidget>
#include <QCloseEvent>
#include <QTextBrowser>
#include <QFile>
#include <QTextCursor>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>
#include <QUuid>
namespace sw {
Monitor::Monitor(QString title,QWidget* parent):QWidget(parent),title_(std::move(title)) { setMinimumSize(180,125);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding); }
void Monitor::updateImage(const Image& i,QString status,QColor accent,bool newImage) {
    if(!newImage&&status==status_&&accent==accent_)return;
    if(newImage&&!i.rgba.empty()) image_=QImage(i.rgba.data(),i.width,i.height,i.width*4,QImage::Format_RGBA8888).copy();
    status_=std::move(status);accent_=accent;update();
}
void Monitor::paintEvent(QPaintEvent*) {
    QPainter p(this);p.fillRect(rect(),QColor("#090d12"));
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    auto area=QRect(2,31,width()-4,height()-35);
    QSize fit=QSize(16,9);fit.scale(area.size(),Qt::KeepAspectRatio);QRect video(QPoint(0,0),fit);video.moveCenter(area.center());
    if(!image_.isNull()) p.drawImage(video,image_);
    else { p.setPen(QColor("#627084"));p.drawText(video,Qt::AlignCenter,"SW / NO VIDEO"); }
    p.setPen(accent_);p.drawRect(rect().adjusted(1,1,-2,-2));p.fillRect(2,2,width()-4,27,accent_.darker(240));
    p.setFont(QFont("Segoe UI",9,QFont::DemiBold));p.drawText(12,21,title_);p.drawText(QRect(65,3,width()-76,26),Qt::AlignRight|Qt::AlignVCenter,status_);
}
Window::Window(int receiver) {
    setWindowTitle("SW 0.7.3 CUDA / CG / Free Routing — Software Video Switcher");resize(1480,940);
    setStyleSheet(R"(QMainWindow,QWidget{background:#111820;color:#dce3ec;font-family:'Segoe UI';font-size:12px;}QGroupBox{border:1px solid #303e4e;border-radius:6px;margin-top:18px;padding:10px;}QGroupBox::title{subcontrol-origin:margin;left:10px;color:#8fa3b9;}QPushButton{background:#263444;border:1px solid #3d5064;padding:9px 14px;border-radius:4px;font-weight:600;}QPushButton:hover{background:#354960;}QPushButton:disabled{color:#627084;background:#1a242f;}QComboBox,QSpinBox,QDoubleSpinBox{background:#1b2835;border:1px solid #3d5064;border-radius:3px;padding:5px;}QLabel#headline{font-size:23px;font-weight:700;}QCheckBox{spacing:8px;}QScrollArea{border:0;})");
    tabs_=new QTabWidget;setCentralWidget(tabs_);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);tabs_->addTab(scroll,"스위처");
    playerPage_=new QWidget;auto* playerTabLayout=new QVBoxLayout(playerPage_);playerTabLayout->setContentsMargins(12,8,12,8);tabs_->addTab(playerPage_,"플레이어");
    playerMode_=new QComboBox;playerMode_->addItems({"KBC Tech Player · RTX 5060 · 2026-09-07 원본","SW 파일 입력 · INPUT 4 직접 연결"});playerTabLayout->addWidget(playerMode_);
    auto* playerStack=new QStackedWidget;playerTabLayout->addWidget(playerStack,1);portablePlayer_=new PortablePlayer;playerStack->addWidget(portablePlayer_);
    auto* internalPage=new QWidget;auto* playerPageLayout=new QVBoxLayout(internalPage);playerPageLayout->setContentsMargins(8,8,8,8);playerStack->addWidget(internalPage);
    connect(playerMode_,qOverload<int>(&QComboBox::currentIndexChanged),playerStack,&QStackedWidget::setCurrentIndex);
    auto* root=new QWidget;scroll->setWidget(root);auto* layout=new QVBoxLayout(root);layout->setContentsMargins(20,14,20,14);layout->setSpacing(12);
    auto* header=new QHBoxLayout;auto* title=new QLabel("SW   /   VIDEO SWITCHER");title->setObjectName("headline");header->addWidget(title);header->addStretch();
    auto* version=new QLabel("C++ · GPU 합성  |  개발 버전 0.7.3 · D3D11 합성 / CUDA 전송 · 입출력 선택");version->setStyleSheet("color:#8fa3b9;");header->addWidget(version);layout->addLayout(header);
    auto* monitorRateRow=new QHBoxLayout;
    monitorRateRow->addWidget(new QLabel("미리보기 갱신"));
    auto* monitorRate=new QComboBox;monitorRate_=monitorRate;
    monitorRate->addItem("59.94 fps",1);monitorRate->addItem("29.97 fps",2);monitorRate->addItem("14.985 fps",4);
    const int savedInterval=QSettings().value("monitorInterval",1).toInt();
    monitorRate->setCurrentIndex(std::max(0,monitorRate->findData(savedInterval)));
    engine_.setMonitorInterval(monitorRate->currentData().toUInt());
    connect(monitorRate,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,monitorRate]{
        const auto interval=monitorRate->currentData().toUInt();engine_.setMonitorInterval(interval);
    });
    monitorRate->setToolTip("GUI 미리보기의 목표 갱신 속도입니다. SDI 출력 포맷과 프레임 속도는 변경하지 않습니다.");
    monitorRateRow->addWidget(monitorRate);previewFps_=new QLabel;monitorRateRow->addWidget(previewFps_);monitorRateRow->addStretch();
    auto* settingsPage=new QWidget;auto* settingsRoot=new QVBoxLayout(settingsPage);tabs_->addTab(settingsPage,"설정");
    auto* settingsSections=new QTabWidget;settingsRoot->addWidget(settingsSections);
    auto* routingScroll=new QScrollArea;routingScroll->setWidgetResizable(true);
    auto* routingPage=new QWidget;auto* routingLayout=new QVBoxLayout(routingPage);routingScroll->setWidget(routingPage);
    settingsSections->addTab(routingScroll,"입출력 라우팅");
    auto* guidePage=new QWidget;auto* settingsLayout=new QVBoxLayout(guidePage);settingsSections->addTab(guidePage,"사용 가이드");
    settingsLayout->setContentsMargins(20,14,20,14);
    auto* settingsTitle=new QLabel("설정 및 운용 GUIDE");settingsTitle->setObjectName("headline");settingsLayout->addWidget(settingsTitle);
    routingLayout->addLayout(monitorRateRow);
    auto* presetRow=new QHBoxLayout;auto* saveSettingsButton=new QPushButton("설정 저장");loadSettings_=new QPushButton("저장 설정 불러오기");
    presetRow->addWidget(saveSettingsButton);presetRow->addWidget(loadSettings_);presetRow->addStretch();routingLayout->addLayout(presetRow);
    settingsStatus_=new QLabel("설정 저장: 현재 입출력·미리보기·플레이어 설정을 저장하고 다음 실행에 복원합니다.");settingsStatus_->setWordWrap(true);routingLayout->addWidget(settingsStatus_);
    connect(saveSettingsButton,&QPushButton::clicked,this,[this]{saveSettings();});connect(loadSettings_,&QPushButton::clicked,this,[this]{loadSettings();});
    logDirectory_=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)+"/logs";
    auto* logRow=new QHBoxLayout;logEnabled_=new QCheckBox("재생 / 세션 로그 기록 (1초 간격)");logEnabled_->setChecked(true);
    auto* openLogs=new QPushButton("로그 폴더 열기");logRow->addWidget(logEnabled_);logRow->addWidget(openLogs);logRow->addStretch();routingLayout->addLayout(logRow);
    logStatus_=new QLabel("로그 저장 위치: "+QDir::toNativeSeparators(logDirectory_));logStatus_->setWordWrap(true);logStatus_->setTextInteractionFlags(Qt::TextSelectableByMouse);routingLayout->addWidget(logStatus_);
    connect(openLogs,&QPushButton::clicked,this,[this]{if(QDir().mkpath(logDirectory_))QDesktopServices::openUrl(QUrl::fromLocalFile(logDirectory_));});
    connect(logEnabled_,&QCheckBox::toggled,this,[this]{logFailed_=false;nextDiagnostics_={};});
    auto* searchRow=new QHBoxLayout;auto* guideSearch=new QLineEdit;guideSearch->setPlaceholderText("가이드 검색: 입력 부족, 버퍼 초과, CG, CUT…");
    auto* findNext=new QPushButton("다음 찾기");auto* guideTop=new QPushButton("맨 위로");
    searchRow->addWidget(guideSearch,1);searchRow->addWidget(findNext);searchRow->addWidget(guideTop);settingsLayout->addLayout(searchRow);
    auto* guide=new QTextBrowser;guide->setObjectName("settingsGuide");guide->setOpenExternalLinks(false);
    guide->setStyleSheet("QTextBrowser{background:#18232e;border:1px solid #3d5064;padding:16px;font-size:14px;}");
    QFile guideFile(":/guide/docs/17-settings-guide.ko.md");
    if(guideFile.open(QIODevice::ReadOnly))guide->setMarkdown(QString::fromUtf8(guideFile.readAll()));
    else guide->setPlainText("가이드 리소스를 불러올 수 없습니다.");
    settingsLayout->addWidget(guide,1);
    auto* searchStatus=new QLabel;settingsLayout->addWidget(searchStatus);
    auto searchGuide=[guide,guideSearch,searchStatus]{
        const auto query=guideSearch->text().trimmed();if(query.isEmpty()){searchStatus->clear();return;}
        bool found=guide->find(query);if(!found){guide->moveCursor(QTextCursor::Start);found=guide->find(query);}
        searchStatus->setText(found?"검색 결과를 선택했습니다. 다음 찾기로 계속 검색합니다.":"검색 결과가 없습니다.");
    };
    connect(findNext,&QPushButton::clicked,this,searchGuide);connect(guideSearch,&QLineEdit::returnPressed,this,searchGuide);
    connect(guideTop,&QPushButton::clicked,this,[guide]{guide->moveCursor(QTextCursor::Start);});
    auto* top=new QHBoxLayout;
    monitors_[5]=new Monitor("PREVIEW");monitors_[4]=new Monitor("PROGRAM");
    monitors_[5]->setMinimumHeight(235);monitors_[4]->setMinimumHeight(235);top->addWidget(monitors_[5]);top->addWidget(monitors_[4]);layout->addLayout(top,2);
    auto* inputs=new QHBoxLayout;
    for(int i=0;i<4;++i) { auto* column=new QVBoxLayout;monitors_[i]=new Monitor(QString("INPUT %1").arg(i+1));column->addWidget(monitors_[i]);previewButtons_[i]=new QPushButton(QString("%1  ·  PVW 선택").arg(i+1));column->addWidget(previewButtons_[i]);inputs->addLayout(column);connect(previewButtons_[i],&QPushButton::clicked,this,[this,i]{engine_.selectPreview(i);});auto* shortcut=new QShortcut(QKeySequence(QString::number(i+1)),this);connect(shortcut,&QShortcut::activated,this,[this,i]{engine_.selectPreview(i);}); }
    layout->addLayout(inputs,1);
    auto* controls=new QHBoxLayout;
    auto* routingButton=new QPushButton("입출력 설정");header->addWidget(routingButton);
    connect(routingButton,&QPushButton::clicked,this,[this,settingsPage,settingsSections]{settingsSections->setCurrentIndex(0);tabs_->setCurrentWidget(settingsPage);});
    cut_=new QPushButton("CUT  [Space]");cut_->setMinimumHeight(44);cut_->setStyleSheet("background:#77373b;border-color:#bc5860;");
    auto_=new QPushButton("AUTO MIX  [Enter]");auto_->setMinimumHeight(44);
    duration_=new QSpinBox;duration_->setRange(1,600);duration_->setValue(30);duration_->setSuffix(" frames");
    mute_=new QCheckBox("출력 음소거");mute_->setChecked(true);
    controls->addWidget(cut_);controls->addWidget(auto_);controls->addWidget(duration_);controls->addSpacing(20);controls->addWidget(mute_);controls->addStretch();
    controls->addWidget(new QLabel("DVE는 PVW에서 편집 → CUT / AUTO로 PGM에 적용"));layout->addLayout(controls);
    auto* cgLive=new QHBoxLayout;
    cgOn_=new QPushButton("CG ON · PVW 설정 적용");cgOff_=new QPushButton("CG OFF");cgOnAir_=new QLabel("CG OFF AIR");
    cgOn_->setMinimumHeight(44);cgOff_->setMinimumHeight(44);
    cgOn_->setToolTip("PVW의 Fill/Key 설정을 PGM에 즉시 적용합니다. 배경은 전환하지 않습니다.");
    cgOff_->setToolTip("PGM의 CG만 내립니다. 배경과 PVW CG 설정은 유지됩니다.");
    cgLive->addWidget(cgOn_);cgLive->addWidget(cgOff_);cgLive->addWidget(cgOnAir_,1);layout->addLayout(cgLive);
    connect(cgOn_,&QPushButton::clicked,this,[this]{engine_.setProgramCg(true);});
    connect(cgOff_,&QPushButton::clicked,this,[this]{engine_.setProgramCg(false);});
    audioStatus_=new QLabel;audioStatus_->setWordWrap(true);audioStatus_->setFixedHeight(audioStatus_->fontMetrics().lineSpacing()*2+6);audioStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);layout->addWidget(audioStatus_);
    connect(cut_,&QPushButton::clicked,this,[this]{engine_.cut();});connect(auto_,&QPushButton::clicked,this,[this]{engine_.autoMix(duration_->value());});connect(mute_,&QCheckBox::toggled,this,[this](bool m){engine_.setMuted(m);});
    auto* cutKey=new QShortcut(QKeySequence(Qt::Key_Space),this);connect(cutKey,&QShortcut::activated,cut_,&QPushButton::click);
    auto* autoKey=new QShortcut(QKeySequence(Qt::Key_Return),this);connect(autoKey,&QShortcut::activated,auto_,&QPushButton::click);
    playerMonitor_=new Monitor("PLAYER / INPUT 4");playerMonitor_->setMinimumHeight(270);playerPageLayout->addWidget(playerMonitor_,1);
    auto* playerBox=new QGroupBox("PLAYER / INPUT 4");auto* playerLayout=new QVBoxLayout(playerBox);auto* fileRow=new QHBoxLayout;
    mediaEnabled_=new QCheckBox("INPUT 4 · 내장 플레이어");mediaFile_=new QLineEdit;mediaFile_->setReadOnly(true);mediaFile_->setPlaceholderText("세션과 같은 포맷의 영상 파일을 선택하세요");mediaBrowse_=new QPushButton("파일 열기");
    fileRow->addWidget(mediaEnabled_);fileRow->addWidget(mediaFile_,1);fileRow->addWidget(mediaBrowse_);playerLayout->addLayout(fileRow);
    auto* transport=new QHBoxLayout;mediaCue_=new QPushButton("CUE / 처음");mediaPlay_=new QPushButton("재생");mediaPause_=new QPushButton("일시정지");mediaLoop_=new QCheckBox("반복");mediaSeek_=new QSlider(Qt::Horizontal);mediaSeek_->setRange(0,10000);
    transport->addWidget(mediaCue_);transport->addWidget(mediaPlay_);transport->addWidget(mediaPause_);transport->addWidget(mediaLoop_);transport->addWidget(mediaSeek_,1);playerLayout->addLayout(transport);
    mediaStatus_=new QLabel("파일 선택 → 세션 시작 → CUE 확인 → 재생 · INPUT 4를 PVW/PGM으로 선택");mediaStatus_->setFixedHeight(mediaStatus_->fontMetrics().lineSpacing()+6);mediaStatus_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);playerLayout->addWidget(mediaStatus_);playerPageLayout->addWidget(playerBox);
    auto* playerLink=new QPushButton("플레이어 탭 열기");controls->addWidget(playerLink);connect(playerLink,&QPushButton::clicked,this,[this]{tabs_->setCurrentWidget(playerPage_);});
    connect(mediaBrowse_,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,"플레이어 영상 선택",mediaFile_->text(),"영상 (*.mxf *.mov *.mp4 *.mkv *.avi *.ts *.m2ts);;모든 파일 (*)");if(!path.isEmpty()){mediaFile_->setText(path);mediaFile_->setToolTip(path);mediaEnabled_->setChecked(true);}});
    connect(mediaEnabled_,&QCheckBox::toggled,this,[this]{refreshDevices();});
    connect(mediaCue_,&QPushButton::clicked,this,[this]{engine_.player().cue();});connect(mediaPlay_,&QPushButton::clicked,this,[this]{engine_.player().play();});connect(mediaPause_,&QPushButton::clicked,this,[this]{engine_.player().pause();});
    connect(mediaLoop_,&QCheckBox::toggled,this,[this](bool b){engine_.player().setLoop(b);});
    connect(mediaSeek_,&QSlider::sliderReleased,this,[this]{engine_.player().seek(engine_.player().status().duration*mediaSeek_->value()/10000.);});
    auto* lower=new QHBoxLayout;
    auto* sessionBox=new QGroupBox("SESSION / SDI ROUTING");auto* sl=new QVBoxLayout(sessionBox);session_=new QWidget;auto* grid=new QGridLayout(session_);grid->setContentsMargins(0,0,0,0);
    mode_=new QComboBox;mode_->addItems({"HD · 1080i29.97 (59.94 fields)","UHD · 2160p59.94"});sourceMode_=new QComboBox;sourceMode_->addItems({"테스트 패턴 · SDI 사용 안 함","실제 SDI · 입출력 자유 선택","DeckLink 수신 확인 · 출력 없음","KONA 5 수신 확인 · 출력 없음"});
    if(receiver)sourceMode_->setCurrentIndex(receiver==2?3:2);
    grid->addWidget(sourceMode_,0,0,1,2);grid->addWidget(mode_,0,2,1,2);
    for(int i=0;i<6;++i) { ports_[i]=new QComboBox;ports_[i]->setMinimumWidth(180);ports_[i]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);grid->addWidget(new QLabel(i<4?QString("IN %1").arg(i+1):(i==4?"OUT 1 / PGM":"OUT 2 / PVW")),1+i/2,(i%2)*2);grid->addWidget(ports_[i],1+i/2,(i%2)*2+1); }
    refresh_=new QPushButton("장치 다시 검색");grid->addWidget(refresh_,4,0,1,2);routingHint_=new QLabel; routingHint_->setWordWrap(true);grid->addWidget(routingHint_,4,2,1,2);sl->addWidget(session_);
    auto* actions=new QHBoxLayout;start_=new QPushButton("세션 시작");start_->setStyleSheet("background:#216455;border-color:#35947d;");stop_=new QPushButton("정지");actions->addWidget(start_);actions->addWidget(stop_);sl->addLayout(actions);
    hardware_=new QLabel;hardware_->setWordWrap(true);hardware_->setStyleSheet("color:#8fa3b9;font-size:11px;");sl->addWidget(hardware_);routingLayout->insertWidget(0,sessionBox);routingLayout->addStretch();
    auto* dveBox=new QGroupBox("DVE / PICTURE IN PICTURE");dvePanel_=dveBox;auto* dg=new QGridLayout(dveBox);
    dveEnabled_=new QCheckBox("PVW PIP 사용");dveSource_=new QComboBox;dveSource_->addItems({"INPUT 1","INPUT 2","INPUT 3","INPUT 4"});dg->addWidget(dveEnabled_,0,0,1,2);dg->addWidget(dveSource_,0,2,1,2);
    const QStringList labels={"X 위치","Y 위치","가로","세로","왼쪽 크롭","위 크롭","오른쪽 크롭","아래 크롭","회전 °","불투명도","테두리"};
    for(int i=0;i<11;++i) { auto* spin=new QDoubleSpinBox;dveValues_[i]=spin;spin->setDecimals(3);spin->setSingleStep(i==8?5:.01);spin->setRange(i<2?-1:0,i==8?360:(i<4?2:1));if(i==8) spin->setMinimum(-360);dg->addWidget(new QLabel(labels[i]),1+i/2,(i%2)*2);dg->addWidget(spin,1+i/2,(i%2)*2+1);connect(spin,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{applyDve();}); }
    connect(dveEnabled_,&QCheckBox::toggled,this,[this]{applyDve();});connect(dveSource_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{applyDve();});lower->addWidget(dveBox,2);
    auto* keyBox=new QGroupBox("CG KEY / FILL · PVW에서 확인 → CG ON으로 송출 적용");keyFillPanel_=keyBox;
    auto* kg=new QGridLayout(keyBox);
    keyFillEnabled_=new QCheckBox("PVW CG 미리보기");fillSource_=new QComboBox;keySource_=new QComboBox;
    for(auto* combo:{fillSource_,keySource_})combo->addItems({"INPUT 1","INPUT 2","INPUT 3","INPUT 4"});
    fillSource_->setCurrentIndex(2);keySource_->setCurrentIndex(3);
    keyInvert_=new QCheckBox("Key 반전");keyPremultiplied_=new QCheckBox("Premultiplied Fill");
    keyOpacity_=new QDoubleSpinBox;keyOpacity_->setRange(0,1);keyOpacity_->setSingleStep(.05);keyOpacity_->setValue(1);
    kg->addWidget(keyFillEnabled_,0,0);kg->addWidget(new QLabel("Fill"),0,1);kg->addWidget(fillSource_,0,2);
    kg->addWidget(new QLabel("Key"),0,3);kg->addWidget(keySource_,0,4);kg->addWidget(keyInvert_,0,5);
    kg->addWidget(keyPremultiplied_,0,6);kg->addWidget(new QLabel("불투명도"),0,7);kg->addWidget(keyOpacity_,0,8);
    keyFillStatus_=new QLabel("Key: 검정=배경 / 흰색=Fill · Fill/Key는 서로 다른 입력 · 동일 포맷·동기 신호 필요");
    keyFillStatus_->setWordWrap(true);kg->addWidget(keyFillStatus_,1,0,1,9);layout->addWidget(keyBox);layout->addLayout(lower);
    for(auto* check:{keyFillEnabled_,keyInvert_,keyPremultiplied_})connect(check,&QCheckBox::toggled,this,[this]{applyKeyFill();});
    for(auto* combo:{fillSource_,keySource_})connect(combo,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{applyKeyFill();});
    connect(keyOpacity_,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{applyKeyFill();});
    status_=new QLabel("준비 · 세션을 시작하면 영상이 표시됩니다.");status_->setWordWrap(true);layout->addWidget(status_);
    // Counters and errors must never resize the video grid as the text wraps.
    status_->setFixedHeight(status_->fontMetrics().lineSpacing()*3+8);status_->setAlignment(Qt::AlignLeft|Qt::AlignTop);
    status_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    connect(refresh_,&QPushButton::clicked,this,[this]{refreshDevices();});connect(start_,&QPushButton::clicked,this,[this]{startSession();});connect(stop_,&QPushButton::clicked,this,[this]{engine_.stop();update();});
    connect(sourceMode_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this]{refreshDevices();});
    QSettings settings;mediaFile_->setText(settings.value("mediaFile").toString());mediaLoop_->setChecked(settings.value("mediaLoop",false).toBool());mode_->setCurrentIndex(receiver?1:settings.value("mode",0).toInt());refreshDevices();
    if(!receiver&&settings.contains("routingPreset"))loadSettings();
    connect(qApp,&QApplication::focusChanged,this,[this](QWidget*,QWidget* focused) {
        bool editing=false;
        for(auto* p=focused;p;p=p->parentWidget()) if(qobject_cast<QLineEdit*>(p)||qobject_cast<QAbstractSpinBox*>(p)||qobject_cast<QComboBox*>(p)) {editing=true;break;}
        for(auto* shortcut:findChildren<QShortcut*>())shortcut->setEnabled(!editing&&tabs_->currentIndex()==0);
    });
    connect(tabs_,&QTabWidget::currentChanged,this,[this]{for(auto* shortcut:findChildren<QShortcut*>())shortcut->setEnabled(tabs_->currentIndex()==0);});
    connect(&timer_,&QTimer::timeout,this,[this]{update();});timer_.setTimerType(Qt::PreciseTimer);timer_.start(16);update();
}
Window::~Window() { engine_.stop(); }
QJsonObject Window::captureSettings() const {
    QJsonArray ports;for(auto* port:ports_)ports.append(port->currentData().toString());
    return {{"schema",1},{"sourceMode",sourceMode_->currentIndex()},{"mode",mode_->currentIndex()},
        {"ports",ports},{"monitorInterval",monitorRate_->currentData().toInt()},{"duration",duration_->value()},
        {"muted",mute_->isChecked()},{"mediaEnabled",mediaEnabled_->isChecked()},{"mediaFile",mediaFile_->text()},{"mediaLoop",mediaLoop_->isChecked()},{"logging",logEnabled_->isChecked()}};
}
bool Window::applySettings(const QJsonObject& value) {
    const auto state=engine_.snapshot();if(state.running||state.starting)return false;
    auto integer=[&](const char* name,int low,int high){const auto v=value.value(name);return v.isDouble()&&v.toDouble()==v.toInt(-999)&&v.toInt()>=low&&v.toInt()<=high;};
    const auto ports=value.value("ports").toArray();const int interval=value.value("monitorInterval").toInt();
    if(!integer("schema",1,1)||!integer("sourceMode",0,3)||!integer("mode",0,1)||!integer("duration",1,600)
        ||!integer("monitorInterval",1,4)||(interval!=1&&interval!=2&&interval!=4)||ports.size()!=6
        ||!value.value("muted").isBool()||!value.value("mediaEnabled").isBool()||!value.value("mediaLoop").isBool()||!value.value("mediaFile").isString()
        ||(value.contains("logging")&&!value.value("logging").isBool()))return false;
    for(const auto& port:ports)if(!port.isString())return false;
    {QSignalBlocker sourceBlock(sourceMode_),mediaBlock(mediaEnabled_);
     sourceMode_->setCurrentIndex(value.value("sourceMode").toInt());mode_->setCurrentIndex(value.value("mode").toInt());
     mediaEnabled_->setChecked(value.value("mediaEnabled").toBool());mediaFile_->setText(value.value("mediaFile").toString());}
    mediaLoop_->setChecked(value.value("mediaLoop").toBool());duration_->setValue(value.value("duration").toInt());mute_->setChecked(value.value("muted").toBool());
    monitorRate_->setCurrentIndex(monitorRate_->findData(interval));refreshDevices();
    logEnabled_->setChecked(value.value("logging").toBool(true));
    QStringList missing;
    for(int i=0;i<6;++i){const auto id=ports[i].toString();int index=ports_[i]->findData(id);
        if(index<0){ports_[i]->addItem("장치 없음 · "+id,id);index=ports_[i]->count()-1;}
        if(!id.isEmpty()&&std::none_of(devices_.begin(),devices_.end(),[&](const Endpoint& e){return QString::fromStdString(e.id)==id;}))missing<<id;
        ports_[i]->setCurrentIndex(index);
    }
    settingsStatus_->setText(missing.isEmpty()?"저장 설정을 불러왔습니다. 확인 후 세션 시작을 누르세요.":"저장 설정 복원 · 연결되지 않은 장치: "+missing.join(", ")+" · 연결 후 장치 다시 검색을 누르세요.");
    return true;
}
void Window::saveSettings() {
    QSettings settings;settings.setValue("routingPreset",QJsonDocument(captureSettings()).toJson(QJsonDocument::Compact));settings.sync();
    settingsStatus_->setText(settings.status()==QSettings::NoError?"설정 저장 완료 · 다음 실행에 복원됩니다. CG/DVE와 ON AIR 상태는 저장하지 않습니다.":"설정 저장 실패 · 저장소 접근 권한을 확인하세요.");
}
void Window::loadSettings() {
    const auto state=engine_.snapshot();if(state.running||state.starting){settingsStatus_->setText("세션을 정지한 후 설정을 불러올 수 있습니다.");return;}
    QSettings settings;if(!settings.contains("routingPreset")){settingsStatus_->setText("저장된 설정이 없습니다. 먼저 설정 저장을 누르세요.");return;}
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(settings.value("routingPreset").toByteArray(),&error);
    if(error.error!=QJsonParseError::NoError||!doc.isObject()||!applySettings(doc.object()))settingsStatus_->setText("저장 설정 형식이 올바르지 않습니다. 현재 설정은 유지됩니다.");
}
void Window::closeEvent(QCloseEvent* event) {
    engine_.stop();nextControls_={};nextDiagnostics_={};update();portablePlayer_->shutdown();QMainWindow::closeEvent(event);
    // A foreign QWindow can keep Qt's last-window tracking alive after the
    // main widget closes. Explicitly finish the single-main-window application.
    if(event->isAccepted())QCoreApplication::quit();
}
void Window::refreshDevices() {
    const bool receive=sourceMode_->currentIndex()>=2,aja=sourceMode_->currentIndex()==3;
    std::vector<std::string> warnings;devices_=receive?probeReceiveDevices(aja?"aja":"decklink",warnings):probeDevices(warnings);QSettings settings;
    for(int i=0;i<6;++i) {
        const auto old=previousRoutingMode_==sourceMode_->currentIndex()?ports_[i]->currentData().toString():QString();ports_[i]->clear();ports_[i]->addItem(i<4?"사용 안 함":"LOCAL · SDI 출력 안 함","");
        for(const auto& e:devices_) if(!(receive&&i>=4)&&(i<4?e.input:e.output)) ports_[i]->addItem(QString(receive&&e.detail.find("capture=busy")!=std::string::npos?"사용 중 · ":"")+QString::fromStdString(e.label)+ (e.uhd?" [HD/UHD]":" [HD]"),QString::fromStdString(e.id));
        QString saved=previousRoutingMode_==sourceMode_->currentIndex()?old:settings.value(QString(receive?(aja?"ajaReceivePort%1":"receivePort%1"):"port%1").arg(i)).toString();
        if(ports_[i]->findData(saved)<0)ports_[i]->addItem("장치 없음 · "+saved,saved);
        ports_[i]->setCurrentIndex(std::max(0,ports_[i]->findData(saved)));
        ports_[i]->setEnabled((sourceMode_->currentIndex()==1||(receive&&i<4))&&!(i==3&&!receive&&mediaEnabled_->isChecked()));
    }
    if(receive&&std::all_of(ports_.begin(),ports_.begin()+4,[](QComboBox* p){return p->currentIndex()==0;})&&ports_[0]->count()>1)ports_[0]->setCurrentIndex(1);
    routingHint_->setText(receive?(aja?"외부 플레이어 → DeckLink SDI → KONA\n입력 1개부터 가능 · 세션 포맷과 일치":"4K 플레이어 → KONA SDI → DeckLink\n입력 1개부터 가능 · 세션 포맷과 일치"):"입력 1~4개 선택 · PGM/PVW 출력은 선택 사항\n각 채널은 입력 또는 출력 한 곳에만 지정");
    QString text=QString("사용 가능한 채널 %1개. 포트는 중복 지정할 수 없습니다.").arg(devices_.size());for(auto& w:warnings) text+="\n"+QString::fromStdString(w);
    for(const auto& e:devices_) if(e.backend=="aja") { text+="\nAJA: "+QString::fromStdString(e.detail);break; }
    hardware_->setText(text);
    if(receive)hardware_->setText(text+(aja?"\nKONA 출력 플레이어를 종료한 뒤 수신을 시작하세요. SW는 KONA 입력만 엽니다.":"\nKONA는 외부 플레이어가 사용합니다. 스위처는 DeckLink 입력만 엽니다."));
    previousRoutingMode_=sourceMode_->currentIndex();
    if(!receive&&mediaEnabled_->isChecked())routingHint_->setText("INPUT 4 = 내장 플레이어\nSDI 입력 1~3 + PGM / PVW 출력");
}
void Window::startSession() {
    try {
        Configuration c;c.mode=mode_->currentIndex()?Mode::Uhd:Mode::Hd;c.synthetic=sourceMode_->currentIndex()==0;c.receiveOnly=sourceMode_->currentIndex()>=2;c.receiveBackend=sourceMode_->currentIndex()==3?"aja":"decklink";QSettings settings;settings.setValue("mode",mode_->currentIndex());
        if(!c.receiveOnly&&mediaEnabled_->isChecked()) {
            if(!QFileInfo(mediaFile_->text()).isFile())throw std::runtime_error("Select an existing player file first.");
            c.mediaPath=mediaFile_->text().toUtf8().toStdString();c.mediaLoop=mediaLoop_->isChecked();
            settings.setValue("mediaFile",mediaFile_->text());settings.setValue("mediaLoop",c.mediaLoop);
        }
        if(!c.synthetic) for(int i=0;i<(c.receiveOnly?4:6);++i) {
            if(i==3&&!c.mediaPath.empty()){c.inputs[3]=mediaEndpoint();continue;}
            auto id=ports_[i]->currentData().toString();auto e=std::find_if(devices_.begin(),devices_.end(),[&](const Endpoint& p){return p.id==id.toStdString();});
            settings.setValue(QString(c.receiveOnly?(c.receiveBackend=="aja"?"ajaReceivePort%1":"receivePort%1"):"port%1").arg(i),id);
            if(id.isEmpty())continue;
            if(e==devices_.end()) throw std::runtime_error("Selected port is no longer available. Refresh devices and select again.");
            if(i<4)c.inputs[i]=*e;else c.outputs[i-4]=*e;
        }
        engine_.setMuted(mute_->isChecked());engine_.start(c);lastMonitorFrames_=0;nextControls_={};update();
    } catch(const std::exception& e) { QMessageBox::warning(this,"SW · 세션 시작 실패",QString::fromUtf8(e.what())); }
}
void Window::startDemo() { sourceMode_->setCurrentIndex(0);startSession(); }
void Window::showPortablePlayer(){playerMode_->setCurrentIndex(0);tabs_->setCurrentWidget(playerPage_);portablePlayer_->launch();}
void Window::startMediaDemo(const QString& path,bool uhd,bool play) {
    playerMode_->setCurrentIndex(1);
    sourceMode_->setCurrentIndex(0);mode_->setCurrentIndex(uhd?1:0);mediaFile_->setText(path);mediaEnabled_->setChecked(true);mute_->setChecked(false);mediaAutoPlay_=play;
    startSession();engine_.selectPreview(3);engine_.cut();engine_.selectPreview(3);
}
void Window::startSdi(int program,bool unmute,bool uhd,const QStringList& ports) {
    sourceMode_->setCurrentIndex(1);mode_->setCurrentIndex(uhd?1:0);mute_->setChecked(!unmute);
    if(ports.size()==6)for(int i=0;i<6;++i)ports_[i]->setCurrentIndex(std::max(0,ports_[i]->findData(ports[i])));
    startSession();engine_.selectPreview(program);engine_.cut();
}
void Window::applyDve() {
    if(syncing_)return;Dve d;d.enabled=dveEnabled_->isChecked();d.source=dveSource_->currentIndex();
    float* values[]={&d.x,&d.y,&d.width,&d.height,&d.cropLeft,&d.cropTop,&d.cropRight,&d.cropBottom,&d.rotation,&d.opacity,&d.border};
    for(int i=0;i<11;++i)*values[i]=float(dveValues_[i]->value());engine_.setDve(d);
}
void Window::applyKeyFill() {
    if(syncing_)return;
    KeyFill k;k.enabled=keyFillEnabled_->isChecked();k.fillSource=fillSource_->currentIndex();k.keySource=keySource_->currentIndex();
    k.invert=keyInvert_->isChecked();k.premultiplied=keyPremultiplied_->isChecked();k.opacity=float(keyOpacity_->value());
    if(k.enabled&&k.fillSource==k.keySource) {
        QMessageBox::warning(this,"CG Key / Fill","Fill과 Key는 서로 다른 입력을 선택하세요.");
        syncing_=true;keyFillEnabled_->setChecked(false);syncing_=false;k.enabled=false;
    }
    engine_.setKeyFill(k);
}
void Window::update() {
    auto s=engine_.snapshot();const bool busy=s.running||s.starting;session_->setEnabled(!busy);start_->setEnabled(!busy);stop_->setEnabled(busy);cut_->setEnabled(s.running&&!s.state.transitioning);auto_->setEnabled(cut_->isEnabled());dvePanel_->setEnabled(s.running&&!s.state.transitioning);
    keyFillPanel_->setEnabled(s.running&&!s.state.transitioning);
    loadSettings_->setEnabled(!busy);
    const auto& liveCg=s.state.program.keyFill;const auto& stagedCg=s.state.preview.keyFill;
    cgOn_->setEnabled(s.running&&stagedCg.fillSource!=stagedCg.keySource&&s.signal[stagedCg.fillSource]&&s.signal[stagedCg.keySource]);
    cgOff_->setEnabled(s.running&&liveCg.enabled);
    const bool cgSignal=s.signal[liveCg.fillSource]&&s.signal[liveCg.keySource];
    cgOnAir_->setText(!s.running?"CG STOPPED":!liveCg.enabled?"CG OFF AIR":!cgSignal?"CG ON · 입력 신호 없음":QString("CG ON AIR · Fill IN%1 / Key IN%2 · 배경 전환과 독립").arg(liveCg.fillSource+1).arg(liveCg.keySource+1));
    cgOnAir_->setStyleSheet(s.running&&liveCg.enabled?(cgSignal?"color:#ff737d;font-weight:bold;":"color:#ffca65;font-weight:bold;"):"color:#93a5b8;font-weight:bold;");
    mute_->setEnabled(sourceMode_->currentIndex()<2);
    const bool canConfigure=!busy&&sourceMode_->currentIndex()<2;
    mediaEnabled_->setEnabled(canConfigure);mediaBrowse_->setEnabled(canConfigure);
    const bool mediaActive=s.running&&s.media.active&&s.media.error.empty();
    mediaPlay_->setEnabled(mediaActive&&!s.media.playing&&s.media.ready);mediaPause_->setEnabled(mediaActive&&s.media.playing);mediaCue_->setEnabled(mediaActive);mediaSeek_->setEnabled(mediaActive&&s.media.duration>0);
    if(mediaAutoPlay_&&mediaActive&&s.media.ready){engine_.player().play();mediaAutoPlay_=false;}
    for(int i=0;i<6;++i) {
        bool valid=i<4?s.signal[i]:s.running;QString label=!busy?"STOPPED":(!valid?"NO SIGNAL":(s.synthetic?"TEST":(i==4?"SDI PGM":i==5?"SDI PVW":"SDI")));
        if(busy&&s.receiveOnly) {
            if(i>=4)label="LOCAL · 출력 없음";
            else if(!s.assigned[i])label="NOT ASSIGNED";
            else if(s.signal[i])label=QString("%1 fps · %2 frames").arg(s.inputFps[i],0,'f',1).arg(s.io[i].frames);
        }
        if(busy&&!s.synthetic) {
            if(i<4&&!s.assigned[i])label="사용 안 함";
            if(i>=4&&!s.outputAssigned[i-4])label="LOCAL · SDI 출력 없음";
        }
        if(i==3&&s.media.active)label=s.media.error.empty()?(s.media.playing?"PLAYER · PLAY":s.media.eof?"PLAYER · END":"PLAYER · HOLD"):"PLAYER · ERROR";
        QColor color=i==4?QColor("#ed6a73"):i==5?QColor("#4ed2a0"):QColor("#69809b");monitors_[i]->updateImage(s.monitors[i],label,color,s.monitorFrames!=lastMonitorFrames_);
    }
    playerMonitor_->updateImage(s.monitors[3],s.media.active?"INPUT 4":"STOPPED",QColor("#4ed2a0"),s.monitorFrames!=lastMonitorFrames_);
    lastMonitorFrames_=s.monitorFrames;
    const auto now=std::chrono::steady_clock::now();if(now<nextControls_)return;nextControls_=now+std::chrono::milliseconds(100);
    if(s.media.active||!s.media.error.empty()) {
        const auto time=[](double value){const int n=int(std::max(0.,value));return QString("%1:%2:%3").arg(n/3600,2,10,QChar('0')).arg(n/60%60,2,10,QChar('0')).arg(n%60,2,10,QChar('0'));};
        const auto text=s.media.error.empty()?QString("%1 / %2 · %3 · 버퍼 %4 frames · 디코드 %5 ms · 재생 대기 %6").arg(time(s.media.position)).arg(time(s.media.duration)).arg(s.media.playing?"재생":s.media.eof?"끝":s.media.ready?"CUE / 일시정지":"불러오는 중").arg(s.media.buffered).arg(s.media.decodeMs,0,'f',1).arg(s.media.underruns):"플레이어 오류: "+QString::fromStdString(s.media.error);
        mediaStatus_->setText(text);mediaStatus_->setToolTip(text);mediaStatus_->setStyleSheet(s.media.error.empty()?"color:#93a5b8;":"color:#ff929a;");
        if(!mediaSeek_->isSliderDown()&&s.media.duration>0)mediaSeek_->setValue(int(s.media.position/s.media.duration*10000));
    }else mediaStatus_->setText("파일 선택 → 세션 시작 → CUE 확인 → 재생 · INPUT 4를 PVW/PGM으로 선택");
    for(int i=0;i<4;++i) { previewButtons_[i]->setEnabled(s.running&&!s.state.transitioning);previewButtons_[i]->setStyleSheet(i==s.state.preview.background?"background:#216455;border-color:#4ed2a0;":""); }
    syncing_=true;const auto& d=s.state.preview.dve;dveEnabled_->setChecked(d.enabled);dveSource_->setCurrentIndex(d.source);
    const float values[]={d.x,d.y,d.width,d.height,d.cropLeft,d.cropTop,d.cropRight,d.cropBottom,d.rotation,d.opacity,d.border};
    for(int i=0;i<11;++i) if(!dveValues_[i]->hasFocus()) dveValues_[i]->setValue(values[i]);syncing_=false;
    syncing_=true;const auto& k=s.state.preview.keyFill;
    keyFillEnabled_->setChecked(k.enabled);fillSource_->setCurrentIndex(k.fillSource);keySource_->setCurrentIndex(k.keySource);
    keyInvert_->setChecked(k.invert);keyPremultiplied_->setChecked(k.premultiplied);
    if(!keyOpacity_->hasFocus())keyOpacity_->setValue(k.opacity);syncing_=false;
    auto cgStatus=[&](const char* bus,const KeyFill& cg) {
        if(!cg.enabled)return QString::fromUtf8(bus)+" CG OFF";
        return QString("%1 Fill IN%2 / Key IN%3 · %4").arg(bus).arg(cg.fillSource+1).arg(cg.keySource+1)
            .arg(s.signal[cg.fillSource]&&s.signal[cg.keySource]?"합성 ON":"신호 없음 · CG 숨김");
    };
    keyFillStatus_->setText(cgStatus("PGM",s.state.program.keyFill)+"   |   "+cgStatus("PVW",k)+"   · Key 검정=배경 / 흰색=Fill (반전 시 반대)");
    uint64_t dropped=0;for(auto& io:s.io)dropped+=io.dropped;
    uint64_t noSignal=0,recoveries=0;for(const auto& io:s.io){noSignal+=io.noSignal;recoveries+=io.timingRecoveries;}
    auto db=[](double level){return level<=-119.?QString("무음"):QString::number(level,'f',1)+" dBFS";};
    QString audioText="입력 CH 1/2 피크: ";for(int i=0;i<4;++i)audioText+=QString("IN%1 %2  ").arg(i+1).arg(s.signal[i]?db(s.io[i].audioPeak):"신호 없음");
    audioText+=s.receiveOnly?"\n수신 확인 모드 · 물리 오디오 출력 없음":QString("\n%1 · PGM(IN%2) %3 / PVW(IN%4) %5 · 입력 부족 %6 · 버퍼 초과 %7 samples · 출력 시계 복구 %8")
        .arg(s.muted?"출력 음소거":"출력 오디오 켜짐").arg(s.state.program.background+1).arg(db(s.outputAudioPeak[0])).arg(s.state.preview.background+1).arg(db(s.outputAudioPeak[1])).arg(s.audioUnderruns).arg(s.audioOverflowFrames).arg(recoveries);
    audioStatus_->setText(audioText);audioStatus_->setToolTip(audioText);
    QString status=s.starting?"장치와 GPU 초기화 중…":s.running?(s.synthetic?"테스트 패턴 실행 중 · SDI 출력 없음":"SDI 세션 실행 중"):"정지 · 마지막 수신 화면 유지";
    if(s.running&&s.receiveOnly) {
        uint64_t frames=0;for(int i=0;i<4;++i)frames+=s.io[i].frames;
        uint64_t skipped=0;for(auto n:s.monitorSkipped)skipped+=n;
        status=QString("%1 수신 · %2 · 유효 수신 %3 frames · 화면 처리 %4 fps\n입력 드롭/무신호 %5 · 모니터 생략 %6 · 마감 초과(누적) %7 · SDI 출력 없음")
            .arg(sourceMode_->currentIndex()==3?"KONA 5":"DeckLink").arg(QString::fromStdString(Format::of(mode_->currentIndex()?Mode::Uhd:Mode::Hd).label())).arg(frames).arg(s.monitorFps,0,'f',1).arg(dropped+noSignal).arg(skipped).arg(s.overruns);
    }
    previewFps_->setText(s.running?QString("실제 %1 fps").arg(s.monitorFps,0,'f',1):"정지 · 목표 갱신 속도 선택");
    if(!s.receiveOnly)status+=QString("  |  %1 ms  ·  마감 초과(누적) %2  ·  I/O 드롭 %3  ·  오디오 부족 %4").arg(s.renderMs,0,'f',1).arg(s.overruns).arg(dropped).arg(s.audioUnderruns);
    if(!s.adapter.empty())status+="  |  "+QString::fromStdString(s.adapter);
    if(!s.error.empty())status="세션 오류: "+QString::fromStdString(s.error);
    status_->setText(status);status_->setStyleSheet(s.error.empty()?"color:#93a5b8;":"color:#ff929a;");
    status_->setToolTip(status);
    if((!diagnosticsPath_.isEmpty()||sessionLog_.isOpen()||(logEnabled_->isChecked()&&busy))
        &&(now>=nextDiagnostics_||(sessionLog_.isOpen()&&(!busy||!logEnabled_->isChecked())))) {
        nextDiagnostics_=now+std::chrono::seconds(1);QJsonArray ports;
        for(int i=0;i<6;++i){const auto& io=s.io[i];ports.append(QJsonObject{{"slot",i},{"endpoint",ports_[i]->currentData().toString()},{"frames",double(io.frames)},{"dropped",double(io.dropped)},{"noSignal",double(io.noSignal)},{"audioFrames",double(io.audioFrames)},{"audioPeakDb",io.audioPeak},{"bufferedAudioFrames",int(io.bufferedAudioFrames)},{"timingRecoveries",double(io.timingRecoveries)},{"partialAudioWrites",double(io.audioPartialWrites)}});}
        QJsonObject report{{"time",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},{"running",s.running},{"receiveOnly",s.receiveOnly},{"muted",s.muted},{"pgmInput",s.state.program.background+1},{"pvwInput",s.state.preview.background+1},{"overruns",double(s.overruns)},{"audioUnderruns",double(s.audioUnderruns)},{"audioOverflowFrames",double(s.audioOverflowFrames)},{"renderMs",s.renderMs},{"ticks",double(s.ticks)},{"ports",ports},{"error",QString::fromStdString(s.error)}};
        report.insert("version","0.7.3");report.insert("gpuPath",QString::fromStdString(s.adapter));
        report.insert("starting",s.starting);report.insert("mode",mode_->currentIndex()==1?"2160p59.94":"1080i29.97");
        report.insert("sourceMode",sourceMode_->currentIndex());report.insert("monitorInterval",monitorRate_->currentData().toInt());
        QJsonArray inputStatus;for(int i=0;i<4;++i)inputStatus.append(QJsonObject{{"input",i+1},{"signal",s.signal[i]},{"fps",s.inputFps[i]},{"audioBufferedFrames",int(s.audioBuffered[i])}});report.insert("inputs",inputStatus);
        report.insert("pgmCgEnabled",s.state.program.keyFill.enabled);report.insert("pvwCgEnabled",s.state.preview.keyFill.enabled);
        report.insert("monitorFps",s.monitorFps);report.insert("monitorFrames",double(s.monitorFrames));
        report.insert("timerPolicyApplied",s.timerPolicyApplied);
        report.insert("timerPolicyError",double(s.timerPolicyError));
        report.insert("timerResolutionResult",int(s.timerResolutionResult));
        report.insert("sourceMs",s.sourceMs);report.insert("gpuAndTransferMs",s.gpuMs);
        report.insert("player",QJsonObject{{"active",s.media.active},{"playing",s.media.playing},{"position",s.media.position},{"duration",s.media.duration},{"bufferedFrames",double(s.media.buffered)},{"underruns",double(s.media.underruns)},{"decodeMs",s.media.decodeMs},{"hardwareDecode",s.media.hardware},{"videoDecodeMs",s.media.videoMs},{"convertMs",s.media.convertMs},{"audioDecodeMs",s.media.audioMs},{"error",QString::fromStdString(s.media.error)}});
        if(s.media.active){auto port=ports[3].toObject();port.insert("endpoint","media:4");ports[3]=port;report.insert("ports",ports);}
        if(!diagnosticsPath_.isEmpty()){QSaveFile file(diagnosticsPath_);if(file.open(QIODevice::WriteOnly)){file.write(QJsonDocument(report).toJson());file.commit();}}
        recordLog(report);
    }
}
bool Window::savePreview(const QString& path) { update();return grab().save(path); }
void Window::recordLog(QJsonObject report) {
    const bool active=report.value("running").toBool()||report.value("starting").toBool();
    if(logFailed_){if(!active)logFailed_=false;return;}
    const bool enabled=logEnabled_->isChecked();
    bool rollover=active&&enabled&&sessionLog_.isOpen()&&sessionLog_.size()>=20*1024*1024;
    if(rollover)sessionLog_.close();
    bool opened=false;
    if(!sessionLog_.isOpen()&&enabled&&active) {
        lastLogPath_=logDirectory_+"/sw-"+QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss-zzz")+"-"+QUuid::createUuid().toString(QUuid::Id128)+".jsonl";
        sessionLog_.setFileName(lastLogPath_);
        if(!QDir().mkpath(logDirectory_)||!sessionLog_.open(QIODevice::WriteOnly|QIODevice::NewOnly)) {
            logFailed_=true;logStatus_->setText("로그 기록 실패: "+sessionLog_.errorString()+" · "+QDir::toNativeSeparators(logDirectory_));return;
        }
        opened=true;
    }
    if(!sessionLog_.isOpen())return;
    if(!report.contains("time"))report.insert("time",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    report.insert("event",!enabled?"logging_disabled":!active?"session_end":opened?(rollover?"segment_start":"session_start"):"sample");
    const auto line=QJsonDocument(report).toJson(QJsonDocument::Compact)+'\n';
    if(sessionLog_.write(line)!=line.size()||!sessionLog_.flush()) {
        logFailed_=true;logStatus_->setText("로그 쓰기 실패: "+sessionLog_.errorString());sessionLog_.close();return;
    }
    if(!enabled||!active)sessionLog_.close();
    logStatus_->setText((sessionLog_.isOpen()?"기록 중: ":"마지막 로그: ")+QDir::toNativeSeparators(lastLogPath_));
}
bool Window::verifyStableLayout() {
    timer_.stop();const auto original=status_->text();
    const auto originalSettings=captureSettings();QTemporaryDir settingsTestDir;
    bool settingsRoundTrip=false;
    if(settingsTestDir.isValid()) {
        QSettings file(settingsTestDir.filePath("preset.ini"),QSettings::IniFormat);
        file.setValue("routingPreset",QJsonDocument(originalSettings).toJson(QJsonDocument::Compact));file.sync();
        QSettings loaded(settingsTestDir.filePath("preset.ini"),QSettings::IniFormat);
        const auto decoded=QJsonDocument::fromJson(loaded.value("routingPreset").toByteArray()).object();
        auto invalid=decoded;invalid.insert("sourceMode",-1);
        settingsRoundTrip=file.status()==QSettings::NoError&&decoded==originalSettings&&applySettings(decoded)&&captureSettings()==originalSettings
            &&!applySettings(invalid)&&captureSettings()==originalSettings;
    }
    auto* guide=findChild<QTextBrowser*>("settingsGuide");
    const bool guideReady=guide&&guide->toPlainText().contains("입력 부족")&&guide->toPlainText().contains("독립 CG")&&tabs_->count()==3;
    const bool routingMoved=tabs_->widget(2)->isAncestorOf(session_)&&!tabs_->widget(0)->isAncestorOf(session_)
        &&tabs_->widget(2)->isAncestorOf(start_)&&tabs_->widget(2)->isAncestorOf(stop_)
        &&tabs_->widget(2)->isAncestorOf(monitorRate_)&&!tabs_->widget(0)->isAncestorOf(monitorRate_);
    const auto previousDirectory=logDirectory_;const bool previousLogging=logEnabled_->isChecked();
    bool logRoundTrip=false;
    if(settingsTestDir.isValid()) {
        logDirectory_=settingsTestDir.path()+"/logs";logEnabled_->setChecked(true);
        recordLog({{"running",true},{"ticks",1}});recordLog({{"running",true},{"ticks",2},{"overruns",1}});recordLog({{"running",false},{"ticks",3}});
        QFile records(lastLogPath_);if(records.open(QIODevice::ReadOnly)){
            const auto lines=records.readAll().trimmed().split('\n');
            logRoundTrip=lines.size()==3&&QJsonDocument::fromJson(lines[0]).object().value("event")=="session_start"
                &&QJsonDocument::fromJson(lines[1]).object().value("overruns").toInt()==1&&QJsonDocument::fromJson(lines[2]).object().value("event")=="session_end";
        }
        logDirectory_=previousDirectory;logEnabled_->setChecked(previousLogging);
    }
    auto settle=[&]{for(int k=0;k<4;++k)QApplication::processEvents();};
    auto geometry=[&]{std::array<QRect,6> r;for(int i=0;i<6;++i)r[i]=QRect(monitors_[i]->mapTo(this,QPoint()),monitors_[i]->size());return r;};
    bool stable=true,legacyExpanded=false;const int footerHeight=status_->height();
    for(int width:{1100,1480,1800}) {
        resize(width,940);status_->setText("READY");settle();auto before=geometry();
        status_->setText(QString("SDI frame rate, adapter, counters and error details. ").repeated(12));settle();stable&=before==geometry();
        // Reproduce the former variable-height footer in the same layout.
        status_->setMinimumHeight(0);status_->setMaximumHeight(QWIDGETSIZE_MAX);status_->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
        status_->setText("READY");settle();const int shortHeight=status_->height();
        // In the scrollable CG/routing layout the root may grow instead of
        // moving monitors. Check that the negative control actually wraps.
        status_->setText(QString("SDI frame rate, adapter, counters and error details. ").repeated(12));settle();legacyExpanded|=status_->height()>shortHeight;
        status_->setFixedHeight(footerHeight);status_->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
    }
    status_->setText(original);std::cout<<"{\"fixed_footer_stable\":"<<(stable?"true":"false")<<",\"variable_footer_wrap_verified\":"<<(legacyExpanded?"true":"false")<<"}\n";
    std::cout<<"{\"embedded_guide_loaded\":"<<(guideReady?"true":"false")<<"}\n";
    std::cout<<"{\"routing_in_settings\":"<<(routingMoved?"true":"false")<<"}\n";
    std::cout<<"{\"settings_round_trip\":"<<(settingsRoundTrip?"true":"false")<<"}\n";
    std::cout<<"{\"session_log_round_trip\":"<<(logRoundTrip?"true":"false")<<"}\n";
    return stable&&legacyExpanded&&guideReady&&routingMoved&&settingsRoundTrip&&logRoundTrip;
}
}
