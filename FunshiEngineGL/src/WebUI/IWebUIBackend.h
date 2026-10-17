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
#ifndef IWEBUIBACKEND_H
#define IWEBUIBACKEND_H

#include <cstdint>
#include <string>

#include "VistaWebUI.h"

// Boton del raton en los terminos del motor (no los de la plataforma): el
// backend traduce cada uno al codigo de su motor web.
enum class BotonRatonWebUI { Izquierdo, Derecho, Medio };

// Contrato del motor web embebido (patron Strategy, mismo rol que
// IAudioBackend en Audio y BackendScript en Behaviour). El motor delega aqui
// TODA la maquinaria de una interfaz HTML/CSS/JS; un backend concreto (CEF en
// el futuro) se oculta detras de esta interfaz. El backend nulo (BackendNull)
// permite compilar y correr todo el ciclo sin motor web ni GPU.
//
// El estado de cada vista vive en `VistaWebUI` (que el motor conserva y pasa de
// vuelta); el backend no guarda una copia propia de esos campos.
class IWebUIBackend {
public:
    virtual ~IWebUIBackend() = default;

    // Nombre corto y estable del backend ("null", "cef", ...).
    virtual const char* nombre() const = 0;

    // Crea la vista, carga el .html y reserva el area de dibujo (ancho/alto en
    // pixeles del contenedor). Devuelve false y describe `error` si no pudo.
    // Con exito, `vista` queda con cargada = true.
    virtual bool cargar(VistaWebUI& vista, const std::string& rutaHtml,
                        int ancho, int alto, std::string& error) = 0;

    // Cambia el area de la vista (al redimensionar la ventana o el contenedor).
    virtual void redimensionar(VistaWebUI& vista, int ancho, int alto) = 0;

    // Avanza el motor web (una llamada por frame). deltaTime en segundos.
    virtual void actualizar(VistaWebUI& vista, float deltaTime) = 0;

    // Entrega a la interfaz el estado JSON que el motor preparo (el backend lo
    // publica y ejecuta `FunshiUI.actualizar(estado)`). En el backend nulo es
    // no-op.
    virtual void establecerEstado(VistaWebUI& vista,
                                  const std::string& jsonEstado) = 0;

    // Salida actual de la interfaz (`FunshiUI.salida`) como texto JSON, o
    // cadena vacia si no hay. El ciclo de vistas la vuelca en el estado de la
    // interfaz. En el backend nulo siempre esta vacia.
    virtual std::string leerSalida(VistaWebUI& vista) = 0;

    // Sube el frame actual a la textura de la vista y devuelve su handle (0 si
    // aun no hay frame listo). Deja `vista.textura`/`vista.handleImGui` al dia.
    virtual std::uint64_t render(VistaWebUI& vista) = 0;

    // --- Entrada, en coordenadas locales a la vista (pixeles, origen arriba-
    // izquierda). Con un backend real el input se reenvia al motor web; el foco
    // lo decide el motor (el backend solo lo notifica). ---
    virtual void ratonMover(VistaWebUI& vista, int x, int y) = 0;
    virtual void ratonBoton(VistaWebUI& vista, BotonRatonWebUI boton,
                            bool abajo) = 0;
    virtual void ratonRueda(VistaWebUI& vista, float delta) = 0;
    // Tecla por codigo GLFW (misma convencion que InputScripts: el backend la
    // traduce al codigo de su motor web). repetir = autorepeat.
    virtual void tecla(VistaWebUI& vista, int codigo, bool abajo,
                       bool repetir) = 0;
    virtual void foco(VistaWebUI& vista, bool activo) = 0;

    // Libera la vista y su estado en el backend (invalida `vista`).
    virtual void descargar(VistaWebUI& vista) = 0;

    // Cierre de la aplicacion: apaga el motor web global del backend (p. ej. el
    // proceso de CEF). Debe invocarse despues de descargar todas las vistas.
    virtual void apagar() {}
};

#endif // IWEBUIBACKEND_H
