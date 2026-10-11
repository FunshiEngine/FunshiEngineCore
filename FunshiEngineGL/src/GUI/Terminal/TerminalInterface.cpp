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
#include "TerminalInterface.h"

#include "../WindowNames.h"
#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace {
std::string caracterUtf8(unsigned int caracter) {
    std::string resultado;
    if (caracter <= 0x7Fu) {
        resultado.push_back(static_cast<char>(caracter));
    } else if (caracter <= 0x7FFu) {
        resultado.push_back(static_cast<char>(0xC0u | (caracter >> 6)));
        resultado.push_back(static_cast<char>(0x80u | (caracter & 0x3Fu)));
    } else if (caracter <= 0xFFFFu &&
               (caracter < 0xD800u || caracter > 0xDFFFu)) {
        resultado.push_back(static_cast<char>(0xE0u | (caracter >> 12)));
        resultado.push_back(
            static_cast<char>(0x80u | ((caracter >> 6) & 0x3Fu)));
        resultado.push_back(static_cast<char>(0x80u | (caracter & 0x3Fu)));
    } else if (caracter > 0xFFFFu && caracter <= 0x10FFFFu) {
        resultado.push_back(static_cast<char>(0xF0u | (caracter >> 18)));
        resultado.push_back(
            static_cast<char>(0x80u | ((caracter >> 12) & 0x3Fu)));
        resultado.push_back(
            static_cast<char>(0x80u | ((caracter >> 6) & 0x3Fu)));
        resultado.push_back(static_cast<char>(0x80u | (caracter & 0x3Fu)));
    }
    return resultado;
}

ImU32 colorTerminal(int indice, bool fondo) {
    static const ImU32 paleta[16] = {
        IM_COL32(0, 0, 0, 255), IM_COL32(205, 49, 49, 255), IM_COL32(13, 188, 121, 255), IM_COL32(229, 229, 16, 255),
        IM_COL32(36, 114, 200, 255), IM_COL32(188, 63, 188, 255), IM_COL32(17, 168, 205, 255), IM_COL32(229, 229, 229, 255),
        IM_COL32(102, 102, 102, 255), IM_COL32(241, 76, 76, 255), IM_COL32(35, 209, 139, 255), IM_COL32(245, 245, 67, 255),
        IM_COL32(59, 142, 234, 255), IM_COL32(214, 112, 214, 255), IM_COL32(41, 184, 219, 255), IM_COL32(255, 255, 255, 255)};
    if (fondo && indice == 0) return IM_COL32(20, 20, 20, 255);
    return paleta[std::clamp(indice, 0, 15)];
}

}

TerminalInterface::TerminalInterface(bool visible)
    : GeneralUserInterface(WindowNames::Terminal, visible, ImGuiWindowFlags_None) {
#if defined(_WIN32)
    shells_ = {"cmd.exe", "powershell.exe", "pwsh.exe"};
    shellActual_ = "powershell.exe";
#else
    shells_ = {"/bin/bash", "/bin/sh", "/usr/bin/zsh"};
    shellActual_ = "/bin/bash";
#endif
    emulador_.redimensionar(80, 24);
}

TerminalInterface::~TerminalInterface() { sesion_.detener(); }

int TerminalInterface::filtrarCaracter(ImGuiInputTextCallbackData* datos) {
    auto* interfaz = static_cast<TerminalInterface*>(datos->UserData);
    if (datos->EventChar >= 0x20 && interfaz->sesion_.estaActiva()) {
        const std::string utf8 =
            caracterUtf8(static_cast<unsigned int>(datos->EventChar));
        if (!utf8.empty()) interfaz->sesion_.escribir(utf8);
    }
    return 1;
}

void TerminalInterface::setDirectorioProyecto(const std::string& directorio) {
    if (directorioProyecto_ == directorio) return;
    directorioProyecto_ = directorio;
    if (sesion_.estaActiva()) sesion_.detener();
}

void TerminalInterface::iniciar() {
    if (directorioProyecto_.empty()) {
        error_ = "No hay un proyecto activo para usar como directorio";
        return;
    }
    error_.clear();
    if (!sesion_.iniciar(shellActual_, directorioProyecto_, columnas_, filas_, error_)) return;
    emulador_ = TerminalEmulator{};
    emulador_.redimensionar(columnas_, filas_);
}

void TerminalInterface::contentGUI() {
    ImGui::Text("Proyecto: %s", directorioProyecto_.empty() ? "(sin proyecto)" : directorioProyecto_.c_str());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200.0f);
    if (!sesion_.estaActiva() && ImGui::BeginCombo("Shell", shellActual_.c_str())) {
        for (const std::string& shell : shells_) {
            const bool seleccionada = shell == shellActual_;
            if (ImGui::Selectable(shell.c_str(), seleccionada)) shellActual_ = shell;
            if (seleccionada) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (!sesion_.estaActiva()) {
        if (ImGui::Button("Iniciar")) iniciar();
    } else if (ImGui::Button("Interrumpir")) {
        sesion_.escribir("\x03");
    }
    ImGui::SameLine();
    if (sesion_.estaActiva() && ImGui::Button("Cerrar sesión")) sesion_.detener();
    if (!error_.empty()) ImGui::TextColored(ImVec4(1, 0.35f, 0.32f, 1), "%s", error_.c_str());

    const float altoLinea = ImGui::GetTextLineHeight();
    const float anchoCaracter = std::max(ImGui::CalcTextSize("M").x, 1.0f);
    const ImVec2 disponible = ImGui::GetContentRegionAvail();
    const float altoEntrada = ImGui::GetFrameHeightWithSpacing();
    columnas_ = static_cast<unsigned>(std::clamp(std::floor(disponible.x / anchoCaracter), 20.0f, 500.0f));
    filas_ = static_cast<unsigned>(std::clamp(std::floor((disponible.y - altoEntrada) / std::max(altoLinea, 1.0f)), 5.0f, 300.0f));
    if (columnas_ != emulador_.columnas() || filas_ != emulador_.filas()) {
        emulador_.redimensionar(columnas_, filas_);
        if (sesion_.estaActiva()) sesion_.redimensionar(columnas_, filas_);
    }
    if (sesion_.estaActiva()) {
        emulador_.procesar(sesion_.leerSalida());
        const std::string respuestas = emulador_.tomarRespuestas();
        if (!respuestas.empty()) sesion_.escribir(respuestas);
    }

    ImGui::BeginChild("TerminalPantalla", ImVec2(0, std::max(40.0f, disponible.y - altoEntrada)), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    const ImVec2 origen = ImGui::GetCursorScreenPos();
    ImDrawList* dibujo = ImGui::GetWindowDrawList();
    for (unsigned y = 0; y < emulador_.filas(); ++y) {
        for (unsigned x = 0; x < emulador_.columnas(); ++x) {
            const auto& celda = emulador_.celdas()[static_cast<std::size_t>(y) * emulador_.columnas() + x];
            const ImVec2 posicion(origen.x + anchoCaracter * x, origen.y + altoLinea * y);
            if (celda.fondoDefinido) dibujo->AddRectFilled(posicion, ImVec2(posicion.x + anchoCaracter, posicion.y + altoLinea), colorTerminal(celda.fondo, true));
            if (celda.texto != " ") dibujo->AddText(posicion, colorTerminal(celda.primerPlano, false), celda.texto.c_str());
            if (sesion_.estaActiva() && emulador_.cursorVisible() && x == emulador_.cursorX() && y == emulador_.cursorY())
                dibujo->AddRect(posicion, ImVec2(posicion.x + anchoCaracter, posicion.y + altoLinea), IM_COL32(230, 230, 230, 220));
        }
    }
    ImGui::Dummy(ImVec2(anchoCaracter * emulador_.columnas(), altoLinea * emulador_.filas()));
    ImGui::EndChild();

    if (sesion_.estaActiva()) {
        ImGui::SetNextItemWidth(-1.0f);
        char captura[2]{};
        ImGui::InputText("##TerminalTeclado", captura, sizeof(captura), ImGuiInputTextFlags_CallbackCharFilter, TerminalInterface::filtrarCaracter, this);
        if (ImGui::IsItemActive()) {
            if (ImGui::IsKeyPressed(ImGuiKey_Enter)) sesion_.escribir("\r");
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace)) sesion_.escribir("\x7f");
            if (ImGui::IsKeyPressed(ImGuiKey_Tab)) sesion_.escribir("\t");
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) sesion_.escribir("\x1b");
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) sesion_.escribir("\x1b[A");
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) sesion_.escribir("\x1b[B");
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) sesion_.escribir("\x1b[C");
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) sesion_.escribir("\x1b[D");
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) sesion_.escribir("\x1b[3~");
            if (ImGui::IsKeyPressed(ImGuiKey_Home)) sesion_.escribir("\x1b[H");
            if (ImGui::IsKeyPressed(ImGuiKey_End)) sesion_.escribir("\x1b[F");
            if (ImGui::GetIO().KeyCtrl) {
                for (int key = ImGuiKey_A; key <= ImGuiKey_Z; ++key)
                    if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key)))
                        sesion_.escribir(std::string(1, static_cast<char>(key - ImGuiKey_A + 1)));
            }
        }
    }
}

void TerminalInterface::printGUI() {
    if (!stateGUI) return;
    const bool mostrar = ImGui::Begin(nameGUI.c_str(), &stateGUI, flagGUI);
    if (mostrar) contentGUI();
    ImGui::End();
}
