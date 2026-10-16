/*
    FunshiEngineGL - Motor de juegos 3D con OpenGL e ImGui
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
#ifndef VISTAWEBUI_H
#define VISTAWEBUI_H

#include <cstdint>
#include <string>

// Una interfaz web ya montada por un backend: el resultado de cargar un .html.
// Es el analogo de ComportamientoCargado para el subsistema web: un valor que
// el motor pasa de vuelta al backend en cada llamada (actualizar/render/input/
// descargar) y que lleva el estado de la vista. El backend guarda su propio
// estado privado detras de punteros opacos; el resto del motor no ve ni el
// motor web ni el handle concreto.
struct VistaWebUI {
    std::string ruta;      // ruta del .html cargado
    std::string backend;   // nombre del backend que la sirve ("null", ...)
    void* manejador = nullptr; // estado interno del backend (opaco)
    // Handle de la textura de la vista en el backend grafico (0 = sin textura
    // todavia). Es un Rendering::Backend::Handle, mantenido como entero para no
    // acoplar el modulo web a la capa de render.
    std::uint64_t textura = 0;
    // Descriptor opaco de la textura para ImGui::Image (ImTextureID). Lo entrega
    // el backend grafico; el motor solo lo reenvia.
    void* handleImGui = nullptr;
    int ancho = 0;         // ancho del area reservada, en pixeles
    int alto = 0;          // alto del area reservada, en pixeles
    bool cargada = false;  // true tras un cargar() exitoso
    std::string error;     // motivo del ultimo fallo, para mostrar en la GUI

    // Una vista sirve para dibujar cuando termino de cargar. Una vista cargada
    // sin textura (backend nulo) no rompe el ciclo: render() devuelve 0 y el
    // overlay simplemente no la dibuja.
    bool valida() const { return cargada; }
};

#endif // VISTAWEBUI_H
