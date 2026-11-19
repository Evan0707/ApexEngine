# AGENTS.md — Apex Engine

Instructions pour tout agent IA qui travaille sur ce dépôt. À lire en entier avant toute modification.

## 1. Le projet en bref

- Prototype de moteur 3D en C++23, Windows x64, Vulkan 1.3.
- **Source de vérité :** `Docs/cahier_des_charges.md` (v0.3). Si ce fichier et le cahier des charges se contredisent, le cahier des charges l'emporte : signale l'écart au lieu de trancher seul.
- **Phase en cours : 5 (World, composants, extraction, scène JSON).** Cette ligne est mise à jour par le développeur à chaque changement de phase. Ne travaille pas sur une phase ultérieure.
- Priorité : une petite expérience 3D complète et robuste. Rien de ce qui est hors périmètre (cahier des charges, section 2.2) ne doit être ajouté.

## 2. Commandes

Depuis un « x64 Native Tools Command Prompt for VS 2026 », avec `VCPKG_ROOT` défini :

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

Même chose avec `windows-release`. L'exécutable est dans `build/<preset>/bin/`.

`windows-profile` : Release instrumenté par Tracy (`APEX_ENABLE_PROFILING=ON`, décision 0003).

Formatage, avant chaque commit (clang-format 18.1.3, la version vérifiée par la CI) :

```
clang-format -i <fichiers .h et .cpp modifiés>
```

## 3. Organisation

```text
Engine/      bibliothèque statique, un dossier par module
Game/        exécutable de démonstration
Tests/       tests Catch2, sans fenêtre ni GPU
Docs/        cahier des charges, architecture, décisions, suivi
cmake/       options de compilation communes
```

Les modules arrivent phase par phase. Ne crée pas un dossier de module vide « pour plus tard ».

## 4. Règles de dépendance entre modules

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

Confinement des bibliothèques tierces :

- en-têtes Vulkan : `Graphics` et le backend ImGui de `Debug` uniquement ;
- en-têtes Jolt : `Physics` uniquement ;
- en-têtes SDL : `Platform` et le backend ImGui de `Debug` uniquement ;
- cgltf et stb : `Resources` uniquement ;
- nlohmann/json : `Serialization` uniquement ;
- en-têtes Tracy : `Core/Profiling.h` uniquement ;
- en-têtes Dear ImGui : `Debug` uniquement.

Le « backend ImGui de `Debug` » est un seul fichier : `Debug/ImGuiBackend.cpp`. `Platform`, `Graphics` et `Debug` n'échangent la fenêtre, la surface et les événements bruts que par handles opaques, et un seul en-tête public de `Graphics` contient des types Vulkan : `Graphics/VulkanBackendAccess.h`, réservé à ce fichier (décision 0009).

## 5. Garde-fous (obligatoires, vérifiés par AC-17)

Ces règles ne se négocient pas, même pour simplifier une tâche. Si une tâche semble impossible sans en enfreindre une, arrête-toi et explique pourquoi.

| ID | Règle |
|---|---|
| G-01 | Seul `Graphics` émet des appels GPU. Il ne reçoit qu'une `RenderScene` : des données copiées, sans pointeur vers les entités. |
| G-02 | Les données par objet sont dans un buffer par frame, lu par `gl_InstanceIndex`. Aucune matrice d'objet en push constant. |
| G-03 | Toute ressource est désignée par un handle avec état. Un handle non prêt ou en échec donne l'asset de secours. Aucun pointeur de ressource conservé d'une frame à l'autre. |
| G-04 | Les importeurs produisent des structures CPU indépendantes du format de fichier (`MeshData`, `TextureData`, `MaterialData`). |
| G-05 | Gameplay et physique avancent uniquement par pas fixes. Aucun système de simulation ne lit le temps réel de la frame. |
| G-06 | Aucun état global mutable, aucun singleton. Chaque système reçoit son contexte en paramètre. Le logger est utilisable depuis plusieurs threads. |
| G-07 | Les positions monde utilisent le type `WorldPosition`. La conversion vers l'espace de rendu se fait uniquement dans l'extraction, en matrices relatives à la caméra. |
| G-08 | Un matériau est un jeu de données, jamais un shader par objet. Les shaders y accèdent par un fichier d'inclusion commun. |
| G-09 | Format, résolution et nombre d'échantillons des cibles de rendu sont des paramètres, jamais des constantes dispersées. |
| G-10 | Confinement des bibliothèques (section 4), aucune API Windows hors de `Platform`, conventions de repère et d'unités appliquées partout. |

Les optimisations listées dans le cahier des charges, section 4.5 (instancing, MSAA, bindless, multithreading, streaming, etc.), sont **reportées**. N'en implémente aucune sans demande explicite.

## 6. Conventions de code

- **Langue :** tout ce qui est dans le code est en anglais : identifiants, commentaires, noms de tests, messages d'erreur et de log, messages CMake, noms des étapes de CI. Seule la documentation (`Docs/`, fichiers `README.md`, ce fichier) est en français.
- **Commentaires :** seulement quand le code ne suffit pas (une raison, un piège, une contrainte). Une ligne en général, jamais un paragraphe. Pas de commentaire qui répète le code, pas de bannière, pas de code commenté, pas de commentaire de documentation sur ce qui est évident par le nom.
- **Nommage :** types en `PascalCase` ; fonctions et variables en `camelCase` ; membres privés préfixés `m_` ; constantes en `kPascalCase` ; macros en `APEX_MAJUSCULES`.
- **Espaces de noms :** `apex::<module>` en minuscules (`apex::core`, `apex::graphics`).
- **Fichiers :** `PascalCase.h` et `PascalCase.cpp`, un type principal par fichier, `#pragma once`.
- **Includes :** relatifs à `Engine/` (`#include "Core/Version.h"`). Ordre : en-tête associé, moteur, bibliothèques tierces, bibliothèque standard.
- **Erreurs :** `std::expected<T, Error>` pour les échecs récupérables, assertions pour les erreurs de programmation. Pas d'exceptions dans l'API du moteur ; celles d'une bibliothèque tierce sont interceptées à la frontière du module.
- **Mémoire :** RAII, `std::unique_ptr`, pas de `new` ni `delete` nus.
- `[[nodiscard]]` sur toute fonction qui renvoie un résultat à ne pas ignorer. `const` et `noexcept` quand c'est vrai.
- Pas de `using namespace` dans un en-tête.
- Conventions de repère et d'unités : main droite, +Y vers le haut, mètre, kilogramme, seconde, radians en interne (cahier des charges, section 3.3).
- **CMake :** sources listées explicitement, pas de `GLOB`. Toute cible du projet lie `Apex::Options`.
- **Avertissements :** `/W4 /permissive- /WX`. Zéro avertissement dans `Engine/` et `Game/`. Ne désactive jamais un avertissement par `#pragma` sans le justifier dans le compte rendu.
- **Tests :** Catch2 v3, fichier `Tests/<Module>/<Sujet>Tests.cpp`, étiquette `[module]`. Aucun test ne doit demander une fenêtre ou un GPU.
- Chaque module a un `README.md` : rôle, interface publique, dépendances. Mets-le à jour quand l'interface change.

## 7. Méthode de travail

1. Une tâche à la fois, sur une branche dédiée. Vise une modification relisible : environ 400 lignes, hors fichiers générés.
2. Avant de modifier : lis ce fichier, `Docs/architecture.md` et le `README.md` du module concerné.
3. Propose un plan et attends la validation avant toute modification qui touche plus d'un module.
4. Ne modifie pas l'interface publique d'un module sans documenter l'impact.
5. Ajoute ou mets à jour les tests concernés.
6. Compile et lance les tests après chaque étape.
7. Ne masque jamais une erreur par un contournement non documenté.
8. Ne désactive ni ne filtre jamais un message des validation layers Vulkan.
9. Ne modifie pas `Docs/cahier_des_charges.md`. Un changement de décision passe par une note dans `Docs/decisions/`, validée par le développeur.

## 8. Dépendances tierces

Une dépendance à la fois, uniquement quand la phase en cours en a besoin. Pour en ajouter, remplacer ou mettre à jour une :

1. écris une note de décision dans `Docs/decisions/` (modèle : `0000-modele.md`) et attends la validation ;
2. ajoute-la à `vcpkg.json` ;
3. ajoute sa licence à `ThirdPartyLicenses.md` ;
4. respecte le confinement de la section 4.

Bibliothèques prévues par le cahier des charges, à ajouter le moment venu : SDL3 (phase 1), Tracy (phase 1), vk-bootstrap, VMA et Dear ImGui (phase 2), GLM (phase 3, décision 0006), cgltf et stb_image (phase 4), EnTT et nlohmann/json (phase 5), Jolt Physics (phase 6).

## 9. Ce que tu ne peux pas vérifier

Tu ne vois pas l'image rendue. Pour toute tâche graphique, termine par une procédure de vérification visuelle destinée au développeur : ce qu'il doit voir, sous quel angle, avec quelles touches. Ne déclare jamais un rendu « correct » sur la seule foi d'une compilation réussie.

## 10. Définition de « terminé »

- le code compile sans avertissement, en Debug et en Release ;
- les tests passent ;
- le code est formaté ;
- le comportement visuel a sa procédure de vérification ;
- aucun message des validation layers ;
- les garde-fous de la section 5 sont respectés ;
- les démonstrations des phases précédentes fonctionnent toujours.

## 11. Compte rendu de fin de tâche

```text
Objectif :
Fichiers modifiés :
Tests exécutés et résultat :
Vérification visuelle à faire par le développeur :
Garde-fous concernés :
Limites restantes :
```
