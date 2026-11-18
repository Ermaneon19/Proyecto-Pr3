# Código Crítico (Godot 4)

Arena survivor incremental en 3D. Port del prototipo en SFML/C++.

## Cómo abrirlo
1. Instala **Godot 4.3 o superior** (versión estándar, no hace falta la de C#).
2. En el Project Manager: **Import** → elige `project.godot` de esta carpeta.
3. Pulsa **F5** para jugar.

Al abrir `scenes/main.tscn`, el editor muestra una vista previa del escenario,
el jugador y algunos enemigos. Es una vista generada por código para orientar la
edición; sus objetos no se guardan como nodos editables en la escena.

## Controles
- **WASD / flechas**: moverse
- Disparo **automático** al enemigo más cercano
- Al subir de nivel eliges 1 de 3 mejoras (con el ratón)

## Bucle incremental
- Los enemigos sueltan **cristales** (XP, cian) y a veces **núcleos** (amarillo).
- Al morir, los núcleos se guardan (`user://save.json`) y se gastan en la **Armería**
  en mejoras permanentes: daño, cadencia, vida, velocidad, imán, % de núcleos y regeneración.
- Cada minuto aparece un **insecto élite** que suelta muchos núcleos.

## Estructura
| Archivo | Equivale en C++ | Qué hace |
|---|---|---|
| `scripts/main.gd` | `Game.cpp` | Mundo, cámara, oleadas, colisiones (grid espacial), XP/niveles |
| `scripts/enemy.gd` | `Enemy.cpp` | Insecto mecánico 3D: persigue, aletea, guadañas, parpadeo al recibir daño |
| `scripts/pickup.gd` | `Core.cpp` | Cristales de XP y núcleos, con imán |
| `scripts/player.gd` | (nuevo) | Soldado, movimiento, rifle, bombardeo orbital, drones |
| `scripts/bullet.gd` | `mockProjectile` | Balas con perforación |
| `scripts/hud.gd` | (nuevo) | HUD estilo Vampire Survivors, panel de nivel y tienda |
| `scripts/game_data.gd` | (nuevo, autoload) | Guardado, moneda y mejoras permanentes |
| `AssetManager.cpp` | — | Ya no hace falta: Godot cachea los recursos con `load()` / `preload()` |

## Cambiar los modelos por modelos reales
Los personajes están hechos con primitivas en código (`_build_model()` en `player.gd` y `enemy.gd`).
Para usar un modelo de Blender/Mixamo: exporta en `.glb`, arrástralo a `res://assets/`
y en `_build_model()` reemplaza las piezas por:
```gdscript
var m = preload("res://assets/soldado.glb").instantiate()
_model.add_child(m)
```

## Balance rápido
- Dificultad / oleadas: `_spawn_tick()` en `main.gd` y `setup()` en `enemy.gd`
- Experiencia por nivel: `xp_next` en `main.gd`
- Precios de mejoras: `UPGRADES` en `game_data.gd`
