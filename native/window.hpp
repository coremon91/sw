#pragma once
#include "engine.hpp"
#include <QMainWindow>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QTimer>
namespace sw {
class Monitor:public QWidget {
public:
    explicit Monitor(QString title,QWidget* parent=nullptr);
    void updateImage(const Image&,QString status,QColor accent);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString title_,status_="STOPPED";
    QColor accent_="#4c5664";
    QImage image_;
};
class Window:public QMainWindow {
public:
    Window();
    ~Window() override;
    void startDemo();
    bool savePreview(const QString&);
private:
    Engine engine_;
    QTimer timer_;
    std::array<Monitor*,6> monitors_{};
    std::array<QPushButton*,4> previewButtons_{};
    std::array<QComboBox*,6> ports_{};
    QComboBox *mode_,*sourceMode_,*dveSource_;
    QPushButton *start_,*stop_,*refresh_,*cut_,*auto_;
    QSpinBox* duration_;
    QCheckBox *dveEnabled_,*mute_;
    std::array<QDoubleSpinBox*,11> dveValues_{};
    QLabel *status_,*hardware_;
    QWidget *session_,*dvePanel_;
    std::vector<Endpoint> devices_;
    bool syncing_=false;
    void refreshDevices();
    void startSession();
    void update();
    void applyDve();
};
}
