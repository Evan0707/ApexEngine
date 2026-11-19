# 0008 — Ajout de Dear ImGui (interface de debug)

**Date :** 19 novembre 2026
**Statut :** proposée

## Contexte

La démonstration de la phase 2 est « triangle et panneau ImGui » (cahier des charges, section 9.1). Le cahier des charges retient Dear ImGui avec ses backends SDL3 et Vulkan, dans le module `Debug` (section 3.2), et décrit le panneau en section 5.9.

## Décision

- Ajouter `imgui` à `vcpkg.json` avec les fonctionnalités `sdl3-binding` et `vulkan-binding` : version 1.93.0 dans la baseline, à confirmer à l'installation.
- Pas de `docking-experimental`, pas de `freetype` : le panneau n'en a pas besoin.
- Le backend Vulkan est utilisé en mode dynamic rendering, avec le format de la swapchain passé en paramètre (G-09). L'API exacte du backend est relue dans l'en-tête installé au moment de l'intégration.
- Le contexte ImGui est créé et détruit par `debug::DebugUi`, qui est le seul point d'entrée du reste du moteur. Les en-têtes publics de `Debug` n'exposent aucun type ImGui.
- Les en-têtes ImGui ne sont inclus que dans `Debug`. Un seul fichier, `Debug/ImGuiBackend.cpp`, inclut à la fois SDL, Vulkan et les backends. `AGENTS.md`, section 4, reçoit la ligne « en-têtes Dear ImGui : `Debug` uniquement ».
- Pas de fichier `imgui.ini` : la disposition des fenêtres n'est pas sauvegardée, pour ne rien écrire dans le dossier de lancement.
- L'interface est dessinée en dernier, dans la passe principale. Quand la passe de tonemapping existera (P1, phase 7), elle sera dessinée après celle-ci (section 5.4).

## Alternatives écartées

- **Backend plateforme écrit à partir des événements de `Platform` :** éviterait SDL dans `Debug`, mais le cahier des charges désigne le backend SDL3 et autorise explicitement SDL dans le backend ImGui de `Debug`.
- **Rendu d'ImGui dans `Graphics` :** `Graphics` dépendrait d'ImGui, alors que le cahier des charges place l'interface dans `Debug`.
- **Branche docking :** fonctionnalité expérimentale, sans usage pour un seul panneau.

## Conséquences

- **Modules touchés :** `Debug` (nouveau), `Graphics` et `Platform` (points d'accroche, note 0009), `Game`, `vcpkg.json`, `Engine/CMakeLists.txt`, `ThirdPartyLicenses.md`, `AGENTS.md`.
- **Garde-fous :**
  - G-01 : le backend Vulkan d'ImGui crée ses propres objets GPU et émet ses commandes de dessin. C'est l'exception prévue par la section 4.2. Le code écrit dans `Debug` n'appelle que les fonctions du backend ; l'enregistrement se fait dans le command buffer fourni par `Graphics`, au moment choisi par `Graphics`.
  - G-06 : ImGui garde un pointeur global vers son contexte courant. C'est un état interne à la bibliothèque, comme pour Tracy ; le moteur n'en ajoute aucun.
  - G-10 : confinement ci-dessus, vérifiable par recherche des `#include`.
- **Entrées :** quand ImGui a le clavier ou la souris, les actions du jeu ne reçoivent pas ces événements.
- **Licence :** MIT, à recopier depuis le fichier `copyright` installé par vcpkg.
- **Coût de retour en arrière :** limité au module `Debug`.
