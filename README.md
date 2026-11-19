# Apex Engine

Prototype de moteur 3D en C++23 : Vulkan, Windows x64. Première fondation d'un futur moteur orienté monde ouvert.

**État : phase 4 en cours (import glTF, textures, matériaux, cache, assets de secours).** `ApexDemo` dessine par Vulkan 1.3 une scène éclairée vue par une caméra libre : sol, cubes, sphères métalliques et non métalliques, et des modèles glTF texturés (cube de référence aux faces marquées, caisses, bouteille). Un panneau de debug Dear ImGui s'affiche par F1. Il fait tourner la boucle à pas fixe, logue les actions et se ferme proprement.

## Prérequis

| Outil | Version | Remarque |
|---|---|---|
| Visual Studio 2026 | 18.12 ou plus récent | Charge de travail « Développement Desktop en C++ ». Fournit MSVC 14.52, CMake et Ninja |
| Git | récente | |
| vcpkg | cloné à part | Voir ci-dessous |
| SDK Vulkan (LunarG) | 1.4.363.1 | Fournit `glslc`, qui compile les shaders pendant le build, et les validation layers utilisées en Debug. Rouvrir le terminal après l'installation, pour que `VULKAN_SDK` soit défini |

Installation de vcpkg, une seule fois :

```
git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\dev\vcpkg
```

Rouvrir le terminal après `setx`.

## Compiler et tester

Depuis un « x64 Native Tools Command Prompt for VS 2026 » :

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
build\windows-debug\bin\ApexDemo.exe
```

Remplacer `windows-debug` par `windows-release` pour la configuration Release, ou par `windows-profile` pour une Release instrumentée par Tracy (voir `Engine/Core/README.md`).

## La démonstration

`ApexDemo` ouvre une fenêtre de 1920 × 1080 et écrit ses logs dans la console et dans `Logs/apex.log`, relatif au dossier de lancement. Il lit ses modèles dans le dossier `Assets` placé à côté de lui : le build l'y copie à chaque compilation.

| Option | Effet |
|---|---|
| `--frames N` | Quitte seul après N frames |
| `--no-vsync` | Désactive la synchronisation verticale, pour les mesures |
| `--assets DIR` | Dossier des assets ; par défaut, le dossier `Assets` copié à côté de l'exécutable par le build |
| `--model PATH` | Ajoute un modèle, chemin relatif au dossier des assets, posé au sol devant la caméra. Un modèle introuvable ou invalide est remplacé par un cube en damier magenta, et l'erreur est logée avec le chemin (AC-11) |
| `--benchmark S` | Mesure de performance (section 7.1) : la caméra suit un cercle autour de la scène ; après 2 s de mise en route, S secondes de frames sont mesurées, le résultat est logué avec la mémoire GPU de fin de mesure, puis l'application quitte. À lancer en Release avec `--no-vsync` (voir `Docs/benchmark.md`) |

| Code de sortie | Signification |
|---|---|
| 0 | Fermeture normale |
| 1 | Échec : fenêtre, GPU, option invalide ou erreur pendant le rendu (voir le log) |
| 2 | Fermeture normale, mais les validation layers ont émis au moins un message |

| Touche (AZERTY) | Effet |
|---|---|
| Z / S, Q / D | Avance ou recule dans la direction du regard, va à gauche ou à droite (5 m/s) ; logue la valeur des axes quand elle change |
| E / A | Monte ou descend à la verticale |
| Maj gauche | Court : vitesse multipliée par 4 |
| Échap | Capture ou libère le curseur |
| Souris | Oriente la caméra quand le curseur est capturé ; logue une fois par seconde le déplacement cumulé |
| F1 | Affiche ou masque le panneau de debug, où se règle le champ de vision (50° à 100°) |
| Espace, F3 | Logue l'appui et le relâchement de `Jump`, `SwitchCamera` ; sans autre effet avant la phase 6 |
| F2, F4 | Logue l'appui ; sans effet avant la phase 7 |
| Croix, Alt + F4 | Ferme l'application, code de sortie 0 |

Avec Visual Studio : ouvrir le dossier du dépôt, les presets sont détectés automatiquement.

La première configuration télécharge et compile les dépendances vcpkg ; les suivantes sont rapides.

## Organisation

```text
Engine/      bibliothèque statique du moteur, un dossier par module
Game/        exécutable de démonstration
Tests/       tests automatisés (Catch2), sans fenêtre ni GPU
Assets/      modèles et textures de la démonstration, licences dans Assets/LICENSES.md
Docs/        cahier des charges, architecture, décisions, suivi des heures
Tools/       scripts Python (génération des assets), voir Tools/README.md
cmake/       options de compilation communes
```

## Documentation

- `Docs/cahier_des_charges.md` : spécification du prototype (v0.3), source de vérité.
- `Docs/architecture.md` : architecture et revue des garde-fous.
- `Docs/decisions/` : notes de décision.
- `Docs/benchmark.md` : machine de référence, protocole et résultats des mesures de performance.
- `Docs/suivi_heures.md` : heures prévues et réelles par phase.
- `AGENTS.md` : règles pour les agents IA.
- `ThirdPartyLicenses.md` : licences des dépendances.

## Intégration continue

À chaque push, GitHub Actions vérifie le formatage, compile en Debug, en Release et avec le preset `windows-profile`, puis lance les tests (`.github/workflows/ci.yml`).
