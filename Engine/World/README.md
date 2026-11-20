# World

**Rôle :** couche d'intégration du moteur : entités, composants, transformations, et extraction vers le rendu (cahier des charges, sections 4.3 et 5.6). `Graphics` et `Physics` ne connaissent ni les entités ni EnTT.

**État (phase 7) :** composants du moteur, conversion des angles des fichiers de scène, recherches dans le registre, extraction vers le rendu, cadrage des ombres, liaison avec la physique (notes de décision 0015, 0017, 0019 et 0021).

## Interface publique

| En-tête | Contenu |
|---|---|
| `World/Components.h` | `SceneId`, `Name`, `Transform`, `PreviousTransform`, `MeshRenderer`, `ColliderShape`, `Collider`, `BodyType`, `RigidBody`, `PhysicsBody`, `CharacterBody`, `PhysicsCharacter`, `CharacterMotion`, `CharacterGround`, `Camera`, `DirectionalLight`, `SceneEnvironment` |
| `World/Rotation.h` | `rotationFromEulerDegrees()`, `eulerDegreesFromRotation()` |
| `World/SceneQueries.h` | `findBySceneId()`, `sceneEntityCount()`, `sceneEnvironment()` |
| `World/PhysicsSync.h` | `scaledShape()`, `createPhysicsObjects()`, `destroyPhysicsObjects()`, `stepPhysics()` |
| `World/RenderExtraction.h` | `rememberTransforms()`, `interpolateTransform()`, `drawnTransform()`, `extractRenderScene()` |
| `World/ShadowFocus.h` | `ShadowSettings`, `lightViewRotation()`, `frameShadow()`, `extractShadow()` |

## Registre et composants

- EnTT est utilisé directement, sans surcouche (section 5.6, décision 0015) : l'application crée un `entt::registry` et le passe par référence ; `World` n'en possède aucun (G-06). Seul `entt/entity/registry.hpp` est inclus.
- Les composants sont de simples données, sans logique :
  - `SceneId` : identifiant texte, unique et stable dans la scène ; `Name` : nom lisible, facultatif ;
  - `Transform` : position (`WorldPosition`), rotation (quaternion), échelle. Pas de parent : la hiérarchie est hors périmètre (section 2.2) ;
  - `PreviousTransform` : la transformation au pas fixe précédent, pour l'interpolation (section 5.1) ;
  - `MeshRenderer` : un modèle du cache de ressources et, au besoin, un matériau qui remplace celui de toutes ses parties (décision 0017) ;
  - `Collider` (boîte, sphère, capsule, centré sur l'entité) et `RigidBody` (statique ou dynamique, masse) : la forme et le type du corps que `Physics` simule pour l'entité ;
  - `CharacterBody` : capsule de personnage (section 5.8), pieds à la position de l'entité ; `CharacterMotion` : la vitesse voulue pour le prochain pas, écrite par le jeu ; `CharacterGround` : l'état au sol relu après le pas ;
  - `PhysicsBody` et `PhysicsCharacter` : les handles des objets créés dans `Physics`, attachés par `createPhysicsObjects`, jamais écrits dans une scène ;
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

## Cadrage des ombres (section 5.4, G-07, décision 0021)

- `extractShadow(registry, camera, focus, alpha, settings, scene)`, appelé après `extractRenderScene()`, remplit `scene.shadow` : une zone carrée vue de la lumière, de `areaSize` mètres de côté (40 par défaut) et `depthRange` mètres de profondeur (100), centrée sur la position interpolée de `focus` (le personnage). Sans `focus`, la zone est centrée sous la caméra. Sans lumière, ou avec `enabled` faux, les ombres sont coupées.
- Vue de la lumière (`lightViewRotation()`) : lignes droite, haut et direction vers la lumière ; la vue regarde dans le sens de la lumière, vers −Z. Le haut est le +Y du monde, ou −Z pour une lumière verticale.
- Calage sur les texels : en travers de la lumière, le centre est arrondi au multiple le plus proche d'un texel (`areaSize / mapSize`), en coordonnées monde. La grille des texels reste donc immobile quand le personnage avance, et les bords d'ombre ne scintillent pas.
- La matrice rendue (`RenderShadow::lightView`) va de l'espace de rendu à la vue de la lumière : la position de la caméra est soustraite du centre avant tout produit de matrices (G-07). `Graphics` y ajoute sa projection orthographique.
- Les biais (`constantBias` en mètres, `slopeBias`) sont recopiés tels quels pour la passe d'ombre.

## Liaison avec la physique (section 4.3, G-05, décision 0019)

`Physics` ne connaît ni les entités ni EnTT. `World` lui pousse les données des entités et recopie ses résultats.

- `createPhysicsObjects(registry, physics)` crée :
  - un corps pour chaque entité qui porte `Transform` et `Collider` : statique sans `RigidBody`, sinon du type et de la masse du `RigidBody` ;
  - un personnage pour chaque entité qui porte `Transform` et `CharacterBody`, et lui ajoute `CharacterMotion` et `CharacterGround`.
  Les entités qui ont déjà leur handle sont laissées telles quelles : la fonction peut être rappelée après un ajout d'entités. Elle s'arrête au premier échec, avec une erreur qui nomme l'entité.
- **Échelle.** `Physics` n'a pas d'échelle : `scaledShape()` multiplie les dimensions du collider par la valeur absolue de l'échelle de l'entité, une fois, à la création. Une sphère demande la même échelle sur les trois axes, une capsule la même sur X et Z ; une échelle nulle est refusée. Changer l'échelle ensuite ne change pas le corps.
- `stepPhysics(registry, physics, deltaSeconds)` fait un pas fixe :
  1. donne à chaque personnage la vitesse de son `CharacterMotion` ;
  2. avance `Physics` ;
  3. écrit la position et la rotation de chaque corps dynamique dans son `Transform` (l'échelle n'est pas touchée) ;
  4. écrit la position de chaque personnage dans son `Transform` et son état dans `CharacterGround`. La rotation d'un personnage reste celle que le jeu lui donne.
- Un corps statique garde la transformation de sa création : déplacer son `Transform` ensuite ne déplace pas le corps.
- L'interpolation ne change pas : `rememberTransforms` avant le pas, et l'extraction interpole les corps comme le reste.
- `destroyPhysicsObjects(registry, physics)` détruit les corps et les personnages créés et retire les handles : à appeler avant de vider le registre quand le monde physique survit à la scène.

## Recherches

- `findBySceneId()` parcourt les entités : à utiliser à la mise en place, pas à chaque frame.
- `sceneEntityCount()` compte les entités qui portent un `SceneId` : le « nombre d'entités » du panneau de debug.

## Dépendances

Autorisé : `Core`, `Resources`, `Graphics` (types de données), `Physics` (interface : `PhysicsWorld.h` et `PhysicsTypes.h`). Interdit : appels Vulkan, types Jolt. Bibliothèque : EnTT 4.0.0, en-têtes seuls (décision 0015).
