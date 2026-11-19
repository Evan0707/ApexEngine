# Core

**Rôle :** socle du moteur. À terme : boucle à pas fixe, temps, logs, erreurs (cahier des charges, sections 4 et 5.1).

**État (phase 1, en cours) :** version du moteur, erreurs et assertions. Le logger et la boucle principale arrivent dans les tâches suivantes.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Core/Version.h` | `Version`, `engineVersion()` |
| `Core/Error.h` | `ErrorCode`, `Error`, `Result<T>`, `makeError()` |
| `Core/Assert.h` | `APEX_ASSERT(condition, message)` |

## Erreurs et assertions

- Un échec récupérable (fichier absent, donnée invalide, refus du système) renvoie un `Result<T>`, c'est-à-dire un `std::expected<T, Error>`. L'appelant décide quoi en faire.
- Une erreur de programmation passe par `APEX_ASSERT`. En Debug, l'assertion écrit la condition, le message et l'emplacement sur la sortie d'erreur, puis interrompt le programme. En Release, elle disparaît et sa condition n'est pas évaluée : la condition ne doit donc avoir aucun effet de bord.

## Dépendances

Bibliothèque standard et GLM uniquement. Aucun autre module du moteur, aucune bibliothèque tierce.
