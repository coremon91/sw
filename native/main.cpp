#include "window.hpp"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
int main(int argc,char** argv) {
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("SW");QCoreApplication::setApplicationName("Switcher");
    const auto fonts=QString::fromLocal8Bit(qgetenv("WINDIR"))+"/Fonts/";
    for(const auto* name:{"segoeui.ttf","segoeuib.ttf","malgun.ttf"}) QFontDatabase::addApplicationFont(fonts+name);
    const auto args=app.arguments();const int receiver=args.contains("--receive-aja")?2:args.contains("--receive")?1:0;
    sw::Window window(receiver);window.show();
    if(args.contains("--cg"))QTimer::singleShot(0,&window,[&]{window.showCg();});
    const int cgTest=args.indexOf("--cg-smoke-test");if(cgTest>=0&&cgTest+1<args.size()) {
        const auto path=args[cgTest+1];window.showCg();
        QTimer::singleShot(12000,&window,[&app,&window,path]{const bool ready=window.cgReady();const bool saved=window.savePreview(path);app.exit(ready&&saved?0:1);});return app.exec();
    }
    if(args.contains("--portable-player"))QTimer::singleShot(0,&window,[&]{window.showPortablePlayer();});
    const int diagnostics=args.indexOf("--diagnostics");if(diagnostics>=0&&diagnostics+1<args.size())window.setDiagnosticsPath(args[diagnostics+1]);
    if(args.contains("--sdi")) {
        int program=1;const int p=args.indexOf("--pgm");if(p>=0&&p+1<args.size())program=args[p+1].toInt();if(program<1||program>4)return 2;
        QStringList ports;const int r=args.indexOf("--ports");if(r>=0){if(r+1>=args.size())return 2;ports=args[r+1].split(',');if(ports.size()!=6)return 2;}
        window.startSdi(program-1,args.contains("--unmute"),args.contains("--uhd"),ports);
    }
    const int routingTest=args.indexOf("--self-test-routing");if(routingTest>=0) {
        const auto path=routingTest+1<args.size()?args[routingTest+1]:QString();
        QTimer::singleShot(0,&window,[&app,&window,path]{app.exit(window.verifyRoutingModes(path)?0:1);});return app.exec();
    }
    if(args.contains("--self-test-layout")) {QTimer::singleShot(0,&window,[&]{app.exit(window.verifyStableLayout()?0:1);});return app.exec();}
    const int media=args.indexOf("--media");
    if(media>=0&&media+1<args.size())window.startMediaDemo(args[media+1],args.contains("--uhd"),args.contains("--play-media"));
    else if(!receiver&&(args.contains("--demo")||args.contains("--screenshot")))window.startDemo();
    int index=args.indexOf("--screenshot");if(index>=0&&index+1<args.size()) {
        const auto path=args[index+1];auto* capture=new QTimer(&window);capture->setInterval(250);
        QObject::connect(capture,&QTimer::timeout,&window,[&app,&window,path,capture,attempts=0]() mutable {
            if(window.previewReady()){capture->stop();app.exit(window.savePreview(path)?0:1);}
            else if(++attempts>=120){capture->stop();window.savePreview(path);app.exit(1);}
        });capture->start();
    }
    return app.exec();
}
