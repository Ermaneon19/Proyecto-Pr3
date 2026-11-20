# Código Crítico (C++ / GDExtension)

Arena survivor incremental en 3D, estilo Vampire Survivors. Todo el juego está
escrito en **C++ puro** con **godot-cpp** (GDExtension): no hay ni un script GDScript.
Las escenas `.tscn` solo contienen un nodo raíz cuyo tipo es una clase C++.

## Cómo abrirlo

1. Abre **Godot 4.3 o superior** e importa `project/project.godot`.
2. Pulsa **F5**.

Las librerías ya están compiladas en `project/bin/` para Linux y Windows (x86_64).
Los algoritmos y estructuras de datos de DeSiGNAR se incluyen como submódulo:
clona el repositorio con `git clone --recurse-submodules` o ejecuta
`git submodule update --init --recursive` antes de compilar.

## Cómo compilar

Requisitos (ya instalados en este WSL): `scons`, `g++` y `g++-mingw-w64-x86-64-posix`.

```bash
./build.sh            # Linux, debug
./build.sh windows    # Windows .dll, debug
./build.sh all        # Linux + Windows, debug + release
```

`godot-cpp/` es la rama `godot-4.3-stable`; la extensión funciona en Godot 4.3 y posteriores.

## Flujo de pantallas

| Escena | Clase C++ | Qué hace |
|---|---|---|
| `scenes/main_menu.tscn` | `MainMenu` | Inicio: **Iniciar Juego**, **Ver Ranking**, Armería, **Salir** |
| `scenes/register.tscn` | `RegisterScreen` | Campo de texto para nombre, equipo o alias |
| `scenes/game.tscn` | `Game` | La partida |
| `scenes/game_over.tscn` | `GameOverScreen` | Fin del juego, puntaje total, **Reintentar** / **Menú Principal** |
| `scenes/ranking.tscn` | `RankingScreen` | Los 10 mejores: nombre + puntaje |
| `scenes/shop.tscn` | `ShopScreen` | Armería: mejoras permanentes con núcleos |

Al morir, `Game::on_player_died()` llama a `GameData::add_run_result()`, que empareja
el nombre registrado con el puntaje final, lo inserta ordenado en el ranking y lo
guarda en `user://ranking.json` (los núcleos y mejoras van en `user://save.json`).
El ranking usa `merge_sort` de DeSiGNAR al cargar, ordenando por puntaje, tiempo,
nivel y bajas. Las estadísticas históricas por jugador se indexan con
`HashMap<std::string, size_t>` y se muestran en la pantalla de ranking.

**Puntaje** = bajas × 10 + élites × 150 + segundos × 2 + (nivel − 1) × 25.

## Controles

- **WASD / flechas**: moverse
- **Espacio / Shift**: impulso (invulnerable un instante, 2,5 s de recarga)
- **Esc / P**: pausa
- **1, 2, 3** o ratón: elegir mejora al subir de nivel
- Disparo automático al enemigo más cercano

## Habilidades (al subir de nivel)

Rifle: calibre, cadencia, ráfaga, perforación y **munición crítica**.
Habilidades: bombardeo orbital, drones de escolta, **pulso de choque**,
**rayo en cadena**, **misiles buscadores** y **escudo de energía**.

## Enemigos

Insecto (básico), **avispa** (rápida y frágil), **escarabajo** (lento y blindado),
**escupidor** (dispara ácido a distancia) e insecto **élite** cada minuto.

## Código (`src/`)

| Archivo | Contenido |
|---|---|
| `register_types.cpp` | Punto de entrada: registra las clases en Godot |
| `game.h/.cpp` | Mundo, oleadas, enemigos, armas, habilidades, colisiones, render |
| `batch.h/.cpp` | Dibujo por lotes con MultiMesh |
| `player.h/.cpp` | Soldado: modelo, animación, vida, escudo, impulso |
| `hud.h/.cpp` | Interfaz en partida, subida de nivel y pausa |
| `screens.h/.cpp` | Menú, registro, game over, ranking y armería |
| `game_data.h/.cpp` | Guardado, mejoras permanentes, ranking, teclas |
| `ui_util.h/.cpp` | Tema y piezas de interfaz compartidas |
| `mesh_util.h/.cpp` | Primitivas y materiales |

## Rendimiento

En la versión GDScript cada enemigo era un nodo con ~20 `MeshInstance3D`.
Ahora enemigos, balas, cristales y efectos son structs en vectores contiguos y se
dibujan con 7 MultiMesh (una llamada de dibujo cada uno), con un grid espacial
reconstruido cada frame para las colisiones. El límite subió de 180 a 320 enemigos.
La búsqueda espacial usa el `QuadTree` de DeSiGNAR. Para comparar la grilla de
referencia con el árbol, inicia el juego con `CC_SPATIAL_INDEX=grid`; el valor
predeterminado es `quadtree`. Usa el profiler de Godot bajo la misma escena y
duración para comparar ambos backends.

## Balance rápido

- Oleadas y tipos de enemigo: `Game::update_spawning()` y `Game::spawn_enemy()`
- Habilidades: `Game::update_weapons()` y la tabla `Game::UPGRADES`
- Precios de la Armería: `GameData::UPGRADES`

## Modo demo (para pruebas)

`CC_AUTOPLAY=1 godot --path project res://scenes/game.tscn` hace que el soldado
camine solo y elija mejoras; con `CC_AUTOPLAY=god` además no muere.
