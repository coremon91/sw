#include "html_cg.hpp"
#include "engine.hpp"
#include <QApplication>
#include <QImage>
#include <QJsonDocument>
#include <QJsonArray>
#include <iostream>
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    if(argc<3)return 2;
    const QUrl url(QString::fromLocal8Bit(argv[1]));
    if(url.port()!=9091)return 2; // Never mutate the operator's default server.
    sw::HtmlCg cg;cg.start(url);sw::Engine engine;sw::Configuration config;config.synthetic=false; // no hardware, no test-pattern inputs
    for(int i=3;i<argc;++i) {if(QString::fromLocal8Bit(argv[i])=="--uhd")config.mode=sw::Mode::Uhd;else {config.mediaPath=argv[i];config.mediaLoop=true;}}
    const bool hasMedia=!config.mediaPath.empty();
    engine.start(config);engine.selectPreview(hasMedia?3:0);engine.cut();engine.selectPreview(hasMedia?3:0);
    sw::DownstreamKey dsk;dsk.internal=true;dsk.preview=true;engine.setDownstreamKey(dsk);
    int stage=0;uint64_t checkpoint=0;QString failure;QTimer poll;poll.setInterval(100);
    auto send=[&](const QString& method,const QString& data="null"){
        const auto message=QString::fromUtf8(QJsonDocument(QJsonArray{method}).toJson(QJsonDocument::Compact));
        cg.page()->runJavaScript("nodecg.sendMessage("+message+"[0],"+data+").catch(e=>{window.swFlowError=String(e)})");
    };
    QObject::connect(&poll,&QTimer::timeout,[&]{
        auto f=cg.frame();engine.setCgFrame(f);const auto s=engine.snapshot();
        if(hasMedia&&s.media.ready&&!s.media.playing)engine.player().play();
        if(!s.error.empty()){std::cerr<<s.error<<'\n';app.exit(1);return;}
        if(!s.running||s.monitors[4].rgba.empty())return;
        const bool different=s.monitors[4].rgba!=s.monitors[5].rgba;
        if(stage==0&&f) {
            // Bundle defaults are invisible. The production CG data is untouched.
            send("lowerThird:update",R"({"name":"SW 내부 CG 합성","title":"NodeCG → RGBA → GPU · 카드 없이 재생"})");
            send("lowerThird:show");stage=1;
        }else if(stage==1&&f&&different&&s.cgFresh) {
            QImage(f->rgba.data(),f->width,f->height,f->width*4,QImage::Format_RGBA8888_Premultiplied).save(QString::fromLocal8Bit(argv[2])+"-alpha.png");
            const auto& monitor=s.monitors[5];QImage(monitor.rgba.data(),monitor.width,monitor.height,monitor.width*4,QImage::Format_RGBA8888).save(QString::fromLocal8Bit(argv[2])+"-preview.png");
            dsk.program=true;dsk.preview=false;engine.setDownstreamKey(dsk);checkpoint=s.monitorFrames;stage=2;
        }else if(stage==2&&s.monitorFrames>checkpoint+3&&different) {
            const auto& monitor=s.monitors[4];QImage(monitor.rgba.data(),monitor.width,monitor.height,monitor.width*4,QImage::Format_RGBA8888).save(QString::fromLocal8Bit(argv[2])+"-program.png");
            send("lowerThird:hide");checkpoint=s.monitorFrames;stage=3;
        }else if(stage==3&&s.monitorFrames>checkpoint+15&&!different) {
            send("lowerThird:show");checkpoint=s.monitorFrames;stage=4;
        }else if(stage==4&&s.monitorFrames>checkpoint+3&&different) {
            cg.page()->runJavaScript("document.querySelectorAll('iframe').forEach(f=>f.contentWindow.nodecg.socket.disconnect())");checkpoint=s.monitorFrames;stage=5;
        }else if(stage==5&&s.monitorFrames>checkpoint+3&&!different&&!s.cgFresh) {
            if(hasMedia&&(!s.media.playing||s.media.frames==0)){std::cerr<<"Media did not play\n";app.exit(1);return;}
            std::cout<<"{\"nodecg_take_out\":true,\"independent_pvw_pgm\":true,\"disconnect_bypasses_cg\":true,\"physical_io_opened\":0,\"session\":\""<<sw::Format::of(config.mode).label()<<"\",\"capture_ms\":"<<cg.captureMs()<<"}\n";app.exit(0);
        }
    });poll.start();QTimer::singleShot(40000,[&]{std::cerr<<"NodeCG flow timed out at stage "<<stage<<"; "<<cg.status().toStdString()<<'\n';app.exit(1);});
    const int result=app.exec();engine.stop();return result;
}
