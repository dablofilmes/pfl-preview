#pragma once

#include <QWidget>
#include <QString>

#include <obs.h>

#include <vector>
#include <QTimer>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class PFLAudioEngine;

class PFLLevelMeter : public QWidget {
    Q_OBJECT

public:
    explicit PFLLevelMeter(QWidget *parent = nullptr);

    void SetLevels(
    float magnitudeL,
    float peakL,
    float inputPeakL,
    float magnitudeR,
    float peakR,
    float inputPeakR);
void SetMuted(bool muted);
void SetPFLActive(bool active);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    float magnitudeDb_[2] = {-60.0f, -60.0f};
float displayedMagnitudeDb_[2] = {-60.0f, -60.0f};
float peakDb_[2] = {-60.0f, -60.0f};
float inputPeakDb_[2] = {-60.0f, -60.0f};

    bool muted_ = false;
bool pflActive_ = false;
bool clipped_ = false;
    QTimer *peakTimer_ = nullptr;

    int silenceTicks_ = 0;
};


class PFLDock : public QWidget {
    Q_OBJECT

public:
    explicit PFLDock(QWidget *parent = nullptr);
    ~PFLDock() override;
void TogglePFLFromHotkey();

    void AddPreviewAudioSource(obs_source_t *source);
void AddAudioSourceToList(obs_source_t *source);

    void UpdatePreviewScene(const QString &sceneName);
    void UpdateLevel(float db);
    void RefreshPreviewScene();

    static void AudioCaptureCallback(
        void *param,
        obs_source_t *source,
        const struct audio_data *audio_data,
        bool muted);

static void VolumeMeterCallback(
	void *param,
	const float magnitude[MAX_AUDIO_CHANNELS],
	const float peak[MAX_AUDIO_CHANNELS],
	const float inputPeak[MAX_AUDIO_CHANNELS]);

    std::vector<obs_source_t *> previewAudioSources_;


private slots:
    void TogglePFL();
    void ToggleMute();
    void ChangeVolume(int value);
    void ChangeAudioSource(int index);


private:
    void StartPreviewAudioCapture();
    void StopPreviewAudioCapture();
void StartVolumeMeter();
void StopVolumeMeter();
QString GetSettingsPath() const;
void CheckForUpdates();

    PFLAudioEngine *audioEngine_ = nullptr;
obs_volmeter_t *volumeMeter_ = nullptr;
obs_fader_t *volumeFader_ = nullptr;

    QLabel *sceneLabel_ = nullptr;
QLabel *volumeLabel_ = nullptr;
    QComboBox *audioSourceCombo_ = nullptr;

QPushButton *pflListenButton_ = nullptr;
QPushButton *muteButton_ = nullptr;
QPushButton *updateButton_ = nullptr;
    QSlider *volumeSlider_ = nullptr;

    PFLLevelMeter *levelMeter_ = nullptr;

    obs_source_t *previewScene_ = nullptr;
    obs_source_t *selectedAudioSource_ = nullptr;

    bool pflActive_ = false;
    bool muted_ = false;
    float volume_ = 0.70f;
};