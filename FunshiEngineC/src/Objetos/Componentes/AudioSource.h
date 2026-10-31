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
#ifndef AUDIOSOURCE_H
#define AUDIOSOURCE_H

#include "Component.h"

#include <string>

class GameObject;
class AudioEngine;

// Fuente de sonido como Component: se adjunta a un GameObject y reproduce un
// clip del proyecto (por NOMBRE). El motor de audio lo inyecta la escena y el
// inspector lo comparte con el componente en modo editor (antes de entrar en
// play), para que se pueda probar sin arrancar la escena. La reproduccion real
// corre en el hilo de audio del motor; aqui solo se guarda el clip, el volumen,
// el loop y el estado del playback.
//
// Por defecto el sonido es global (2D): suena igual sin importar donde este el
// objeto. Con `espacial = true` el sonido es local (3D): se sitúa en la
// posicion del objeto y se escucha segun la distancia a la camara activa.
class AudioSource : public Component {
private:
    std::string clip;              // nombre del clip registrado en la escena
    float volume = 1.0f;
    bool loop = false;
    bool reproduccionAutomatica = false; // se reproduce al entrar en play
    bool espacial = false;        // false = global (2D); true = local (3D)
    int handleActual = -1;         // handle de AudioEngine::reproducir
    bool sonando = false;
    AudioEngine* motor = nullptr;  // inyectado por GameScene

protected:
    void serializeComponent(std::ofstream* file) override;
    void deserializeComponent(std::ifstream* file) override;

public:
    AudioSource() = default;

    void saveComponent(std::ofstream* file) override;
    void loadComponent(std::ifstream* file) override;

    void setMotor(AudioEngine* m) noexcept { motor = m; }
    AudioEngine* getMotor() const noexcept { return motor; }

    const std::string& getClip() const noexcept { return clip; }
    void setClip(const std::string& nombre) noexcept { clip = nombre; }

    float getVolume() const noexcept { return volume; }
    void setVolume(float v) noexcept { volume = v; }

    bool isLoop() const noexcept { return loop; }
    void setLoop(bool b) noexcept { loop = b; }

    bool isReproduccionAutomatica() const noexcept { return reproduccionAutomatica; }
    void setReproduccionAutomatica(bool b) noexcept { reproduccionAutomatica = b; }

    // Modo espacial (3D) o global (2D).
    bool isEspacial() const noexcept { return espacial; }
    void setEspacial(bool b) noexcept { espacial = b; }

    // Reposiciona el sonido en la posicion actual del objeto (se llama cada
    // frame desde GameObject::update mientras suena).
    void onUpdate(GameObject& owner, float deltaTime) override;

    // Reprograma el handle si el clip cambio y el motor ya no tiene el anterior.
    void reproducir();
    void detener();
    bool isReproduciendo() const noexcept { return sonando; }
};

#endif // AUDIOSOURCE_H
