# 0001 — Socle technique de la phase 0

**Date :** 19 novembre 2026
**Statut :** acceptée

## Contexte

Le cahier des charges v0.3 fige les choix techniques (section 3). La phase 0 les met en place et précise quelques points qu'il laisse ouverts : conventions de nommage, version de clang-format, manière d'activer C++23, image de la CI.

## Décision

- **C++23 :** l'option `/std:c++23` est passée explicitement par la cible `Apex::Options`, au lieu de `CMAKE_CXX_STANDARD`. La configuration échoue si MSVC est antérieur à 19.52.
- **Dépendances :** vcpkg en mode manifeste, baseline `d13c536672dced4b8c671955ea7d057a1b7a1ff8`. Seul Catch2 est installé en phase 0 ; les autres bibliothèques arrivent phase par phase.
- **Presets :** `windows-debug` et `windows-release`, générateur Ninja, triplet `x64-windows`.
- **CI :** GitHub Actions, image `windows-2025` (Visual Studio 2026) pour la compilation, `ubuntu-24.04` pour le contrôle du formatage.
- **Formatage :** clang-format 18.1.3, style défini dans `.clang-format` (base LLVM, indentation de 4, 110 colonnes).
- **Nommage et commentaires :** conventions de `AGENTS.md`, section 6. Tout le code est en anglais (identifiants, commentaires, noms de tests, messages) ; les commentaires sont rares et courts. La documentation reste en français.
- **Compilateurs autres que MSVC :** le CMake les accepte sans les garantir, pour ne pas fermer la porte à un portage (garde-fou G-10).

## Alternatives écartées

- `CMAKE_CXX_STANDARD 23` : l'option réellement passée à MSVC dépend de la version de CMake, alors que le cahier des charges exige `/std:c++23`.
- Toutes les dépendances dès la phase 0 : contraire à la règle « une dépendance à la fois », et CI plus lente sans bénéfice.
- vcpkg livré avec Visual Studio : version non maîtrisée ; un clone dédié rend l'installation identique sur le poste et en CI.

## Conséquences

- Un poste avec un Visual Studio trop ancien échoue dès la configuration, avec un message explicite.
- Mettre à jour les dépendances revient à changer la baseline dans `vcpkg.json`, par une nouvelle note de décision.
- À valider au premier push, car non exécuté lors de la création du squelette : la compilation avec MSVC, les presets Windows, la CI et l'intégration de Catch2 (`find_package`, `catch_discover_tests`). Le moteur et la démonstration ont été compilés et exécutés avec GCC 13 et Clang 18 ; la logique des tests a été exécutée avec GCC 13.
