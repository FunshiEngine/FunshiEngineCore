/*
    FunshiEngineC - Motor de juegos 3D con API grafica intercambiable e ImGui
    Copyright 2026 Gianfranco Ivan Enrique

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

    SPDX-License-Identifier: Apache-2.0
*/
#include "MiniAudioBackend.h"

// miniaudio se usa con su implementacion en una UNICA TU (igual que stb_image
// en IconosGUI.cpp): el header trae los decoders WAV/OGG(MP3/FLAC) embebidos.
// El define va ANTES del include para generar la implementacion aqui mismo.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include <utility>
#include <iostream>

struct MiniAudioBackend::Impl {
    ma_engine motor;
    bool inicializado = false;
    int proximoId = 1;
    // id del backend -> sonido activo. miniaudio mezcla en el hilo de su
    // device; los accesos a esta estructura ocurren solo desde el hilo de
    // trabajo del AudioEngine (serializados por su cola).
    struct SonidoActivo {
        ma_sound sonido;
        bool tienePosicion = false;
        float px = 0.f, py = 0.f, pz = 0.f;
    };
    std::unordered_map<int, SonidoActivo> activas;
};

MiniAudioBackend::MiniAudioBackend() : impl_(new Impl()) {}

MiniAudioBackend::~MiniAudioBackend() { cerrar(); delete impl_; }

bool MiniAudioBackend::iniciar() {
    if (impl_->inicializado) return true;
    ma_engine_config cfg = ma_engine_config_init();
    cfg.channels = 2;
    // Sin device explicito: usa el device por defecto del sistema.
    if (ma_engine_init(&cfg, &impl_->motor) != MA_SUCCESS) return false;
    impl_->inicializado = true;
    return true;
}

void MiniAudioBackend::cerrar() {
    if (!impl_->inicializado) return;
    detenerTodo();
    ma_engine_uninit(&impl_->motor);
    impl_->inicializado = false;
}

int MiniAudioBackend::reproducirArchivo(const std::string& ruta, float volumen,
                                        bool bucle, const float* posicion) {
    if (ruta.empty()) return 0;
    // El comando Iniciar se encola de forma asincronica en el ctor de
    // AudioEngine; si el hilo de audio aun no lo ha procesado, inicializar
    // de forma sincronica aqui evita que el primer Reproducir se pierda por
    // condicion de carrera (impl_->inicializado == false).
    if (!impl_->inicializado && !iniciar()) return 0;

    // El ma_sound se construye directo en su slot del mapa: copiar un ma_sound
    // no es valido (su nodo interno queda enlazado al grafo del engine
    // apuntando al objeto original; al salir de la funcion ese original muere y
    // el callback de audio termina leyendo un nodo colgado, SEGV en
    // ma_engine_node_process_pcm_frames__sound).
    const int id = impl_->proximoId++;
    auto& activo = impl_->activas[id];
    ma_sound& sonido = activo.sonido;
    // MA_SOUND_FLAG_DECODE: decodifica el archivo completo a memoria, ideal
    // para efectos cortos: el stream no depende de que el archivo siga
    // abierto en disco y el loop/volumen responden al instante.
    if (ma_sound_init_from_file(&impl_->motor, ruta.c_str(),
                                MA_SOUND_FLAG_DECODE, nullptr, nullptr,
                                &sonido) != MA_SUCCESS) {
        impl_->activas.erase(id);
        std::cerr << "[Audio] MiniAudioBackend::reproducirArchivo: no se pudo "
                     "inicializar sonido desde '" << ruta << "'\n";
        return 0;
    }
    ma_sound_set_volume(&sonido, volumen);
    ma_sound_set_looping(&sonido, bucle ? MA_TRUE : MA_FALSE);
    if (posicion) {
        // Sonido espacial (3D): se ubica en el mundo y se atenuara segun la
        // distancia al listener (la camara activa).
        ma_sound_set_position(&sonido, posicion[0], posicion[1], posicion[2]);
        activo.tienePosicion = true;
        activo.px = posicion[0];
        activo.py = posicion[1];
        activo.pz = posicion[2];
    } else {
        // Sonido global (2D): sin spatializacion, suena igual desde cualquier
        // punto de la escena.
        ma_sound_set_spatialization_enabled(&sonido, MA_FALSE);
    }
    if (ma_sound_start(&sonido) != MA_SUCCESS) {
        ma_sound_uninit(&sonido);
        impl_->activas.erase(id);
        std::cerr << "[Audio] MiniAudioBackend::reproducirArchivo: ma_sound_start "
                     "fallo para '" << ruta << "'\n";
        return 0;
    }
    return id;
}

void MiniAudioBackend::detener(int id) {
    const auto it = impl_->activas.find(id);
    if (it == impl_->activas.end()) return;
    ma_sound_stop(&it->second.sonido);
    ma_sound_uninit(&it->second.sonido);
    impl_->activas.erase(it);
}

void MiniAudioBackend::detenerTodo() {
    for (auto& [id, activo] : impl_->activas) {
        ma_sound_stop(&activo.sonido);
        ma_sound_uninit(&activo.sonido);
    }
    impl_->activas.clear();
}

void MiniAudioBackend::setVolumenMaestro(float volumen) {
    if (impl_->inicializado) ma_engine_set_volume(&impl_->motor, volumen);
}

void MiniAudioBackend::despachar(float) {
    if (impl_->activas.empty()) return;
    for (auto it = impl_->activas.begin(); it != impl_->activas.end();) {
        if (ma_sound_at_end(&it->second.sonido)) {
            ma_sound_uninit(&it->second.sonido);
            it = impl_->activas.erase(it);
        } else {
            ++it;
        }
    }
}

void MiniAudioBackend::setPosicionSonido(int id, float x, float y, float z) {
    const auto it = impl_->activas.find(id);
    if (it == impl_->activas.end()) return;
    // Un sonido al que se le reposiciona es espacial: miniaudio lo crea con
    // spatializacion activa, pero si nacio global (2D) hay que volver a
    // activarla para que se atenue segun la distancia al listener.
    ma_sound_set_spatialization_enabled(&it->second.sonido, MA_TRUE);
    it->second.tienePosicion = true;
    it->second.px = x;
    it->second.py = y;
    it->second.pz = z;
    ma_sound_set_position(&it->second.sonido, x, y, z);
}

void MiniAudioBackend::setListenerPosition(float x, float y, float z) {
    if (impl_->inicializado)
        ma_engine_listener_set_position(&impl_->motor, 0, x, y, z);
}
