# Proyecto-Pr3

Base de un proyecto C++17 con SFML 2.6.1 y Dear ImGui mediante ImGui-SFML.
Al iniciar, muestra un enemigo que persigue el cursor como objetivo temporal.

## Estructura

- `src/`: implementacion del juego, el punto de entrada y el gestor de recursos.
- `include/`: encabezados publicos del proyecto.
- `assets/`: sprites, fuentes y sonidos del juego.
- `CMakeLists.txt`: descarga y configura las dependencias, y define el ejecutable.

SFML, Dear ImGui e ImGui-SFML se descargan con CMake en `build/_deps`; no es
necesario copiar dependencias a `vendor/`. La primera configuracion requiere
conexion a Internet.

## Requisitos

- CMake 3.16 o posterior.
- Un compilador compatible con C++17 (GCC, Clang o MSVC).
- Git.
- Las dependencias de desarrollo de SFML para el sistema operativo.

## Compilar y ejecutar

Linux y macOS:

```sh
cmake -S . -B build
cmake --build build --parallel
./build/ProyectoPr3
```

Windows (PowerShell, generador predeterminado de Visual Studio):

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\ProyectoPr3.exe
```

En Linux, aseguren tener instaladas las dependencias de desarrollo de SFML y
OpenGL requeridas por el sistema antes de configurar. En Windows, CMake necesita
una instalacion de Visual Studio con el workload de desarrollo de escritorio
C++.

## Trabajo en equipo

Cada integrante puede clonar el repositorio y ejecutar los mismos comandos de
CMake. La carpeta `build/` y los artefactos del compilador estan ignorados por
Git; solo se deben compartir los archivos fuente y los recursos del juego.