// Audio.h
#pragma once
#include <xaudio2.h>
#include <wrl.h>
#include <cstdint>
#include <string>
#include <vector>

#pragma comment(lib, "xaudio2.lib")

// 音声データ構造体
struct SoundData {
    WAVEFORMATEX wfex;
    BYTE* pBuffer;
    unsigned int bufferSize;
};

class AudioManager {
public:
    static AudioManager* GetInstance();

    // 初期化
    void Initialize();
    // 終了処理
    void Finalize();

    // 音声データの読み込み
    SoundData LoadWave(const char* filename);
    // 音声データの解放
    void Unload(SoundData* soundData);
    // 音声再生
    void Play(const SoundData& soundData, bool loop = false);

private:
    AudioManager() = default;
    ~AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    const AudioManager& operator=(const AudioManager&) = delete;

    // RIFFチャンクヘッダ
    struct RiffHeader {
        char chunkID[4]; // "RIFF"
        uint32_t size;
        char format[4]; // "WAVE"
    };

    // チャンクヘッダ
    struct ChunkHeader {
        char id[4];
        uint32_t size;
    };

    Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
    IXAudio2MasteringVoice* masterVoice_ = nullptr;
};