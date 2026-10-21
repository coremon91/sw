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
#include <QLineEdit>
#include <QSlider>
#include <QTabWidget>
#include "portable_player.hpp"
#include "cg_panel.hpp"
namespace sw {
class Monitor:public QWidget {
public:
    explicit Monitor(QString title,QWidget* parent=nullptr);
    void updateImage(const Image&,QString status,QColor accent,bool newImage=true);
protected:
    void paintEvent(QPaintEvent*) override;
private:
    QString title_,status_="STOPPED";
    QColor accent_="#4c5664";
    QImage image_;
};
class Window:public QMainWindow {
public:
    explicit Window(int receiver=0);
    ~Window() override;
    void startDemo();
    void startSdi(int program,bool unmute,bool uhd,const QStringList& ports);
    void startMediaDemo(const QString& path,bool uhd,bool play);
    void showPortablePlayer();
    void showCg(){tabs_->setCurrentWidget(cgPanel_);cgPanel_->activate();}
    bool cgReady() const{return cgPanel_->ready();}
    bool previewReady() const{const auto s=engine_.snapshot();return s.running&&s.monitorFrames>=2;}
    void setDiagnosticsPath(QString path) {diagnosticsPath_=std::move(path);}
    bool savePreview(const QString&);
    bool verifyStableLayout();
    bool verifyRoutingModes(const QString& screenshot);
protected:
    void closeEvent(QCloseEvent*) override;
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
    QLabel* routingHint_;
    std::vector<Endpoint> devices_;
    bool syncing_=false;
    uint64_t lastMonitorFrames_=0;
    std::chrono::steady_clock::time_point nextControls_{};
    std::chrono::steady_clock::time_point nextDiagnostics_{};
    QString diagnosticsPath_;
    int previousRoutingMode_=-1;
    QLabel* audioStatus_;
    QCheckBox *mediaEnabled_,*mediaLoop_;
    QLineEdit* mediaFile_;
    QPushButton *mediaBrowse_,*mediaPlay_,*mediaPause_,*mediaCue_;
    QSlider* mediaSeek_;
    QLabel* mediaStatus_;
    bool mediaAutoPlay_=false;
    QTabWidget* tabs_;
    QWidget* playerPage_;
    Monitor* playerMonitor_;
    PortablePlayer* portablePlayer_;
    QComboBox* playerMode_;
    CgPanel* cgPanel_;
    QComboBox *cgSource_,*cgFill_,*cgKey_;
    QCheckBox *cgPreview_,*cgInvert_,*cgPremultiplied_;
    QPushButton *cgOn_,*cgOff_;
    QLabel* cgStatus_;
    bool cgProgramRequested_=false;
    void applyCg(bool program);
    void selectInput(int input);
    QPushButton* cgPlayerTake_;
    void refreshDevices();
    void startSession(bool synthetic=false);
    void update();
    void applyDve();
};
}
