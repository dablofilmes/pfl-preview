#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mutex>
#include <deque>
#include <thread>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

#include <obs.h>

class PFLAudioEngine {
public:
    PFLAudioEngine();
    ~PFLAudioEngine();

    PFLAudioEngine(const PFLAudioEngine &) = delete;
    PFLAudioEngine &operator=(const PFLAudioEngine &) = delete;

    bool Start();
    void Stop();

    void SetVolume(float volume);
    void SetMuted(bool muted);

    void AddSource(obs_source_t *source);
    void RemoveSource(obs_source_t *source);
    void ClearSources();

    bool IsRunning() const;

    static void AudioCallback(
        void *param,
        obs_source_t *source,
        const struct audio_data *audio_data,
        bool muted);

private:
    struct AudioFrame {
        std::vector<float> left;
        std::vector<float> right;
        uint64_t frames = 0;
        uint32_t sampleRate = 0;
    };

    std::mutex audioMutex_;
    std::deque<AudioFrame> audioQueue_;
std::thread renderThread_;
std::atomic<bool> renderRunning_{false};

    static constexpr size_t MAX_QUEUE_FRAMES = 48000;
    size_t queuedFrames_ = 0;

#ifdef _WIN32
    IMMDeviceEnumerator *deviceEnumerator_ = nullptr;
    IMMDevice *audioDevice_ = nullptr;
    IAudioClient *audioClient_ = nullptr;
    IAudioRenderClient *renderClient_ = nullptr;

    HANDLE audioEvent_ = nullptr;
    UINT32 audioBufferFrames_ = 0;
#endif

void RenderThread();
    struct SourceEntry {
        obs_source_t *source = nullptr;
    };

    std::vector<SourceEntry> sources_;

    bool running_ = false;
    bool muted_ = false;
    float volume_ = 0.70f;
};