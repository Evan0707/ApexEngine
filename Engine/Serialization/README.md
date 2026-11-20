# Serialization

**Rôle :** lecture et écriture des scènes et de la configuration en JSON (cahier des charges, sections 3.1, 5.2 et 5.6).

**État (phase 5) :** lecture et écriture d'une scène, version 1 du format, et lecture de la configuration (notes de décision 0016 et 0017).

## Interface publique

Aucun en-tête public n'expose de type JSON.

| En-tête | Contenu |
|---|---|
| `Serialization/SceneDescription.h` | `kSceneVersion`, `TransformDesc`, `MaterialDesc`, `CameraDesc`, `LightDesc`, `EntityDesc`, `SceneDescription` : une scène en données pures, dans les unités du fichier |
| `Serialization/SceneJson.h` | `parseScene(text, sourceName)`, `writeScene(scene)` |
| `Serialization/SceneLoader.h` | `instantiateScene()`, `describeScene()`, `loadSceneFile()` |
| `Serialization/EngineConfig.h` | `kConfigVersion`, `EngineConfig`, `parseEngineConfig()`, `loadEngineConfigFile()` |
| `Serialization/InputConfig.h` | `ButtonConfig`, `KeyAxisConfig`, `MouseAxisName`, `MouseAxisConfig`, `InputConfig`, `defaultInputConfig()`, `parseInputConfig()`, `loadInputConfigFile()` |

En-tête interne : `Serialization/Json.h`, seul fichier qui inclut nlohmann/json, avec les fonctions de lecture d'un champ et de construction des messages d'erreur.

## Lecture en deux temps

1. `parseScene()` transforme le texte en `SceneDescription`, en validant tout. En cas d'erreur, rien n'est rendu.
2. `instantiateScene()` crée les entités dans le registre, dans l'ordre du fichier, et place le terme ambiant dans le contexte du registre (`world::SceneEnvironment`).

`loadSceneFile()` enchaîne lecture du fichier, validation et instanciation. Un fichier invalide ne crée donc aucune entité.

Un modèle introuvable n'est pas une erreur de scène : le cache l'écrit dans le log et l'entité est dessinée avec le modèle de secours (AC-11).

## Écriture et aller-retour (section 7.4)

- `describeScene(registry, cache)` fait le chemin inverse d'`instantiateScene()` : les entités qui portent un `SceneId`, triées par identifiant (le registre n'a pas d'ordre propre), dans les unités du fichier. Le chemin d'un modèle vient du cache (`ResourceCache::modelPath()`) ; un modèle créé en code n'a pas de chemin, et son entité est décrite sans `mesh`.
- `writeScene(scene)` écrit une description dans le format lu par `parseScene()`. Les champs sont dans un ordre fixe et les nombres sous leur forme la plus courte qui redonne le même `float` (0,1 s'écrit `0.1`) : une même scène donne toujours le même texte.
- Garanties testées : texte → description → texte → description redonne la même description, exactement ; une scène passée par le registre revient identique, aux arrondis près sur les angles (qui passent par un quaternion) et le champ de vision (qui passe par des radians).
- La démonstration ne sauvegarde pas de scène : l'écriture sert aux tests et à de futurs outils.

## Format de scène, version 1

```json
{
  "version": 1,
  "ambient": [0.06, 0.07, 0.09],
  "entities": [
    {
      "id": "crate_01",
      "name": "Caisse",
      "transform": { "position": [2.0, 0.5, -3.0], "rotation_deg": [0, 45, 0], "scale": [1, 1, 1] },
      "mesh": "Models/crate.gltf",
      "material": { "base_color": [1, 0.8, 0.3], "metallic": 1.0, "roughness": 0.25 },
      "collider": { "shape": "box", "half_extents": [0.5, 0.5, 0.5] },
      "rigid_body": { "type": "dynamic", "mass": 20.0 }
    },
    { "id": "sun", "light": { "direction": [-0.4, -1.0, -0.35], "color": [1, 0.95, 0.88], "intensity": 3.0 } },
    { "id": "camera", "transform": { "position": [0, 2.5, 7], "rotation_deg": [-15, 0, 0] },
      "camera": { "fov_deg": 70, "near": 0.1, "far": 500 } }
  ]
}
```

| Champ | Règle |
|---|---|
| `version` | Obligatoire, entier, égal à 1 |
| `ambient` | Trois nombres positifs ou nuls ; 0,03 par défaut |
| `entities` | Obligatoire, tableau |
| `id` | Obligatoire, chaîne non vide, unique dans la scène |
| `name` | Chaîne non vide |
| `transform` | `position`, `rotation_deg`, `scale` : trois nombres chacun, tous facultatifs. Chaque composante de `scale` est strictement positive |
| `rotation_deg` | Degrés autour de X, Y et Z, appliqués dans l'ordre Y, X, Z : lacet, tangage, roulis (voir `Engine/World/README.md`) |
| `mesh` | Chemin d'asset d'un modèle, normalisé à la lecture. Une entité avec `mesh` et sans `transform` est placée à l'origine |
| `material` | `base_color` (trois nombres positifs ou nuls), `metallic`, `roughness` (dans [0 ; 1]), tous facultatifs. Demande un `mesh` ; remplace le matériau de tout le modèle |
| `collider` | `shape` obligatoire : `box` avec `half_extents`, `sphere` avec `radius`, `capsule` avec `radius` et `half_height`. Dimensions strictement positives, en mètres |
| `rigid_body` | `type` obligatoire : `static` ou `dynamic`. `mass` en kilogrammes, strictement positive, obligatoire pour `dynamic`. Demande un `collider` |
| `camera` | `fov_deg` (vertical, entre 0 et 180 exclus), `near` (positif), `far` (supérieur à `near`), tous facultatifs |
| `light` | `direction` obligatoire, non nulle : le sens dans lequel la lumière voyage. `color` (positive ou nulle), `intensity` (positive ou nulle) |

## Erreurs

- Toute erreur nomme le fichier, l'entité et le champ (section 5.6) : `Scene 'Scenes/demo.json', entity 'crate_01', field 'transform.position': expected an array of 3 numbers`.
- Tant que son `id` n'est pas lu, une entité est désignée par sa place dans le fichier : `#3` est la quatrième.
- Un champ inconnu est une erreur, à tous les niveaux (décision 0017) : `postion` ne passe pas inaperçu. Les champs d'une autre forme de collider sont des champs inconnus.
- Un JSON mal formé donne le message de la bibliothèque, avec la ligne et la colonne. L'exception qu'elle lève est interceptée dans ce module (section 3.1).
- Un nombre trop grand pour un `float` est refusé.

## Configuration (section 5.2, décision 0017)

Deux fichiers dans `Config/`, chacun avec un champ `version` obligatoire, égal à 1. Mêmes règles que pour la scène : champs inconnus refusés, erreur qui nomme le fichier et le champ (`Configuration 'Config/input.json', field 'buttons[2].key': missing`).

`engine.json` : tous les champs sont facultatifs, et un champ absent garde la valeur du cahier des charges.

| Champ | Valeur par défaut | Règle |
|---|---|---|
| `window.width`, `window.height` | 1920, 1080 | Entiers, de 1 à 16384 pixels |
| `free_camera.speed` | 5 | Mètres par seconde, strictement positif |
| `free_camera.sprint_multiplier` | 4 | Strictement positif |
| `free_camera.look_sensitivity` | 0,0025 | Radians par pixel de souris, strictement positif |
| `physics.gravity` | [0, −9,81, 0] | Trois nombres, en m/s² (section 5.7) |
| `character.height`, `character.radius` | 1,8 et 0,3 | Capsule du personnage en mètres (section 5.8) ; la hauteur dépasse deux fois le rayon |
| `character.walk_speed`, `character.run_speed` | 4 et 7 | Mètres par seconde, strictement positifs |
| `character.jump_height` | 1,2 | Mètres, strictement positif |
| `character.max_slope_deg` | 45 | Degrés, entre 0 et 90 exclus |
| `character.step_height` | 0,3 | Mètres, positif ou nul |
| `character.push_force` | 100 | Newtons, positif ou nul : la force maximale exercée sur un corps poussé |
| `third_person_camera.distance` | 4 | Mètres entre le point visé et la caméra (section 5.3), strictement positif |
| `third_person_camera.aim_height` | 1,6 | Hauteur du point visé au-dessus des pieds du personnage, en mètres, positive ou nulle |

`input.json` : la table d'actions.

| Tableau | Élément |
|---|---|
| `buttons` | `action`, `key` |
| `key_axes` | `action`, `negative`, `positive` |
| `mouse_axes` | `action`, `axis` (`"x"` ou `"y"`), `scale` (facultatif, 1 par défaut) |

- Les touches sont des noms de touches physiques (`Space`, `LeftShift`, `W`) : en AZERTY, Z, Q et A sont aux places de W, A et Q (section 3.3).
- `Serialization` ne connaît ni `Input` ni `Platform` (section 4.2) : il rend une `InputConfig` faite de chaînes. C'est `Game` qui la convertit en table d'actions, et c'est là qu'un nom de touche inconnu est refusé.
- `defaultInputConfig()` est la table de la section 5.2, utilisée quand `input.json` est absent. Un test vérifie que les deux fichiers du dépôt sont valides et égaux aux valeurs par défaut.
- `loadEngineConfigFile()` et `loadInputConfigFile()` renvoient `NotFound` quand le fichier n'existe pas : l'appelant choisit alors les valeurs par défaut. Toute autre erreur est une configuration invalide.

## Dépendances

Autorisé : `Core`, `World`, `Resources`. Interdit : Vulkan, Jolt. Bibliothèque : nlohmann/json 3.12.0 (décision 0016), incluse par `Serialization/Json.h` uniquement, conversions implicites désactivées. EnTT par `World`.
