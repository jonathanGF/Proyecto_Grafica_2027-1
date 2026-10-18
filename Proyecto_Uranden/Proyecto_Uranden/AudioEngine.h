#pragma once

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <iostream>
#include <string>

class AudioEngine {
public:
    ma_engine engine;
    ma_sound music;
    ma_sound ambient;
    ma_sound voiceover;

    bool initialized;
    bool musicLoaded;
    bool ambientLoaded;
    bool voiceLoaded;

    AudioEngine()
        : initialized(false), musicLoaded(false), ambientLoaded(false), voiceLoaded(false) {
    }

    bool init() {
        ma_result result = ma_engine_init(NULL, &engine);
        if (result != MA_SUCCESS) {
            std::cout << "ERROR::MINIAUDIO::No se pudo inicializar el motor de audio." << std::endl;
            return false;
        }
        initialized = true;
        return true;
    }

    // 1. Música de fondo (con bucle y volumen configurable)
    bool playMusic(const std::string& filepath, float volume = 0.5f, bool loop = true) {
        if (!initialized) return false;

        if (musicLoaded) {
            ma_sound_uninit(&music);
            musicLoaded = false;
        }

        ma_result result = ma_sound_init_from_file(&engine, filepath.c_str(), 0, NULL, NULL, &music);
        if (result != MA_SUCCESS) {
            std::cout << "ERROR::MINIAUDIO::No se pudo cargar la musica: " << filepath << std::endl;
            return false;
        }

        ma_sound_set_looping(&music, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&music, volume);
        ma_sound_start(&music);
        musicLoaded = true;
        return true;
    }

    // 2. Sonido ambiente (agua, viento, noche)
    bool playAmbient(const std::string& filepath, float volume = 0.3f, bool loop = true) {
        if (!initialized) return false;

        if (ambientLoaded) {
            ma_sound_uninit(&ambient);
            ambientLoaded = false;
        }

        ma_result result = ma_sound_init_from_file(&engine, filepath.c_str(), 0, NULL, NULL, &ambient);
        if (result != MA_SUCCESS) {
            std::cout << "ERROR::MINIAUDIO::No se pudo cargar el ambiente: " << filepath << std::endl;
            return false;
        }

        ma_sound_set_looping(&ambient, loop ? MA_TRUE : MA_FALSE);
        ma_sound_set_volume(&ambient, volume);
        ma_sound_start(&ambient);
        ambientLoaded = true;
        return true;
    }

    // 3. Locución / Narración (normalmente de un solo disparo)
    bool playVoiceover(const std::string& filepath, float volume = 1.0f) {
        if (!initialized) return false;

        if (voiceLoaded) {
            ma_sound_uninit(&voiceover);
            voiceLoaded = false;
        }

        ma_result result = ma_sound_init_from_file(&engine, filepath.c_str(), 0, NULL, NULL, &voiceover);
        if (result != MA_SUCCESS) {
            std::cout << "ERROR::MINIAUDIO::No se pudo cargar la locucion: " << filepath << std::endl;
            return false;
        }

        ma_sound_set_looping(&voiceover, MA_FALSE);
        ma_sound_set_volume(&voiceover, volume);
        ma_sound_start(&voiceover);
        voiceLoaded = true;
        return true;
    }

    void cleanup() {
        if (musicLoaded)   ma_sound_uninit(&music);
        if (ambientLoaded) ma_sound_uninit(&ambient);
        if (voiceLoaded)   ma_sound_uninit(&voiceover);
        if (initialized)   ma_engine_uninit(&engine);
        initialized = false;
    }

    ~AudioEngine() {
        cleanup();
    }
};