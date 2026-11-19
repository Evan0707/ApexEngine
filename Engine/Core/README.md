# Core

**Rôle :** socle du moteur. À terme : application, boucle à pas fixe, temps, logs, erreurs (cahier des charges, sections 4 et 5.1).

**État (phase 0) :** seule la version du moteur existe. Le reste arrive en phase 1.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Core/Version.h` | `Version`, `engineVersion()` |

## Dépendances

Bibliothèque standard et GLM uniquement. Aucun autre module du moteur, aucune bibliothèque tierce.
