#pragma once
#include <QWidget>
#include <QWebEngineView>
#include <QLineEdit>
#include <QLabel>
#include "html_cg.hpp"
namespace sw {
class CgPanel:public QWidget {
public:
    explicit CgPanel(QWidget* parent=nullptr);
    void activate();
    bool ready() const {return ready_;}
    void setInternalActive(bool);
    HtmlCg& renderer(){return renderer_;}
private:
    void load();
    void launchServer(bool caspar);
    QWebEngineView* browser_=nullptr;
    QLineEdit *url_,*root_;
    QLabel* status_;
    bool ready_=false;
    HtmlCg renderer_{this};
    QLabel* renderStatus_;
    QTimer statusTimer_;
    QUrl compositeUrl() const;
};
}
