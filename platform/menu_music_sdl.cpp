// iOS port of menu_music.cpp: the menus' music through SDL3 audio instead of
// Windows MCI. The same files (third_party/menu/music.mp3 or .wav of your own,
// else res/menu/music.mp3), MP3 decoded with minimp3, looped, with the same
// fade and volume curve.
#include "menu_music.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include "minimp3.h"

namespace {
SDL_AudioStream* g_music = nullptr;
std::vector<uint8_t> g_pcm;   // the whole tune, in g_spec's format
SDL_AudioSpec g_spec{};

bool load_mp3(const char* path) {
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) return false;
    size_t size = 0;
    void* data = SDL_LoadFile_IO(io, &size, true);
    if (!data) return false;
    static mp3dec_t dec;
    mp3dec_init(&dec);
    std::vector<float> pcm;
    mp3d_sample_t frame[MINIMP3_MAX_SAMPLES_PER_FRAME];
    const auto* p = static_cast<const uint8_t*>(data);
    size_t pos = 0;
    int channels = 0, hz = 0;
    while (pos < size) {
        mp3dec_frame_info_t info{};
        int samples = mp3dec_decode_frame(&dec, p + pos, int(std::min<size_t>(size - pos, 1 << 20)), frame, &info);
        if (info.frame_bytes <= 0) break;
        pos += size_t(info.frame_bytes);
        if (samples <= 0) continue;   // (an ID3 tag or a frame it skipped)
        if (!channels) channels = info.channels, hz = info.hz;
        if (info.channels != channels) continue;
        pcm.insert(pcm.end(), frame, frame + samples * channels);
    }
    SDL_free(data);
    if (pcm.empty() || channels <= 0 || hz <= 0) return false;
    g_spec = {SDL_AUDIO_F32, channels, hz};
    const auto* b = reinterpret_cast<const uint8_t*>(pcm.data());
    g_pcm.assign(b, b + pcm.size() * sizeof(float));
    return true;
}

bool load_wav(const char* path) {
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if (!SDL_LoadWAV(path, &g_spec, &buf, &len)) return false;
    g_pcm.assign(buf, buf + len);
    SDL_free(buf);
    return true;
}
}  // namespace

bool MenuMusic::open() {
    if (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && !SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;
    for (const char* path : {"third_party/menu/music.mp3", "third_party/menu/music.wav", "res/menu/music.mp3"}) {
        std::string p = path;
        bool ok = p.size() > 4 && p.compare(p.size() - 4, 4, ".wav") == 0 ? load_wav(path) : load_mp3(path);
        if (!ok) continue;
        // Topped up as it plays: a loop
        auto loop = [](void*, SDL_AudioStream* s, int additional, int) {
            for (int guard = 0; guard < 4 && !g_pcm.empty() && SDL_GetAudioStreamQueued(s) < additional; guard++)
                SDL_PutAudioStreamData(s, g_pcm.data(), int(g_pcm.size()));
        };
        g_music = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &g_spec, loop, nullptr);
        if (!g_music) {
            fprintf(stderr, "[menu] could not play the music: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetAudioStreamGain(g_music, 0);
        printf("[menu] music %s\n", path);
        open_ = true;
        return true;
    }
    return false;
}

void MenuMusic::close() {
    if (g_music) SDL_DestroyAudioStream(g_music);
    g_music = nullptr;
    g_pcm.clear();
    open_ = playing_ = false;
}

void MenuMusic::update(bool want, int volume, double dt) {
    if (!open_) return;
    // A second to come in, half a second to go when the game starts
    level_ = want ? std::min(1.0f, level_ + float(dt) / 1.0f) : std::max(0.0f, level_ - float(dt) / 0.5f);
    if (want && !playing_) {
        sent_ = -1;
        playing_ = SDL_ResumeAudioStreamDevice(g_music);
    }
    if (!playing_) return;
    if (level_ <= 0) {   // faded out: it waits where it was
        SDL_PauseAudioStreamDevice(g_music);
        playing_ = false;
        return;
    }
    float g = std::sqrt(std::clamp(float(volume) / 10.0f, 0.0f, 1.0f));
    int v = int(std::clamp(level_ * g, 0.0f, 1.0f) * 1000.0f + 0.5f);
    if (v != sent_) {
        SDL_SetAudioStreamGain(g_music, float(v) / 1000.0f);
        sent_ = v;
    }
}
