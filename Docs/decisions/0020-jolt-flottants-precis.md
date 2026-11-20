# 0020 — Jolt compilé en flottants précis, par un triplet vcpkg du dépôt

**Date :** 20 novembre 2026
**Statut :** acceptée par délégation (choix fait pendant la phase 6, à relire par le développeur)

## Contexte

Pendant la tâche « corps rigides » de la phase 6, les tests de physique passaient en Debug et échouaient en Release : une boîte posée sur une autre boîte se mettait à tourner seule, gagnait de l'énergie et traversait un mur.

Le défaut ne vient pas du moteur. Un programme de trente lignes qui n'utilise que Jolt le reproduit :

| Bibliothèque Jolt 5.6.0 (Release, MSVC 19.52) | Boîte lâchée de 2 m sur une boîte |
|---|---|
| Options par défaut du port vcpkg : `/O2 /fp:fast` | Se met à tourner après 0,8 s, ne se stabilise pas |
| Même compilation avec `/fp:precise` | Au repos après 0,8 s, immobile ensuite |

La seule différence entre les deux lignes est le mode flottant. La version Debug (`/Od`) n'est pas touchée. La cause exacte n'est pas établie : défaut d'optimisation du compilateur, qui est une préversion, ou sensibilité de Jolt à `/fp:fast` avec ce compilateur.

## Décision

- Jolt est compilé avec son option `CROSS_PLATFORM_DETERMINISTIC=ON`, qui remplace `/fp:fast` par `/fp:precise` et désactive les instructions FMA. Les autres bibliothèques ne changent pas.
- L'option est donnée par un triplet vcpkg du dépôt, `cmake/triplets/x64-windows.cmake`, qui reprend le triplet `x64-windows` de vcpkg et ajoute cette option pour le seul port `joltphysics`. `CMakePresets.json` le désigne par `VCPKG_OVERLAY_TRIPLETS`.
- La définition `JPH_CROSS_PLATFORM_DETERMINISTIC` arrive dans le moteur par la cible CMake `Jolt::Jolt`. `physics::Runtime::create` vérifie toujours que la bibliothèque et le moteur ont les mêmes options.
- La clé du cache vcpkg de la CI tient compte du triplet.
- Les tests de `Tests/Physics/` tournent en Debug et en Release : ils auraient détecté le défaut et détecteront son retour.

## Alternatives écartées

- **Port vcpkg recopié et modifié dans le dépôt :** une copie à maintenir à chaque mise à jour de Jolt, pour une option.
- **Drapeaux de compilation donnés par le triplet (`VCPKG_CXX_FLAGS`) :** le projet CMake de Jolt ajoute `/fp:fast` après eux ; le dernier gagne.
- **Compiler Jolt hors de vcpkg :** contraire au socle (cahier des charges, section 3.1 : dépendances par vcpkg en mode manifeste).
- **Changer de compilateur :** le cahier des charges fixe MSVC 14.52.
- **Ne rien faire et tester en Debug seulement :** la démonstration et les mesures se font en Release.

## Conséquences

- Premier lancement après ce changement : vcpkg recompile toutes les dépendances une fois, parce que le triplet fait partie de leur empreinte.
- La simulation devient identique d'une machine à l'autre, en plus d'être identique d'une exécution à l'autre.
- Coût : environ 8 % de temps de simulation en plus dans ce mode, d'après la documentation de Jolt ; à l'échelle de la démo, c'est négligeable.
- Garde-fous : aucun n'est touché. Règle de méthode concernée : un contournement doit être documenté (`AGENTS.md`, section 7).
- À revoir quand MSVC ou Jolt change de version : refaire l'essai avec `/fp:fast` ; si le défaut a disparu, supprimer le triplet et cette option.
- Le défaut n'a été signalé ni à Microsoft ni à l'auteur de Jolt : c'est au développeur de décider de le faire.
