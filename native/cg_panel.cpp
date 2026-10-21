#include "cg_panel.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSettings>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QWebEngineSettings>
#include <QTcpSocket>
#include <QTimer>
#include <windows.h>
#include <memory>
namespace sw {
CgPanel::CgPanel(QWidget* parent):QWidget(parent) {
    auto* layout=new QVBoxLayout(this);
    auto* serverRow=new QHBoxLayout;root_=new QLineEdit(QSettings().value("nodecg/root","Z:/Nodecg").toString());
    auto* choose=new QPushButton("NodeCG 폴더");auto* start=new QPushButton("NodeCG 시작");auto* caspar=new QPushButton("CasparCG 시작 · SDI 출력");
    serverRow->addWidget(choose);serverRow->addWidget(root_,1);serverRow->addWidget(start);serverRow->addWidget(caspar);layout->addLayout(serverRow);
    auto* navigation=new QHBoxLayout;url_=new QLineEdit(QSettings().value("nodecg/url","http://127.0.0.1:9090/bundles/my-cg/dashboard/studio.html?standalone=true").toString());
    auto* open=new QPushButton("연결 / 새로고침");navigation->addWidget(new QLabel("CG 제어 주소"));navigation->addWidget(url_,1);navigation->addWidget(open);layout->addLayout(navigation);
    caspar->setVisible(false);
    auto* hint=new QLabel("통합 CG: 레이어 선택 → 오른쪽 설정 → 가운데 프리뷰 → 선택 TAKE. 최종 영상은 상단 CG PVW에서 확인하고 CG ON으로 합성합니다.\nPVW 체크와 순서 편집은 송출 상태와 별개입니다. CG OFF는 전체 합성을 해제하고, 선택 OUT은 해당 레이어만 퇴장시킵니다.");
    hint->setWordWrap(true);layout->addWidget(hint);
    status_=new QLabel("CG 탭을 열면 NodeCG에 연결합니다.");status_->setWordWrap(true);layout->addWidget(status_);
    auto* renderRow=new QHBoxLayout;renderStatus_=new QLabel("내부 CG · 세션 시작 후 연결");auto* reloadCg=new QPushButton("내부 CG 다시 연결");
    renderRow->addWidget(renderStatus_,1);renderRow->addWidget(reloadCg);layout->addLayout(renderRow);
    connect(reloadCg,&QPushButton::clicked,this,[this]{if(renderer_.active())renderer_.start(compositeUrl());else renderStatus_->setText("상단 내부 CG 선택 후 세션을 시작하세요.");});
    statusTimer_.setInterval(500);connect(&statusTimer_,&QTimer::timeout,this,[this]{if(renderer_.active())renderStatus_->setText(renderer_.status());});statusTimer_.start();
    connect(choose,&QPushButton::clicked,this,[this]{const auto dir=QFileDialog::getExistingDirectory(this,"NodeCG 폴더",root_->text());if(!dir.isEmpty()){root_->setText(dir);QSettings().setValue("nodecg/root",dir);}});
    connect(open,&QPushButton::clicked,this,[this]{load();});connect(url_,&QLineEdit::returnPressed,this,[this]{load();});
    connect(start,&QPushButton::clicked,this,[this]{launchServer(false);});connect(caspar,&QPushButton::clicked,this,[this]{launchServer(true);});
}
void CgPanel::activate(){if(!browser_)load();}
QUrl CgPanel::compositeUrl() const {
    auto url=QUrl::fromUserInput(url_->text());url.setPath("/bundles/my-cg/graphics/sw-composite.html");url.setQuery(QString());url.setFragment(QString());return url;
}
void CgPanel::setInternalActive(bool active) {
    if(active&&!renderer_.active())renderer_.start(compositeUrl());
    else if(!active&&renderer_.active()){renderer_.stop();renderStatus_->setText("내부 CG · 세션 시작 후 연결");}
}
void CgPanel::load() {
    auto url=QUrl::fromUserInput(url_->text());
    if((url.path()=="/dashboard/"||url.path()=="/dashboard")&&url.fragment().isEmpty()) {
        url.setPath("/bundles/my-cg/dashboard/studio.html");url.setQuery("standalone=true");url_->setText(url.toString());
    }
    if((url.scheme()!="http"&&url.scheme()!="https")||url.host().isEmpty()){status_->setText("http 또는 https 주소를 입력하세요.");return;}
    if(!browser_) {
        browser_=new QWebEngineView(this);static_cast<QVBoxLayout*>(layout())->addWidget(browser_,1);
        browser_->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,true);
        connect(browser_,&QWebEngineView::loadStarted,this,[this]{ready_=false;status_->setText("NodeCG 대시보드 연결 중…");});
        connect(browser_,&QWebEngineView::loadFinished,this,[this](bool ok){ready_=ok;status_->setText(ok?"대시보드 로드 완료 · TAKE/OUT으로 그래픽을 제어하세요.":"대시보드 연결 실패 · NodeCG 서버와 주소를 확인한 뒤 새로고침하세요.");});
        connect(browser_->page(),&QWebEnginePage::renderProcessTerminated,this,[this]{ready_=false;status_->setText("CG 제어 브라우저가 종료되었습니다. 새로고침으로 다시 연결하세요.");});
    }
    QSettings().setValue("nodecg/url",url.toString());browser_->load(url);
    if(renderer_.active())renderer_.start(compositeUrl());
}
void CgPanel::launchServer(bool caspar) {
    const QString root=QDir::cleanPath(root_->text());
    const QString program=caspar?root+"/casparcg-server/casparcg.exe":root+"/runtime/node/node.exe";
    const QString entry=root+"/node_modules/nodecg/index.js";
    if(!QFileInfo::exists(program)||(!caspar&&!QFileInfo::exists(entry))) {status_->setText("실행 파일이 없습니다. NodeCG 폴더와 설치 상태를 확인하세요.");return;}
    // Probe asynchronously so a running local service is not started twice.
    auto* probe=new QTcpSocket(this);auto* timeout=new QTimer(probe);timeout->setSingleShot(true);
    auto finished=std::make_shared<bool>(false);
    auto launch=[this,probe,finished,program,entry,root,caspar] {
        if(*finished)return;*finished=true;probe->deleteLater();
        QProcess process;process.setProgram(program);if(!caspar)process.setArguments({entry});
        process.setWorkingDirectory(caspar?root+"/casparcg-server":root);
        auto env=QProcessEnvironment::systemEnvironment();env.insert("PATH",root+"/runtime/node;"+env.value("PATH"));process.setProcessEnvironment(env);
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* a){a->flags|=CREATE_NO_WINDOW;});
        QDir().mkpath(root+"/logs");const auto log=root+"/logs/sw-"+(caspar?"casparcg":"nodecg");
        process.setStandardOutputFile(log+".stdout.log",QIODevice::Append);process.setStandardErrorFile(log+".stderr.log",QIODevice::Append);
        qint64 pid=0;
        if(process.startDetached(&pid)) {
            QSettings().setValue("nodecg/root",root);
            status_->setText(QString("%1 시작 요청 · PID %2 · SW 종료 후에도 서버는 유지됩니다.").arg(caspar?"CasparCG":"NodeCG").arg(pid));
            if(!caspar)QTimer::singleShot(4000,this,[this]{load();});
        }else status_->setText("서버 실행 실패: "+process.errorString());
    };
    connect(probe,&QTcpSocket::connected,this,[this,probe,finished,caspar]{if(*finished)return;*finished=true;probe->deleteLater();status_->setText(caspar?"로컬 AMCP 포트 5250이 이미 사용 중입니다.":"로컬 포트 9090이 이미 사용 중입니다. 대시보드에 연결합니다.");if(!caspar)load();});
    connect(probe,&QTcpSocket::errorOccurred,this,[launch](QAbstractSocket::SocketError){launch();});
    connect(timeout,&QTimer::timeout,this,launch);timeout->start(1200);probe->connectToHost("127.0.0.1",caspar?5250:9090);
}
}
