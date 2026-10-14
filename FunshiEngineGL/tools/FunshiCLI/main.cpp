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
#include "Configuracion/ConfigPersistence.h"
#include "Configuracion/DimensionProyecto.h"
#include "Configuracion/EditorConfig.h"
#include "Diagnosticos/RegistroDiagnosticos.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPath.h"
#include "Assets/AssimpMeshLoader.h"
#include "Assets/TextureManager.h"
#include "Behaviour/ScriptRuntime.h"
#include "Behaviour/Backends/BackendCpp.h"
#include "Behaviour/Backends/BackendJava.h"
#include "Scenes/SceneRegistry.h"
#include "Scenes/EditorController.h"
#include "Scenes/SceneSerializer.h"
#include "Objetos/GameObject.h"
#include "Objetos/Componentes/Component.h"
#include "Objetos/Componentes/Transform.h"
#include "Objetos/Componentes/Colliders/EsfereCollider.h"
#include "Objetos/Componentes/Colliders/CubeCollider.h"
#include "Objetos/Componentes/Colliders/MallaCollider.h"
#include "Objetos/Componentes/Colliders/TilemapCollider.h"
#include "Objetos/Componentes/RigidBody/RigidBody.h"
#include "Objetos/Componentes/Script.h"
#include "Objetos/Componentes/Model.h"
#include "Objetos/Componentes/Material.h"
#include "Objetos/Componentes/Color.h"
#include "Objetos/Componentes/Light.h"
#include "Objetos/Componentes/Skybox.h"
#include "Objetos/Componentes/SpriteRenderer.h"
#include "Objetos/Componentes/Animador2D.h"
#include "Objetos/Componentes/Tilemap.h"
#include "Objetos/Componentes/CameraComponent.h"
#include "Objetos/Componentes/AudioSource.h"
#include "Objetos/Componentes/InterfaceComponent.h"
#include "Objetos/Componentes/Grid.h"
#include "Fisicas/PhysicsEngine.h"
#include "Fisicas/BulletPhysicsAdapter.h"
#include "Events/EventBus.h"
#include "Input/EditorInput.h"
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

// Dispatch de comandos: enum cerrado + switch exhaustivo (patrón AGENTS).
// Facilita añadir comandos, evita typos y permite al compilador avisar
// de casos no cubiertos al agregar valores.
enum class ComandoCLI {
    Help,
    Version,
    ProjectCreate,
    ProjectList,
    ProjectConfigGet,
    AssetImport,
    ScriptList,
    ScriptCompile,
    DiagnosticsList,
    DiagnosticsClear,
    SceneObjectGet,
    SceneHierarchy,
    ProjectFsCreate,
    FileCreate,
    ProjectDirImport,
    SceneList,
    SceneObjects,
    ProjectSrcCreate,
    Desconocido
};

ComandoCLI comandoDesdeTexto(const std::string& texto) {
    static const std::unordered_map<std::string, ComandoCLI> mapa = {
        {"help", ComandoCLI::Help},
        {"version", ComandoCLI::Version},
        {"project.create", ComandoCLI::ProjectCreate},
        {"project.list", ComandoCLI::ProjectList},
        {"project.config.get", ComandoCLI::ProjectConfigGet},
        {"asset.import", ComandoCLI::AssetImport},
        {"script.list", ComandoCLI::ScriptList},
        {"script.compile", ComandoCLI::ScriptCompile},
        {"diagnostics.list", ComandoCLI::DiagnosticsList},
        {"diagnostics.clear", ComandoCLI::DiagnosticsClear},
        {"scene.object.get", ComandoCLI::SceneObjectGet},
        {"scene.hierarchy", ComandoCLI::SceneHierarchy},
        {"project.fs.create", ComandoCLI::ProjectFsCreate},
        {"file.create", ComandoCLI::FileCreate},
        {"project.dir.import", ComandoCLI::ProjectDirImport},
        {"scene.list", ComandoCLI::SceneList},
        {"scene.objects", ComandoCLI::SceneObjects},
        {"project.src.create", ComandoCLI::ProjectSrcCreate},
    };
    auto it = mapa.find(texto);
    return it != mapa.end() ? it->second : ComandoCLI::Desconocido;
}

json respuestaCorrecta(json resultado, const json* id = nullptr) {
    json resp = {{"ok", true}, {"result", std::move(resultado)}, {"error", nullptr}, {"apiVersion", 1}};
    if (id && !id->is_null()) resp["id"] = *id;
    return resp;
}

json respuestaError(const std::string& codigo, const std::string& mensaje, const json* id = nullptr) {
    json resp = {{"ok", false}, {"result", nullptr}, {"error", {{"code", codigo}, {"message", mensaje}}}, {"apiVersion", 1}};
    if (id && !id->is_null()) resp["id"] = *id;
    return resp;
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

bool resolverProyectoPorNombre(const std::string& nombre,
                               ProjectManager::Proyecto& proyecto,
                               std::string& error) {
    if (!ProjectPaths::esNombreValido(nombre) || nombre == "." || nombre == "..") {
        error = "El nombre del proyecto no es valido";
        return false;
    }
    if (!ProjectManager::esProyectoValido(nombre)) {
        error = "El proyecto no existe o no tiene estructura valida";
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

json componenteJson(Component* comp) {
    json result;
    result["type"] = typeid(*comp).name();
    
    std::string typeName = typeid(*comp).name();
    if (typeName.rfind("class ", 0) == 0) typeName = typeName.substr(6);
    else if (typeName.rfind("struct ", 0) == 0) typeName = typeName.substr(7);
    result["typeName"] = typeName;
    
    if (auto* transform = dynamic_cast<Transform*>(comp)) {
        float* pos = transform->getTranslatef();
        float* rot = transform->getRotatef();
        float* scale = transform->getScalef();
        result["position"] = {pos[0], pos[1], pos[2]};
        result["rotation"] = {rot[0], rot[1], rot[2], rot[3]};
        result["scale"] = {scale[0], scale[1], scale[2]};
    } else if (auto* script = dynamic_cast<Script*>(comp)) {
        result["scriptPath"] = script->getPath();
        result["className"] = script->getNameClass();
        json campos = json::array();
        for (const auto& def : script->obtenerCampos()) {
            json campo;
            campo["name"] = def.nombre;
            campo["type"] = static_cast<int>(def.tag);
            campos.push_back(std::move(campo));
        }
        result["serializeFields"] = std::move(campos);
    } else if (auto* model = dynamic_cast<Model*>(comp)) {
        result["modelPath"] = model->getPath();
    } else if (auto* color = dynamic_cast<Color*>(comp)) {
        const float* c = color->getColor();
        if (c) result["color"] = {c[0], c[1], c[2], c[3]};
    }
    // Otros componentes: solo el tipo
    
    return result;
}

json gameObjectJson(GameObject* obj) {
    json result;
    result["id"] = obj->getId();
    result["name"] = obj->inputName;
    result["tag"] = obj->getTag();
    result["active"] = obj->getState();
    
    // Transform local del objeto
    if (auto* transform = obj->getComponent<Transform>()) {
        float* pos = transform->getTranslatef();
        float* rot = transform->getRotatef();
        float* scale = transform->getScalef();
        result["transform"] = {
            {"position", {pos[0], pos[1], pos[2]}},
            {"rotation", {rot[0], rot[1], rot[2], rot[3]}},
            {"scale", {scale[0], scale[1], scale[2]}}
        };
    }
    
    json components = json::array();
    if (auto* componentsList = obj->getComponents()) {
        if (!componentsList->isEmpty()) {
            auto* pos = componentsList->first();
            while (pos != nullptr) {
                components.push_back(componenteJson(pos->getElement()));
                pos = (pos != componentsList->last()) ? componentsList->next(pos) : nullptr;
            }
        }
    }
    result["components"] = std::move(components);
    
    json children = json::array();
    for (Entity* child : obj->getChildEntities()) {
        if (auto* go = dynamic_cast<GameObject*>(child)) {
            children.push_back(gameObjectJson(go));
        }
    }
    result["children"] = std::move(children);
    
    return result;
}

json sceneHierarchyJson(SceneRegistry* registry) {
    GameObject* root = registry->getRoot();
    if (!root) return json::array();
    
    json result = json::array();
    for (Entity* child : root->getChildEntities()) {
        if (auto* go = dynamic_cast<GameObject*>(child)) {
            result.push_back(gameObjectJson(go));
        }
    }
    return result;
}

GameObject* findObjectById(SceneRegistry* registry, int id) {
    if (id == 0) return registry->getRoot();
    GameObject* root = registry->getRoot();
    if (!root) return nullptr;
    
    // Búsqueda recursiva
    std::function<GameObject*(GameObject*)> search = [&](GameObject* obj) -> GameObject* {
        if (obj->getId() == id) return obj;
        for (Entity* child : obj->getChildEntities()) {
            if (auto* go = dynamic_cast<GameObject*>(child)) {
                if (auto* found = search(go)) return found;
            }
        }
        return nullptr;
    };
    
    return search(root);
}

GameObject* findObjectByTag(SceneRegistry* registry, const std::string& tag) {
    GameObject* root = registry->getRoot();
    if (!root) return nullptr;
    
    std::function<GameObject*(GameObject*)> search = [&](GameObject* obj) -> GameObject* {
        if (obj->getTag() == tag) return obj;
        for (Entity* child : obj->getChildEntities()) {
            if (auto* go = dynamic_cast<GameObject*>(child)) {
                if (auto* found = search(go)) return found;
            }
        }
        return nullptr;
    };
    
    return search(root);
}

}  // namespace

int main(int argc, char* argv[]) {
    // B4: la resolución de --project se difiere al comando que la necesite.
    // B5: política consistente de registro — errores de uso (invocation errors)
    // solo van al JSON de salida; errores operacionales sí se registran.
    auto registrarErrorUso = [](const std::string& codigo,
                                const std::string& mensaje) {
        // Solo JSON de salida, sin tocar el log compartido.
        (void)codigo; (void)mensaje;
    };
    auto registrarErrorOperacional = [](const std::string& codigo,
                                        const std::string& mensaje) {
        EventoDiagnostico evento;
        evento.nivel = NivelDiagnostico::Error;
        evento.modulo = "FunshiCLI";
        evento.tipoExcepcion = codigo;
        evento.mensaje = mensaje;
        RegistroDiagnosticos::global().registrar(std::move(evento));
    };

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
            << "Comandos: help, version, project.create, project.list, project.config.get, project.src.create, project.dir.import, scene.list, scene.objects, scene.object.get, asset.import, script.list, script.compile, diagnostics.list, diagnostics.clear\n";
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
            registrarErrorUso("invalid_arguments",
                              "Argumento no reconocido o incompleto: " + argumento);
            imprimirJson(respuestaError("invalid_arguments",
                                        "Argumento no reconocido o incompleto: " + argumento));
            return 2;
        }
    }
    if (stdinSolicitud == !archivoSolicitud.empty()) {
        registrarErrorUso("invalid_arguments",
                          "Indica exactamente una fuente: --request o --request-file");
        imprimirJson(respuestaError(
            "invalid_arguments",
            "Indica exactamente una fuente: --request o --request-file"));
        return 2;
    }

    std::string textoSolicitud;
    if (stdinSolicitud) {
        textoSolicitud.assign(std::istreambuf_iterator<char>(std::cin),
                              std::istreambuf_iterator<char>());
    } else {
        std::ifstream entrada(archivoSolicitud, std::ios::binary);
        if (!entrada) {
            registrarErrorUso("request_file_unavailable",
                              "No se pudo leer el archivo de solicitud");
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
        registrarErrorUso("invalid_json", excepcion.what());
        imprimirJson(respuestaError("invalid_json", excepcion.what()));
        return 2;
    }
    if (!solicitud.is_object() || !solicitud.contains("command") ||
        !solicitud["command"].is_string()) {
        registrarErrorUso("invalid_request", "La solicitud requiere un campo command de tipo string");
        imprimirJson(respuestaError(
            "invalid_request", "La solicitud requiere un campo command de tipo string"));
        return 2;
    }

    const std::string comandoStr = solicitud["command"].get<std::string>();
    const json argumentos = solicitud.value("args", json::object());
    if (!argumentos.is_object()) {
        registrarErrorUso("invalid_request", "args debe ser un objeto JSON");
        imprimirJson(respuestaError("invalid_request", "args debe ser un objeto JSON", nullptr));
        return 2;
    }

    // Eco de id si la solicitud lo trae (para correlación cliente-servidor)
    const json* reqId = solicitud.contains("id") ? &solicitud["id"] : nullptr;

    // Dispatch por enum + switch exhaustivo (patrón AGENTS: enum class + switch)
    switch (comandoDesdeTexto(comandoStr)) {
        case ComandoCLI::Help: {
            imprimirJson(respuestaCorrecta(
                {{"commands", {"help", "version", "project.create", "project.list", "project.config.get", "project.src.create", "project.dir.import", "scene.list", "scene.objects", "scene.object.get", "scene.hierarchy", "asset.import", "script.list", "script.compile", "diagnostics.list", "diagnostics.clear", "project.fs.create", "file.create"}},
                 {"request", "JSON por stdin (--request) o archivo (--request-file)"}},
                reqId));
            return 0;
        }
        case ComandoCLI::Version: {
            imprimirJson(respuestaCorrecta({{"version", FUNSHI_VERSION}}, reqId));
            return 0;
        }
        case ComandoCLI::ProjectCreate: {
            if (argumentos.contains("name") && !argumentos["name"].is_string()) {
                registrarErrorUso("invalid_request", "args.name debe ser un string");
                imprimirJson(respuestaError("invalid_request", "args.name debe ser un string", reqId));
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
                registrarErrorOperacional(codigo, mensaje);
                imprimirJson(respuestaError(codigo, mensaje, reqId));
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
                 {"path", ProjectPaths::directorioProyecto(nombre)}},
                reqId));
            return 0;
        }
        case ComandoCLI::ProjectList: {
            const auto proyectos = ProjectManager::descubrirProyectos();
            json resultado = json::array();
            for (const auto& p : proyectos) {
                ConfigPersistence::Proyecto config = ConfigPersistence::cargarProyecto(p.nombre);
                resultado.push_back({
                    {"name", p.nombre},
                    {"path", p.ruta},
                    {"srcPath", p.rutaSrc},
                    {"valid", p.valido},
                    {"dimension", dimensionProyectoAEntero(config.dimension)}
                });
            }
            imprimirJson(respuestaCorrecta({{"projects", std::move(resultado)}, {"count", proyectos.size()}}, reqId));
            return 0;
        }
        case ComandoCLI::SceneList: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        std::string sceneDir = ProjectPaths::rutaSceneDir(nombreProyecto);
        std::error_code ec;
        json scenes = json::array();
        if (std::filesystem::exists(sceneDir, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(sceneDir, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                std::string ext = entry.path().extension().string();
                if (ext == ".db" || ext == ".txt") {
                    scenes.push_back({
                        {"name", entry.path().stem().string()},
                        {"file", entry.path().filename().string()},
                        {"size", static_cast<long long>(entry.file_size(ec))}
                    });
                }
            }
        }
        imprimirJson(respuestaCorrecta({{"project", nombreProyecto}, {"scenes", std::move(scenes)}, {"count", scenes.size()}}));
        return 0;
        }
        case ComandoCLI::SceneObjects: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        
        // Cargar escena
        SceneRegistry registry;
        PhysicsEngine physics;
        EventBus events;
        AssetManager assets(std::make_unique<AssimpMeshLoader>());
        EditorController editor(&registry, &physics, &events, &assets);
        editor.setAssetManager(&assets);
        
        SceneSerializer serializer(&registry, &editor, &assets);
        std::string pathTxt = ProjectPaths::rutaSceneBBDD(nombreProyecto);
        std::string semiPath = ProjectPaths::directorioBinarios(nombreProyecto);
        bool cargaCompleta = false;
        serializer.load(pathTxt, semiPath);
        editor.registerSceneRigidBodies();
        
        json hierarchy = sceneHierarchyJson(&registry);
        imprimirJson(respuestaCorrecta({
            {"project", nombreProyecto},
            {"root", hierarchy}
        }));
        return 0;
        }
        case ComandoCLI::SceneObjectGet: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        
        // Cargar escena
        SceneRegistry registry;
        PhysicsEngine physics;
        EventBus events;
        AssetManager assets(std::make_unique<AssimpMeshLoader>());
        EditorController editor(&registry, &physics, &events, &assets);
        editor.setAssetManager(&assets);
        
        SceneSerializer serializer(&registry, &editor, &assets);
        std::string pathTxt = ProjectPaths::rutaSceneBBDD(nombreProyecto);
        std::string semiPath = ProjectPaths::directorioBinarios(nombreProyecto);
        bool cargaCompleta = false;
        serializer.load(pathTxt, semiPath);
        editor.registerSceneRigidBodies();
        
        GameObject* obj = nullptr;
        if (argumentos.contains("id") && argumentos["id"].is_number_integer()) {
            obj = findObjectById(&registry, argumentos["id"].get<int>());
        } else if (argumentos.contains("tag") && argumentos["tag"].is_string()) {
            obj = findObjectByTag(&registry, argumentos["tag"].get<std::string>());
        } else {
            registrarErrorUso("invalid_request", "args.id (int) o args.tag (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.id (int) o args.tag (string) es requerido"));
            return 2;
        }
        
        if (!obj) {
            registrarErrorOperacional("object_not_found", "Objeto no encontrado");
            imprimirJson(respuestaError("object_not_found", "Objeto no encontrado"));
            return 4;
        }
        
        imprimirJson(respuestaCorrecta({
            {"project", nombreProyecto},
            {"object", gameObjectJson(obj)}
        }));
        return 0;
        }
        case ComandoCLI::ProjectSrcCreate: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        if (!argumentos.contains("path") || !argumentos["path"].is_string()) {
            registrarErrorUso("invalid_request", "args.path (string) es requerido: ruta relativa dentro de src/");
            imprimirJson(respuestaError("invalid_request", "args.path (string) es requerido: ruta relativa dentro de src/"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        const std::string rutaRelativa = argumentos["path"].get<std::string>();
        bool esArchivo = argumentos.value("file", false);
        std::string contenido = argumentos.value("content", std::string{});
        
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        
        std::string srcDir = ProjectPaths::directorioSrc(nombreProyecto);
        std::error_code ec;
        std::filesystem::path rutaCompleta = std::filesystem::path(srcDir) / rutaRelativa;
        
        // Validar que la ruta no escape del directorio src
        std::filesystem::path srcAbsoluto = std::filesystem::weakly_canonical(srcDir, ec);
        std::filesystem::path destinoAbsoluto = std::filesystem::weakly_canonical(rutaCompleta.parent_path(), ec);
        if (ec || destinoAbsoluto.string().find(srcAbsoluto.string()) != 0) {
            registrarErrorOperacional("invalid_path", "La ruta intenta escapar del directorio src del proyecto");
            imprimirJson(respuestaError("invalid_path", "La ruta intenta escapar del directorio src del proyecto"));
            return 4;
        }
        
        if (esArchivo) {
            // Crear directorios padre si no existen
            std::filesystem::create_directories(rutaCompleta.parent_path(), ec);
            if (ec) {
                registrarErrorOperacional("create_failed", "No se pudieron crear directorios padre: " + ec.message());
                imprimirJson(respuestaError("create_failed", "No se pudieron crear directorios padre: " + ec.message()));
                return 5;
            }
            
            // Crear archivo
            std::ofstream archivo(rutaCompleta, std::ios::binary | std::ios::trunc);
            if (!archivo) {
                registrarErrorOperacional("create_failed", "No se pudo crear el archivo: " + rutaCompleta.string());
                imprimirJson(respuestaError("create_failed", "No se pudo crear el archivo"));
                return 5;
            }
            archivo << contenido;
            archivo.close();
            
            registrarErrorOperacional("file_created", "Archivo creado: " + rutaRelativa);
            imprimirJson(respuestaCorrecta({
                {"project", nombreProyecto},
                {"path", rutaRelativa},
                {"type", "file"},
                {"created", true}
            }));
        } else {
            // Crear directorio
            bool creado = std::filesystem::create_directories(rutaCompleta, ec);
            if (ec) {
                registrarErrorOperacional("create_failed", "No se pudo crear el directorio: " + ec.message());
                imprimirJson(respuestaError("create_failed", "No se pudo crear el directorio: " + ec.message()));
                return 5;
            }
            
            registrarErrorOperacional("dir_created", "Directorio creado: " + rutaRelativa);
            imprimirJson(respuestaCorrecta({
                {"project", nombreProyecto},
                {"path", rutaRelativa},
                {"type", "directory"},
                {"created", creado}
            }));
        }
        return 0;
        }
        case ComandoCLI::ProjectDirImport: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        if (!argumentos.contains("source") || !argumentos["source"].is_string()) {
            registrarErrorUso("invalid_request", "args.source (string) es requerido: ruta absoluta de la carpeta origen a importar");
            imprimirJson(respuestaError("invalid_request", "args.source (string) es requerido: ruta absoluta de la carpeta origen a importar"));
            return 2;
        }
        if (!argumentos.contains("path") || !argumentos["path"].is_string()) {
            registrarErrorUso("invalid_request", "args.path (string) es requerido: ruta relativa destino dentro de src/");
            imprimirJson(respuestaError("invalid_request", "args.path (string) es requerido: ruta relativa destino dentro de src/"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        const std::string sourcePath = argumentos["source"].get<std::string>();
        const std::string rutaRelativa = argumentos["path"].get<std::string>();
        bool overwrite = argumentos.value("overwrite", false);
        
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        
        std::error_code ec;
        if (!std::filesystem::exists(sourcePath, ec) || ec) {
            registrarErrorOperacional("source_not_found", "La carpeta origen no existe: " + sourcePath);
            imprimirJson(respuestaError("source_not_found", "La carpeta origen no existe: " + sourcePath));
            return 4;
        }
        if (!std::filesystem::is_directory(sourcePath, ec) || ec) {
            registrarErrorOperacional("source_not_directory", "El origen no es un directorio: " + sourcePath);
            imprimirJson(respuestaError("source_not_directory", "El origen no es un directorio: " + sourcePath));
            return 4;
        }
        
        std::string srcDir = ProjectPaths::directorioSrc(nombreProyecto);
        std::filesystem::path destPath = std::filesystem::path(srcDir) / rutaRelativa;
        
        // Validar que la ruta no escape del directorio src
        std::filesystem::path srcAbsoluto = std::filesystem::weakly_canonical(srcDir, ec);
        std::filesystem::path destinoAbsoluto = std::filesystem::weakly_canonical(destPath, ec);
        if (ec || destinoAbsoluto.string().find(srcAbsoluto.string()) != 0) {
            registrarErrorOperacional("invalid_path", "La ruta destino intenta escapar del directorio src del proyecto");
            imprimirJson(respuestaError("invalid_path", "La ruta destino intenta escapar del directorio src del proyecto"));
            return 4;
        }
        
        // Si el destino existe y no se permite overwrite, error
        if (std::filesystem::exists(destPath, ec) && !overwrite) {
            registrarErrorOperacional("destination_exists", "El destino ya existe: " + rutaRelativa + " (usa overwrite: true para reemplazar)");
            imprimirJson(respuestaError("destination_exists", "El destino ya existe: " + rutaRelativa + " (usa overwrite: true para reemplazar)"));
            return 4;
        }
        
        // Si existe y overwrite=true, borrar primero
        if (std::filesystem::exists(destPath, ec) && overwrite) {
            std::filesystem::remove_all(destPath, ec);
            if (ec) {
                registrarErrorOperacional("remove_failed", "No se pudo eliminar el destino existente: " + ec.message());
                imprimirJson(respuestaError("remove_failed", "No se pudo eliminar el destino existente: " + ec.message()));
                return 5;
            }
        }
        
        // Copiar recursivamente
        std::filesystem::copy_options opciones = std::filesystem::copy_options::recursive |
                                                std::filesystem::copy_options::overwrite_existing;
        std::filesystem::copy(sourcePath, destPath, opciones, ec);
        if (ec) {
            registrarErrorOperacional("copy_failed", "Error al copiar la carpeta: " + ec.message());
            imprimirJson(respuestaError("copy_failed", "Error al copiar la carpeta: " + ec.message()));
            return 5;
        }
        
        // Contar archivos copiados
        size_t fileCount = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(destPath, ec)) {
            if (entry.is_regular_file(ec)) fileCount++;
        }
        
        registrarErrorOperacional("dir_imported", "Carpeta importada: " + rutaRelativa + " (" + std::to_string(fileCount) + " archivos)");
        imprimirJson(respuestaCorrecta({
            {"project", nombreProyecto},
            {"source", sourcePath},
            {"destination", rutaRelativa},
            {"filesCopied", static_cast<long long>(fileCount)},
            {"overwrite", overwrite}
        }));
        return 0;
        }
        case ComandoCLI::ProjectConfigGet: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        ConfigPersistence::Proyecto config = ConfigPersistence::cargarProyecto(nombreProyecto);
        json resultado = json{
            {"name", nombreProyecto},
            {"dimension", dimensionProyectoAEntero(config.dimension)},
            {"version", config.version},
            {"ventanaCamarasAbierta", config.ventanaCamarasAbierta},
            {"gizmoOperacion", config.gizmoOperacion},
            {"gizmoGlobal", config.gizmoGlobal},
            {"camaraActivaId", config.camaraActivaId},
            {"estadoVentanas", config.estadoVentanas}
        };
        imprimirJson(respuestaCorrecta(std::move(resultado)));
        return 0;
        }
        case ComandoCLI::ScriptList: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        std::string scriptsDir = ProjectPaths::directorioScripts(nombreProyecto);
        std::error_code ec;
        json scripts = json::array();
        if (std::filesystem::exists(scriptsDir, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(scriptsDir, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                std::string ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".java") {
                    scripts.push_back({
                        {"name", entry.path().filename().string()},
                        {"path", AssetPath::normalize(std::filesystem::relative(entry.path(), ProjectPaths::directorioProyecto(nombreProyecto)).string())},
                        {"language", ext == ".cpp" ? "cpp" : "java"},
                        {"size", static_cast<long long>(entry.file_size(ec))}
                    });
                }
            }
        }
        imprimirJson(respuestaCorrecta({{"scripts", std::move(scripts)}, {"count", scripts.size()}}));
        return 0;
        }
        case ComandoCLI::ScriptCompile: {
        if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
            registrarErrorUso("invalid_request", "args.project (string) es requerido");
            imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido"));
            return 2;
        }
        const std::string nombreProyecto = argumentos["project"].get<std::string>();
        std::string error;
        ProjectManager::Proyecto proyecto;
        if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
            registrarErrorOperacional("invalid_project", error);
            imprimirJson(respuestaError("invalid_project", error));
            return 3;
        }
        std::string scriptsDir = ProjectPaths::directorioScripts(nombreProyecto);
        std::error_code ec;
        json results = json::array();
        int compiled = 0, failed = 0, skipped = 0;
        if (std::filesystem::exists(scriptsDir, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(scriptsDir, ec)) {
                if (!entry.is_regular_file(ec)) continue;
                std::string ext = entry.path().extension().string();
                std::string sourcePath = entry.path().string();
                if (ext == ".cpp" || ext == ".java") {
                    std::string className = AssetPath::basename(sourcePath);
                    ComportamientoCargado comportamiento;
                    std::string compileError;
                    if (ScriptRuntime::compilarYCargar(sourcePath, className, comportamiento, compileError)) {
                        results.push_back({{"file", entry.path().filename().string()}, {"language", ext}, {"status", "ok"}, {"message", "Compilado y cargado correctamente"}});
                        compiled++;
                    } else {
                        results.push_back({{"file", entry.path().filename().string()}, {"language", ext}, {"status", "error"}, {"message", compileError}});
                        failed++;
                    }
                }
            }
        }
        imprimirJson(respuestaCorrecta({
            {"project", nombreProyecto},
            {"compiled", compiled},
            {"failed", failed},
            {"skipped", skipped},
            {"results", std::move(results)}
        }));
        return 0;
        }
        case ComandoCLI::DiagnosticsClear: {
        std::error_code ec;
        std::filesystem::remove(RegistroDiagnosticos::archivoPorDefecto(), ec);
        if (ec) {
            registrarErrorOperacional("clear_failed", "No se pudo borrar el archivo de diagnosticos: " + ec.message());
            imprimirJson(respuestaError("clear_failed", "No se pudo borrar el archivo de diagnosticos: " + ec.message()));
            return 5;
        }
        EventoDiagnostico evento;
        evento.nivel = NivelDiagnostico::Informacion;
        evento.modulo = "FunshiCLI";
        evento.mensaje = "Archivo de diagnosticos limpiado";
        RegistroDiagnosticos::global().registrar(std::move(evento));
        imprimirJson(respuestaCorrecta({{"message", "Archivo de diagnosticos limpiado"}}));
        return 0;
        }
        case ComandoCLI::DiagnosticsList: {
        std::size_t limite = 100;
        if (argumentos.contains("limit")) {
            const json& valorLimite = argumentos["limit"];
            if (!valorLimite.is_number_integer()) {
                registrarErrorUso("invalid_request", "args.limit debe ser un entero no negativo");
                imprimirJson(respuestaError("invalid_request", "args.limit debe ser un entero no negativo"));
                return 2;
            }
            if (valorLimite.is_number_unsigned()) {
                limite = static_cast<std::size_t>(
                    std::min<unsigned long long>(valorLimite.get<unsigned long long>(),
                                                 1000));
            } else {
                const long long valorEntero = valorLimite.get<long long>();
                if (valorEntero < 0) {
                    registrarErrorUso("invalid_request", "args.limit debe ser un entero no negativo");
                    imprimirJson(respuestaError("invalid_request", "args.limit debe ser un entero no negativo"));
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
        case ComandoCLI::AssetImport: {
            if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
                registrarErrorUso("invalid_request", "args.project (string) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido", reqId));
                return 2;
            }
            if (!argumentos.contains("source") || !argumentos["source"].is_string()) {
                registrarErrorUso("invalid_request", "args.source (string, ruta absoluta) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.source (string, ruta absoluta) es requerido", reqId));
                return 2;
            }
            const std::string nombreProyecto = argumentos["project"].get<std::string>();
            const std::string fuente = argumentos["source"].get<std::string>();
            std::string error;
            ProjectManager::Proyecto proyecto;
            if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
                registrarErrorOperacional("invalid_project", error);
                imprimirJson(respuestaError("invalid_project", error, reqId));
                return 3;
            }

            std::error_code ec;
            const fs::path origen(fuente);
            if (!fs::exists(origen, ec) || ec) {
                registrarErrorOperacional("source_not_found", "El archivo origen no existe: " + fuente);
                imprimirJson(respuestaError("source_not_found", "El archivo origen no existe", reqId));
                return 4;
            }
            if (!fs::is_regular_file(origen, ec) || ec) {
                registrarErrorOperacional("source_not_file", "El origen no es un archivo regular");
                imprimirJson(respuestaError("source_not_file", "El origen no es un archivo regular", reqId));
                return 4;
            }

            // Clasificacion por extension: modelo, textura o audio.
            const std::string ext = AssetPath::extension(fuente);
            std::string tipo;
            std::string subcarpeta;
            static const char* extModelo[] = {"obj", "fbx", "gltf", "glb", "dae", "blend"};
            static const char* extTextura[] = {"png", "jpg", "jpeg", "tga", "bmp", "hdr"};
            static const char* extAudio[] = {"wav", "ogg", "mp3", "flac"};
            if (std::find(std::begin(extModelo), std::end(extModelo), ext) != std::end(extModelo)) {
                tipo = "model";
                subcarpeta = "Modelos";
            } else if (std::find(std::begin(extTextura), std::end(extTextura), ext) != std::end(extTextura)) {
                tipo = "texture";
                subcarpeta = "Texturas";
            } else if (std::find(std::begin(extAudio), std::end(extAudio), ext) != std::end(extAudio)) {
                tipo = "audio";
                subcarpeta = "Sonidos";
            } else {
                registrarErrorUso("unsupported_extension", "Extension no soportada para importar: " + ext);
                imprimirJson(respuestaError("unsupported_extension", "Extension no soportada para importar: " + ext, reqId));
                return 2;
            }

            // El audio vive en src<proyecto>/Sonidos; modelo y textura en la raiz
            // de assets src<proyecto>/<subcarpeta>. Ambas cuelgan de directorioSrc.
            const fs::path dirDestino = fs::path(ProjectPaths::directorioSrc(nombreProyecto)) / subcarpeta;
            fs::create_directories(dirDestino, ec);
            if (ec) {
                registrarErrorOperacional("create_failed", "No se pudo crear el directorio destino: " + ec.message());
                imprimirJson(respuestaError("create_failed", "No se pudo crear el directorio destino", reqId));
                return 5;
            }
            const fs::path destino = dirDestino / origen.filename();
            fs::copy_file(origen, destino, fs::copy_options::overwrite_existing, ec);
            if (ec) {
                registrarErrorOperacional("copy_failed", "No se pudo copiar el asset: " + ec.message());
                imprimirJson(respuestaError("copy_failed", "No se pudo copiar el asset", reqId));
                return 5;
            }

            const std::string rutaRelativa =
                AssetPath::normalize(subcarpeta + "/" + origen.filename().string());
            registrarErrorOperacional("asset_imported", "Asset importado: " + rutaRelativa);
            imprimirJson(respuestaCorrecta({
                {"project", nombreProyecto},
                {"path", rutaRelativa},
                {"type", tipo},
                {"exists", true}
            }, reqId));
            return 0;
        }
        case ComandoCLI::SceneHierarchy: {
            if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
                registrarErrorUso("invalid_request", "args.project (string) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido", reqId));
                return 2;
            }
            const std::string nombreProyecto = argumentos["project"].get<std::string>();
            std::string error;
            ProjectManager::Proyecto proyecto;
            if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
                registrarErrorOperacional("invalid_project", error);
                imprimirJson(respuestaError("invalid_project", error, reqId));
                return 3;
            }

            SceneRegistry registry;
            PhysicsEngine physics;
            EventBus events;
            AssetManager assets(std::make_unique<AssimpMeshLoader>());
            EditorController editor(&registry, &physics, &events, &assets);
            editor.setAssetManager(&assets);

            SceneSerializer serializer(&registry, &editor, &assets);
            std::string pathTxt = ProjectPaths::rutaSceneBBDD(nombreProyecto);
            std::string semiPath = ProjectPaths::directorioBinarios(nombreProyecto);
            bool cargaCompleta = false;
            serializer.load(pathTxt, semiPath);
            editor.registerSceneRigidBodies();

            json hierarchy = sceneHierarchyJson(&registry);
            imprimirJson(respuestaCorrecta({
                {"project", nombreProyecto},
                {"root", hierarchy}
            }, reqId));
            return 0;
        }
        case ComandoCLI::ProjectFsCreate: {
            if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
                registrarErrorUso("invalid_request", "args.project (string) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido", reqId));
                return 2;
            }
            if (!argumentos.contains("name") || !argumentos["name"].is_string()) {
                registrarErrorUso("invalid_request", "args.name (string, nombre del directorio) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.name (string, nombre del directorio) es requerido", reqId));
                return 2;
            }
            const std::string nombreProyecto = argumentos["project"].get<std::string>();
            const std::string nombre = argumentos["name"].get<std::string>();
            std::string error;
            ProjectManager::Proyecto proyecto;
            if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
                registrarErrorOperacional("invalid_project", error);
                imprimirJson(respuestaError("invalid_project", error, reqId));
                return 3;
            }

            std::error_code ec;
            const fs::path raizFs = fs::path(ProjectPaths::directorioProyecto(nombreProyecto)) / "fs";
            const fs::path destino = raizFs / nombre;

            // No permitir que la ruta escape de <proyecto>/fs.
            const fs::path raizAbsoluta = fs::weakly_canonical(raizFs, ec);
            const fs::path destinoAbsoluto = fs::weakly_canonical(destino, ec);
            if (ec || destinoAbsoluto.string().find(raizAbsoluta.string()) != 0) {
                registrarErrorOperacional("invalid_path", "La ruta intenta escapar del directorio fs del proyecto");
                imprimirJson(respuestaError("invalid_path", "La ruta intenta escapar del directorio fs del proyecto", reqId));
                return 4;
            }

            fs::create_directories(destino, ec);
            if (ec) {
                registrarErrorOperacional("create_failed", "No se pudo crear el directorio: " + ec.message());
                imprimirJson(respuestaError("create_failed", "No se pudo crear el directorio", reqId));
                return 5;
            }

            registrarErrorOperacional("fs_dir_created", "Directorio fs creado: " + nombre);
            imprimirJson(respuestaCorrecta({
                {"project", nombreProyecto},
                {"name", nombre},
                {"type", "directory"},
                {"created", true}
            }, reqId));
            return 0;
        }
        case ComandoCLI::FileCreate: {
            if (!argumentos.contains("project") || !argumentos["project"].is_string()) {
                registrarErrorUso("invalid_request", "args.project (string) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.project (string) es requerido", reqId));
                return 2;
            }
            if (!argumentos.contains("name") || !argumentos["name"].is_string()) {
                registrarErrorUso("invalid_request", "args.name (string, nombre del archivo) es requerido");
                imprimirJson(respuestaError("invalid_request", "args.name (string, nombre del archivo) es requerido", reqId));
                return 2;
            }
            const std::string nombreProyecto = argumentos["project"].get<std::string>();
            const std::string nombre = argumentos["name"].get<std::string>();
            const std::string subruta = argumentos.value("path", std::string{});
            const std::string contenido = argumentos.value("content", std::string{});
            std::string error;
            ProjectManager::Proyecto proyecto;
            if (!resolverProyectoPorNombre(nombreProyecto, proyecto, error)) {
                registrarErrorOperacional("invalid_project", error);
                imprimirJson(respuestaError("invalid_project", error, reqId));
                return 3;
            }

            std::error_code ec;
            const fs::path raizFs = fs::path(ProjectPaths::directorioProyecto(nombreProyecto)) / "fs";
            const fs::path dirDestino = subruta.empty() ? raizFs : (raizFs / subruta);
            const fs::path destino = dirDestino / nombre;

            // No permitir que la ruta escape de <proyecto>/fs.
            const fs::path raizAbsoluta = fs::weakly_canonical(raizFs, ec);
            const fs::path destinoAbsoluto = fs::weakly_canonical(destino, ec);
            if (ec || destinoAbsoluto.string().find(raizAbsoluta.string()) != 0) {
                registrarErrorOperacional("invalid_path", "La ruta intenta escapar del directorio fs del proyecto");
                imprimirJson(respuestaError("invalid_path", "La ruta intenta escapar del directorio fs del proyecto", reqId));
                return 4;
            }

            fs::create_directories(dirDestino, ec);
            if (ec) {
                registrarErrorOperacional("create_failed", "No se pudo crear el directorio padre: " + ec.message());
                imprimirJson(respuestaError("create_failed", "No se pudo crear el directorio padre", reqId));
                return 5;
            }

            std::ofstream archivo(destino, std::ios::binary | std::ios::trunc);
            if (!archivo) {
                registrarErrorOperacional("create_failed", "No se pudo crear el archivo: " + nombre);
                imprimirJson(respuestaError("create_failed", "No se pudo crear el archivo", reqId));
                return 5;
            }
            archivo << contenido;
            archivo.close();

            registrarErrorOperacional("file_created", "Archivo creado en fs: " + nombre);
            imprimirJson(respuestaCorrecta({
                {"project", nombreProyecto},
                {"name", nombre},
                {"path", subruta},
                {"type", "file"},
                {"created", true}
            }, reqId));
            return 0;
        }
        case ComandoCLI::Desconocido: {
        const std::string mensaje = "Comando no encontrado: " + comandoStr;
        registrarErrorUso("unknown_command", mensaje);
        imprimirJson(respuestaError("unknown_command", mensaje, reqId));
        return 2;
        }
}
}
