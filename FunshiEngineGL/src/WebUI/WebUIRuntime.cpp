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
#include "WebUIRuntime.h"

#include <map>
#include <memory>

#include "BackendNull.h"
#include "IWebUIBackend.h"

#if defined(FUNSHI_WEBUI)
// Aqui se registran los backends web reales (p. ej. BackendCef) cuando el
// motor se compila con FUNSHI_WEBUI y sus dependencias. Mientras no exista
// ninguno, el runtime queda con el backend nulo.
#endif

namespace {

// Instancias unicas de backend, compartidas por el motor y la GUI (mismo rol
// que el registro de ScriptRuntime). El backend nulo siempre esta: es el
// fallback que garantiza que el runtime nunca quede sin backend.
std::map<std::string, std::unique_ptr<IWebUIBackend>>& registro() {
    static std::map<std::string, std::unique_ptr<IWebUIBackend>> registro;
    if (registro.empty()) {
        registro.insert({"null", std::make_unique<BackendNullWebUI>()});
#if defined(FUNSHI_WEBUI)
        // registro.insert({"cef", std::make_unique<BackendCef>()});
#endif
    }
    return registro;
}

// Nombre del backend elegido al arrancar: el primer backend real disponible o,
// si no hay, el nulo. Se calcula una vez.
std::string& backendSeleccionado() {
    static std::string seleccionado = [] {
        for (const auto& par : registro())
            if (par.first != "null") return par.first;
        return std::string("null");
    }();
    return seleccionado;
}

} // namespace

bool WebUIRuntime::soporteBackendReal() {
    for (const auto& par : registro())
        if (par.first != "null") return true;
    return false;
}

const char* WebUIRuntime::nombreBackendActivo() {
    return backendSeleccionado().c_str();
}

bool WebUIRuntime::seleccionarBackend(const std::string& nombre) {
    if (registro().find(nombre) == registro().end()) return false;
    backendSeleccionado() = nombre;
    return true;
}

IWebUIBackend& WebUIRuntime::backendActivo() {
    auto& r = registro();
    auto it = r.find(backendSeleccionado());
    if (it == r.end()) it = r.find("null"); // el nulo siempre existe
    return *it->second;
}

VistaWebUI WebUIRuntime::cargar(const std::string& rutaHtml, int ancho, int alto,
                                std::string& error) {
    VistaWebUI vista;
    backendActivo().cargar(vista, rutaHtml, ancho, alto, error);
    return vista;
}

void WebUIRuntime::redimensionar(VistaWebUI& vista, int ancho, int alto) {
    backendActivo().redimensionar(vista, ancho, alto);
}

void WebUIRuntime::actualizar(VistaWebUI& vista, float deltaTime) {
    backendActivo().actualizar(vista, deltaTime);
}

void WebUIRuntime::establecerEstado(VistaWebUI& vista,
                                    const std::string& jsonEstado) {
    backendActivo().establecerEstado(vista, jsonEstado);
}

std::string WebUIRuntime::leerSalida(VistaWebUI& vista) {
    return backendActivo().leerSalida(vista);
}

std::uint64_t WebUIRuntime::render(VistaWebUI& vista) {
    return backendActivo().render(vista);
}

void WebUIRuntime::ratonMover(VistaWebUI& vista, int x, int y) {
    backendActivo().ratonMover(vista, x, y);
}

void WebUIRuntime::ratonBoton(VistaWebUI& vista, int boton, bool abajo) {
    BotonRatonWebUI b = BotonRatonWebUI::Izquierdo;
    switch (boton) {
    case 1: b = BotonRatonWebUI::Derecho; break;
    case 2: b = BotonRatonWebUI::Medio; break;
    default: b = BotonRatonWebUI::Izquierdo; break;
    }
    backendActivo().ratonBoton(vista, b, abajo);
}

void WebUIRuntime::ratonRueda(VistaWebUI& vista, float delta) {
    backendActivo().ratonRueda(vista, delta);
}

void WebUIRuntime::tecla(VistaWebUI& vista, int codigo, bool abajo,
                         bool repetir) {
    backendActivo().tecla(vista, codigo, abajo, repetir);
}

void WebUIRuntime::foco(VistaWebUI& vista, bool activo) {
    backendActivo().foco(vista, activo);
}

void WebUIRuntime::descargar(VistaWebUI& vista) {
    backendActivo().descargar(vista);
}

void WebUIRuntime::apagar() {
    for (auto& par : registro()) par.second->apagar();
}
