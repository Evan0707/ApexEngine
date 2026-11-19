# Apex Engine

Prototype de moteur 3D en C++23 : Vulkan, Windows x64. Première fondation d'un futur moteur orienté monde ouvert.

**État : phase 2 en cours (Vulkan, swapchain, triangle, ImGui).** `ApexDemo` ouvre une fenêtre et y dessine par Vulkan 1.3 un triangle coloré sur fond bleu nuit, avec un panneau de debug Dear ImGui (F1). Il fait tourner la boucle à pas fixe, logue les actions et se ferme proprement.

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

`ApexDemo` ouvre une fenêtre de 1920 × 1080 et écrit ses logs dans la console et dans `Logs/apex.log`, relatif au dossier de lancement.

| Option | Effet |
|---|---|
| `--frames N` | Quitte seul après N frames |
| `--no-vsync` | Désactive la synchronisation verticale, pour les mesures |

| Code de sortie | Signification |
|---|---|
| 0 | Fermeture normale |
| 1 | Échec : fenêtre, GPU, option invalide ou erreur pendant le rendu (voir le log) |
| 2 | Fermeture normale, mais les validation layers ont émis au moins un message |

| Touche (AZERTY) | Effet |
|---|---|
| Z / S, Q / D, A / E | Logue la valeur des axes `MoveForward`, `MoveRight`, `MoveUp` quand elle change |
| Espace, Maj gauche, F3 | Logue l'appui et le relâchement de `Jump`, `Sprint`, `SwitchCamera` |
| Échap | Capture ou libère le curseur |
| F1 | Affiche ou masque le panneau de debug |
| F2, F4 | Logue l'appui ; sans effet avant la phase 7 |
| Souris | Logue une fois par seconde le déplacement cumulé, quand elle a bougé |
| Croix, Alt + F4 | Ferme l'application, code de sortie 0 |

Avec Visual Studio : ouvrir le dossier du dépôt, les presets sont détectés automatiquement.

La première configuration télécharge et compile les dépendances vcpkg ; les suivantes sont rapides.

## Organisation

```text
Engine/      bibliothèque statique du moteur, un dossier par module
Game/        exécutable de démonstration
Tests/       tests automatisés (Catch2), sans fenêtre ni GPU
Docs/        cahier des charges, architecture, décisions, suivi des heures
cmake/       options de compilation communes
```

## Documentation

- `Docs/cahier_des_charges.md` : spécification du prototype (v0.3), source de vérité.
- `Docs/architecture.md` : architecture et revue des garde-fous.
- `Docs/decisions/` : notes de décision.
- `Docs/suivi_heures.md` : heures prévues et réelles par phase.
- `AGENTS.md` : règles pour les agents IA.
- `ThirdPartyLicenses.md` : licences des dépendances.

## Intégration continue

À chaque push, GitHub Actions vérifie le formatage, compile en Debug, en Release et avec le preset `windows-profile`, puis lance les tests (`.github/workflows/ci.yml`).
