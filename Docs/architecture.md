# Architecture

Ce document décrit l'architecture réellement en place. Il est complété à la fin de chaque phase. La cible est décrite dans `cahier_des_charges.md`, section 4.

## État actuel (phase 0)

| Cible CMake | Type | Contenu |
|---|---|---|
| `ApexEngine` | Bibliothèque statique | Module `Core` : version du moteur |
| `ApexDemo` | Exécutable | Affiche la version et se termine |
| `ApexTests` | Exécutable de tests | Tests de `Core` et de la chaîne de compilation |
| `Apex::Options` | Cible d'interface | Options de compilation communes : C++23, avertissements en erreurs |

## Modules en place

| Module | Rôle | Dépend de | Documentation |
|---|---|---|---|
| Core | Version du moteur (phase 0) | Bibliothèque standard | `Engine/Core/README.md` |

## Revue des garde-fous (AC-17)

À remplir à la fin de chaque phase. Un garde-fou sans objet à ce stade est noté « sans objet ».

| ID | Phase 0 | Phase 1 | Phase 2 | Phase 3 | Phase 4 | Phase 5 | Phase 6 | Phase 7 | Phase 8 |
|---|---|---|---|---|---|---|---|---|---|
| G-01 | sans objet | | | | | | | | |
| G-02 | sans objet | | | | | | | | |
| G-03 | sans objet | | | | | | | | |
| G-04 | sans objet | | | | | | | | |
| G-05 | sans objet | | | | | | | | |
| G-06 | conforme | | | | | | | | |
| G-07 | sans objet | | | | | | | | |
| G-08 | sans objet | | | | | | | | |
| G-09 | sans objet | | | | | | | | |
| G-10 | conforme | | | | | | | | |
