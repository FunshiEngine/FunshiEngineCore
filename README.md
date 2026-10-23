# FunshiEngineGL

Motor y editor de juegos en tiempo real escrito en C++17, con interfaz Dear ImGui y arquitectura modular (backends intercambiables).

Este repositorio define el **contrato de render** —qué recursos, estados y uniformes necesita el motor— y deja la implementación gráfica en manos de un backend. Por defecto el motor arranca con el backend en blanco: define el contrato y no dibuja. Un backend de verdad (por ejemplo, uno sobre Vulkan) se instala antes de arrancar el bucle.

**Licencia:** [Apache License 2.0](LICENSE).

## Compilación

```bash
cmake -S FunshiEngineGL -B FunshiEngineGL/build \
    -DCMAKE_BUILD_TYPE=Release -DENABLE_ASAN=OFF -DFUNSHI_JAVA=ON
cmake --build FunshiEngineGL/build -j$(nproc)
```

## Ejecución

```bash
cd FunshiEngineGL && ./build/FunshiEngineGL
```

## Documentación

- [MANUAL_DE_USO.md](MANUAL_DE_USO.md) – Guía de uso del editor y scripting
- [PROJECT_STRUCTURE.md](PROJECT_STRUCTURE.md) – Arquitectura y estructura del proyecto
- [FLUJO_DE_RAMAS.md](FLUJO_DE_RAMAS.md) – Convención de ramas
- [DocuTecnicoBugs.md](DocuTecnicoBugs.md) – Registro técnico de bugs
