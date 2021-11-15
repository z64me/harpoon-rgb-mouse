#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QTimer>
#include <math.h>

#define DEFAULT_INDEX 1

static void onConnect(void *udata)
{
    MainWindow *mw = (MainWindow*)udata;
    struct harpoon *hp = mw->hp;

    fprintf(stderr, "onConnect\n");
    mw->ui->centralwidget->setEnabled(true);
    mw->ui->statusBar->clearMessage();

    /* default: locked to one mode with all DPI settings disabled */
    harpoon_send(hp, harpoonPacket_dpimode(DEFAULT_INDEX));
    harpoon_send(hp, harpoonPacket_dpisetenabled(
        false
        , false
        , false
        , false
        , false
        , false
    ));

    /* on a mouse restart, repropagate every setting except polling rate */
    mw->sendPackets(MOST);
}

static void onDisconnect(void *udata)
{
    MainWindow *mw = (MainWindow*)udata;

    fprintf(stderr, "onDisconnect\n");

    mw->ui->centralwidget->setEnabled(false);
    if (mw->ui->statusBar->currentMessage().isEmpty())
        mw->ui->statusBar->showMessage("Searching for mouse...");
}

void MainWindow::harpoonFunc(void)
{
    harpoon_monitor(hp);
}

void MainWindow::autoFunc(void)
{
    int nextValue = (ui->sliderHue->value() + 1) % ui->sliderHue->maximum();
    ui->sliderHue->setValue(nextValue);
    doColor();
}

void MainWindow::sendPackets(enum packetType types)
{
    struct harpoon *hp = this->hp;
    int most = (types == MOST);

    if (!harpoon_isConnected(hp))
        return;

    if (types & POLLRATE)
    {
        int index = ui->comboPollRate->currentIndex();
        int rates[] = {8, 4, 2, 1}; /* polling rates, matching the indices */

        ui->centralwidget->setEnabled(false);
        ui->statusBar->showMessage("Restarting mouse...");
        harpoon_send(hp, harpoonPacket_pollrate(rates[index]));
    }
    if (most || (types & DPICONFIG))
    {
        int precision = spinDpi_validate(ui->spinDpi->value());

        harpoon_send(hp, harpoonPacket_dpiconfig(
            DEFAULT_INDEX
            , precision /* x, y */
            , precision
            , ledColor >> 16 /* r, g, b */
            , ledColor >> 8
            , ledColor
        ));
    }
    if (most || (types & COLOR))
    {
        harpoon_send(hp, harpoonPacket_color(
            ledColor >> 16 /* r, g, b */
            , ledColor >> 8
            , ledColor
        ));
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    hp = harpoon_new();
    harpoon_set_onDisconnect(hp, onDisconnect, this);
    harpoon_set_onConnect(hp, onConnect, this);

    ui->setupUi(this);
    doColor();
    onDisconnect(this);

    monitorTimer = new QTimer(this);
    connect(monitorTimer, SIGNAL(timeout()), this, SLOT(harpoonFunc()));
    monitorTimer->start(1000);

    autoTimer = new QTimer(this);
    connect(autoTimer, SIGNAL(timeout()), this, SLOT(autoFunc()));
}

MainWindow::~MainWindow()
{
    monitorTimer->stop();
    harpoon_delete(hp);
    delete ui;
}

/* hsv to rgb implementation adapted from the code here:
 * https://github.com/stolk/hsvbench
 */

#define CLAMP01(x) ( (x) < 0 ? 0 : ( x > 1 ? 1 : (x) ) )

void HsvToRgb(float h, float s, float v, float *r, float *g, float *b)
{
    const float h6 = 6.0f * h;
    const float rC = fabsf( h6 - 3.0f ) - 1.0f;
    const float gC = 2.0f - fabsf( h6 - 2.0f );
    const float bC = 2.0f - fabsf( h6 - 4.0f );
    const float is = 1.0f - s;

    *r = v * ( s * CLAMP01(rC) + is );
    *g = v * ( s * CLAMP01(gC) + is );
    *b = v * ( s * CLAMP01(bC) + is );
}

void HsvToRgb8(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float rn;
    float gn;
    float bn;

    assert(r);
    assert(g);
    assert(b);

    HsvToRgb(h, s, v, &rn, &gn, &bn);

    *r = rn * 255;
    *g = gn * 255;
    *b = bn * 255;
}

uint32_t HsvToRgb24(float h, float s, float v)
{
    uint8_t r;
    uint8_t g;
    uint8_t b;

    HsvToRgb8(h, s, v, &r, &g, &b);

    return (r << 16) | (g << 8) | b;
}

/*
 * end hsv stuff
 */

/* get the best contrasting font color against a background color */
uint32_t bestFontContrast(uint32_t bgcolor, float brightness)
{
    uint8_t red = bgcolor >> 16;
    uint8_t green = bgcolor >> 8;
    uint8_t blue = bgcolor;
    double y = (800 * red + 587 * green + 114 * blue) / 1000;

    if (brightness <= 0.7)
        return -1;

    return y > 127 ? 0 : -1;
}

void MainWindow::doColor(void)
{
    /* derive color from hue, saturation, and brightness */
    float h = float(ui->sliderHue->value()) / ui->sliderHue->maximum();
    float s = float(ui->sliderSaturation->value()) / ui->sliderSaturation->maximum();
    float v = float(ui->sliderBright->value()) / ui->sliderBright->maximum();
    unsigned color = HsvToRgb24(h, s, v);
    unsigned textColor = bestFontContrast(color, v);
    char style[64];
    char text[64];

    sprintf(style, "background-color:#%06x;color:#%06x;", color, textColor);
    ui->labelResultPreview->setStyleSheet(style);
    sprintf(text, "#%06x", textColor);
    ui->labelResultPreview->setText(text);

    ledColor = color;

    sendPackets(COLOR);
}

void MainWindow::on_cbAuto_stateChanged(int enabled)
{
    ui->labelSpeed->setEnabled(enabled);
    ui->spinSpeed->setEnabled(enabled);

    if (enabled)
        autoTimer->start(100);
    else
        autoTimer->stop();
}

int MainWindow::spinDpi_validate(int v)
{
    int step = ui->spinDpi->singleStep();
    int modulo = v % step;

    if (modulo)
    {
        if (modulo >= step / 2)
            v += step - modulo;
        else
            v -= modulo;
    }

    return v;
}

void MainWindow::on_spinDpi_valueChanged(int arg1)
{
    sendPackets(DPICONFIG);

    (void)arg1;
}

void MainWindow::on_spinDpi_editingFinished()
{
    ui->spinDpi->setValue(spinDpi_validate(ui->spinDpi->value()));
}

void MainWindow::on_comboPollRate_currentIndexChanged(int index)
{
    (void)index;

    sendPackets(POLLRATE);
}

void MainWindow::on_sliderHue_valueChanged(int value)
{
    (void)value;
    doColor();
}

void MainWindow::on_sliderBright_valueChanged(int value)
{
    (void)value;
    doColor();
}

void MainWindow::on_spinSpeed_valueChanged(int v)
{
    int minDelay = 5 /* minimum delay (milliseconds) */;
    int delay = fmax(ui->spinSpeed->maximum() - v, minDelay);

    if (v == 0)
    {
        autoTimer->stop();
        return;
    }
    autoTimer->start(delay);
}

void MainWindow::on_sliderHue_sliderMoved(int position)
{
    (void)position;
    doColor();
}

void MainWindow::on_sliderSaturation_valueChanged(int value)
{
    (void)value;
    doColor();
}
