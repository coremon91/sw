#include "portable_player.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QSettings>
#include <QCryptographicHash>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
namespace sw {
namespace {
constexpr auto expectedHash="1a19decc5c580515e847baff6c7057f598eb350d324fff944f0434658e0d7d03";
struct Search {DWORD process;HWND window=nullptr;};
BOOL CALLBACK findPlayer(HWND hwnd,LPARAM value) {
    auto& search=*reinterpret_cast<Search*>(value);DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    if(pid!=search.process)return TRUE;
    wchar_t name[128]{};GetClassNameW(hwnd,name,128);
    if(wcscmp(name,L"DeckLinkMxfCustomPaintWindow")!=0)return TRUE;
    search.window=hwnd;return FALSE;
}
}
PortablePlayer::PortablePlayer(QWidget* parent):QWidget(parent) {
    auto* layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);
    auto* row=new QHBoxLayout;row->addWidget(new QLabel("KBC Tech Player · GeForce RTX 5060 · 2026-09-07"));row->addStretch();
    launch_=new QPushButton("플레이어 실행");close_=new QPushButton("플레이어 종료");choose_=new QPushButton("설치 폴더 선택");
    row->addWidget(launch_);row->addWidget(close_);row->addWidget(choose_);layout->addLayout(row);
    path_=new QLabel;path_->setWordWrap(true);layout->addWidget(path_);
    status_=new QLabel("원본 플레이어의 DeckLink 출력을 사용합니다. SW 입력은 SDI로 연결하세요.");layout->addWidget(status_);
    auto* scroll=new QScrollArea;scroll->setWidgetResizable(true);area_=new QWidget;area_->setMinimumSize(1280,720);auto* hostLayout=new QVBoxLayout(area_);hostLayout->setContentsMargins(0,0,0,0);scroll->setWidget(area_);layout->addWidget(scroll,1);
    const auto bundled=QCoreApplication::applicationDirPath()+"/player/BlackmagicDeckLinkUhdMxfPlayer.exe";
    QSettings settings;executable_=QFileInfo::exists(bundled)?bundled:settings.value("portablePlayerExe").toString();path_->setText(QDir::toNativeSeparators(executable_));close_->setEnabled(false);
    connect(launch_,&QPushButton::clicked,this,[this]{launch();});connect(close_,&QPushButton::clicked,this,[this]{closePlayer();});
    connect(choose_,&QPushButton::clicked,this,[this]{auto folder=QFileDialog::getExistingDirectory(this,"KBC Tech Player 2026-09-07 폴더",QFileInfo(executable_).absolutePath());if(folder.isEmpty())return;executable_=folder+"/BlackmagicDeckLinkUhdMxfPlayer.exe";path_->setText(QDir::toNativeSeparators(executable_));QSettings().setValue("portablePlayerExe",executable_);});
    connect(&timer_,&QTimer::timeout,this,[this]{poll();});timer_.setInterval(100);
}
PortablePlayer::~PortablePlayer() {shutdown();}
void PortablePlayer::shutdown() {
    timer_.stop();auto ownedWindow=child_;
    if(!ownedWindow&&process_){Search search{processId_};EnumWindows(findPlayer,reinterpret_cast<LPARAM>(&search));ownedWindow=search.window;}
    detach();
    if(ownedWindow&&IsWindow(ownedWindow))PostMessageW(ownedWindow,WM_CLOSE,0,0);
    if(process_){CloseHandle(process_);process_=nullptr;}
}
void PortablePlayer::launch() {
    if(process_)return;
    QFile executable(executable_);
    if(!executable.open(QIODevice::ReadOnly)){status_->setText("실행 파일을 찾을 수 없습니다. 지정 버전의 전체 폴더를 선택하세요.");return;}
    QCryptographicHash hash(QCryptographicHash::Sha256);hash.addData(&executable);executable.close();
    if(hash.result().toHex()!=expectedHash){status_->setText("지정한 2026-09-07 버전과 실행 파일이 다릅니다. 원본 패키지를 선택하세요.");return;}
    // Keep startup visible if the host closes before the player creates its
    // main window; never leave an invisible standalone player owning a card.
    STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_SHOWNOACTIVATE;
    PROCESS_INFORMATION info{};const auto exe=QDir::toNativeSeparators(executable_).toStdWString();auto command=L"\""+exe+L"\"";const auto directory=QFileInfo(executable_).absolutePath().toStdWString();
    if(!CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,directory.c_str(),&startup,&info)){status_->setText(QString("플레이어 실행 실패 · Windows 오류 %1").arg(GetLastError()));return;}
    CloseHandle(info.hThread);process_=info.hProcess;processId_=info.dwProcessId;polls_=0;launch_->setEnabled(false);choose_->setEnabled(false);close_->setEnabled(true);status_->setText("원본 플레이어 시작 중…");timer_.start();
}
void PortablePlayer::poll() {
    if(!process_)return;
    if(WaitForSingleObject(process_,0)==WAIT_OBJECT_0) {
        DWORD exitCode=0;GetExitCodeProcess(process_,&exitCode);detach();CloseHandle(process_);process_=nullptr;timer_.stop();launch_->setEnabled(true);choose_->setEnabled(true);close_->setEnabled(false);status_->setText(QString("플레이어 종료 · 코드 %1").arg(exitCode));return;
    }
    if(child_)return;
    Search search{processId_};EnumWindows(findPlayer,reinterpret_cast<LPARAM>(&search));
    if(!search.window) {
        if(++polls_==300)status_->setText("플레이어 창을 기다리고 있습니다. 초기화 오류 대화상자가 있는지 확인하세요.");
        return;
    }
    child_=search.window;originalStyle_=GetWindowLongPtrW(child_,GWL_STYLE);GetWindowRect(child_,&originalRect_);
    SetWindowLongPtrW(child_,GWL_STYLE,(originalStyle_&~(WS_OVERLAPPEDWINDOW|WS_POPUP))|WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS);
    foreign_=QWindow::fromWinId(WId(child_));
    if(!foreign_){SetWindowLongPtrW(child_,GWL_STYLE,originalStyle_);ShowWindow(child_,SW_SHOWNORMAL);status_->setText("탭 연결 실패 · 원본 플레이어를 별도 창으로 열었습니다.");return;}
    container_=QWidget::createWindowContainer(foreign_,area_);container_->setFocusPolicy(Qt::StrongFocus);area_->layout()->addWidget(container_);container_->show();
    SetWindowPos(child_,nullptr,0,0,container_->width(),container_->height(),SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED|SWP_SHOWWINDOW);
    status_->setText("원본 플레이어 연결됨 · 탭을 바꿔도 재생 유지 · DeckLink 출력 포트는 SW와 중복 지정하지 마세요.");
}
void PortablePlayer::detach() {
    if(foreign_)foreign_->setParent(nullptr);
    if(child_&&IsWindow(child_)) {
        SetParent(child_,nullptr);SetWindowLongPtrW(child_,GWL_STYLE,originalStyle_);
        SetWindowPos(child_,nullptr,originalRect_.left,originalRect_.top,originalRect_.right-originalRect_.left,originalRect_.bottom-originalRect_.top,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        ShowWindow(child_,SW_SHOWNOACTIVATE);
    }
    delete container_;container_=nullptr;foreign_=nullptr;child_=nullptr;
}
void PortablePlayer::closePlayer() {
    if(child_&&IsWindow(child_)){PostMessageW(child_,WM_CLOSE,0,0);status_->setText("플레이어 종료 중…");}
    else status_->setText("시작 중인 플레이어의 창을 기다리고 있습니다.");
}
}
