#include "pfl-dock.hpp"
#include "pfl-audio-engine.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QFontMetrics>
#include <QVBoxLayout>

#include <QHBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QVBoxLayout>
#include <QMetaObject>
#include <thread>
#include <QDebug>
#include <QIcon>
#include <QSpacerItem>
#include <obs-audio-controls.h>
#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkRequest>
#include <QVersionNumber>
#include <QMouseEvent>

#include <cmath>

#include <obs.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <obs-hotkey.h>
#include <obs-module.h>
#include <util/platform.h>
#include <windows.h>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

class PFLVolumeSlider : public QSlider
{
public:
    explicit PFLVolumeSlider(
        obs_fader_t *fader,
        QWidget *parent = nullptr)
        : QSlider(Qt::Horizontal, parent),
          fader_(fader)
    {
    }

protected:

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {

            obs_fader_conversion_t faderDbToDef =
                obs_fader_db_to_def(fader_);

            float zeroDb =
                faderDbToDef(0.0f);

            int sliderValue =
                qRound(
                    zeroDb *
                    (maximum() - minimum()) +
                    minimum());

            setValue(sliderValue);

            event->accept();
            return;
        }

        QSlider::mouseDoubleClickEvent(event);
    }

    void paintEvent(QPaintEvent *event) override
    {
        QPainter painter(this);

        const QColor tickColor(
            91,
            98,
            115,
            255);

        obs_fader_conversion_t
            faderDbToDef =
                obs_fader_db_to_def(fader_);

        QStyleOptionSlider opt;
        initStyleOption(&opt);

        QRect groove =
            style()->subControlRect(
                QStyle::CC_Slider,
                &opt,
                QStyle::SC_SliderGroove,
                this);

        QRect handle =
            style()->subControlRect(
                QStyle::CC_Slider,
                &opt,
                QStyle::SC_SliderHandle,
                this);

        const int sliderWidth =
            groove.width() -
            handle.width();

        float tickLength =
            groove.height() * 1.5f;

        tickLength =
            std::max(
                static_cast<int>(tickLength) +
                    groove.height(),
                8 +
                    groove.height());

        float yPos =
            groove.center().y() -
            (tickLength / 2.0f) +
            1.0f;

        for (int db = -10;
             db >= -90;
             db -= 10) {

            float tickValue =
                faderDbToDef(db);

            float xPos =
                groove.left() +
                (tickValue * sliderWidth) +
                (handle.width() / 2.0f);

            painter.fillRect(
                static_cast<int>(xPos),
                static_cast<int>(yPos),
                1,
                static_cast<int>(tickLength),
                tickColor);
        }

        QSlider::paintEvent(event);
    }

private:
    obs_fader_t *fader_ = nullptr;
};


PFLLevelMeter::PFLLevelMeter(QWidget *parent)
	: QWidget(parent)
{
peakTimer_ = new QTimer(this);
peakTimer_->setInterval(30);

connect(
	peakTimer_,
	&QTimer::timeout,
	this,
	[this]() {

silenceTicks_++;

if (silenceTicks_ >= 5) {

    for (int channel = 0; channel < 2; channel++) {

        magnitudeDb_[channel] = -60.0f;

        if (peakDb_[channel] < -60.0f)
            peakDb_[channel] = -60.0f;
    }
}

for (int channel = 0; channel < 2; channel++) {

    if (displayedMagnitudeDb_[channel] >
        magnitudeDb_[channel]) {

        displayedMagnitudeDb_[channel] -= 1.2f;

        if (displayedMagnitudeDb_[channel] <
            magnitudeDb_[channel]) {

            displayedMagnitudeDb_[channel] =
                magnitudeDb_[channel];
        }

        if (displayedMagnitudeDb_[channel] < -60.0f)
            displayedMagnitudeDb_[channel] = -60.0f;

        update();
    }

    if (peakDb_[channel] >
        displayedMagnitudeDb_[channel]) {

        peakDb_[channel] -= 0.45f;

        if (peakDb_[channel] <
            displayedMagnitudeDb_[channel]) {

            peakDb_[channel] =
                displayedMagnitudeDb_[channel];
        }

        update();
    }
}
	});
	
peakTimer_->start();

	setMinimumHeight(34);
	setMaximumHeight(40);
	setSizePolicy(
		QSizePolicy::Expanding,
		QSizePolicy::Fixed);
}


void PFLLevelMeter::SetLevels(
    float magnitudeL,
    float peakL,
    float inputPeakL,
    float magnitudeR,
    float peakR,
    float inputPeakR)
{
    Q_UNUSED(inputPeakL);
    Q_UNUSED(inputPeakR);

    silenceTicks_ = 0;

    float magnitudes[2] = {
        magnitudeL,
        magnitudeR
    };

    float peaks[2] = {
        peakL,
        peakR
    };

    for (int channel = 0; channel < 2; channel++) {

        if (magnitudes[channel] < -60.0f)
            magnitudes[channel] = -60.0f;

        if (magnitudes[channel] > 0.0f)
            magnitudes[channel] = 0.0f;

        if (peaks[channel] < -60.0f)
            peaks[channel] = -60.0f;

        if (peaks[channel] > 0.0f)
            peaks[channel] = 0.0f;

        magnitudeDb_[channel] =
            magnitudes[channel];

        peakDb_[channel] =
            peaks[channel];

        if (magnitudeDb_[channel] >
            displayedMagnitudeDb_[channel]) {

            displayedMagnitudeDb_[channel] =
                magnitudeDb_[channel];
        }

        if (magnitudeDb_[channel] >= 0.0f ||
            peakDb_[channel] >= 0.0f) {

            clipped_ = true;
        }
    }

    update();
}
void PFLLevelMeter::SetMuted(bool muted)
{
	muted_ = muted;
	update();
}

void PFLLevelMeter::SetPFLActive(bool active)
{
    pflActive_ = active;
    update();
}

void PFLLevelMeter::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, false);
painter.setPen(Qt::NoPen);

    const int left = 0;
    const int right = 0;
    const int top = 4;

    const int meterHeight = 6;
    const int channelGap = 2;

    const int meterWidth =
        width() - left - right;

    if (meterWidth <= 0)
        return;

    const bool colorMode = pflActive_;

    const QColor greenBackground =
        colorMode ? QColor("#267F26") : QColor(90, 90, 90);

    const QColor yellowBackground =
        colorMode ? QColor("#7F7F26") : QColor(117, 117, 117);

    const QColor redBackground =
        colorMode ? QColor("#7F2626") : QColor(65, 65, 65);

    const QColor greenActive =
        colorMode ? QColor("#4CFF4C") : QColor(163, 163, 163);

    const QColor yellowActive =
        colorMode ? QColor("#FFFF4C") : QColor(217, 217, 217);

    const QColor redActive =
        colorMode ? QColor("#FF4C4C") : QColor(113, 113, 113);

    const int channelTopL = top;

    const int channelTopR =
        top + meterHeight + channelGap;

    /*
     * Converte dB para posição horizontal.
     *
     * -60 dB = início
     *   0 dB = fim
     */
    auto dbToPosition =
        [meterWidth](float db) {

            if (db < -60.0f)
                db = -60.0f;

            if (db > 0.0f)
                db = 0.0f;

            return static_cast<int>(
                ((db + 60.0f) / 60.0f) *
                meterWidth);
        };

    /*
     * Níveis atuais — L/R
     */
    const int levelWidthL =
        dbToPosition(
            displayedMagnitudeDb_[0]);

    const int levelWidthR =
        dbToPosition(
            displayedMagnitudeDb_[1]);

    /*
     * Escala de cores
     */
    const int greenEnd =
        dbToPosition(-20.0f);

    const int yellowStart =
        dbToPosition(-20.0f);

    const int yellowEnd =
        dbToPosition(-9.0f);

    const int redStart =
        dbToPosition(-9.0f);

    const int redEnd =
        dbToPosition(0.0f);

    /*
     * Desenha o fundo de um canal.
     */
    auto drawBackground =
        [&](int channelTop) {

            // VERDE
            painter.setBrush(
                greenBackground);

            painter.drawRect(
                left,
                channelTop,
                greenEnd,
                meterHeight);

            // AMARELO
            painter.setBrush(
                yellowBackground);

            painter.drawRect(
                left + yellowStart,
                channelTop,
                yellowEnd - yellowStart,
                meterHeight);

            // VERMELHO
            painter.setBrush(
                redBackground);

            painter.drawRect(
                left + redStart,
                channelTop,
                redEnd - redStart,
                meterHeight);
        };

    drawBackground(channelTopL);
    drawBackground(channelTopR);

    /*
     * Desenha o nível ativo de um canal.
     */
    auto drawActiveLevel =
        [&](int levelWidth, int channelTop) {

            if (muted_ || levelWidth <= 0)
                return;

            // VERDE
            {
                const int width =
                    qMin(
                        levelWidth,
                        greenEnd);

                if (width > 0) {

                    painter.setBrush(
                        greenActive);

                    painter.drawRect(
                        left,
                        channelTop,
                        width,
                        meterHeight);
                }
            }

            // AMARELO
            if (levelWidth > yellowStart) {

                const int width =
                    qMin(
                        levelWidth,
                        yellowEnd)
                    - yellowStart;

                if (width > 0) {

                    painter.setBrush(
                        yellowActive);

                    painter.drawRect(
                        left + yellowStart,
                        channelTop,
                        width,
                        meterHeight);
                }
            }

            // VERMELHO
            if (levelWidth > redStart) {

                const int width =
                    levelWidth - redStart;

                if (width > 0) {

                    painter.setBrush(
                        redActive);

                    painter.drawRect(
                        left + redStart,
                        channelTop,
                        width,
                        meterHeight);
                }
            }
        };

    drawActiveLevel(
        levelWidthL,
        channelTopL);

    drawActiveLevel(
        levelWidthR,
        channelTopR);

    /*
     * PEAK HOLD
     *
     * Linha branca independente
     * para L e R.
     */
    auto drawPeak =
        [&](float peakDb, int channelTop) {

            if (muted_ || peakDb <= -60.0f)
                return;

            const int peakPosition =
                dbToPosition(peakDb);

            painter.setPen(
                QColor("#FFFFFF"));

            painter.drawRect(
                left + peakPosition,
                channelTop,
                2,
                meterHeight);
        };

    drawPeak(
        peakDb_[0],
        channelTopL);

    drawPeak(
        peakDb_[1],
        channelTopR);

    /*
     * Escala
     */
    painter.setPen(
        QColor("#888888"));

    QFont font =
        painter.font();

    font.setPointSize(7);

    painter.setFont(font);

    const int values[] = {
        -60,
        -54,
        -48,
        -42,
        -36,
        -30,
        -24,
        -18,
        -12,
        -6,
        0
    };

    /*
     * A escala fica abaixo das duas barras.
     */
    const int scaleTop =
        channelTopR + meterHeight;

    for (int db : values) {

        const int position =
            dbToPosition(
                static_cast<float>(db));

        painter.drawLine(
            left + position,
            scaleTop,
            left + position,
            scaleTop + 3);

        QString text =
            QString::number(db);

        const QFontMetrics metrics(font);

        const int textWidth =
            metrics.horizontalAdvance(text);

        int textX =
            left +
            position -
            textWidth / 2;

        if (db == -60)
            textX = left;

        if (db == 0)
            textX =
                left +
                meterWidth -
                textWidth;

        painter.drawText(
            textX,
            scaleTop + 15,
            text);
    }

    /*
     * CLIP
     */
    if (clipped_) {

        painter.setPen(
            QColor("#F04444"));

        QFont clipFont =
            painter.font();

        clipFont.setPointSize(7);
        clipFont.setBold(true);

        painter.setFont(clipFont);

        painter.drawText(
            left,
            0,
            meterWidth,
            8,
            Qt::AlignLeft |
                Qt::AlignVCenter,
            "CLIP");

        const int zeroPosition =
            dbToPosition(0.0f);

        painter.setPen(
            QColor("#F04444"));

        painter.drawLine(
            left + zeroPosition,
            channelTopL,
            left + zeroPosition,
            channelTopR + meterHeight);
    }
}
PFLDock::PFLDock(QWidget *parent)
	: QWidget(parent)
{
	audioEngine_ = new PFLAudioEngine();
	audioEngine_->Start();
volumeFader_ = obs_fader_create(OBS_FADER_LOG);

	setMinimumWidth(260);
	setObjectName("PFLPreviewDock");

	auto *layout = new QVBoxLayout(this);


	layout->setContentsMargins(6, 6, 6, 6);
layout->setSpacing(10);

	// =========================
	// PREVIEW
	// =========================

	auto *previewRow = new QHBoxLayout();
	previewRow->setSpacing(6);

	auto *previewTitle = new QLabel("Preview:", this);
	previewTitle->setStyleSheet(
		"font-weight: bold;"
	);

	previewRow->addWidget(previewTitle);

	sceneLabel_ = new QLabel("Studio Mode Inativo", this);

	sceneLabel_->setStyleSheet(
		"padding: 4px 6px;"
		"border: 1px solid #555;"
		"border-radius: 4px;"
	);

	sceneLabel_->setMinimumHeight(28);
	sceneLabel_->setMaximumHeight(32);
	sceneLabel_->setSizePolicy(
		QSizePolicy::Expanding,
		QSizePolicy::Fixed);

	sceneLabel_->setWordWrap(false);

	previewRow->addWidget(sceneLabel_);

	layout->addLayout(previewRow);

	// =========================
	// FONTE DE ÁUDIO
	// =========================
layout->addSpacing(14);
	auto *audioSourceRow = new QHBoxLayout();
audioSourceRow->setSpacing(6);

auto *audioSourceTitle =
	new QLabel("Fonte de áudio:", this);

audioSourceTitle->setStyleSheet(
	"font-weight: bold;"
);

audioSourceRow->addWidget(
	audioSourceTitle);

audioSourceCombo_ = new QComboBox(this);
audioSourceCombo_->setToolTip(
    "Selecione a fonte de áudio da cena Preview");

audioSourceCombo_->setFixedHeight(32);

audioSourceCombo_->setSizePolicy(
	QSizePolicy::Expanding,
	QSizePolicy::Fixed);

connect(
	audioSourceCombo_,
	QOverload<int>::of(&QComboBox::currentIndexChanged),
	this,
	&PFLDock::ChangeAudioSource);

audioSourceRow->addWidget(
	audioSourceCombo_);

pflListenButton_ = new QPushButton(this);

const char *moduleDataPath =
	obs_get_module_data_path(obs_current_module());

if (moduleDataPath) {
	pflListenButton_->setIcon(
		QIcon(
			QString::fromUtf8(moduleDataPath) +
			"/headphones-off.svg"
		)
	);
}

pflListenButton_->setIconSize(
	QSize(20, 20)
);

pflListenButton_->setCheckable(true);
pflListenButton_->setFixedSize(32, 32);

pflListenButton_->setToolTip(
	"Ouvir Preview");

connect(
	pflListenButton_,
	&QPushButton::clicked,
	this,
	&PFLDock::TogglePFL);

/*
 * Centraliza o botão verticalmente
 * em relação ao campo de áudio.
 */
audioSourceRow->addWidget(
	pflListenButton_,
	0,
	Qt::AlignVCenter);

layout->addLayout(audioSourceRow);
layout->addSpacing(18);

	// =========================
	// VOLUME
	// =========================

// VOLUME
layout->addSpacing(12);

volumeSlider_ =
    new PFLVolumeSlider(
        volumeFader_,
        this);
volumeSlider_->setToolTip(
    "Volume do áudio do Preview");

volumeSlider_->setRange(0, 100);
volumeSlider_->setValue(100);

volumeSlider_->setTickPosition(QSlider::NoTicks);

volumeSlider_->setMinimumHeight(16);
volumeSlider_->setMaximumHeight(18);

volumeSlider_->setSizePolicy(
    QSizePolicy::Expanding,
    QSizePolicy::Fixed);

volumeSlider_->setStyleSheet(
    "QSlider::groove:horizontal {"
    "    height: 4px;"
    "    background: #28282A;"
    "    border: none;"
    "    border-radius: 2px;"
    "}"
    "QSlider::sub-page:horizontal {"
    "    background: #2A82DA;"
    "    border-radius: 2px;"
    "}"
    "QSlider::add-page:horizontal {"
    "    background: #13141A;"
    "    border-radius: 2px;"
    "}"
    "QSlider::handle:horizontal {"
    "    background: #F0EFF0;"
    "    border: 1px solid #3A393A;"
    "    width: 18px;"
    "    height: 10px;"
    "    margin: -3px 0;"
    "    border-radius: 3px;"
    "}"
    "QSlider::handle:horizontal:hover {"
    "    background: #C8C7C8;"
    "}"
    "QSlider::handle:horizontal:pressed {"
    "    background: #F0EFF0;"
    "}"
    "QSlider::tick-mark:horizontal {"
    "    width: 1px;"
    "    height: 4px;"
    "    background: #76797C;"
    "}"
);

connect(
    volumeSlider_,
    &QSlider::valueChanged,
    this,
    &PFLDock::ChangeVolume);

auto *volumeRow = new QHBoxLayout();

volumeRow->setContentsMargins(0, 0, 0, 0);
volumeRow->setSpacing(6);

volumeSlider_->setSizePolicy(
    QSizePolicy::Expanding,
    QSizePolicy::Fixed);

volumeRow->addWidget(volumeSlider_);

volumeLabel_ = new QLabel("0,0 dB", this);

volumeLabel_->setAlignment(
    Qt::AlignVCenter | Qt::AlignRight);

volumeLabel_->setStyleSheet(
    "color: #AAAAAA;"
    "font-size: 10px;"
);

volumeLabel_->setFixedWidth(42);

volumeRow->addWidget(
    volumeLabel_,
    0,
    Qt::AlignVCenter);

auto *volumeSection = new QVBoxLayout();

volumeSection->setContentsMargins(
    0, 0, 0, 0);

volumeSection->setSpacing(0);

volumeSection->addLayout(
    volumeRow);

levelMeter_ = new PFLLevelMeter(this);

levelMeter_->setToolTip(
    "Nível de áudio do Preview");

levelMeter_->setSizePolicy(
    QSizePolicy::Expanding,
    QSizePolicy::Fixed);

levelMeter_->setMaximumHeight(15);

volumeSection->addWidget(
    levelMeter_,
    0,
    Qt::AlignTop);

layout->addLayout(
    volumeSection);

// =========================
// RODAPÉ
// =========================

auto *footerFrame = new QFrame(this);

footerFrame->setFixedHeight(30);

footerFrame->setStyleSheet(
    "QFrame {"
    "    background-color: palette(window);"
    "    border-top: 1px solid palette(mid);"
    "}"
);

auto *footerLayout = new QHBoxLayout(footerFrame);

footerLayout->setContentsMargins(6, 0, 2, 0);
footerLayout->setSpacing(0);

auto *info = new QLabel(
    "v1.3.0 by DABLO",
    footerFrame
);

info->setWordWrap(false);

info->setAlignment(
    Qt::AlignLeft |
    Qt::AlignVCenter
);

info->setStyleSheet(
    "QLabel {"
    "    color: #D0D0D0;"
    "    background: transparent;"
    "    border: none;"
    "    font-size: 11px;"
    "}"
);

footerLayout->addWidget(info);

footerLayout->addStretch();

// BOTÃO DE CONFIGURAÇÕES
auto *settingsButton =
    new QPushButton(footerFrame);

settingsButton->setFixedSize(28, 28);

settingsButton->setToolTip(
    "Sobre e atualizações"
);

connect(
    settingsButton,
    &QPushButton::clicked,
    this,
    [this]() {

        QDialog dialog(this);

        dialog.setWindowTitle("Sobre / Atualizações");
        dialog.setModal(true);
        dialog.setFixedSize(360, 260);

        auto *dialogLayout =
            new QVBoxLayout(&dialog);

        dialogLayout->setContentsMargins(
            20, 20, 20, 20);

        dialogLayout->setSpacing(10);

        auto *title =
            new QLabel("PFL Preview", &dialog);

        title->setAlignment(
            Qt::AlignCenter);

        title->setStyleSheet(
            "font-size: 18px;"
            "font-weight: bold;"
        );

        dialogLayout->addWidget(title);

        auto *description =
            new QLabel(
                "É um painel de monitoramento de áudio  "
                "dedicado para o Preview do OBS Studio.",
                &dialog
            );

        description->setAlignment(
            Qt::AlignCenter);

        description->setWordWrap(true);

        dialogLayout->addWidget(
            description);

        dialogLayout->addSpacing(8);

auto *version =
    new QLabel(
        QString("Versão instalada: %1")
            .arg(PLUGIN_VERSION),
        &dialog
    );

        version->setAlignment(
            Qt::AlignCenter);

        dialogLayout->addWidget(version);

        auto *copyright =
            new QLabel(
                "© 2026 DABLO",
                &dialog
            );

        copyright->setAlignment(
            Qt::AlignCenter);

        copyright->setStyleSheet(
            "color: #999999;"
        );

        dialogLayout->addWidget(
            copyright);

        dialogLayout->addStretch();

        
auto *updateStatus =
    new QLabel(
        "Verificando atualizações...",
        &dialog
    );

updateStatus->setAlignment(
    Qt::AlignCenter
);

updateStatus->setWordWrap(true);

updateStatus->setStyleSheet(
    "color: #AAAAAA;"
);

dialogLayout->addWidget(updateStatus);
auto *updateButton =
    new QPushButton(
        "ATUALIZAR",
        &dialog
    );

updateButton->setVisible(false);

dialogLayout->addWidget(updateButton);
connect(
    updateButton,
    &QPushButton::clicked,
    []() {
        QDesktopServices::openUrl(
            QUrl(
                "https://github.com/dablofilmes/pfl-preview/releases"
            )
        );
    }
);

std::thread([updateStatus, updateButton]() {

    HINTERNET session = WinHttpOpen(
        L"PFL-Preview/1.1.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!session) {
        QMetaObject::invokeMethod(
            updateStatus,
            [updateStatus]() {
                updateStatus->setText(
                    "Não foi possível conectar ao GitHub."
                );
            }
        );
        return;
    }

    HINTERNET connection = WinHttpConnect(
        session,
        L"api.github.com",
        INTERNET_DEFAULT_HTTPS_PORT,
        0
    );

    if (!connection) {
        WinHttpCloseHandle(session);

        QMetaObject::invokeMethod(
            updateStatus,
            [updateStatus]() {
                updateStatus->setText(
                    "Não foi possível conectar ao GitHub."
                );
            }
        );
        return;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"GET",
        L"/repos/dablofilmes/pfl-preview/releases/latest",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE
    );

    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        QMetaObject::invokeMethod(
            updateStatus,
            [updateStatus]() {
                updateStatus->setText(
                    "Não foi possível verificar atualizações."
                );
            }
        );
        return;
    }

    const wchar_t *headers =
        L"Accept: application/vnd.github+json\r\n"
        L"User-Agent: PFL-Preview/1.1.0\r\n";

    BOOL sent = WinHttpSendRequest(
        request,
        headers,
        (DWORD)-1L,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    BOOL received = FALSE;

    if (sent) {
        received = WinHttpReceiveResponse(
            request,
            nullptr
        );
    }

    QByteArray responseData;

    if (received) {

        DWORD available = 0;

        while (
            WinHttpQueryDataAvailable(
                request,
                &available
            ) &&
            available > 0
        ) {

            QByteArray buffer(
                static_cast<int>(available),
                Qt::Uninitialized
            );

            DWORD downloaded = 0;

            if (!WinHttpReadData(
                    request,
                    buffer.data(),
                    available,
                    &downloaded
                )) {
                break;
            }

            responseData.append(
                buffer.constData(),
                static_cast<int>(downloaded)
            );
        }
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    if (!received || responseData.isEmpty()) {

        QMetaObject::invokeMethod(
            updateStatus,
            [updateStatus]() {
                updateStatus->setText(
                    "Não foi possível verificar atualizações."
                );
            }
        );

        return;
    }

    const QJsonDocument document =
        QJsonDocument::fromJson(responseData);

    if (!document.isObject()) {

        QMetaObject::invokeMethod(
            updateStatus,
            [updateStatus]() {
                updateStatus->setText(
                    "Resposta inválida do GitHub."
                );
            }
        );

        return;
    }

    const QJsonObject object =
        document.object();

    const QString latestVersion =
        object.value("tag_name").toString();

    if (latestVersion.isEmpty()) {

        QMetaObject::invokeMethod(
            updateStatus,
            [updateStatus]() {
                updateStatus->setText(
                    "Não foi possível identificar a versão."
                );
            }
        );

        return;
    }

    QMetaObject::invokeMethod(
    updateStatus,
    [updateStatus, updateButton, latestVersion]() {

        const QVersionNumber currentVersion =
    QVersionNumber::fromString(
        PLUGIN_VERSION
    );

const QVersionNumber latest =
    QVersionNumber::fromString(latestVersion);

if (latest > currentVersion) {

            updateStatus->setText(
                "Nova versão disponível: " +
                latestVersion
            );

            updateButton->setVisible(true);

        } else {

            updateStatus->setText(
                "✓ Você está usando a versão mais recente."
            );

            updateButton->setVisible(false);
        }
    }
);

}).detach();


        dialog.exec();
    }
);

if (moduleDataPath) {

    settingsButton->setIcon(
        QIcon(
            QString::fromUtf8(moduleDataPath) +
            "/general.svg"
        )
    );
}

settingsButton->setIconSize(
    QSize(16, 16)
);

settingsButton->setStyleSheet(
    "QPushButton {"
    "    background-color: transparent;"
    "    border: none;"
    "    border-radius: 0px;"
    "}"
    "QPushButton:hover {"
    "    background-color: #3A3A3A;"
    "}"
    "QPushButton:pressed {"
    "    background-color: #252525;"
    "}"
);


updateButton_ =
    new QPushButton("ATUALIZAR", footerFrame);

updateButton_->setVisible(false);

updateButton_->setCursor(Qt::PointingHandCursor);

updateButton_->setStyleSheet(
    "QPushButton {"
    "  padding: 4px 10px;"
    "  font-weight: bold;"
    "}"
);

connect(
    updateButton_,
    &QPushButton::clicked,
    []() {
        QDesktopServices::openUrl(
            QUrl(
                "https://github.com/dablofilmes/pfl-preview/releases"
            )
        );
    }
);

auto *footerDivider = new QFrame(footerFrame);

footerDivider->setFrameShape(QFrame::VLine);
footerDivider->setFrameShadow(QFrame::Plain);

footerDivider->setFixedSize(1, 18);

footerDivider->setStyleSheet(
    "QFrame {"
    "    background-color: #444444;"
    "    border: none;"
    "}"
);

footerLayout->addWidget(updateButton_);

footerLayout->addWidget(footerDivider);

footerLayout->addSpacing(4);

footerLayout->addWidget(
    settingsButton,
    0,
    Qt::AlignVCenter
);

layout->addStretch(1);

layout->addWidget(footerFrame);

setStyleSheet(
    "QPushButton {"
    "  padding: 0px;"
    "}"
    "QPushButton:checked {"
    "  font-weight: bold;"
    "}"
);

CheckForUpdates();
}

PFLDock::~PFLDock()
{
    StopPreviewAudioCapture();
    StopVolumeMeter();

if (volumeFader_) {
    obs_fader_destroy(volumeFader_);
    volumeFader_ = nullptr;
}

    if (audioEngine_) {
        audioEngine_->Stop();
        delete audioEngine_;
        audioEngine_ = nullptr;
    }

    if (selectedAudioSource_) {
        obs_source_release(selectedAudioSource_);
        selectedAudioSource_ = nullptr;
    }

    if (previewScene_) {
        obs_source_release(previewScene_);
        previewScene_ = nullptr;
    }
}

void PFLDock::VolumeMeterCallback(
    void *param,
    const float magnitude[MAX_AUDIO_CHANNELS],
    const float peak[MAX_AUDIO_CHANNELS],
    const float inputPeak[MAX_AUDIO_CHANNELS])
{
    auto *dock = static_cast<PFLDock *>(param);

    if (!dock || !dock->levelMeter_)
        return;

    const float magnitudeL = magnitude[0];
    const float peakL = peak[0];
    const float inputPeakL = inputPeak[0];

    const float magnitudeR =
        MAX_AUDIO_CHANNELS > 1
            ? magnitude[1]
            : magnitudeL;

    const float peakR =
        MAX_AUDIO_CHANNELS > 1
            ? peak[1]
            : peakL;

    const float inputPeakR =
        MAX_AUDIO_CHANNELS > 1
            ? inputPeak[1]
            : inputPeakL;

   QMetaObject::invokeMethod(
    dock,
    [dock,
     magnitudeL,
     peakL,
     inputPeakL,
     magnitudeR,
     peakR,
     inputPeakR]() {

        if (!dock->levelMeter_)
            return;

        dock->levelMeter_->SetMuted(
            dock->muted_);

        dock->levelMeter_->SetLevels(
            magnitudeL,
            peakL,
            inputPeakL,
            magnitudeR,
            peakR,
            inputPeakR);
    },
    Qt::QueuedConnection);
}

void PFLDock::UpdatePreviewScene(const QString &sceneName)
{

UpdateLevel(-60.0f);

	sceneLabel_->setText(sceneName);
}

void PFLDock::UpdateLevel(float db)
{
	if (!levelMeter_)
		return;

	levelMeter_->SetMuted(muted_);
	levelMeter_->SetLevels(
    db,
    db,
    db,
    db,
    db,
    db);
}

void PFLDock::TogglePFLFromHotkey()
{
    if (!pflListenButton_)
        return;

    pflListenButton_->setChecked(
        !pflListenButton_->isChecked());

    TogglePFL();
}

void PFLDock::CheckForUpdates()
{
    if (!updateButton_)
        return;

    std::thread([this]() {

        HINTERNET session = WinHttpOpen(
            L"PFL-Preview/1.3.0",
            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0
        );

        if (!session)
            return;

        HINTERNET connection = WinHttpConnect(
            session,
            L"api.github.com",
            INTERNET_DEFAULT_HTTPS_PORT,
            0
        );

        if (!connection) {
            WinHttpCloseHandle(session);
            return;
        }

        HINTERNET request = WinHttpOpenRequest(
            connection,
            L"GET",
            L"/repos/dablofilmes/pfl-preview/releases/latest",
            nullptr,
            WINHTTP_NO_REFERER,
            WINHTTP_DEFAULT_ACCEPT_TYPES,
            WINHTTP_FLAG_SECURE
        );

        if (!request) {
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);
            return;
        }

        const wchar_t *headers =
            L"Accept: application/vnd.github+json\r\n"
            L"User-Agent: PFL-Preview/1.3.0\r\n";

        BOOL sent = WinHttpSendRequest(
            request,
            headers,
            (DWORD)-1L,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0
        );

        BOOL received = FALSE;

        if (sent) {
            received = WinHttpReceiveResponse(
                request,
                nullptr
            );
        }

        QByteArray responseData;

        if (received) {

            DWORD available = 0;

            while (
                WinHttpQueryDataAvailable(
                    request,
                    &available
                ) &&
                available > 0
            ) {

                QByteArray buffer(
                    static_cast<int>(available),
                    Qt::Uninitialized
                );

                DWORD downloaded = 0;

                if (!WinHttpReadData(
                        request,
                        buffer.data(),
                        available,
                        &downloaded
                    )) {
                    break;
                }

                responseData.append(
                    buffer.constData(),
                    static_cast<int>(downloaded)
                );
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);

        if (!received || responseData.isEmpty())
            return;

        const QJsonDocument document =
            QJsonDocument::fromJson(responseData);

        if (!document.isObject())
            return;

        const QJsonObject object =
            document.object();

        const QString latestVersion =
            object.value("tag_name").toString();

        if (latestVersion.isEmpty())
            return;

QString normalizedVersion = latestVersion.trimmed();

if (normalizedVersion.startsWith("v", Qt::CaseInsensitive))
    normalizedVersion.remove(0, 1);

        const QVersionNumber currentVersion =
    QVersionNumber::fromString(
        PLUGIN_VERSION
    );

        const QVersionNumber latest =
    QVersionNumber::fromString(
        normalizedVersion
    );

        if (latest <= currentVersion)
            return;

        QMetaObject::invokeMethod(
            this,
            [this]() {

                if (updateButton_)
                    updateButton_->setVisible(true);

            },
            Qt::QueuedConnection
        );

    }).detach();
}

void PFLDock::TogglePFL()
{
	pflActive_ = pflListenButton_->isChecked();
if (levelMeter_)
    levelMeter_->SetPFLActive(pflActive_);

const char *moduleDataPath =
	obs_get_module_data_path(obs_current_module());

if (moduleDataPath) {

	const QString iconPath =
		QString::fromUtf8(moduleDataPath) +
		(pflActive_
			? "/headphones.svg"
			: "/headphones-off.svg");

	pflListenButton_->setIcon(
		QIcon(iconPath));
}

	if (pflActive_) {

		pflListenButton_->setStyleSheet(
			"QPushButton {"
			"background-color: #18C964;"
			"color: white;"
			"border: 1px solid #18C964;"
			"border-radius: 5px;"
			"font-size: 17px;"
			"}"
		);
	} else {
		pflListenButton_->setStyleSheet(
			"QPushButton {"
			"background-color: #2A2A2A;"
			"color: #AAAAAA;"
			"border: 1px solid #444;"
			"border-radius: 5px;"
			"font-size: 17px;"
			"}"
		);
	}

	if (pflActive_) {

		audioEngine_->SetMuted(false);

		/*
		 * Começa a ouvir somente a fonte selecionada.
		 */
		if (selectedAudioSource_) {

			audioEngine_->ClearSources();

			blog(
				LOG_INFO,
				"[PFL Preview] FONTE SELECIONADA: '%s'",
				obs_source_get_name(selectedAudioSource_));

			audioEngine_->AddSource(
				selectedAudioSource_);

			/*
			 * Conecta o VU à fonte selecionada.
			 */
			if (volumeMeter_) {
				obs_volmeter_detach_source(volumeMeter_);

				obs_volmeter_attach_source(
					volumeMeter_,
					selectedAudioSource_);
			}
		}

		blog(
			LOG_INFO,
			"[PFL Preview] PFL ATIVADO");

	} else {

		/*
		 * Desconecta o VU do Preview.
		 */
		if (volumeMeter_)
			obs_volmeter_detach_source(volumeMeter_);

		/*
		 * Remove qualquer fonte do PFL.
		 */
		audioEngine_->ClearSources();

		audioEngine_->SetMuted(true);

		/*
		 * Zera o medidor visual.
		 */
if (levelMeter_)
    levelMeter_->SetLevels(
        -60.0f,
        -60.0f,
        -60.0f,
        -60.0f,
        -60.0f,
        -60.0f);

		blog(
			LOG_INFO,
			"[PFL Preview] PFL DESATIVADO");
	}
}
void PFLDock::ToggleMute()
{
	if (muteButton_->isChecked()) {
		muteButton_->setText("MUTE ATIVO");
	} else {
		muteButton_->setText("MUTE");
	}
}

void PFLDock::ChangeVolume(int value)
{
    if (!audioEngine_ || !volumeFader_)
        return;

    /*
     * Converte a posição do slider para
     * a mesma curva logarítmica usada pelo OBS.
     *
     * 0   = -infinito
     * 100 = 0 dB
     */
    const float deflection =
        static_cast<float>(value) / 100.0f;

    obs_fader_set_deflection(
        volumeFader_,
        deflection);

    const float db =
        obs_fader_get_db(volumeFader_);

    const float volume =
        obs_db_to_mul(db);

    volume_ = volume;

    audioEngine_->SetVolume(volume);

    if (volumeLabel_) {

        if (!std::isfinite(db)) {

            volumeLabel_->setText(
                "-∞ dB");

        } else {

            volumeLabel_->setText(
                QString("%1 dB")
                    .arg(
                        db,
                        0,
                        'f',
                        1));
        }
    }

    blog(
        LOG_INFO,
        "[PFL Preview] Volume: %.1f dB",
        db);
}


static bool ListPreviewSceneItem(
    obs_scene_t *scene,
    obs_sceneitem_t *item,
    void *param)
{
    Q_UNUSED(scene);

    PFLDock *dock =
        static_cast<PFLDock *>(param);

    obs_source_t *source =
        obs_sceneitem_get_source(item);

    if (!source)
        return true;

    const char *name =
        obs_source_get_name(source);

    const char *id =
        obs_source_get_id(source);

    uint32_t flags =
        obs_source_get_output_flags(source);

    bool hasAudio =
        (flags & OBS_SOURCE_AUDIO) != 0;

    bool audioActive =
        obs_source_audio_active(source);

    blog(LOG_INFO,
        "[PFL Preview] Fonte: '%s' | Tipo: '%s' | Áudio: %s | Ativa: %s",
        name ? name : "(sem nome)",
        id ? id : "(sem tipo)",
        hasAudio ? "SIM" : "NAO",
        audioActive ? "SIM" : "NAO");

    /*
     * Se possui áudio, registra o callback
     * diretamente na fonte.
     */

if (hasAudio && dock) {

    blog(
        LOG_INFO,
        "[PFL Preview] >>> REGISTRANDO FONTE DE ÁUDIO: '%s'",
        name ? name : "(sem nome)");

    obs_source_get_ref(source);

    dock->previewAudioSources_.push_back(source);

    obs_source_add_audio_capture_callback(
        source,
        PFLDock::AudioCaptureCallback,
        dock);

    dock->AddAudioSourceToList(source);
}
    /*
     * GRUPO
     */
    if (obs_sceneitem_is_group(item)) {

        blog(LOG_INFO,
            "[PFL Preview] >>> ENTRANDO NO GRUPO: '%s'",
            name ? name : "(sem nome)");

        obs_sceneitem_group_enum_items(
            item,
            ListPreviewSceneItem,
            dock);

        blog(LOG_INFO,
            "[PFL Preview] <<< SAINDO DO GRUPO: '%s'",
            name ? name : "(sem nome)");
    }

    /*
     * CENA ANINHADA
     */
    obs_scene_t *nestedScene =
        obs_scene_from_source(source);

    if (nestedScene) {

        blog(LOG_INFO,
            "[PFL Preview] >>> ENTRANDO NA CENA: '%s'",
            name ? name : "(sem nome)");

        obs_scene_enum_items(
            nestedScene,
            ListPreviewSceneItem,
            dock);

        blog(LOG_INFO,
            "[PFL Preview] <<< SAINDO DA CENA: '%s'",
            name ? name : "(sem nome)");
    }

    return true;
}

void PFLDock::ChangeAudioSource(int index)
{
	if (!audioEngine_)
		return;

	if (index < 0 || !audioSourceCombo_)
		return;

	QVariant data =
		audioSourceCombo_->itemData(index);

	if (!data.isValid())
		return;

	obs_source_t *source =
		reinterpret_cast<obs_source_t *>(
			data.value<quintptr>());

	if (!source)
		return;

	/*
	 * Remove o medidor da fonte anterior.
	 */
	if (volumeMeter_)
		obs_volmeter_detach_source(volumeMeter_);

	if (selectedAudioSource_) {
    obs_source_release(selectedAudioSource_);
    selectedAudioSource_ = nullptr;
}

selectedAudioSource_ =
    obs_source_get_ref(source);

	/*
	 * Remove qualquer fonte que estivesse sendo ouvida.
	 */
	audioEngine_->ClearSources();

	/*
	 * Se o PFL estiver desligado, apenas guarda a seleção.
	 */
	if (!pflActive_)
		return;

	blog(
		LOG_INFO,
		"[PFL Preview] FONTE SELECIONADA: '%s'",
		obs_source_get_name(source));

	audioEngine_->AddSource(source);

	/*
	 * Faz o VU acompanhar a nova fonte.
	 */
	if (volumeMeter_)
		obs_volmeter_attach_source(
			volumeMeter_,
			selectedAudioSource_);
}
void PFLDock::RefreshPreviewScene()
{
    // Primeiro encerra completamente a captura da cena anterior.
    StopPreviewAudioCapture();

	if (volumeMeter_)
		obs_volmeter_detach_source(volumeMeter_);

    QString previousAudioSourceName;

if (selectedAudioSource_) {
    const char *previousName =
        obs_source_get_name(selectedAudioSource_);

    if (previousName)
        previousAudioSourceName =
            QString::fromUtf8(previousName);
}

if (audioSourceCombo_) {
    audioSourceCombo_->blockSignals(true);
    audioSourceCombo_->clear();
    audioSourceCombo_->blockSignals(false);
}

if (selectedAudioSource_) {
    obs_source_release(selectedAudioSource_);
    selectedAudioSource_ = nullptr;
}

    obs_source_t *scene =
        obs_frontend_get_current_preview_scene();

    if (!scene) {
        sceneLabel_->setText("Studio Mode Inativo");

        // Sem Preview: VU em silêncio.
        UpdateLevel(-60.0f);
        return;
    }

    const char *name =
        obs_source_get_name(scene);

    sceneLabel_->setText(
        name
            ? QString::fromUtf8(name)
            : "Preview"
    );

    if (previewScene_) {
    obs_source_release(previewScene_);
    previewScene_ = nullptr;
}

previewScene_ = obs_source_get_ref(scene);

    obs_scene_t *obsScene =
        obs_scene_from_source(previewScene_);

    if (obsScene) {
        obs_scene_enum_items(
            obsScene,
            ListPreviewSceneItem,
            this);
    }

if (audioSourceCombo_ &&
    audioSourceCombo_->count() > 0) {

    int selectedIndex = 0;

    if (!previousAudioSourceName.isEmpty()) {

        for (int i = 0;
             i < audioSourceCombo_->count();
             i++) {

            if (audioSourceCombo_->itemText(i) ==
                previousAudioSourceName) {

                selectedIndex = i;
                break;
            }
        }
    }

    audioSourceCombo_->setCurrentIndex(
        selectedIndex);
}

// Só inicia a captura se o PFL estiver ativo.
if (pflActive_) {
    StartPreviewAudioCapture();
}

    // A nova cena começa em silêncio.
    // O callback de áudio atualizará o valor quando houver sinal.
    UpdateLevel(-60.0f);

    blog(LOG_INFO,
        "[PFL Preview] Preview atualizado: '%s' — VU resetado",
        name ? name : "(sem nome)");
}


void PFLDock::StopVolumeMeter()
{
	if (!volumeMeter_)
		return;

	obs_volmeter_remove_callback(
		volumeMeter_,
		&PFLDock::VolumeMeterCallback,
		this);

	obs_volmeter_detach_source(
		volumeMeter_);

	obs_volmeter_destroy(
		volumeMeter_);

	volumeMeter_ = nullptr;
}

void PFLDock::AudioCaptureCallback(
    void *param,
    obs_source_t *source,
    const struct audio_data *audio_data,
    bool muted)
{
    PFLDock *dock =
        static_cast<PFLDock *>(param);

    if (!dock ||
        !audio_data ||
        audio_data->frames == 0)
        return;

    /*
     * O VU deve acompanhar somente
     * a fonte selecionada no PFL.
     */
    if (!dock->selectedAudioSource_)
        return;

    if (source != dock->selectedAudioSource_)
        return;

    const char *sourceName =
        obs_source_get_name(source);

    blog(
        LOG_INFO,
        "[PFL Preview] VU — fonte selecionada='%s' frames=%u muted=%s",
        sourceName ? sourceName : "(sem nome)",
        audio_data->frames,
        muted ? "SIM" : "NAO");

    if (muted) {
        QMetaObject::invokeMethod(
            dock,
            [dock]() {

                if (!dock->levelMeter_)
                    return;

                dock->levelMeter_->SetMuted(
                    dock->muted_);

                dock->levelMeter_->SetLevels(
                    -60.0f,
                    -60.0f,
                    -60.0f,
                    -60.0f,
                    -60.0f,
                    -60.0f);
            },
            Qt::QueuedConnection);

        return;
    }

    /*
     * O áudio do OBS é recebido em planos.
     *
     * Plano 0 = canal esquerdo
     * Plano 1 = canal direito
     *
     * Se a fonte for mono, usamos o canal esquerdo
     * também para o direito.
     */

    const float *samplesL =
        reinterpret_cast<const float *>(
            audio_data->data[0]);

    const float *samplesR =
        reinterpret_cast<const float *>(
            audio_data->data[1]);

    if (!samplesL)
        return;

    if (!samplesR)
        samplesR = samplesL;

    float sumL = 0.0f;
    float sumR = 0.0f;

    for (uint32_t i = 0;
         i < audio_data->frames;
         i++) {

        const float sampleL =
            samplesL[i];

        const float sampleR =
            samplesR[i];

        sumL +=
            sampleL * sampleL;

        sumR +=
            sampleR * sampleR;
    }

    const float magnitudeL =
        sqrtf(
            sumL /
            audio_data->frames);

    const float magnitudeR =
        sqrtf(
            sumR /
            audio_data->frames);

    const float dbL =
        20.0f *
        log10f(
            qMax(
                magnitudeL,
                0.000001f));

    const float dbR =
        20.0f *
        log10f(
            qMax(
                magnitudeR,
                0.000001f));

    QMetaObject::invokeMethod(
        dock,
        [dock, dbL, dbR]() {

            if (!dock->levelMeter_)
                return;

            dock->levelMeter_->SetMuted(
                dock->muted_);

            dock->levelMeter_->SetLevels(
                dbL,
                dbL,
                dbL,
                dbR,
                dbR,
                dbR);

        },
        Qt::QueuedConnection);
}
void PFLDock::AddAudioSourceToList(obs_source_t *source)
{
    if (!source || !audioSourceCombo_)
        return;

    const char *name =
        obs_source_get_name(source);

    audioSourceCombo_->addItem(
        name
            ? QString::fromUtf8(name)
            : "Fonte de áudio",
        QVariant::fromValue(
            static_cast<quintptr>(
                reinterpret_cast<uintptr_t>(source)
            )
        )
    );
}

void PFLDock::AddPreviewAudioSource(obs_source_t *source)
{
    if (!source || !audioEngine_)
        return;

    // Só conecta a fonte ao motor PFL se o PFL estiver ativo.
    if (!pflActive_)
        return;

    blog(
        LOG_INFO,
        "[PFL Preview] >>> ENVIANDO FONTE PARA O MOTOR PFL: '%s'",
        obs_source_get_name(source));

    audioEngine_->AddSource(source);
}

void PFLDock::StartPreviewAudioCapture()
{
    if (!previewScene_)
        return;

    obs_source_add_audio_capture_callback(
        previewScene_,
        AudioCaptureCallback,
        this);

}
void PFLDock::StopPreviewAudioCapture()
{
    for (obs_source_t *source : previewAudioSources_) {
        if (!source)
            continue;

        // Remove a captura do dock
        obs_source_remove_audio_capture_callback(
            source,
            AudioCaptureCallback,
            this);

        // Remove a fonte do motor PFL
        if (audioEngine_) {
            audioEngine_->RemoveSource(source);
        }

        obs_source_release(source);
    }

    previewAudioSources_.clear();

    // Limpa qualquer áudio que ainda esteja na fila do PFL
    if (audioEngine_) {
        audioEngine_->ClearSources();
    }

    if (previewScene_) {
        obs_source_remove_audio_capture_callback(
            previewScene_,
            AudioCaptureCallback,
            this);

        obs_source_release(previewScene_);
        previewScene_ = nullptr;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] Fontes da Preview anterior removidas");
}

static PFLDock *g_pflDock = nullptr;
static obs_hotkey_id g_pflHotkeyId = OBS_INVALID_HOTKEY_ID;

extern "C" void pfl_dock_initialize(void)
{
    if (g_pflDock)
        return;

    g_pflDock = new PFLDock();
g_pflDock->RefreshPreviewScene();

g_pflHotkeyId = obs_hotkey_register_frontend(
    "pfl_preview_toggle",
    "PFL Preview — Ativar/desativar PFL",
    [](void *data, obs_hotkey_id id, obs_hotkey_t *hotkey, bool pressed) {
        Q_UNUSED(id);
        Q_UNUSED(hotkey);

        if (!pressed)
            return;


        PFLDock *dock =
            static_cast<PFLDock *>(data);

        if (!dock)
            return;

        QMetaObject::invokeMethod(
    dock,
    [dock]() {
        dock->TogglePFLFromHotkey();
    },
    Qt::QueuedConnection);

    },
    g_pflDock);

    obs_frontend_add_dock_by_id(
        "pfl-preview",
        "PFL Preview",
        g_pflDock);
}

extern "C" void pfl_dock_update_preview_scene(void)
{
    if (!g_pflDock)
        return;

    obs_source_t *scene =
        obs_frontend_get_current_preview_scene();

    if (scene) {
        const char *name =
            obs_source_get_name(scene);

        blog(LOG_INFO,
            "[PFL Preview] PREVIEW ATUAL: %s",
            name ? name : "(sem nome)");

        obs_source_release(scene);
    } else {
        blog(LOG_INFO,
            "[PFL Preview] PREVIEW ATUAL: nenhuma cena");
    }

    g_pflDock->RefreshPreviewScene();
}

extern "C" void pfl_dock_shutdown(void)
{
    if (g_pflHotkeyId != OBS_INVALID_HOTKEY_ID) {

        obs_hotkey_unregister(
            g_pflHotkeyId);

        g_pflHotkeyId =
            OBS_INVALID_HOTKEY_ID;
    }

    if (!g_pflDock)
        return;

    obs_frontend_remove_dock(
        "pfl-preview");

    delete g_pflDock;
    g_pflDock = nullptr;
}