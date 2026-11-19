# 0004 — Deux agents en parallèle dès la phase 1

**Date :** 19 novembre 2026
**Statut :** acceptée

## Contexte

Le cahier des charges impose un seul agent à la fois et n'envisage le parallélisme qu'après la phase 5, à deux agents au plus, sur des modules distincts (section 10.1). En phase 1, plusieurs tâches du plan touchent des modules distincts et ne dépendent pas l'une de l'autre : la tâche 3 (boucle principale, `Core`) et la tâche 4 (touches et événements, `Platform`). Le développeur demande à les mener en parallèle.

## Décision

Dès la phase 1, deux agents au plus travaillent en même temps, aux conditions suivantes :

- chaque agent mène une seule tâche du plan, sur sa propre branche et dans son propre worktree Git ;
- les deux tâches touchent des modules distincts ; seuls les fichiers de liste de sources (`Engine/CMakeLists.txt`, `Tests/CMakeLists.txt`) peuvent être modifiés par les deux ;
- une tâche qui dépend d'une autre ne démarre qu'une fois celle-ci terminée et intégrée ;
- l'agent principal intègre les branches, résout les conflits, recompile et relance les tests après chaque intégration ;
- toutes les autres règles de `AGENTS.md` s'appliquent sans changement à chaque agent.

## Alternatives écartées

- **Rester séquentiel :** conforme au cahier des charges, mais plus lent sur les tâches indépendantes.
- **Autant d'agents que de tâches indépendantes :** au-delà de deux, les tâches de la phase 1 se chevauchent (la tâche 9 modifie `MainLoop.cpp`, créé par la tâche 3) et la relecture devient le goulot d'étranglement.

## Conséquences

- Écart assumé avec la section 10.1 du cahier des charges, qui n'est pas modifié.
- Les branches sont relues et intégrées une par une ; un conflit hors des fichiers de liste de sources signale un mauvais découpage et arrête le parallélisme pour le lot suivant.
- Coût de retour en arrière : nul, il suffit de revenir à un seul agent.
