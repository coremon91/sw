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
    const int diagnostics=args.indexOf("--diagnostics");if(diagnostics>=0&&diagnostics+1<args.size())window.setDiagnosticsPath(args[diagnostics+1]);
    if(args.contains("--sdi")) {
        int program=1;const int p=args.indexOf("--pgm");if(p>=0&&p+1<args.size())program=args[p+1].toInt();if(program<1||program>4)return 2;
        QStringList ports;const int r=args.indexOf("--ports");if(r>=0){if(r+1>=args.size())return 2;ports=args[r+1].split(',');if(ports.size()!=6)return 2;}
        window.startSdi(program-1,args.contains("--unmute"),args.contains("--uhd"),ports);
    }
    if(args.contains("--self-test-layout")) {QTimer::singleShot(0,&window,[&]{app.exit(window.verifyStableLayout()?0:1);});return app.exec();}
    const int media=args.indexOf("--media");
    if(media>=0&&media+1<args.size())window.startMediaDemo(args[media+1],args.contains("--uhd"),args.contains("--play-media"));
    else if(!receiver&&(args.contains("--demo")||args.contains("--screenshot")))window.startDemo();
    int index=args.indexOf("--screenshot");if(index>=0&&index+1<args.size()) {
        const auto path=args[index+1];QTimer::singleShot(3500,&window,[&app,&window,path]{app.exit(window.savePreview(path)?0:1);});
    }
    return app.exec();
}
