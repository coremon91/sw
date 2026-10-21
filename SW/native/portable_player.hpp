#pragma once
#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QPushButton>
#include <QWindow>
#include <windows.h>
namespace sw {
class PortablePlayer:public QWidget {
public:
    explicit PortablePlayer(QWidget* parent=nullptr);
    ~PortablePlayer() override;
    void launch();
    void shutdown();
    bool embedded() const{return container_!=nullptr;}
private:
    void poll();
    void detach();
    void closePlayer();
    QString executable_;
    QPushButton *launch_,*close_,*choose_;
    QLabel *status_,*path_;
    QWidget* area_;
    QWidget* container_=nullptr;
    QWindow* foreign_=nullptr;
    QTimer timer_;
    HANDLE process_=nullptr;
    DWORD processId_=0;
    HWND child_=nullptr;
    LONG_PTR originalStyle_=0;
    RECT originalRect_{};
    unsigned polls_=0;
};
}
