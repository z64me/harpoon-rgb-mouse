#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

extern "C" {
#include "../harpoon.h"
};

enum packetType
{
    NONE = 0
    , DPICONFIG = (1 << 0)
    , DPIENABLED = (1 << 1)
    , DPIMODE = (1 << 2)
    , COLOR = (1 << 3)
    , POLLRATE = (1 << 4)
    , MOST = (1 << 5)
};

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected slots:
    /* timer functions */
    void harpoonFunc(void);
    void autoFunc(void);

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    struct harpoon *hp;
    Ui::MainWindow *ui;

    /* simple driver abstraction */
    void sendPackets(enum packetType types);

private slots:
    void on_cbAuto_stateChanged(int enabled);

    void on_spinDpi_valueChanged(int arg1);

    void on_spinDpi_editingFinished();

    void on_comboPollRate_currentIndexChanged(int index);

    void on_sliderHue_valueChanged(int value);

    void on_sliderBright_valueChanged(int value);

    void on_spinSpeed_valueChanged(int arg1);

    void on_sliderHue_sliderMoved(int position);

    void on_sliderSaturation_valueChanged(int value);

private:

    QTimer *autoTimer;
    QTimer *monitorTimer;

    int spinDpi_validate(int v);
    void doColor(void);
    uint32_t ledColor;
};
#endif // MAINWINDOW_H
