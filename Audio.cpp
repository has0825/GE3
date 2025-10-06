// Audio.cpp
#include "Audio.h"
#include <fstream>
#include <cassert>

AudioManager* AudioManager::GetInstance() {
    static AudioManager instance;
    return &instance;
}

void AudioManager::Initialize() {
    HRESULT hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
    assert(SUCCEEDED(hr));
    hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
    assert(SUCCEEDED(hr));
}

void AudioManager::Finalize() {
    if (masterVoice_) {
        masterVoice_->DestroyVoice();
        masterVoice_ = nullptr;
    }
    xAudio2_.Reset();
}

SoundData AudioManager::LoadWave(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    assert(file.is_open());

    RiffHeader riff;
    file.read(reinterpret_cast<char*>(&riff), sizeof(riff));
    assert(strncmp(riff.chunkID, "RIFF", 4) == 0);
    assert(strncmp(riff.format, "WAVE", 4) == 0);

    SoundData soundData = {};
    ChunkHeader chunk;

    while (file.read(reinterpret_cast<char*>(&chunk), sizeof(chunk))) {
        if (strncmp(chunk.id, "fmt ", 4) == 0) {
            file.read(reinterpret_cast<char*>(&soundData.wfex), chunk.size);
        } else if (strncmp(chunk.id, "data", 4) == 0) {
            soundData.pBuffer = new BYTE[chunk.size];
            soundData.bufferSize = chunk.size;
            file.read(reinterpret_cast<char*>(soundData.pBuffer), chunk.size);
            break; // dataチャンクを読んだら終了
        } else {
            file.seekg(chunk.size, std::ios_base::cur);
        }
    }

    return soundData;
}

void AudioManager::Unload(SoundData* soundData) {
    delete[] soundData->pBuffer;
    soundData->pBuffer = nullptr;
    soundData->bufferSize = 0;
    soundData->wfex = {};
}

void AudioManager::Play(const SoundData& soundData, bool loop) {
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    HRESULT hr = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
    assert(SUCCEEDED(hr));

    XAUDIO2_BUFFER buffer = {};
    buffer.pAudioData = soundData.pBuffer;
    buffer.AudioBytes = soundData.bufferSize;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    if (loop) {
        buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
    }

    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    assert(SUCCEEDED(hr));
    hr = pSourceVoice->Start(0);
    assert(SUCCEEDED(hr));
}