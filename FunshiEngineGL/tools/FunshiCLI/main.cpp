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
#include "Configuracion/ProjectManager.h"
#include "Configuracion/ProjectPaths.h"
#include "Diagnosticos/RegistroDiagnosticos.h"
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

namespace {
using json = nlohmann::json;
namespace fs = std::filesystem;

json respuestaCorrecta(json resultado) {
    return json{{"ok", true}, {"result", std::move(resultado)}, {"error", nullptr}};
}

json respuestaError(const std::string& codigo, const std::string& mensaje) {
    return json{{"ok", false}, {"result", nullptr},
                {"error", {{"code", codigo}, {"message", mensaje}}}};
}

bool resolverProyecto(const std::string& ruta,
                      ProjectManager::Proyecto& proyecto,
                      std::string& error) {
    if (ruta.empty()) {
        error = "La ruta del proyecto esta vacia";
        return false;
    }

    std::error_code ec;
    const fs::path raiz = fs::weakly_canonical(
        fs::absolute(ProjectPaths::directorioProyects(), ec), ec);
    if (ec) {
        error = "No se pudo resolver la ubicacion fija de proyectos";
        return false;
    }
    const fs::path recibida = fs::weakly_canonical(fs::absolute(ruta, ec), ec);
    if (ec || !fs::is_directory(recibida, ec) || ec) {
        error = "El proyecto indicado no existe o no es un directorio";
        return false;
    }

    const fs::path nombrePath = recibida.filename();
    const std::string nombre = nombrePath.string();
    if (!ProjectPaths::esNombreValido(nombre) || nombre == "." || nombre == "..") {
        error = "La ruta no identifica un proyecto valido";
        return false;
    }

    const fs::path esperado = fs::weakly_canonical(
        fs::path(ProjectPaths::directorioProyecto(nombre)), ec);
    if (ec) {
        error = "No se pudo resolver la ruta del proyecto";
        return false;
    }
    const bool mismaCarpeta = fs::equivalent(esperado, recibida, ec);
    if (ec || !mismaCarpeta) {
        error = "El proyecto debe estar dentro de la ubicacion fija del motor";
        return false;
    }
    const bool bajoRaiz =
        fs::equivalent(recibida.parent_path(), raiz, ec);
    if (ec || !bajoRaiz) {
        error = "El proyecto debe estar dentro de la ubicacion fija del motor";
        return false;
    }
    if (!ProjectManager::esProyectoValido(nombre)) {
        error = "El directorio no contiene la estructura de un proyecto Funshi";
        return false;
    }

    const auto encontrado = ProjectManager::buscarProyecto(nombre);
    if (!encontrado) {
        error = "No se pudo abrir el proyecto";
        return false;
    }
    proyecto = *encontrado;
    return true;
}

json eventoJson(const EventoDiagnostico& evento) {
    return json{{"timestamp", evento.fechaUtc},
                {"level", RegistroDiagnosticos::nombreNivel(evento.nivel)},
                {"module", evento.modulo},
                {"message", evento.mensaje},
                {"project", evento.proyecto},
                {"exception", evento.tipoExcepcion},
                {"stack", evento.traza}};
}

void imprimirJson(const json& valor) {
    std::cout << valor.dump(-1, ' ', false,
                            json::error_handler_t::replace)
              << '\n';
}

void registrarError(const std::string& codigo, const std::string& mensaje) {
    EventoDiagnostico evento;
    evento.nivel = NivelDiagnostico::Error;
    evento.modulo = "FunshiCLI";
    evento.tipoExcepcion = codigo;
    evento.mensaje = mensaje;
    RegistroDiagnosticos::global().registrar(std::move(evento));
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "help" ||
                      std::string(argv[1]) == "--help" ||
                      std::string(argv[1]) == "-h")) {
        std::cout
            << "Funshi CLI " << FUNSHI_VERSION << "\n"
            << "Uso:\n"
            << "  FunshiCLI help | version\n"
            << "  FunshiCLI [--project RUTA] --request\n"
            << "  FunshiCLI [--project RUTA] --request-file ARCHIVO.json\n"
            << "La solicitud JSON se envia por stdin o desde un archivo.\n"
            << "Comandos iniciales: help, version, project.create, diagnostics.list\n";
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "version" ||
                      std::string(argv[1]) == "--version")) {
        std::cout << FUNSHI_VERSION << '\n';
        return 0;
    }

    std::string rutaProyecto;
    std::string archivoSolicitud;
    bool stdinSolicitud = false;
    for (int i = 1; i < argc; ++i) {
        const std::string argumento(argv[i]);
        if (argumento == "--project" && i + 1 < argc) {
            rutaProyecto = argv[++i];
        } else if (argumento == "--request") {
            stdinSolicitud = true;
        } else if (argumento == "--request-file" && i + 1 < argc) {
            archivoSolicitud = argv[++i];
        } else {
            imprimirJson(respuestaError("invalid_arguments",
                                        "Argumento no reconocido o incompleto: " +
                                            argumento));
            return 2;
        }
    }
    if (stdinSolicitud == !archivoSolicitud.empty()) {
        imprimirJson(respuestaError(
            "invalid_arguments",
            "Indica exactamente una fuente: --request o --request-file"));
        return 2;
    }

    ProjectManager::Proyecto proyecto;
    if (!rutaProyecto.empty()) {
        std::string error;
        if (!resolverProyecto(rutaProyecto, proyecto, error)) {
            registrarError("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
    }

    std::string textoSolicitud;
    if (stdinSolicitud) {
        textoSolicitud.assign(std::istreambuf_iterator<char>(std::cin),
                              std::istreambuf_iterator<char>());
    } else {
        std::ifstream entrada(archivoSolicitud, std::ios::binary);
        if (!entrada) {
            imprimirJson(respuestaError("request_file_unavailable",
                                        "No se pudo leer el archivo de solicitud"));
            return 2;
        }
        textoSolicitud.assign(std::istreambuf_iterator<char>(entrada),
                              std::istreambuf_iterator<char>());
    }

    json solicitud;
    try {
        solicitud = json::parse(textoSolicitud);
    } catch (const json::exception& excepcion) {
        registrarError("invalid_json", excepcion.what());
        imprimirJson(respuestaError("invalid_json", excepcion.what()));
        return 2;
    }
    if (!solicitud.is_object() || !solicitud.contains("command") ||
        !solicitud["command"].is_string()) {
        imprimirJson(respuestaError(
            "invalid_request", "La solicitud requiere un campo command de tipo string"));
        return 2;
    }

    const std::string comando = solicitud["command"].get<std::string>();
    const json argumentos = solicitud.value("args", json::object());
    if (!argumentos.is_object()) {
        imprimirJson(respuestaError("invalid_request", "args debe ser un objeto JSON"));
        return 2;
    }

    if (comando == "help") {
        imprimirJson(respuestaCorrecta(
            {{"commands", {"help", "version", "project.create", "diagnostics.list"}},
             {"request", "JSON por stdin (--request) o archivo (--request-file)"}}));
        return 0;
    }
    if (comando == "version") {
        imprimirJson(respuestaCorrecta({{"version", FUNSHI_VERSION}}));
        return 0;
    }
    if (comando == "project.create") {
        if (argumentos.contains("name") && !argumentos["name"].is_string()) {
            imprimirJson(respuestaError(
                "invalid_request", "args.name debe ser un string"));
            return 2;
        }
        const std::string nombre = argumentos.value("name", std::string{});
        std::error_code error;
        if (!ProjectManager::crearProyectoNuevo(nombre, error)) {
            const std::string codigo = error == std::errc::file_exists
                                           ? "project_exists"
                                           : error == std::errc::invalid_argument
                                                 ? "invalid_project_name"
                                                 : "project_create_failed";
            const std::string mensaje =
                codigo == "project_exists"
                    ? "Ya existe un proyecto con ese nombre"
                    : codigo == "invalid_project_name"
                          ? "El nombre del proyecto no es valido"
                          : error.message();
            registrarError(codigo, mensaje);
            imprimirJson(respuestaError(codigo, mensaje));
            return 1;
        }
        EventoDiagnostico evento;
        evento.nivel = NivelDiagnostico::Informacion;
        evento.modulo = "FunshiCLI";
        evento.proyecto = nombre;
        evento.mensaje = "Proyecto creado";
        RegistroDiagnosticos::global().registrar(std::move(evento));
        imprimirJson(respuestaCorrecta(
            {{"name", nombre},
             {"path", ProjectPaths::directorioProyecto(nombre)}}));
        return 0;
    }
    if (comando == "diagnostics.list") {
        std::size_t limite = 100;
        if (argumentos.contains("limit")) {
            const json& valorLimite = argumentos["limit"];
            if (!valorLimite.is_number_integer()) {
                imprimirJson(respuestaError(
                    "invalid_request", "args.limit debe ser un entero no negativo"));
                return 2;
            }
            if (valorLimite.is_number_unsigned()) {
                limite = static_cast<std::size_t>(
                    std::min<unsigned long long>(valorLimite.get<unsigned long long>(),
                                                 1000));
            } else {
                const long long valorEntero = valorLimite.get<long long>();
                if (valorEntero < 0) {
                    imprimirJson(respuestaError(
                        "invalid_request",
                        "args.limit debe ser un entero no negativo"));
                    return 2;
                }
                limite = static_cast<std::size_t>(
                    std::min<long long>(valorEntero, 1000));
            }
        }
        const auto eventos = RegistroDiagnosticos::leerArchivo(
            RegistroDiagnosticos::archivoPorDefecto(), limite);
        json resultado = json::array();
        for (const auto& evento : eventos) resultado.push_back(eventoJson(evento));
        imprimirJson(respuestaCorrecta(
            {{"items", std::move(resultado)}, {"count", eventos.size()}}));
        return 0;
    }

    const std::string mensaje = "Comando no encontrado: " + comando;
    registrarError("unknown_command", mensaje);
    imprimirJson(respuestaError("unknown_command", mensaje));
    return 2;
}
