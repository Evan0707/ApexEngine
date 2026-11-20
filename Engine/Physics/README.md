# Physics

**Rôle :** simulation physique du moteur, par encapsulation de Jolt Physics (cahier des charges, sections 5.7 et 5.8). Le module ne connaît ni les entités ni EnTT : `World` lui pousse des données et recopie les résultats (section 4.3).

**État (phase 6, tâche 3) :** état global de Jolt, monde physique, gravité, pas fixe, corps rigides statiques et dynamiques, raycast et spherecast (notes de décision 0018, 0019 et 0020). Le personnage arrive dans la tâche suivante.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Physics/Runtime.h` | `Runtime` : crée et détruit l'état global de Jolt |
| `Physics/PhysicsSettings.h` | `PhysicsSettings` : gravité, capacités |
| `Physics/PhysicsTypes.h` | `BodyHandle`, `ShapeType`, `Shape`, `MotionType`, `BodyTransform`, `BodyDesc`, `QueryFilter`, `CastHit` |
| `Physics/PhysicsWorld.h` | `PhysicsWorld` : `create()`, `step()`, `gravity()`, `bodyCount()`, `activeBodyCount()`, `createBody()`, `destroyBody()`, `contains()`, `transform()`, `setTransform()`, `linearVelocity()`, `setLinearVelocity()`, `applyImpulse()`, `isActive()`, `raycast()`, `spherecast()` |

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

## Corps rigides

- Formes (section 5.7) : boîte (demi-dimensions), sphère (rayon), capsule debout le long de Y (rayon et demi-hauteur entre les centres des deux sphères). Dimensions en mètres, centrées sur la position du corps. `Physics` ne connaît pas l'échelle : `World` l'applique avant (décision 0019).
- Un corps est statique (il ne bouge jamais) ou dynamique (masse en kilogrammes, déplacé par la gravité, les contacts et les impulsions). La couche de collision vient de ce type.
- `createBody` valide la description : dimensions et masse strictement positives, valeurs finies. Une description invalide donne `InvalidArgument` avec la raison ; un monde plein donne `PhysicsFailure`.
- **Handles.** Un `BodyHandle` désigne un corps. Après `destroyBody`, le handle est périmé : toutes les fonctions l'acceptent et ne font rien, `transform()` rend une valeur vide. Jolt ne garde que 8 bits de génération : un emplacement réutilisé 256 fois peut faire correspondre un vieux handle au nouveau corps.
- `setTransform` téléporte, sans balayage. `setLinearVelocity` et `applyImpulse` (en newtons-secondes, au centre de masse) réveillent un corps dynamique et ignorent un corps statique.
- Un corps au repos s'endort après une demi-seconde environ ; `activeBodyCount()` compte ceux qui ne dorment pas.
- Les corps dynamiques sont balayés (`LinearCast`) : un corps petit ou rapide ne traverse pas une géométrie mince (AC-08). Un corps gros et rapide peut s'enfoncer pendant un pas avant d'être repoussé.
- Jolt laisse les corps au repos se recouvrir de 2 cm au plus (sa tolérance de pénétration, gardée par défaut pour la stabilité des piles) : une caisse posée repose jusqu'à 2 cm sous sa hauteur théorique.
- Frottement 0,5 et restitution 0 par défaut, réglables par corps dans `BodyDesc`.

## Requêtes

- `raycast(origine, direction, distance maximale, filtre)` rend le premier corps touché ; `spherecast` fait de même avec une sphère dont le centre part de l'origine (section 5.7).
- La direction n'a pas besoin d'être unitaire ; la distance maximale est en mètres. Une direction nulle, une distance ou un rayon négatifs ou nuls, une valeur non finie : pas de résultat.
- `CastHit` donne le corps, l'endroit où la requête s'arrête (le point touché pour un rayon, le centre de la sphère pour un balayage), le point de contact sur le corps, la normale unitaire qui sort du corps, et la distance parcourue.
- Une requête qui démarre à l'intérieur d'un corps le touche à la distance zéro.
- `QueryFilter` choisit les couches : corps statiques, corps dynamiques, ou les deux (par défaut). Un personnage n'est pas un corps : aucune requête ne le touche.

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

`Core` uniquement. Bibliothèque tierce : Jolt Physics 5.6.0, liée en privé par sa cible CMake ; `Jolt.dll` est copiée à côté des exécutables par vcpkg. Jolt est compilé en flottants précis par le triplet `cmake/triplets/x64-windows.cmake` : avec ses options par défaut, sa version Release est instable avec MSVC 19.52 (décision 0020).

## Tests

`Tests/Physics/`, étiquette `[physics]`, sans fenêtre ni GPU (section 5.7). Ils couvrent AC-08 (un corps lâché de 10 m ne traverse pas le sol) et AC-09 (une pile de trois caisses reste en place 10 s), et tournent en Debug comme en Release.
