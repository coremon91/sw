#include "html_cg.hpp"
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QPointer>
#include <QScopeGuard>
#include <cstring>
namespace sw {
HtmlCg::HtmlCg(QObject* parent):QObject(parent) {
    timer_.setTimerType(Qt::PreciseTimer);timer_.setInterval(std::chrono::nanoseconds(1001000000000LL/60000));
    connect(&timer_,&QChronoTimer::timeout,this,[this]{capture();});
}
HtmlCg::~HtmlCg(){stop();}
void HtmlCg::createView() {
    view_=std::make_unique<QWebEngineView>();
    view_->setAttribute(Qt::WA_DontShowOnScreen);
    view_->setAttribute(Qt::WA_TranslucentBackground);
    view_->setAttribute(Qt::WA_ShowWithoutActivating);
    view_->setAttribute(Qt::WA_QuitOnClose,false);
    view_->setStyleSheet("background:transparent;");view_->resize(3840,2160);view_->setZoomFactor(2.0);
    view_->page()->setBackgroundColor(Qt::transparent);
    view_->page()->setAudioMuted(true);
    view_->settings()->setAttribute(QWebEngineSettings::PlaybackRequiresUserGesture,true);
    connect(view_.get(),&QWebEngineView::loadFinished,this,[this](bool ok){loaded_=ok;if(!ok)fail("내부 CG 페이지 연결 실패 · 다시 연결하세요.");});
    connect(view_->page(),&QWebEnginePage::renderProcessTerminated,this,[this]{loaded_=false;fail("내부 CG 렌더러 종료 · 다시 연결하세요.");});
    view_->show();
    const auto dpr=view_->devicePixelRatioF();view_->resize(qRound(3840/dpr),qRound(2160/dpr));view_->setZoomFactor(2.0/dpr);
    view_->page()->setVisible(true);
}
void HtmlCg::start(const QUrl& url) {
    stop();active_=true;test_=false;createView();status_="내부 CG 연결 중…";
    view_->load(url);rateClock_.start();timer_.start();
}
void HtmlCg::startTestHtml(const QString& html) {
    stop();active_=true;test_=true;createView();view_->setHtml(html);rateClock_.start();timer_.start();
}
void HtmlCg::stop(){++generation_;timer_.stop();active_=loaded_=inFlight_=false;frame_.reset();view_.reset();frames_=rateFrames_=0;fps_=captureMs_=0;status_="내부 CG 대기";}
void HtmlCg::fail(const QString& message){frame_.reset();status_=message;}
void HtmlCg::capture() {
    if(!active_||!loaded_||!view_||inFlight_)return;
    inFlight_=true;const auto generation=generation_;QPointer<HtmlCg> self(this);
    const auto query=test_?"true":"Boolean(window.swCgReady && window.swCgReady())";
    view_->page()->runJavaScript(query,[self,generation](const QVariant& result){
        if(!self||generation!=self->generation_)return;
        const auto release=qScopeGuard([self,generation]{if(self&&generation==self->generation_)self->inFlight_=false;});
        if(!result.toBool()){self->fail("NodeCG 연결 또는 그래픽 로드 대기 · CG 합성 제외");return;}
        QElapsedTimer elapsed;elapsed.start();
        auto image=self->view_->grab().toImage();
        if(!self||generation!=self->generation_)return;
        if(image.size()!=QSize(3840,2160))image=image.scaled(3840,2160,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
        image=image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if(image.isNull()){self->fail("내부 CG 프레임 생성 실패");return;}
        auto f=std::make_shared<RgbaFrame>();f->width=image.width();f->height=image.height();f->rgba.resize(size_t(f->width)*f->height*4);
        for(int y=0;y<f->height;++y)std::memcpy(f->rgba.data()+size_t(y)*f->width*4,image.constScanLine(y),size_t(f->width)*4);
        f->captured=std::chrono::steady_clock::now();f->sequence=++self->frames_;self->frame_=std::move(f);
        self->captureMs_=double(elapsed.nsecsElapsed())/1000000.;
        if(self->rateClock_.elapsed()>=1000){self->fps_=double(self->frames_-self->rateFrames_)*1000./self->rateClock_.elapsed();self->rateFrames_=self->frames_;self->rateClock_.restart();}
        self->status_=QString("내부 CG · UHD 3840×2160 · 설정 59.94 fps · 실측 %1 fps").arg(self->fps_,0,'f',1);
    });
}
}
