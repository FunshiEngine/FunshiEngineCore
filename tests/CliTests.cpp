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
// Prueba headless del CLI (FunshiCLI): verifica respuesta JSON, eco de id y
// apiVersion en help, version y project.config.get.
//
// El binario del CLI se resuelve junto a este ejecutable (mismo directorio de
// build), con la extension .exe en Windows. Asi el test funciona en local y en
// CI sin rutas absolutas.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <array>
#include <memory>

#include "TempPruebas.h"

namespace fs = std::filesystem;

namespace {
int total = 0;
int fallos = 0;
#define CHECK(cond, msg) do { ++total; if (!(cond)) { ++fallos; std::cout << "FALLO: " << msg << " (linea " << __LINE__ << ")\\n"; } } while (0)

std::string ejecutarSistema(const std::string& cmd) {
    std::array<char, 4096> buffer;
    std::string resultado;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("Error al ejecutar: " + cmd);
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        resultado += buffer.data();
    }
    return resultado;
}

// Ruta del binario del CLI: mismo directorio que este ejecutable.
fs::path rutaBinarioCLI(const char* argv0) {
    fs::path dir = fs::absolute(argv0).parent_path();
#ifdef _WIN32
    return dir / "FunshiCLI.exe";
#else
    return dir / "FunshiCLI";
#endif
}
} // namespace

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    TempPruebas::CarpetaPrueba tmpDir("cli-tests");

    if (argc < 1) {
        std::cerr << "Uso: cli-tests\n";
        return 1;
    }
    const fs::path bin = rutaBinarioCLI(argv[0]);

    // 1. help
    {
        fs::path f = tmpDir.ruta() / "help.json";
        {
            std::ofstream fh(f);
            fh << R"({"command":"help"})";
        }
        std::string cmd = bin.string() + " --request-file " + f.string();
        int rc = std::system(cmd.c_str());
        CHECK(WIFEXITED(rc) && WEXITSTATUS(rc) == 0,
              "help: codigo de salida " + std::to_string(rc));
    }

    // 2. version
    {
        fs::path f = tmpDir.ruta() / "version.json";
        {
            std::ofstream fh(f);
            fh << R"({"command":"version"})";
        }
        std::string cmd = bin.string() + " --request-file " + f.string();
        int rc = std::system(cmd.c_str());
        CHECK(WIFEXITED(rc) && WEXITSTATUS(rc) == 0,
              "version: codigo de salida " + std::to_string(rc));
    }

    // 3. eco de id
    {
        fs::path f = tmpDir.ruta() / "with-id.json";
        {
            std::ofstream fh(f);
            fh << R"({"command":"version","id":"123"})";
        }
        std::string cmd = bin.string() + " --request-file " + f.string();
        std::string salida = ejecutarSistema(cmd);

        CHECK(salida.find("\"apiVersion\":1") != std::string::npos,
              "respuesta debe tener apiVersion:1");
        CHECK(salida.find("\"id\":\"123\"") != std::string::npos,
              "respuesta debe incluir id:123");
    }

    std::cout << (fallos == 0 ? "OK" : "FALLOS") << ": "
              << total << " comprobaciones, " << fallos << " fallos\n";
    return fallos == 0 ? 0 : 1;
}
