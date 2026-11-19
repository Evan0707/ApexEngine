# World

**Rôle :** couche d'intégration du moteur : entités, composants, transformations, et extraction vers le rendu (cahier des charges, sections 4.3 et 5.6). `Graphics` et `Physics` ne connaissent ni les entités ni EnTT.

**État (phase 5) :** composants du moteur, conversion des angles des fichiers de scène, recherches dans le registre, extraction vers le rendu (notes de décision 0015 et 0017). La liaison avec la physique arrive en phase 6.

## Interface publique

| En-tête | Contenu |
|---|---|
| `World/Components.h` | `SceneId`, `Name`, `Transform`, `PreviousTransform`, `MeshRenderer`, `ColliderShape`, `Collider`, `BodyType`, `RigidBody`, `Camera`, `DirectionalLight`, `SceneEnvironment` |
| `World/Rotation.h` | `rotationFromEulerDegrees()`, `eulerDegreesFromRotation()` |
| `World/SceneQueries.h` | `findBySceneId()`, `sceneEntityCount()`, `sceneEnvironment()` |
| `World/RenderExtraction.h` | `rememberTransforms()`, `interpolateTransform()`, `extractRenderScene()` |

## Registre et composants

- EnTT est utilisé directement, sans surcouche (section 5.6, décision 0015) : l'application crée un `entt::registry` et le passe par référence ; `World` n'en possède aucun (G-06). Seul `entt/entity/registry.hpp` est inclus.
- Les composants sont de simples données, sans logique :
  - `SceneId` : identifiant texte, unique et stable dans la scène ; `Name` : nom lisible, facultatif ;
  - `Transform` : position (`WorldPosition`), rotation (quaternion), échelle. Pas de parent : la hiérarchie est hors périmètre (section 2.2) ;
  - `PreviousTransform` : la transformation au pas fixe précédent, pour l'interpolation (section 5.1) ;
  - `MeshRenderer` : un modèle du cache de ressources et, au besoin, un matériau qui remplace celui de toutes ses parties (décision 0017) ;
  - `Collider` (boîte, sphère, capsule) et `RigidBody` (statique ou dynamique, masse) : lus et gardés dès la phase 5, sans effet avant `Physics` (phase 6) ;
  - `Camera` : champ de vision vertical, plans proche et lointain ; elle regarde le long du −Z de l'entité ;
  - `DirectionalLight` : le sens dans lequel la lumière voyage, couleur linéaire, intensité.
- `SceneEnvironment` (terme ambiant) n'appartient à aucune entité : il est dans le contexte du registre (`registry.ctx()`), lu par `sceneEnvironment()`.
- Les composants propres à la démonstration (contrôleur de caméra, rotation d'un objet) sont dans `Game`.

## Rotations

- Les fichiers de scène donnent les rotations en degrés autour de X, Y et Z (section 3.3) ; le moteur travaille en quaternions et en radians.
- Ordre d'application : Y, X, Z, c'est-à-dire lacet autour de la verticale du monde, puis tangage, puis roulis. C'est la convention de la caméra libre : un lacet positif tourne vers la gauche, un tangage positif regarde vers le haut.
- `eulerDegreesFromRotation()` fait le chemin inverse : tangage dans [−90 ; 90], lacet et roulis dans ]−180 ; 180]. À ±90° de tangage, lacet et roulis tournent autour du même axe : le roulis est rendu nul et l'orientation est conservée.

## Extraction vers le rendu (sections 4.3 et 5.1, G-01, G-07)

- `extractRenderScene(registry, cache, camera, alpha, scene)` remplit la `RenderScene` de copies : aucun pointeur vers une entité n'en sort, et `Graphics` ne voit jamais le registre.
- C'est le seul endroit où une position monde devient une position de rendu, relative à la caméra : la position de la caméra est soustraite avant tout produit de matrices, ce qui garde la précision loin de l'origine (testé à 10 km). La vue n'est qu'une rotation.
- Chaque entité qui porte `Transform` et `MeshRenderer` donne un objet par partie de son modèle, tous avec la même matrice. Un modèle pas prêt donne les parties du modèle de secours (cube en damier, G-03), et garde alors son damier même si l'entité a un matériau de remplacement.
- Caméra : l'entité donnée par l'appelant, avec `Transform` et `Camera`. Sans elle, la vue part de l'origine du monde avec les réglages par défaut.
- Lumière : la première `DirectionalLight` trouvée (une seule lumière directionnelle, section 5.5). Le composant donne le sens dans lequel la lumière voyage ; le renderer reçoit la direction opposée, vers la lumière. Sans lumière, seul le terme ambiant éclaire.
- Interpolation (section 5.1) : `rememberTransforms(registry)`, appelé au début de chaque pas fixe, copie chaque `Transform` dans `PreviousTransform`. L'extraction interpole entre les deux avec `alpha` (positions et échelles linéairement, rotations par le plus court chemin), caméra comprise. Une entité sans `PreviousTransform` est dessinée à sa transformation courante.

## Recherches

- `findBySceneId()` parcourt les entités : à utiliser à la mise en place, pas à chaque frame.
- `sceneEntityCount()` compte les entités qui portent un `SceneId` : le « nombre d'entités » du panneau de debug.

## Dépendances

Autorisé : `Core`, `Resources`, `Graphics` (types de données), `Physics` (interface, phase 6). Interdit : appels Vulkan, types Jolt. Bibliothèque : EnTT 4.0.0, en-têtes seuls (décision 0015).
