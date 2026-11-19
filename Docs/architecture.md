# Architecture

Ce document décrit l'architecture réellement en place. Il est complété à la fin de chaque phase. La cible est décrite dans `cahier_des_charges.md`, section 4.

## État actuel (phase 1)

| Cible CMake | Type | Contenu |
|---|---|---|
| `ApexEngine` | Bibliothèque statique | Modules `Core`, `Platform`, `Input` |
| `ApexDemo` | Exécutable | Ouvre la fenêtre, fait tourner la boucle à pas fixe, logue les événements et les actions |
| `ApexTests` | Exécutable de tests | Tests de `Core`, `Platform`, `Input` et de la table d'actions de la démo, sans fenêtre ni GPU |
| `Apex::Options` | Cible d'interface | Options de compilation communes : C++23, avertissements en erreurs |

Presets : `windows-debug`, `windows-release`, et `windows-profile` (Release instrumenté par Tracy, décision 0003). Les trois sont compilés et testés par la CI.

## Modules en place

| Module | Rôle | Dépend de | Bibliothèque tierce | Documentation |
|---|---|---|---|---|
| Core | Erreurs, assertions, logger, pas fixe, boucle principale, macros de profilage | Bibliothèque standard | Tracy, dans `Core/Profiling.h` uniquement et seulement avec `windows-profile` | `Engine/Core/README.md` |
| Platform | Touches physiques, événements, fenêtre | Core | SDL3, dans `Platform/*.cpp` uniquement | `Engine/Platform/README.md` |
| Input | Table des actions, état des actions | Core, Platform | Aucune | `Engine/Input/README.md` |

`Game` contient la démonstration : la table d'actions par défaut (`DefaultBindings`) et l'implémentation des phases de la frame (`DemoApp`).

## Déroulement d'une frame (phase 1)

`Game/main.cpp` crée les systèmes dans l'ordre de la section 5.1 (logger, fenêtre, table d'actions, application), puis lance `core::MainLoop::run`. L'arrêt se fait dans l'ordre inverse, par la destruction des objets.

1. **Entrées.** `DemoApp::processInput` vide la file d'événements de la fenêtre. L'état `frame` applique les événements. Les actions qui ne relèvent pas de la simulation (capture du curseur, bascules de debug) sont traitées ici. L'état `frame` est ensuite fusionné dans l'état `simulation`. Une demande de fermeture arrête la boucle.
2. **Simulation.** De zéro à quatre pas de 1/60 s. Chaque pas lit uniquement l'état `simulation`, puis vide ses fronts.
3. **Extraction et rendu.** Vides en phase 1.
4. **Présentation.** Sans swapchain, la démo dort 1 ms (10 ms si la fenêtre est réduite) pour ne pas occuper un cœur. À retirer en phase 2.

Logs : console et `Logs/apex.log`, relatif au dossier de lancement.

## Décisions prises pendant la phase 1

| Note | Objet |
|---|---|
| 0002 | Ajout de SDL3 |
| 0003 | Ajout de Tracy, preset `windows-profile` |
| 0004 | Deux agents en parallèle dès la phase 1 (écart assumé avec la section 10.1 du cahier des charges) |
| 0005 | Inclusion de `SDL_main.h` avec `SDL_MAIN_HANDLED` ; licence SDL par renvoi au fichier installé par vcpkg |

## Revue des garde-fous (AC-17)

À remplir à la fin de chaque phase. Un garde-fou sans objet à ce stade est noté « sans objet ».

| ID | Phase 0 | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 | Phase 6 | Phase 7 | Phase 8 |
|---|---|---|---|---|---|---|---|---|---|
| G-01 | sans objet | sans objet | | | | | | | |
| G-02 | sans objet | sans objet | | | | | | | |
| G-03 | sans objet | sans objet | | | | | | | |
| G-04 | sans objet | sans objet | | | | | | | |
| G-05 | sans objet | conforme | | | | | | | |
| G-06 | conforme | conforme | | | | | | | |
| G-07 | sans objet | sans objet | | | | | | | |
| G-08 | sans objet | sans objet | | | | | | | |
| G-09 | sans objet | sans objet | | | | | | | |
| G-10 | conforme | conforme | | | | | | | |

Détail de la phase 1 :

- **G-05 :** `FramePhases::simulate` ne reçoit que `FixedStep` (numéro et durée du pas). Seul `MainLoop::run` lit l'horloge de la frame ; `Input` ne voit jamais le temps. Couvert par les tests du pas fixe et de la boucle.
- **G-06 :** aucune variable `static` modifiable dans `Engine/` ni `Game/` (recherche faite). Le logger est une instance passée par référence, protégée par un mutex, et testée depuis 8 threads. Tracy garde un état interne à la bibliothèque ; le moteur n'en ajoute aucun.
- **G-10 :** `#include <SDL3/…>` seulement dans `Engine/Platform/Window.cpp`, `#include <tracy/…>` seulement dans `Engine/Core/Profiling.h`, aucune API Windows (recherches faites). Touches désignées par codes physiques (section 3.3), temps en secondes.
