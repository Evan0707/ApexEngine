# Apex Engine

Prototype de moteur 3D en C++23 : Vulkan, Windows x64. Première fondation d'un futur moteur orienté monde ouvert.

**État : phase 0 (socle).** Le dépôt compile une bibliothèque `ApexEngine`, un exécutable `ApexDemo` qui affiche la version, et des tests.

## Prérequis

| Outil | Version | Remarque |
|---|---|---|
| Visual Studio 2026 | 18.12 ou plus récent | Charge de travail « Développement Desktop en C++ ». Fournit MSVC 14.52, CMake et Ninja |
| Git | récente | |
| vcpkg | cloné à part | Voir ci-dessous |
| SDK Vulkan (LunarG) | 1.3 ou plus récent | À partir de la phase 2 seulement |

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

Remplacer `windows-debug` par `windows-release` pour la configuration Release.

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

À chaque push, GitHub Actions vérifie le formatage, compile en Debug et en Release, puis lance les tests (`.github/workflows/ci.yml`).
