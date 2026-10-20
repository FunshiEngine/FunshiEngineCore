/*
    FunshiEngineGL - Motor de juegos 3D con API grafica intercambiable
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
#include "BackendImGui.h"

#include "imgui_impl_glfw.h"

// Implementacion sin backend de render.
//
// El motor no implementa ninguna API grafica, asi que no hay backend de ImGui
// que sepa enviar los draw lists a la GPU: la interfaz se calcula (los widgets
// existen, responden y el estado del editor avanza) pero no se dibuja.
//
// El backend de plataforma es el de GLFW, que no depende de la API grafica:
// se inicializa con InitForOther, que engancha teclado, raton y foco sin
// tocar el portapapeles (el de InitForVulkan copia con la API de Vulkan, y el
// de InitForOpenGL depende de un contexto de OpenGL).
namespace {

bool gPlataformaLista = false;
bool gRendererListo = false;

} // namespace

bool MotorImGui::init(void* ventana, bool instalarCallbacks) {
    gRendererListo = false;
    gPlataformaLista = ImGui_ImplGlfw_InitForOther(
        static_cast<GLFWwindow*>(ventana), instalarCallbacks);
    return gPlataformaLista;
}

void MotorImGui::shutdown() {
    if (gPlataformaLista) {
        ImGui_ImplGlfw_Shutdown();
        gPlataformaLista = false;
    }
    gRendererListo = false;
}

void MotorImGui::newFrame() {
    // Sin backend de render no hay frame de render que preparar.
    if (gPlataformaLista) ImGui_ImplGlfw_NewFrame();
}

void MotorImGui::render(const ImDrawData* datos) {
    (void)datos;
    // Sin backend de render los draw lists no van a ningun lado.
}

bool MotorImGui::hayRenderer() {
    return gRendererListo;
}

const char* MotorImGui::nombreRenderer() {
    return gRendererListo ? "nulo" : "ninguno";
}
