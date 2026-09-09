#include "window.hpp"
#include <QApplication>
#include <QDir>
#include <QFontDatabase>
int main(int argc,char** argv) {
    QApplication app(argc,argv);QCoreApplication::setOrganizationName("SW");QCoreApplication::setApplicationName("Switcher");
    const auto fonts=QString::fromLocal8Bit(qgetenv("WINDIR"))+"/Fonts/";
    for(const auto* name:{"segoeui.ttf","segoeuib.ttf","malgun.ttf"}) QFontDatabase::addApplicationFont(fonts+name);
    sw::Window window;window.show();
    const auto args=app.arguments();if(args.contains("--demo")||args.contains("--screenshot"))window.startDemo();
    int index=args.indexOf("--screenshot");if(index>=0&&index+1<args.size()) {
        const auto path=args[index+1];QTimer::singleShot(3500,&window,[&app,&window,path]{app.exit(window.savePreview(path)?0:1);});
    }
    return app.exec();
}
