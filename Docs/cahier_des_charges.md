# Cahier des charges — Prototype de moteur 3D

**Nom de travail :** Apex Engine
**Version :** 0.3 (remplace la 0.2)
**Date :** 19 novembre 2026
**Statut :** spécification figée pour démarrage
**Objectif :** livrer un prototype 3D complet et robuste, première fondation d'un futur moteur orienté monde ouvert.

---

## 0. Historique des changements

### 0.1 De la v0.2 à la v0.3

| Sujet | v0.2 | v0.3 |
|---|---|---|
| Rendu | Ambiant constant, sortie directe en sRGB | HDR avec tonemapping, IBL et skybox ajoutés en P1 |
| Optimisations futures | Non traitées | Dix garde-fous obligatoires (4.4) et liste des optimisations reportées, avec leur niveau de certitude (4.5) |
| Données par objet | Matrice en push constant | Buffer par frame indexé par instance : l'instancing reste un ajout local |
| Ressources | Handles | Handles avec état, résolus vers l'asset de secours tant qu'ils ne sont pas prêts |
| Positions monde | `glm::vec3` | Type `WorldPosition` et rendu relatif à la caméra |
| Critères | 16 critères de livraison, 4 critères P1 | 17 critères de livraison (ajout des garde-fous), 6 critères P1 |
| Planning | 375 à 550 heures | 415 à 605 heures |

### 0.2 De la v0.1 à la v0.2

| Sujet | v0.1 | v0.2 |
|---|---|---|
| Décisions techniques | Laissées ouvertes (section 14) | Toutes figées (section 3) |
| Priorités | Scène depuis un fichier et corps rigides en P1, mais exigés par les critères | Passés en P0 ; les ombres restent P1 et sortent du parcours obligatoire |
| Couche RHI | Prévue | Supprimée : une seule API, isolée dans `Graphics` |
| Import de modèles | Assimp ou cgltf | cgltf (glTF uniquement) |
| Matériaux | Blinn-Phong ou PBR | PBR metallic-roughness simplifié |
| Bibliothèques | Liste incomplète | Ajout de vk-bootstrap, VMA, nlohmann/json, glslc, vcpkg |
| Critères d'acceptation | Qualitatifs, statut flou pour AC-13 et AC-14 | Mesurables, deux niveaux clairs (livraison, bonus P1) |
| Validation GPU | Non mentionnée | Zéro message des validation layers exigé |
| Planning | En semaines, pour un profil non précisé | En heures, converti selon le rythme réel |
| Agents IA | Sept agents spécialisés | Un seul agent, tâche par tâche |
| Audio, hiérarchie parent/enfant | P2 ou « si possible » | Hors périmètre |

---

## 1. Objectif et résultat attendu

### 1.1 Objectif

Développer un moteur 3D propriétaire, modulaire, capable d'exécuter une petite scène interactive en temps réel. Le prototype prouve la maîtrise des briques de base : boucle principale, fenêtre, rendu, caméra, entités, ressources, entrées, physique simple et outils de diagnostic.

Il ne s'agit pas de reproduire un moteur AAA. Le prototype ne valide aucune brique propre au monde ouvert (streaming, LOD, grandes coordonnées) : il valide la fondation sur laquelle elles seront construites.

### 1.2 Résultat attendu

Une application qui se lance et affiche une scène contenant :

- un sol étendu avec rampes et plateformes ;
- des objets 3D importés depuis des fichiers glTF ;
- un personnage contrôlable ;
- une caméra libre et une caméra à la troisième personne ;
- un éclairage directionnel ;
- des collisions et des corps rigides ;
- un panneau de diagnostic ;
- une scène décrite dans un fichier JSON, modifiable sans recompiler.

### 1.3 Principes

- Le cœur du moteur est écrit en interne ; les bibliothèques tierces couvrent les fonctions spécialisées.
- Chaque bibliothèque tierce est confinée à un seul module (voir 4.2).
- Le moteur ne contient aucune règle propre au jeu de démonstration.
- Fiabilité et lisibilité passent avant le nombre de fonctionnalités.
- Aucune abstraction n'est créée pour un besoin hypothétique. Seule exception : les garde-fous de la section 4.4, peu coûteux aujourd'hui et très coûteux à ajouter après coup.
- Une optimisation qui peut attendre n'est pas faite maintenant, à condition qu'un garde-fou garantisse son report (section 4.5).
- Chaque fonctionnalité a un test automatisé ou une procédure de validation écrite.

---

## 2. Périmètre

### 2.1 Fonctionnalités

| Domaine | Fonctionnalité | Priorité |
|---|---|---|
| Core | Initialisation, boucle à pas fixe, arrêt propre | P0 |
| Core | Logs et gestion d'erreurs | P0 |
| Platform | Fenêtre redimensionnable, clavier, souris | P0 |
| Input | Système d'actions configurable par fichier | P0 |
| Graphics | Rendu 3D Vulkan, profondeur, culling des faces arrière | P0 |
| Graphics | Matériaux PBR simplifiés et textures | P0 |
| Graphics | Lumière directionnelle et terme ambiant | P0 |
| Resources | Import glTF 2.0 / GLB et textures PNG, JPG | P0 |
| Resources | Cache, handles et assets de secours | P0 |
| World | Entités, composants, transformations | P0 |
| Serialization | Chargement d'une scène JSON au démarrage | P0 |
| Physics | Colliders boîte, sphère, capsule | P0 |
| Physics | Gravité et corps rigides dynamiques | P0 |
| Gameplay | Personnage contrôlable et caméra troisième personne | P0 |
| Debug | Panneau de statistiques et console de logs | P0 |
| Graphics | Ombres directionnelles (shadow mapping) | P1 |
| Graphics | HDR et tonemapping | P1 |
| Graphics | IBL (éclairage par environnement) et skybox | P1 |
| Graphics | Frustum culling, mipmaps, normal maps | P1 |
| Debug | Affichage filaire des colliders | P1 |
| Debug | Temps GPU par frame | P1 |
| Platform | Manette | P2 |
| Graphics | Rechargement à chaud des shaders | P2 |

**P0 :** indispensable à la livraison.
**P1 :** engagé seulement quand tous les critères P0 sont validés, dans l'ordre du tableau.
**P2 :** optionnel, ne doit jamais retarder la livraison.

### 2.2 Hors périmètre

- monde ouvert, streaming, LOD et HLOD ;
- animation squelettique, IK, motion matching ;
- hiérarchie parent/enfant des transformations ;
- audio ;
- post-traitement autre que le tonemapping, MSAA ;
- ombres en cascade, lumières ponctuelles multiples ;
- instancing, bindless, rendu piloté par le GPU, multithreading du moteur (reportés, voir 4.5) ;
- plusieurs API graphiques ou couche d'abstraction RHI ;
- scripting, éditeur de niveaux ;
- réseau et multijoueur ;
- IA, véhicules, destruction, météo, cycle jour/nuit ;
- Linux, macOS, consoles.

---

## 3. Décisions techniques figées

Toute modification d'une décision ci-dessous passe par une note de décision dans `Docs/decisions/`.

### 3.1 Socle

| Sujet | Décision | Raison |
|---|---|---|
| Plateforme | Windows 10/11 x64 | Une seule cible pour le prototype |
| Langage | C++23, option `/std:c++23` | Standard complet dans MSVC depuis novembre 2026 |
| Compilateur | MSVC Build Tools 14.52 ou plus récent | Première version où C++23 est prêt pour la production |
| IDE | Visual Studio 2026, version 18.12 ou plus récente | Fournit le compilateur ci-dessus |
| Build | CMake avec `CMakePresets.json`, générateur Ninja | Configuration reproductible en une commande |
| Dépendances | vcpkg en mode manifeste, baseline épinglée | Versions identiques sur toutes les machines |
| API graphique | Vulkan 1.3 minimum, sans couche RHI | Dynamic rendering et synchronization2 dans le cœur de l'API |
| Shaders | GLSL 4.60 compilé en SPIR-V par `glslc` pendant le build | Une erreur de shader fait échouer la compilation |
| Configuration et scènes | JSON, avec champ `version` | Un seul format, versionné dès le départ |
| Erreurs | `std::expected<T, Error>` pour les échecs récupérables, assertions pour les erreurs de programmation | Pas d'exceptions dans l'API du moteur |
| Logs | Logger interne basé sur `std::format`, sortie console, fichier et panneau ImGui, utilisable depuis plusieurs threads | Aucune dépendance supplémentaire |
| Avertissements | `/W4 /permissive-`, zéro avertissement dans `Engine/` et `Game/` | Garde-fou sur le code généré |
| Formatage | clang-format, vérifié en CI | Homogénéité |

Les exceptions levées par une bibliothèque tierce sont interceptées à la frontière du module qui l'utilise et converties en `Error`.

### 3.2 Bibliothèques

| Besoin | Bibliothèque | Licence | Module qui l'utilise |
|---|---|---|---|
| Fenêtre et entrées | SDL3 (branche 3.4) | zlib | Platform |
| Initialisation Vulkan | vk-bootstrap | MIT | Graphics |
| Mémoire GPU | Vulkan Memory Allocator (VMA) | MIT | Graphics |
| Mathématiques | GLM | MIT | Tous |
| Import de modèles | cgltf | MIT | Resources |
| Images | stb_image | MIT ou domaine public | Resources |
| Physique | Jolt Physics 5.x | MIT | Physics |
| Interface de debug | Dear ImGui (backends SDL3 et Vulkan) | MIT | Debug |
| ECS | EnTT | MIT | World, Game |
| JSON | nlohmann/json | MIT | Serialization |
| Tests | Catch2 v3 | BSL-1.0 | Tests |
| Profilage | Tracy | BSD-3-Clause | Tous, par macros |

Le SDK Vulkan de LunarG est installé séparément : il fournit les validation layers et `glslc`. Les licences sont à revérifier au moment de l'intégration et copiées dans `ThirdPartyLicenses.md`.

Outils : RenderDoc (inspection des frames), Tracy (profilage CPU), CTest (tests), Git, GitHub Actions (CI).

### 3.3 Conventions

| Sujet | Convention |
|---|---|
| Repère | Main droite, +Y vers le haut, identique à glTF 2.0 et à Jolt : aucun changement de repère à l'import |
| Unités | Mètre, kilogramme, seconde ; radians en interne, degrés dans les fichiers JSON |
| Espace de clip Vulkan | Profondeur 0 à 1 et inversion de Y traitées uniquement dans la matrice de projection |
| Faces avant | Sens antihoraire, comme glTF |
| Couleurs | Calculs en espace linéaire, textures de couleur en format sRGB, swapchain sRGB |
| Chemins d'assets | Relatifs au dossier `Assets/`, séparateur `/`, aucun chemin absolu |
| Touches | Codes physiques (scancodes), pour un comportement identique en AZERTY et en QWERTY |

---

## 4. Architecture

### 4.1 Arborescence

```text
ApexEngine/
├── Engine/                  bibliothèque statique
│   ├── Core/                application, boucle, temps, logs, erreurs
│   ├── Platform/            fenêtre et événements bruts (SDL3)
│   ├── Input/               actions et bindings
│   ├── Resources/           cache, handles, import glTF et textures
│   ├── Graphics/            renderer Vulkan
│   │   └── Shaders/
│   ├── Physics/             encapsulation de Jolt
│   ├── World/               entités, composants, extraction vers rendu et physique
│   ├── Serialization/       scènes et configuration JSON
│   └── Debug/               ImGui, panneaux, dessin de debug
├── Game/                    exécutable de démonstration
├── Assets/
│   ├── Models/
│   ├── Textures/
│   └── Scenes/
├── Config/                  input.json, engine.json
├── Tests/
├── Tools/                   scripts Python
├── Docs/                    architecture.md, benchmark.md, decisions/
├── .github/workflows/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── AGENTS.md
├── ThirdPartyLicenses.md
└── README.md
```

### 4.2 Règles de dépendance

Ces règles sont vérifiables par simple recherche d'`#include` et font partie de la revue de code.

| Module | Peut dépendre de | Interdit |
|---|---|---|
| Core | Bibliothèque standard, GLM | Tout autre module |
| Platform | Core | Vulkan, Jolt, EnTT |
| Input | Core, Platform | Toute règle de gameplay |
| Resources | Core | Vulkan, EnTT |
| Graphics | Core, Platform, Resources | EnTT, Jolt, World |
| Physics | Core | EnTT, Vulkan, World |
| World | Core, Resources, Graphics (types de données), Physics (interface) | Appels Vulkan, types Jolt |
| Serialization | Core, World, Resources | Vulkan, Jolt |
| Debug | Tous les modules du moteur | Game |
| Game | Engine | Vulkan, Jolt, SDL |

Trois confinements stricts :

- les en-têtes Vulkan ne sont inclus que dans `Graphics` et dans le backend ImGui de `Debug` ;
- les en-têtes Jolt ne sont inclus que dans `Physics` ;
- les en-têtes SDL ne sont inclus que dans `Platform` et dans le backend ImGui de `Debug`.

### 4.3 Flux de données

`World` est la couche d'intégration. `Graphics` et `Physics` ne connaissent ni les entités ni EnTT.

1. `Input` transforme les événements bruts en actions.
2. `Game` lit les actions et modifie les composants.
3. `World` pousse les intentions vers `Physics`, qui avance d'un pas fixe.
4. `World` recopie les transformations physiques dans les composants.
5. `World` remplit une `RenderScene` : liste d'objets à dessiner, caméra, lumière.
6. `Graphics` dessine la `RenderScene` et présente l'image.

Aucun appel GPU n'est émis depuis un composant ou un système de gameplay.

### 4.4 Garde-fous : ce qui ne peut pas être reporté

Ces dix règles coûtent peu aujourd'hui. Les ajouter après coup obligerait à reprendre tout le moteur. Elles s'appliquent dès la première ligne de code et sont vérifiées par AC-17.

| ID | Garde-fou | Vérification |
|---|---|---|
| G-01 | Seul `Graphics` émet des appels GPU. Il ne reçoit qu'une `RenderScene` : des données copiées, sans pointeur vers les entités. | Recherche des `#include` Vulkan |
| G-02 | Les données par objet (matrice, index de matériau) sont dans un buffer par frame, lu par `gl_InstanceIndex`. Aucune matrice d'objet en push constant. | Revue des shaders |
| G-03 | Toute ressource est désignée par un handle avec état. Un handle non prêt ou en échec donne l'asset de secours. Aucun pointeur de ressource n'est conservé d'une frame à l'autre. | Test automatisé |
| G-04 | Les importeurs produisent des structures CPU indépendantes du format de fichier (`MeshData`, `TextureData`, `MaterialData`). `Graphics` ne connaît qu'elles. | Recherche des `#include` cgltf et stb |
| G-05 | Gameplay et physique avancent uniquement par pas fixes. Aucun système de simulation ne lit le temps réel de la frame. | Revue et test automatisé du pas fixe |
| G-06 | Aucun état global mutable, aucun singleton : chaque système reçoit son contexte en paramètre. Le logger est utilisable depuis plusieurs threads. | Recherche des variables `static` non constantes |
| G-07 | Les positions monde utilisent le type `WorldPosition`, aujourd'hui un alias de `glm::vec3`. La conversion vers l'espace de rendu se fait en un seul endroit, l'extraction, qui produit des matrices relatives à la caméra. | Revue, recherche des usages de `WorldPosition` |
| G-08 | Un matériau est un jeu de données (paramètres et textures), jamais un shader par objet. Les shaders y accèdent par un fichier d'inclusion commun. | Revue des shaders |
| G-09 | Format, résolution et nombre d'échantillons des cibles de rendu sont des paramètres passés à la création des pipelines, jamais des constantes dispersées. | Revue |
| G-10 | Confinement des bibliothèques tierces (4.2), aucune API Windows hors de `Platform`, conventions de repère et d'unités (3.3) appliquées partout. | Recherche des `#include` |

### 4.5 Optimisations reportées

Aucune de ces optimisations n'apporte de gain mesurable sur une scène de vingt objets. Elles sont reportées. Les garde-fous garantissent que chacune reste un chantier limité aux modules indiqués.

| Optimisation | Garde-fous | Chantier futur limité à | Certitude |
|---|---|---|---|
| Instancing | G-01, G-02 | `Graphics` | Sûr |
| MSAA | G-09 | `Graphics` | Sûr |
| Ombres en cascade | G-01 | `Graphics` | Sûr |
| Occlusion culling, LOD | G-01 | `Graphics`, extraction dans `World` | Sûr |
| Lumières multiples, forward+ ou rendu différé | G-01, G-08 | `Graphics`, extraction dans `World` | Sûr |
| Chargement asynchrone et streaming des ressources | G-03 | `Resources` | Sûr |
| Assets précompilés, textures compressées | G-04 | `Resources`, `Tools` | Sûr |
| Allocateurs mémoire dédiés | G-06 | Local à chaque module | Sûr |
| Portage Linux | G-10 | `Platform`, build | Sûr |
| Bindless et rendu piloté par le GPU | G-01, G-02, G-08 | `Graphics` | Sûr, avec réécriture interne de `Graphics` |
| Commandes de rendu sur plusieurs threads, thread de rendu | G-01, G-06 | `Graphics`, `Core` | Sûr, avec réécriture interne de `Graphics` |
| Job system et simulation multithread | G-05, G-06 | `Core`, `World`, `Game` | Sous condition : tient tant que G-06 est respecté sans exception |
| Streaming du monde par cellules | G-03, G-07, format de scène versionné | `World`, `Serialization`, `Resources` | Sous condition : conception à faire dans un cahier des charges dédié |
| Grandes coordonnées (monde de plusieurs kilomètres) | G-07 | `World`, `Physics`, `Serialization` | Sous condition : passage de `WorldPosition` en double précision, que Jolt prend en charge par une option de compilation |

Lecture de la colonne Certitude :

- **Sûr** : modification locale aux modules cités.
- **Sûr, avec réécriture interne** : le module change en profondeur, mais son interface et le reste du moteur ne bougent pas.
- **Sous condition** : faisable sans reprendre le moteur, à confirmer par un prototype ciblé avant d'engager le chantier.

Une optimisation ne sort de cette liste que sur la base d'une mesure Tracy ou RenderDoc qui montre un besoin.

---

## 5. Spécifications fonctionnelles

### 5.1 Application et boucle principale

Ordre d'initialisation : Core, Platform, Input, Graphics, Resources, Physics, World, Debug, puis chargement de la scène. L'arrêt se fait dans l'ordre inverse.

- La simulation (gameplay et physique) avance par pas fixes de 1/60 s, avec accumulateur.
- Quatre pas de simulation au maximum par frame ; le temps de frame est plafonné à 0,25 s.
- Le rendu interpole les transformations entre les deux derniers états de simulation.
- Phases d'une frame, dans cet ordre : entrées, simulation (zéro à quatre pas), extraction, rendu, présentation.

### 5.2 Fenêtre et entrées

- Fenêtre redimensionnable, 1920 × 1080 par défaut, taille lue dans `Config/engine.json`.
- Recréation de la swapchain au redimensionnement ; rendu suspendu quand la fenêtre est réduite.
- Capture et libération du curseur.
- Fermeture par l'événement de fenêtre (croix ou Alt + F4, géré par le système).

Les actions sont définies dans `Config/input.json`. Aucun système de gameplay ne lit une touche directement.

| Action | Touche par défaut (clavier AZERTY) |
|---|---|
| Avancer / reculer | Z / S |
| Gauche / droite | Q / D |
| Monter / descendre (caméra libre) | E / A |
| Sauter | Espace |
| Courir | Maj gauche |
| Orienter la caméra | Souris |
| Capturer ou libérer le curseur | Échap |
| Afficher le panneau de debug | F1 |
| Afficher les colliders (P1) | F2 |
| Changer de caméra | F3 |
| Activer les ombres (P1) | F4 |

### 5.3 Caméra

**Caméra libre**

- déplacement sur trois axes, rotation à la souris ;
- vitesse configurable, 5 m/s par défaut, multipliée par 4 en courant ;
- champ de vision vertical réglable de 50° à 100°, 70° par défaut.

**Caméra troisième personne**

- orbite autour du personnage, distance 4 m et hauteur de visée 1,6 m par défaut, configurables ;
- un balayage de sphère (spherecast) entre la cible et la caméra la rapproche quand un obstacle s'interpose ;
- lissage du déplacement pour éviter les à-coups.

### 5.4 Rendu

Rendu direct (forward), une passe principale, plus une passe d'ombre en P1.

- Vulkan 1.3 avec dynamic rendering ; validation layers actives en configuration Debug.
- Swapchain sRGB ; présentation avec synchronisation verticale par défaut, désactivable pour les mesures.
- Tampon de profondeur 32 bits flottant.
- Deux frames en vol.
- Géométrie indexée ; format de sommet unique : position, normale, tangente, UV.
- Descripteurs : un jeu par frame (caméra, lumière, buffer des données par objet), un jeu par matériau. La matrice de chaque objet est lue dans ce buffer par `gl_InstanceIndex`.
- Liste d'objets triée par matériau avant émission des commandes.
- Destruction des ressources GPU différée jusqu'à ce qu'aucune frame en vol ne les utilise.
- Échec de création d'une ressource GPU : erreur loguée avec le nom de la ressource, sans plantage quand un asset de secours existe.

**P1**

- Ombres : une shadow map de 2048 × 2048 (résolution configurable), cadrée sur une zone fixe autour du joueur, filtrage PCF 3 × 3, biais constant et biais de pente réglables, désactivable.
- HDR : rendu de la scène dans une image flottante 16 bits, puis passe de tonemapping (ACES approché, exposition réglable) vers la swapchain. L'interface ImGui est dessinée après le tonemapping.
- IBL : environnement chargé depuis un fichier `.hdr`, préfiltré au chargement (irradiance diffuse, spéculaire par niveau de rugosité, table BRDF) et affiché en skybox. Il remplace le terme ambiant constant.
- Frustum culling par sphère englobante.
- Génération des mipmaps au chargement, filtrage anisotrope.

### 5.5 Matériaux et ressources

Modèle de matériau unique : PBR metallic-roughness de glTF 2.0, simplifié.

- Pris en charge : couleur de base (facteur et texture), métal et rugosité (facteurs et texture).
- P1 : normal map.
- Non pris en charge : émissif, occlusion, transparence.
- Éclairage : une lumière directionnelle et un terme ambiant constant, remplacé par l'IBL en P1.

Ressources :

- formats : glTF 2.0 et GLB pour les modèles, PNG et JPG pour les textures, HDR Radiance (`.hdr`) pour l'environnement en P1 ;
- accès par handles typés avec état (en chargement, prêt, en échec) ; un handle non prêt donne l'asset de secours ;
- cache indexé par chemin normalisé : une ressource n'est chargée qu'une fois ;
- libération au déchargement de la scène ;
- assets de secours intégrés : texture damier magenta, cube, matériau par défaut ;
- la hiérarchie de nœuds glTF est aplatie à l'import en transformations monde.

### 5.6 Monde et entités

EnTT est utilisé directement dans `World` et `Game`, sans surcouche.

Composants du moteur :

| Composant | Contenu |
|---|---|
| `SceneId` | Identifiant texte unique et stable dans la scène |
| `Name` | Nom lisible, facultatif |
| `Transform` | Position (`WorldPosition`), rotation (quaternion), échelle |
| `MeshRenderer` | Handles de mesh et de matériau |
| `Collider` | Forme et dimensions |
| `RigidBody` | Type (statique ou dynamique), masse |
| `Camera` | Champ de vision, plans proche et lointain |
| `DirectionalLight` | Direction, couleur, intensité |

Les composants propres au personnage (contrôleur, réglages de caméra) vivent dans `Game`.

Format de scène, version 1 :

```json
{
  "version": 1,
  "entities": [
    {
      "id": "crate_01",
      "name": "Caisse",
      "transform": {
        "position": [2.0, 0.5, -3.0],
        "rotation_deg": [0, 45, 0],
        "scale": [1, 1, 1]
      },
      "mesh": "Models/crate.glb",
      "collider": { "shape": "box", "half_extents": [0.5, 0.5, 0.5] },
      "rigid_body": { "type": "dynamic", "mass": 20.0 }
    }
  ]
}
```

Un fichier dont la version est inconnue ou dont un champ est invalide produit une erreur qui nomme le fichier, l'entité et le champ.

### 5.7 Physique

- Jolt Physics, pas fixe de 1/60 s, gravité configurable (-9,81 m/s² sur Y par défaut).
- Colliders : boîte, sphère, capsule.
- Trois couches de collision : statique, dynamique, personnage.
- Interface exposée par `Physics` : créer et détruire un corps, lire et écrire une transformation, appliquer une impulsion, raycast, spherecast.
- Aucun type Jolt ne sort du module ; les conversions GLM vers Jolt y sont internes.
- Les tests de physique tournent sans fenêtre ni GPU.

### 5.8 Personnage de démonstration

Représenté par une capsule ou un modèle statique simple. Basé sur `CharacterVirtual` de Jolt : aucun contrôleur de personnage n'est réécrit.

| Paramètre | Valeur par défaut |
|---|---|
| Capsule | Hauteur 1,8 m, rayon 0,3 m |
| Vitesse de marche | 4 m/s |
| Vitesse de course | 7 m/s |
| Hauteur de saut | 1,2 m |
| Pente maximale | 45° |
| Hauteur de marche franchissable | 0,3 m |

Comportements : déplacement relatif à la caméra, accélération et freinage progressifs, rotation vers la direction de déplacement, saut uniquement au sol, poussée des corps dynamiques par impulsion au contact.

### 5.9 Interface de débogage

Panneau ImGui, affiché par F1 :

- FPS, temps CPU par frame (moyenne et maximum sur une seconde) ;
- nombre d'entités, d'objets dessinés, de triangles ;
- mémoire GPU utilisée (budget VMA) ;
- console des 100 derniers avertissements et erreurs ;
- P1 : temps GPU, affichage des colliders, bascules des ombres, du tonemapping et de l'IBL, réglage de l'exposition.

Les zones Tracy sont posées dès la phase 1 sur les phases de la frame.

---

## 6. Scène de démonstration

### 6.1 Contenu

Fichier `Assets/Scenes/demo.json` : un terrain de test extérieur.

- sol de 100 m × 100 m au minimum ;
- deux rampes et trois plateformes, dont une à 1 m de hauteur ;
- 10 à 20 objets statiques texturés, de formes variées ;
- un mur de 10 cm d'épaisseur (référence pour les tests de collision) ;
- un cube de référence de 1 m (contrôle d'échelle et d'orientation) ;
- au moins cinq caisses dynamiques pouvant être poussées, dont une pile de trois ;
- une lumière directionnelle ;
- le personnage ;
- en P1 : cinq sphères de référence, de rugosité 0 à 1, dont deux métalliques (contrôle du PBR et de l'IBL).

Budget : 300 000 triangles au total, textures de 2048 × 2048 au maximum.

Les assets sont libres de droits ou créés pour le projet. Licences et attributions figurent dans `Assets/LICENSES.md`.

### 6.2 Parcours de validation

1. Lancer l'application : la scène se charge seule.
2. Se déplacer en caméra libre.
3. Passer en caméra troisième personne (F3).
4. Marcher, courir, monter une rampe.
5. Sauter sur la plateforme de 1 m.
6. Courir contre le mur de 10 cm : le personnage ne le traverse pas.
7. Pousser une caisse, faire tomber la pile.
8. Afficher le panneau de debug (F1).
9. Redimensionner la fenêtre, la réduire, la restaurer.
10. Fermer l'application.

Étapes supplémentaires si le P1 est livré : observer les ombres (F4), afficher les colliders (F2), comparer le rendu avec et sans tonemapping et IBL depuis le panneau.

---

## 7. Exigences non fonctionnelles

### 7.1 Performance

La machine de référence est celle du développeur. Son processeur, sa carte graphique, sa mémoire, la version du pilote et celle de Windows sont consignés dans `Docs/benchmark.md` avant la première mesure.

- résolution : 1920 × 1080, configuration Release, synchronisation verticale désactivée ;
- moyenne d'au moins 60 FPS sur le parcours de validation enregistré pendant 60 s ;
- 99e centile du temps de frame inférieur ou égal à 20 ms.

Chaque mesure consigne le nombre d'objets, de triangles et les options graphiques actives. Une première mesure est faite dès la fin de la phase 3.

### 7.2 Stabilité

- 30 minutes de session sans plantage ;
- dérive mémoire (processus et GPU) inférieure à 5 % entre la 5e et la 30e minute ;
- 20 chargements et déchargements successifs de la scène sans croissance mémoire de plus de 5 % ;
- zéro message des validation layers Vulkan, fermeture comprise.

### 7.3 Qualité du code

- zéro erreur et zéro avertissement dans `Engine/` et `Game/` ;
- règles de dépendance de la section 4.2 respectées ;
- garde-fous de la section 4.4 respectés ;
- un fichier `README.md` par module : rôle, interface publique, dépendances ;
- aucun fichier de plus de 800 lignes sans justification ;
- aucun code tiers copié hors de vcpkg sans sa licence.

### 7.4 Intégration continue

GitHub Actions, sur un runner Windows disposant de MSVC 14.52 ou plus récent, à chaque push :

1. vérification du formatage ;
2. configuration et compilation en Debug et en Release ;
3. exécution des tests avec CTest.

Les tests automatisés ne nécessitent ni fenêtre ni GPU : mathématiques, transformations, cache de ressources, aller-retour de sérialisation, mapping des actions, physique.

---

## 8. Critères d'acceptation

### 8.1 Livraison (tous obligatoires)

| ID | Critère | Validation |
|---|---|---|
| AC-01 | Sur une machine propre, le dépôt cloné se configure en suivant le guide, sans modifier un fichier | Essai sur une seconde machine ou une VM |
| AC-02 | Compilation Debug et Release : zéro erreur, zéro avertissement dans `Engine/` et `Game/` | CI |
| AC-03 | Tous les tests automatisés passent | CI |
| AC-04 | L'application ouvre une fenêtre et charge `demo.json` ; déplacer un objet dans le JSON se voit au lancement suivant, sans recompiler | Test manuel |
| AC-05 | Modèles et textures visibles ; le cube de référence mesure 1 m et est orienté correctement | Inspection |
| AC-06 | Caméra libre : trois axes, rotation à la souris, champ de vision réglable | Parcours |
| AC-07 | Le personnage marche, court et saute sur la plateforme de 1 m ; la caméra troisième personne ne traverse pas le sol | Parcours |
| AC-08 | En course, le personnage ne traverse pas le mur de 10 cm ; un corps lâché de 10 m ne traverse pas le sol | Tests automatisés et parcours |
| AC-09 | Une caisse poussée se déplace ; une pile de trois caisses reste stable 10 s | Test automatisé et parcours |
| AC-10 | Le panneau affiche FPS, temps CPU, entités, objets dessinés, triangles | Test manuel |
| AC-11 | Modèle ou texture manquant : erreur loguée avec le chemin, asset de secours affiché, pas de plantage | Test avec fichier supprimé |
| AC-12 | Vingt redimensionnements, une réduction et une restauration : aucun plantage | Test manuel |
| AC-13 | Zéro message des validation layers pendant le parcours complet, fermeture comprise ; code de sortie 0 | Logs en Debug |
| AC-14 | Session de 30 minutes sans plantage, dérive mémoire inférieure à 5 % | Test de stabilité |
| AC-15 | Vingt cycles de chargement et déchargement de la scène, croissance mémoire inférieure à 5 % | Test scripté |
| AC-16 | Objectifs de performance de la section 7.1 atteints et rapport rédigé | `Docs/benchmark.md` |
| AC-17 | Les dix garde-fous de la section 4.4 sont respectés | Revue sur liste, consignée dans `Docs/architecture.md` |

### 8.2 Bonus P1

| ID | Critère | Validation |
|---|---|---|
| AC-P1-1 | Le personnage et les objets projettent une ombre sur le sol, sans acné ni décollement visibles dans la scène de référence ; bascule par F4 | Inspection |
| AC-P1-2 | L'affichage filaire des colliders correspond aux formes physiques | Inspection |
| AC-P1-3 | Le temps GPU s'affiche dans le panneau | Test manuel |
| AC-P1-4 | Le nombre d'objets dessinés diminue quand des objets sortent du champ | Panneau de debug |
| AC-P1-5 | Avec le tonemapping, une surface claire en plein soleil garde du détail au lieu de saturer en blanc ; l'exposition se règle dans le panneau | Inspection |
| AC-P1-6 | La skybox est visible et les sphères métalliques reflètent l'environnement, de net à flou selon la rugosité | Inspection |

Le prototype est livré quand AC-01 à AC-17 sont validés. Les critères P1 n'entrent pas dans la décision de livraison.

---

## 9. Plan de réalisation

### 9.1 Phases

Estimations en heures de travail effectif, pour un développeur à l'aise en C++ qui découvre Vulkan. Ce sont des ordres de grandeur, à recaler après la phase 2.

| Phase | Contenu | Heures | Démonstration de fin de phase |
|---|---|---|---|
| 0 | Dépôt, CMake, vcpkg, CI, conventions, `AGENTS.md` | 15 à 20 | La CI compile un exécutable vide et lance un test |
| 1 | Core, fenêtre, boucle à pas fixe, logs, actions | 30 à 45 | Une fenêtre s'ouvre, logue les actions, se ferme proprement |
| 2 | Initialisation Vulkan, swapchain, triangle, ImGui | 50 à 80 | Triangle et panneau ImGui, redimensionnement sans erreur de validation |
| 3 | Caméra, meshes, profondeur, shader PBR, lumière | 45 à 70 | Cubes éclairés, caméra libre, première mesure de performance |
| 4 | Import glTF, textures, matériaux, cache, assets de secours | 45 à 65 | Un modèle GLB texturé s'affiche ; un fichier manquant ne plante pas |
| 5 | World, composants, extraction, scène JSON | 45 à 60 | La scène vient entièrement de `demo.json` |
| 6 | Jolt, corps rigides, personnage, caméra troisième personne | 60 à 85 | Parcours de validation jouable de bout en bout |
| 7 | Ombres, HDR, IBL, puis outils de debug P1 | 75 à 110 | Critères P1 validés ou explicitement abandonnés |
| 8 | Scène finale, stabilité, benchmark, documentation | 50 à 70 | AC-01 à AC-17 validés |
| | **Total** | **415 à 605** | |

### 9.2 Durée selon le rythme

| Rythme | Durée |
|---|---|
| 10 h par semaine | 9,5 à 14 mois |
| 15 h par semaine | 6,5 à 9,5 mois |
| 35 h par semaine | 2,5 à 4 mois |

### 9.3 Règles de pilotage

- Chaque phase se termine par un tag Git et un exécutable qui fait la démonstration prévue.
- Une phase ne commence pas tant que la précédente laisse un message des validation layers.
- Point de contrôle à la fin de la phase 3 (140 à 215 heures cumulées) : si le cumul dépasse 215 heures, la phase 7 est plafonnée à 40 heures, en suivant l'ordre de priorité des P1.
- Les heures réelles sont notées par phase, pour recaler les estimations suivantes.

---

## 10. Travail avec un agent IA

L'agent est un assistant de développement. Il n'a pas la main sur l'architecture.

### 10.1 Organisation

- Un seul agent à la fois, une tâche à la fois.
- Une tâche correspond à une branche et à une demande de fusion de taille relisible (400 lignes modifiées environ, hors fichiers générés).
- Le fichier `AGENTS.md` à la racine contient les conventions, les règles de dépendance (4.2), les garde-fous (4.4) et la commande de build et de test.
- Le parallélisme n'est envisagé qu'après la phase 5, à deux agents au plus, sur des modules distincts.

### 10.2 Règles

1. Lire `AGENTS.md`, `Docs/architecture.md` et le `README.md` du module avant de modifier.
2. Proposer un plan avant toute modification qui touche plus d'un module.
3. Ne pas ajouter, remplacer ou mettre à jour une dépendance sans note de décision validée.
4. Ne pas modifier l'interface publique d'un module sans documenter l'impact.
5. Ajouter ou mettre à jour les tests concernés.
6. Compiler et lancer les tests après chaque étape.
7. Ne jamais désactiver ni filtrer un message des validation layers pour faire passer une tâche.
8. Ne pas masquer une erreur par un contournement non documenté.
9. Livrer un résumé : fichiers modifiés, tests exécutés, limites restantes.
10. Ne jamais enfreindre un garde-fou de la section 4.4 pour simplifier une tâche, et ne pas implémenter une optimisation de la section 4.5 sans demande explicite.

### 10.3 Ce que l'agent ne peut pas vérifier

L'agent ne voit pas l'image rendue. Toute tâche graphique se termine par une vérification visuelle faite par le développeur, décrite dans la demande de fusion (ce qu'on doit voir, sous quel angle), avec une capture RenderDoc en cas de doute.

### 10.4 Définition de « terminé »

- le code compile sans avertissement ;
- les tests passent ;
- le comportement est vérifié à l'écran quand il est visuel ;
- aucun message des validation layers ;
- les garde-fous de la section 4.4 sont respectés ;
- les limites connues sont notées ;
- les démonstrations des phases précédentes fonctionnent toujours.

---

## 11. Risques

| Risque | Conséquence | Mesure |
|---|---|---|
| Dérive du périmètre | Projet interminable | Liste hors périmètre de la section 2.2, P1 conditionné au P0 |
| Blocage sur Vulkan en phase 2 | Semaines perdues avant la première image | vk-bootstrap, VMA, dynamic rendering, validation layers dès le premier jour |
| Code GPU généré par IA et non compris | Bugs impossibles à diagnostiquer | Demandes de fusion courtes, relecture, zéro message de validation, RenderDoc |
| Synchronisation et durée de vie des ressources GPU | Plantages au redimensionnement ou à la fermeture | Destruction différée, AC-12 et AC-13 testés dès la phase 2 |
| Couplage du jeu au moteur | Moteur non réutilisable | Règles de dépendance de la section 4.2 vérifiées en revue |
| Projet mené en parallèle d'autres activités | Longues interruptions, perte de contexte | Phases courtes avec démonstration, notes de décision, heures suivies |
| Assets incompatibles | Temps perdu sur l'import | Un seul format, cube de référence, test d'import dès la phase 4 |
| Optimisation à l'aveugle | Mauvaises décisions | Tracy dès la phase 1, première mesure en phase 3 |
| Optimisation reportée devenue impossible | Reprise de tout le moteur au lieu d'un module | Garde-fous de la section 4.4, vérifiés par AC-17 |

---

## 12. Livrables

1. Code source du moteur et de la démonstration.
2. `CMakeLists.txt`, `CMakePresets.json`, `vcpkg.json`.
3. `ThirdPartyLicenses.md` et `Assets/LICENSES.md`.
4. Exécutable Release avec ses assets.
5. `Assets/Scenes/demo.json` et les assets de la scène.
6. Sources des shaders.
7. Tests automatisés et configuration de la CI.
8. `README.md` : installation, compilation, lancement, commandes.
9. `Docs/architecture.md` et un `README.md` par module.
10. `Docs/decisions/` : notes de décision.
11. `Docs/benchmark.md` : configuration de test et mesures.
12. `Docs/limitations.md` : limites connues et prochaines étapes.

---

## 13. Après le prototype

Ordre proposé, chaque étape ayant son propre cahier des charges :

1. Hiérarchie de transformations et animation squelettique.
2. Renderer plus complet : lumières multiples, ombres en cascade, MSAA, instancing.
3. Terrain et grandes scènes.
4. Streaming des ressources et du monde, LOD.
5. Audio.
6. Navigation et IA.
7. Véhicules.
8. Éditeur de monde.
9. Météo et cycle jour/nuit.

L'ordre des optimisations de la section 4.5 se décide sur mesures, pas à l'avance.

---

## 14. Définition du succès

Le prototype est réussi si un développeur peut cloner le dépôt, suivre le guide, compiler, lancer l'application et faire le parcours de validation sans toucher au code, avec AC-01 à AC-17 validés.

**La priorité absolue est une petite expérience 3D complète et robuste, pas une longue liste de fonctionnalités inachevées.**
