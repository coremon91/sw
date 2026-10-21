#include "html_cg.hpp"
#include <QApplication>
#include <iostream>
#include <set>
#include <QScreen>
int main(int argc,char** argv) {
    QApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    std::cout<<"screen_dpr="<<app.primaryScreen()->devicePixelRatio()<<std::endl;sw::HtmlCg cg;std::set<int> positions;bool transparent=false,half=false;uint64_t seen=0;QElapsedTimer duration;duration.start();
    cg.startTestHtml(R"(<html><style>html,body{margin:0;background:transparent;overflow:hidden}#box{position:absolute;top:100px;width:100px;height:100px;background:rgba(255,0,0,.5)}</style><div id='box'></div><script>function step(t){document.getElementById('box').style.left=(100+Math.floor(t/20)%400)+'px';requestAnimationFrame(step)}requestAnimationFrame(step)</script></html>)");
    QTimer poll;poll.setInterval(100);QObject::connect(&poll,&QTimer::timeout,[&]{
        auto f=cg.frame();if(!f||f->sequence==seen)return;seen=f->sequence;
        if(f->width!=3840||f->height!=2160){std::cerr<<"Expected native UHD CG frame";app.exit(1);return;}
        transparent|=f->rgba[3]==0;
        for(int x=0;x<f->width;++x){const size_t p=(size_t(300)*f->width+x)*4;if(f->rgba[p+3]>0){positions.insert(x);half|=std::abs(int(f->rgba[p+3])-128)<=1&&std::abs(int(f->rgba[p])-128)<=1;break;}}
        if(duration.elapsed()>=4000&&seen>=20&&positions.size()>=5&&transparent&&half){std::cout<<"{\"offscreen_alpha\":true,\"animation_positions\":"<<positions.size()<<",\"frames\":"<<seen<<",\"capture_ms\":"<<cg.captureMs()<<"}\n";std::cout<<"UHD 3840x2160 target=60000/1001 measured_fps="<<cg.fps()<<std::endl;app.exit(0);}
    });poll.start();QTimer::singleShot(15000,[&]{std::cerr<<"HTML CG failure: frames="<<seen<<" positions="<<positions.size()<<" alpha="<<transparent<<" premult="<<half<<" status="<<cg.status().toStdString()<<'\n';app.exit(1);});
    return app.exec();
}
