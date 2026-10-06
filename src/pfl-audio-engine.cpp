#define NOMINMAX

#include "pfl-audio-engine.hpp"

#include <algorithm>
#include <cmath>

#include <util/platform.h>

#ifdef _WIN32
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

PFLAudioEngine::PFLAudioEngine() = default;

PFLAudioEngine::~PFLAudioEngine()
{
    Stop();
}

bool PFLAudioEngine::Start()
{
    if (running_)
        return true;

    running_ = true;

#ifdef _WIN32

    const char *deviceName = nullptr;
    const char *deviceId = nullptr;

    obs_get_audio_monitoring_device(
        &deviceName,
        &deviceId);

    blog(
        LOG_INFO,
        "[PFL Preview] Dispositivo de monitoramento do OBS: nome='%s' | id='%s'",
        deviceName ? deviceName : "(nenhum)",
        deviceId ? deviceId : "(nenhum)");

    if (!deviceId || !deviceId[0]) {

        blog(
            LOG_WARNING,
            "[PFL Preview] Nenhum dispositivo de monitoramento configurado no OBS");

        running_ = false;
        return false;
    }

    HRESULT hr;

    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator),
        nullptr,
        CLSCTX_ALL,
        IID_PPV_ARGS(&deviceEnumerator_));

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Erro ao criar MMDeviceEnumerator: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    if (strcmp(deviceId, "default") == 0) {

        hr = deviceEnumerator_->GetDefaultAudioEndpoint(
            eRender,
            eConsole,
            &audioDevice_);

    } else {

        int length = MultiByteToWideChar(
            CP_UTF8,
            0,
            deviceId,
            -1,
            nullptr,
            0);

        if (length <= 0) {

            blog(
                LOG_ERROR,
                "[PFL Preview] Não foi possível converter o ID do dispositivo");

            running_ = false;
            return false;
        }

        std::vector<wchar_t> wideId(
            static_cast<size_t>(length));

        MultiByteToWideChar(
            CP_UTF8,
            0,
            deviceId,
            -1,
            wideId.data(),
            length);

        hr = deviceEnumerator_->GetDevice(
            wideId.data(),
            &audioDevice_);
    }

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Não foi possível abrir o dispositivo de monitoramento: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] Dispositivo WASAPI localizado com sucesso");

    hr = audioDevice_->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        nullptr,
        reinterpret_cast<void **>(&audioClient_));

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Não foi possível ativar IAudioClient: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] IAudioClient ativado com sucesso");


    WAVEFORMATEX *deviceFormat = nullptr;

    hr = audioClient_->GetMixFormat(
        &deviceFormat);

    if (FAILED(hr) || !deviceFormat) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Não foi possível obter o formato do dispositivo: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] Formato WASAPI: %u Hz | %u canais | %u bits | tag=0x%04X",
        deviceFormat->nSamplesPerSec,
        deviceFormat->nChannels,
        deviceFormat->wBitsPerSample,
        deviceFormat->wFormatTag);

    hr = audioClient_->Initialize(
    AUDCLNT_SHAREMODE_SHARED,
    AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM,
    0,
    0,
    deviceFormat,
    nullptr);

CoTaskMemFree(deviceFormat);

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Falha ao inicializar WASAPI: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] IAudioClient inicializado com sucesso");

    hr = audioClient_->GetService(
        __uuidof(IAudioRenderClient),
        reinterpret_cast<void **>(&renderClient_));

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Falha ao criar IAudioRenderClient: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] IAudioRenderClient criado com sucesso");
    hr = audioClient_->GetBufferSize(
        &audioBufferFrames_);

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Falha ao obter tamanho do buffer WASAPI: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] Buffer WASAPI: %u frames",
        audioBufferFrames_);
    hr = audioClient_->Start();

    if (FAILED(hr)) {

        blog(
            LOG_ERROR,
            "[PFL Preview] Falha ao iniciar IAudioClient: 0x%08lX",
            static_cast<unsigned long>(hr));

        running_ = false;
        return false;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] IAudioClient iniciado com sucesso");

    renderRunning_ = true;

    renderThread_ =
        std::thread(
            &PFLAudioEngine::RenderThread,
            this);

    blog(
        LOG_INFO,
        "[PFL Preview] Thread de reprodução WASAPI iniciada");

#endif

    return true;
}

void PFLAudioEngine::Stop()
{
    if (!running_)
        return;

    running_ = false;

#ifdef _WIN32

    renderRunning_ = false;

    if (audioClient_) {
        audioClient_->Stop();
    }

    if (renderThread_.joinable()) {
        renderThread_.join();
    }

    if (renderClient_) {
        renderClient_->Release();
        renderClient_ = nullptr;
    }

    if (audioClient_) {
        audioClient_->Release();
        audioClient_ = nullptr;
    }

    if (audioDevice_) {
        audioDevice_->Release();
        audioDevice_ = nullptr;
    }

    if (deviceEnumerator_) {
        deviceEnumerator_->Release();
        deviceEnumerator_ = nullptr;
    }

    audioBufferFrames_ = 0;

#endif

    blog(
        LOG_INFO,
        "[PFL Preview] Motor de áudio PFL parado");
}

void PFLAudioEngine::SetVolume(float volume)
{
    volume_ = std::clamp(volume, 0.0f, 1.0f);
}

void PFLAudioEngine::SetMuted(bool muted)
{
    muted_ = muted;

    if (muted_) {
        std::lock_guard<std::mutex> lock(audioMutex_);

        audioQueue_.clear();
        queuedFrames_ = 0;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] PFL %s",
        muted_ ? "MUTADO" : "ATIVO");
}

void PFLAudioEngine::AddSource(obs_source_t *source)
{
    if (!source)
        return;

    const auto it = std::find_if(
        sources_.begin(),
        sources_.end(),
        [source](const SourceEntry &entry) {
            return entry.source == source;
        });

    if (it != sources_.end())
        return;

    sources_.push_back({source});

    obs_source_add_audio_capture_callback(
        source,
        AudioCallback,
        this);

    blog(LOG_INFO,
         "[PFL Preview] Motor PFL conectado à fonte '%s'",
         obs_source_get_name(source));
}

void PFLAudioEngine::RemoveSource(obs_source_t *source)
{
    if (!source)
        return;

    auto it = std::find_if(
        sources_.begin(),
        sources_.end(),
        [source](const SourceEntry &entry) {
            return entry.source == source;
        });

    if (it == sources_.end())
        return;

    obs_source_remove_audio_capture_callback(
        source,
        AudioCallback,
        this);

    sources_.erase(it);

    blog(LOG_INFO,
         "[PFL Preview] Motor PFL desconectado da fonte");
}

void PFLAudioEngine::ClearSources()
{
    for (const SourceEntry &entry : sources_) {
        if (!entry.source)
            continue;

        obs_source_remove_audio_capture_callback(
            entry.source,
            AudioCallback,
            this);
    }

    sources_.clear();

    blog(LOG_INFO,
         "[PFL Preview] Fontes do motor PFL limpas");
}

bool PFLAudioEngine::IsRunning() const
{
    return running_;
}

void PFLAudioEngine::AudioCallback(
    void *param,
    obs_source_t *source,
    const struct audio_data *audio_data,
    bool muted)
{
    auto *engine =
        static_cast<PFLAudioEngine *>(param);

    if (!engine ||
        !engine->running_ ||
        engine->muted_ ||
        muted ||
        !audio_data)
        return;

    if (audio_data->frames == 0)
        return;

    const size_t channels = 2;

const size_t frames =
    static_cast<size_t>(audio_data->frames);

if (frames == 0)
    return;

AudioFrame frame;

frame.frames = frames;
struct obs_audio_info audioInfo = {};

if (obs_get_audio_info(&audioInfo))
    frame.sampleRate = audioInfo.samples_per_sec;
else
    frame.sampleRate = 48000;

frame.left.resize(frames);
frame.right.resize(frames);

float peak = 0.0f;

for (size_t i = 0; i < frames; i++) {

    float left = 0.0f;
    float right = 0.0f;

    if (audio_data->data[0])
        left = reinterpret_cast<const float *>(
            audio_data->data[0])[i];

    if (channels > 1 && audio_data->data[1])
        right = reinterpret_cast<const float *>(
            audio_data->data[1])[i];

    left *= engine->volume_;
right *= engine->volume_;

    frame.left[i] = left;
    frame.right[i] = right;

    peak = std::max(
        peak,
        std::fabs(left));

    peak = std::max(
        peak,
        std::fabs(right));
}

{
    std::lock_guard<std::mutex> lock(engine->audioMutex_);

    engine->audioQueue_.push_back(
    std::move(frame));

    engine->queuedFrames_ += frames;

    while (engine->queuedFrames_ > MAX_QUEUE_FRAMES &&
       !engine->audioQueue_.empty()) {

    engine->queuedFrames_ -=
        engine->audioQueue_.front().frames;

    engine->audioQueue_.pop_front();
}
}

if (peak > 0.000001f) {

    const float db =
        20.0f * std::log10(peak);

    blog(LOG_DEBUG,
         "[PFL Preview] PFL recebeu áudio — fonte='%s' %.1f dB",
         source ? obs_source_get_name(source) : "(sem fonte)",
         db);
}

}

void PFLAudioEngine::RenderThread()
{
#ifdef _WIN32

    HRESULT hr = CoInitializeEx(
        nullptr,
        COINIT_MULTITHREADED);
bool comInitialized = SUCCEEDED(hr);

    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {

        blog(
            LOG_ERROR,
            "[PFL Preview] RenderThread: falha no COM: 0x%08lX",
            static_cast<unsigned long>(hr));

        renderRunning_ = false;
        return;
    }

    blog(
        LOG_INFO,
        "[PFL Preview] Thread de áudio iniciada");

    while (renderRunning_) {

        UINT32 padding = 0;

        hr = audioClient_->GetCurrentPadding(
            &padding);

        if (FAILED(hr)) {

            blog(
                LOG_ERROR,
                "[PFL Preview] GetCurrentPadding falhou: 0x%08lX",
                static_cast<unsigned long>(hr));

            break;
        }

        UINT32 available =
            audioBufferFrames_ - padding;

        if (available == 0) {

            Sleep(2);
            continue;
        }

        BYTE *buffer = nullptr;

        hr = renderClient_->GetBuffer(
            available,
            &buffer);

        if (FAILED(hr)) {

            blog(
                LOG_ERROR,
                "[PFL Preview] GetBuffer falhou: 0x%08lX",
                static_cast<unsigned long>(hr));

            break;
        }

        float *output =
            reinterpret_cast<float *>(buffer);

        UINT32 framesToWrite = available;

        {
            std::lock_guard<std::mutex> lock(
                audioMutex_);

            UINT32 written = 0;

            while (written < framesToWrite &&
                   !audioQueue_.empty()) {

                AudioFrame &frame =
                    audioQueue_.front();

                size_t frameOffset = 0;

                size_t remaining =
                    static_cast<size_t>(
                        frame.frames);

                size_t count =
                    std::min(
                        remaining,
                        static_cast<size_t>(
                            framesToWrite - written));

                for (size_t i = 0; i < count; ++i) {

                    output[
                        (written + i) * 2
                    ] = frame.left[
                        frameOffset + i];

                    output[
                        (written + i) * 2 + 1
                    ] = frame.right[
                        frameOffset + i];
                }

                written +=
                    static_cast<UINT32>(count);

                frameOffset += count;

                if (count >= frame.frames) {

                    queuedFrames_ -=
                        frame.frames;

                    audioQueue_.pop_front();

                } else {

                    frame.left.erase(
                        frame.left.begin(),
                        frame.left.begin() + count);

                    frame.right.erase(
                        frame.right.begin(),
                        frame.right.begin() + count);

                    frame.frames -= count;

                    queuedFrames_ -= count;

                    break;
                }
            }

            while (written < framesToWrite) {

                output[written * 2] = 0.0f;
                output[written * 2 + 1] = 0.0f;

                ++written;
            }
        }

        hr = renderClient_->ReleaseBuffer(
            framesToWrite,
            0);

        if (FAILED(hr)) {

            blog(
                LOG_ERROR,
                "[PFL Preview] ReleaseBuffer falhou: 0x%08lX",
                static_cast<unsigned long>(hr));

            break;
        }
    }

if (comInitialized)
    CoUninitialize();

    blog(
        LOG_INFO,
        "[PFL Preview] Thread de áudio encerrada");

#endif
}