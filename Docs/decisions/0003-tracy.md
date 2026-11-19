# 0003 — Ajout de Tracy (profilage CPU)

**Date :** 19 novembre 2026
**Statut :** acceptée

## Contexte

Le cahier des charges demande que les zones Tracy soient posées dès la phase 1 sur les phases de la frame (section 5.9). Tracy est la bibliothèque retenue, utilisée par tous les modules « par macros » (section 3.2). Toute sortie de la liste des optimisations reportées repose sur une mesure Tracy ou RenderDoc (section 4.5).

## Décision

- Ajouter `tracy` à `vcpkg.json`, sans les fonctionnalités par défaut et avec `on-demand` : `{ "name": "tracy", "default-features": false, "features": ["on-demand"] }`. Version 0.14.1 dans le dépôt vcpkg local au 7 novembre 2026, à confirmer à l'installation.
- Un seul en-tête, `Core/Profiling.h`, inclut Tracy. Il définit deux macros : `APEX_PROFILE_FRAME()` et `APEX_PROFILE_SCOPE(name)`. Le reste du moteur n'utilise que ces macros.
- Option CMake `APEX_ENABLE_PROFILING`, désactivée par défaut. Désactivée : les macros sont vides et Tracy n'est pas lié. Activée : `Tracy::TracyClient` est lié à `ApexEngine`.
- Nouveau preset `windows-profile` : Release avec `APEX_ENABLE_PROFILING=ON`. Il est compilé par la CI, pour que les macros ne se dégradent pas sans qu'on le voie.
- Zones posées en phase 1, dans `core::MainLoop` : la marque de frame et les cinq phases (entrées, simulation, extraction, rendu, présentation).

## Alternatives écartées

- **Tracy toujours actif :** chaque processus de test démarrerait le thread de Tracy et ouvrirait un port d'écoute, et les mesures de performance de la section 7.1 incluraient l'instrumentation.
- **Sans `on-demand` :** Tracy garde tous les événements en mémoire tant qu'aucun profileur n'est connecté. La mémoire croît sans limite, ce qui est incompatible avec la dérive maximale de 5 % (section 7.2).
- **Fonctionnalité `crash-handler` (activée par défaut dans le port) :** elle installe un gestionnaire d'exceptions dans le processus. Le prototype n'en a pas besoin et un plantage doit arriver tel quel dans le débogueur.
- **Fonctionnalité `gui-tools` :** compiler le profileur par vcpkg tire curl, freetype, glfw et une dizaine d'autres bibliothèques. Le profileur se télécharge depuis la page des versions de Tracy.
- **Chronomètres internes ou autre profileur :** écartés par le cahier des charges.

## Conséquences

- **Modules touchés :** `Core` (`Profiling.h`, zones dans `MainLoop`), `Engine/CMakeLists.txt`, `CMakeLists.txt`, `CMakePresets.json`, `.github/workflows/ci.yml`, `vcpkg.json`, `ThirdPartyLicenses.md`.
- **Écart à valider :** le tableau de dépendances (section 4.2) limite `Core` à la bibliothèque standard et à GLM, alors que la section 3.2 prévoit Tracy dans tous les modules. Proposition : ajouter à `AGENTS.md`, section 4, la ligne « en-têtes Tracy : `Core/Profiling.h` uniquement ».
- **Garde-fous :** G-06. Tracy a un état global interne, propre à la bibliothèque ; le moteur n'en ajoute aucun. G-10 : confinement à un seul en-tête.
- **Réseau :** un exécutable profilé écoute sur le port TCP 8086. Le pare-feu Windows peut afficher une demande au premier lancement. La variable d'environnement `TRACY_ONLY_LOCALHOST=1` limite l'écoute à la machine locale (à vérifier à l'intégration).
- **Profileur :** sa version doit être exactement celle du client, sinon la connexion est refusée.
- **Licence :** BSD-3-Clause, à recopier depuis le fichier `copyright` installé par vcpkg.
- **Coût de retour en arrière :** supprimer l'en-tête, l'option et le preset ; les macros deviennent vides.
