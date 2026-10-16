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

// Pruebas headless del runtime de interfaces web (WebUIRuntime) y del gestor de
// vistas (VistasWebUI). Corren enteras con el backend nulo (sin CEF ni GL):
//   - seleccion del backend por configuracion y fallback (un nombre que no
//     existe no deja al motor sin backend),
//   - ciclo de vida completo de una vista sin GPU (cargar/redimensionar/
//     actualizar/render/input/descargar),
//   - el gestor crea una vista por interfaz activa, la recarga si cambia el
//     .html, la redimensiona con el contenedor y la descarga al desaparecer.

#include <iostream>
#include <string>
#include <vector>

#include "../FunshiEngineGL/src/Objetos/Componentes/InterfaceComponent.h"
#include "../FunshiEngineGL/src/WebUI/VistaWebUI.h"
#include "../FunshiEngineGL/src/WebUI/VistasWebUI.h"
#include "../FunshiEngineGL/src/WebUI/WebUIRuntime.h"

namespace {

int total = 0;
int fallos = 0;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        ++total;                                                              \
        if (!(cond)) {                                                        \
            ++fallos;                                                         \
            std::cout << "FALLO: " << msg << " (linea " << __LINE__ << ")"    \
                      << std::endl;                                           \
        }                                                                     \
    } while (0)

// Seleccion por configuracion y fallback: sin backend real el activo es el
// nulo; pedir uno inexistente no cambia la seleccion ni deja al motor sin
// backend.
void seleccionDeBackend() {
    CHECK(std::string(WebUIRuntime::nombreBackendActivo()) == "null",
          "sin backend real el activo es el nulo");
    CHECK(!WebUIRuntime::soporteBackendReal(),
          "no hay backend web real compilado");

    CHECK(!WebUIRuntime::seleccionarBackend("cef"),
          "un backend inexistente no se selecciona");
    CHECK(std::string(WebUIRuntime::nombreBackendActivo()) == "null",
          "la seleccion no cambia tras un nombre invalido");
    CHECK(WebUIRuntime::seleccionarBackend("null"),
          "el backend nulo siempre es seleccionable");
}

// Ciclo de vida de una vista con el backend nulo: carga, redimensiona, avanza,
// no entrega textura, acepta input sin efecto y se descarga.
void cicloDeVidaSinGL() {
    std::string error;
    VistaWebUI vista = WebUIRuntime::cargar("UI/hud.html", 800, 600, error);
    CHECK(vista.cargada, "la carga con el backend nulo tiene exito");
    CHECK(vista.valida(), "la vista cargada es valida");
    CHECK(vista.ruta == "UI/hud.html", "la vista guarda la ruta del .html");
    CHECK(vista.ancho == 800 && vista.alto == 600,
          "la vista guarda el area inicial");
    CHECK(vista.textura == 0, "el backend nulo no entrega textura");
    CHECK(vista.handleImGui == nullptr,
          "el backend nulo no entrega descriptor para ImGui");
    CHECK(error.empty(), "no hay error con el backend nulo");

    WebUIRuntime::redimensionar(vista, 1024, 768);
    CHECK(vista.ancho == 1024 && vista.alto == 768,
          "redimensionar actualiza el area");

    WebUIRuntime::actualizar(vista, 0.016f);
    CHECK(WebUIRuntime::render(vista) == 0,
          "render no produce textura con el backend nulo");

    // Input: no debe tener efecto ni romper.
    WebUIRuntime::ratonMover(vista, 10, 20);
    WebUIRuntime::ratonBoton(vista, 0, true);
    WebUIRuntime::ratonBoton(vista, 1, false);
    WebUIRuntime::ratonRueda(vista, 1.0f);
    WebUIRuntime::tecla(vista, 65, true, false);
    WebUIRuntime::foco(vista, true);
    CHECK(vista.cargada, "la vista sigue cargada tras el input");

    WebUIRuntime::descargar(vista);
    CHECK(!vista.cargada, "descargar invalida la vista");
}

// Gestor de vistas: crea una por interfaz, recarga al cambiar el .html,
// redimensiona con el contenedor y descarga las que ya no estan.
void gestorDeVistas() {
    InterfaceComponent c1;
    c1.setRutaHtml("UI/a.html");
    InterfaceComponent c2;
    c2.setRutaHtml("UI/b.html");

    VistasWebUI vistas;
    std::vector<InterfaceComponent*> lista{&c1, &c2};
    vistas.sincronizar(lista, 800, 600, 0.016f);

    CHECK(vistas.buscar(&c1) != nullptr, "se crea la vista de la primera interfaz");
    CHECK(vistas.buscar(&c2) != nullptr, "se crea la vista de la segunda interfaz");
    CHECK(vistas.buscar(&c1)->ruta == "UI/a.html", "la vista usa la ruta del .html");

    // Cambio de .html: la vista se rehace con la ruta nueva.
    c1.setRutaHtml("UI/otra.html");
    vistas.sincronizar(std::vector<InterfaceComponent*>{&c1, &c2}, 800, 600,
                       0.016f);
    CHECK(vistas.buscar(&c1)->ruta == "UI/otra.html",
          "cambiar la ruta del componente recarga la vista");

    // Una interfaz sale de la lista: su vista se descarga.
    std::vector<InterfaceComponent*> soloC1{&c1};
    vistas.sincronizar(soloC1, 800, 600, 0.016f);
    CHECK(vistas.buscar(&c2) == nullptr,
          "la interfaz que sale de la lista descarga su vista");
    CHECK(vistas.buscar(&c1) != nullptr, "la que sigue activa conserva su vista");

    // Cambia el contenedor: la vista de tamano automatico se redimensiona.
    vistas.sincronizar(soloC1, 1280, 720, 0.016f);
    CHECK(vistas.buscar(&c1)->ancho == 1280 && vistas.buscar(&c1)->alto == 720,
          "el contenedor nuevo redimensiona la vista automatica");

    // Tamano fijo: la vista usa el tamano del componente, no el contenedor.
    InterfaceComponent fijo;
    fijo.setRutaHtml("UI/fijo.html");
    fijo.setTamanoAuto(false);
    fijo.setTamano(glm::vec2(200.0f, 100.0f));
    std::vector<InterfaceComponent*> conFijo{&fijo};
    vistas.sincronizar(conFijo, 1280, 720, 0.016f);
    CHECK(vistas.buscar(&fijo)->ancho == 200 && vistas.buscar(&fijo)->alto == 100,
          "la vista de tamano fijo usa el tamano del componente");

    vistas.vaciar();
    CHECK(vistas.buscar(&c1) == nullptr && vistas.buscar(&fijo) == nullptr,
          "vaciar descarga todas las vistas");
}

} // namespace

int main() {
    seleccionDeBackend();
    cicloDeVidaSinGL();
    gestorDeVistas();

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": " << total
              << " comprobaciones" << std::endl;
    return fallos == 0 ? 0 : 1;
}
