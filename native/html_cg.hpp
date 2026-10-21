#pragma once
#include "gpu.hpp"
#include <QObject>
#include <QWebEngineView>
#include <QTimer>
#include <QChronoTimer>
#include <QElapsedTimer>
namespace sw {
// A separate, non-screen widget keeps CG rendering independent of the selected
// dashboard tab. No desktop/window capture or physical video endpoint is used.
class HtmlCg:public QObject {
public:
    explicit HtmlCg(QObject* parent=nullptr);
    ~HtmlCg() override;
    void start(const QUrl&);
    void startTestHtml(const QString&);
    void stop();
    RgbaFramePtr frame() const{return frame_;}
    QString status() const{return status_;}
    uint64_t frames() const{return frames_;}
    double fps() const{return fps_;}
    double captureMs() const{return captureMs_;}
    bool active() const{return active_;}
    QWebEnginePage* page() const{return view_?view_->page():nullptr;}
private:
    void createView();
    void capture();
    void fail(const QString&);
    std::unique_ptr<QWebEngineView> view_;
    QChronoTimer timer_;
    RgbaFramePtr frame_;
    QString status_="내부 CG 대기";
    bool active_=false,loaded_=false,inFlight_=false,test_=false;
    uint64_t frames_=0,generation_=0,rateFrames_=0;
    QElapsedTimer rateClock_;
    double fps_=0,captureMs_=0;
};
}
