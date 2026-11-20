# Physics

**Rôle :** simulation physique du moteur, par encapsulation de Jolt Physics (cahier des charges, sections 5.7 et 5.8). Le module ne connaît ni les entités ni EnTT : `World` lui pousse des données et recopie les résultats (section 4.3).

**État (phase 6, tâche 1) :** état global de Jolt, monde physique vide, gravité, pas fixe (notes de décision 0018 et 0019). Les corps, les requêtes et le personnage arrivent dans les tâches suivantes.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Physics/Runtime.h` | `Runtime` : crée et détruit l'état global de Jolt |
| `Physics/PhysicsSettings.h` | `PhysicsSettings` : gravité, capacités |
| `Physics/PhysicsWorld.h` | `PhysicsWorld` : `create()`, `step()`, `gravity()`, `bodyCount()` |

Aucun de ces en-têtes n'inclut Jolt ni n'expose un type Jolt.

## État global de Jolt

- Jolt garde un état pour tout le processus : son allocateur, sa fabrique de types et sa table de types. `Runtime` le crée et le détruit (décision 0019).
- L'application possède un seul `Runtime`, créé avant tout `PhysicsWorld` et détruit après. `PhysicsWorld::create` le reçoit en paramètre : on ne peut pas créer un monde sans lui.
- Le moteur n'ajoute aucune variable statique (G-06). Un second `Runtime` vivant en même temps est une erreur de programmation, détectée par une assertion sur l'état de Jolt.
- `Runtime::create` échoue si la bibliothèque Jolt installée n'a pas été compilée avec les mêmes options que le moteur.
- Les fonctions de trace et d'assertion de Jolt gardent leur valeur par défaut. En Debug, une assertion de Jolt arrête le programme sans passer par le logger.

## Monde physique

- `PhysicsWorld::step(deltaSeconds)` avance d'un pas fixe (G-05) : un pas de collision par appel. La durée vient de la boucle principale, 1/60 s (section 5.1).
- Gravité : −9,81 m/s² sur Y par défaut, donnée par `PhysicsSettings` (section 5.7).
- Capacités fixes, données à la création : 1 024 corps, 4 096 paires, 2 048 contraintes de contact par défaut.
- Un seul thread (décision 0018) : le résultat est le même d'une exécution à l'autre.
- Repère et unités : ceux du moteur, identiques à ceux de Jolt (section 3.3). Simple précision.

## Couches de collision

| Couche | Touche |
|---|---|
| Statique | Dynamique, personnage |
| Dynamique | Statique, dynamique, personnage |
| Personnage | Statique, dynamique |

## Fichiers internes

- `Physics/JoltInclude.h` : premier en-tête Jolt de chaque source du module, et conversions entre GLM et Jolt.
- `Physics/PhysicsWorldState.h` : état privé de `PhysicsWorld`, partagé par ses sources.

## Dépendances

`Core` uniquement. Bibliothèque tierce : Jolt Physics 5.6.0, liée en privé par sa cible CMake ; `Jolt.dll` est copiée à côté des exécutables par vcpkg.

## Tests

`Tests/Physics/`, étiquette `[physics]`, sans fenêtre ni GPU (section 5.7).
