# Core

**Rôle :** socle du moteur. À terme : boucle à pas fixe, temps, logs, erreurs (cahier des charges, sections 4 et 5.1).

**État (phase 1, en cours) :** version du moteur, erreurs, assertions, logger, pas fixe et boucle principale.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Core/Version.h` | `Version`, `engineVersion()` |
| `Core/Error.h` | `ErrorCode`, `Error`, `Result<T>`, `makeError()` |
| `Core/Assert.h` | `APEX_ASSERT(condition, message)` |
| `Core/LogSink.h` | `LogLevel`, `LogRecord`, `LogSink`, `formatRecord()` |
| `Core/Logger.h` | `Logger` : `addSink()`, `setMinimumLevel()`, `log()`, `trace()` à `error()` |
| `Core/ConsoleLogSink.h` | `ConsoleLogSink` : stdout, ou stderr pour les avertissements et les erreurs |
| `Core/FileLogSink.h` | `FileLogSink::open(path)` : un fichier par exécution |
| `Core/FixedTimestep.h` | `FixedTimestepConfig`, `FrameAdvance`, `FixedTimestep` : `advance(frameSeconds)`, `config()` |
| `Core/FramePhases.h` | `FixedStep`, `FramePhases` : interface des phases d'une frame, implémentée par l'application |
| `Core/MainLoop.h` | `MainLoop` : `runFrame(phases, frameSeconds)`, `run(phases)` |

## Erreurs et assertions

- Un échec récupérable (fichier absent, donnée invalide, refus du système) renvoie un `Result<T>`, c'est-à-dire un `std::expected<T, Error>`. L'appelant décide quoi en faire.
- Une erreur de programmation passe par `APEX_ASSERT`. En Debug, l'assertion écrit la condition, le message et l'emplacement sur la sortie d'erreur, puis interrompt le programme. En Release, elle disparaît et sa condition n'est pas évaluée : la condition ne doit donc avoir aucun effet de bord.

## Logger

- Il n'existe pas de logger global (garde-fou G-06) : l'application crée un `Logger` et le passe par référence à chaque système qui logue.
- Les messages sont formatés par `std::format`, vérifié à la compilation. Un message sous le niveau minimum (`Info` par défaut) n'est pas formaté.
- Le logger est utilisable depuis plusieurs threads. Les sinks sont appelés sous verrou, un message à la fois, dans l'ordre d'ajout ; un sink n'a donc pas à se protéger lui-même.
- Format d'une ligne : `2026-11-19 14:03:22.123 [warning] message`, heure en UTC.
- `FileLogSink` crée les dossiers manquants, écrase le fichier de l'exécution précédente et vide son tampon après chaque ligne, pour qu'un plantage ne perde rien.
- Le panneau ImGui de la phase 2 sera un sink de plus.

## Pas fixe et boucle principale

- La simulation avance par pas fixes de 1/60 s, avec accumulateur (cahier des charges, section 5.1). Le temps de frame est ramené dans [0 ; 0,25] s : une valeur négative ou invalide compte pour 0.
- Quatre pas au maximum par frame. Au-delà, le temps accumulé en trop est abandonné, sauf la fraction de pas restante : la simulation ralentit au lieu de s'emballer.
- `FrameAdvance::alpha`, dans [0 ; 1[, situe l'instant courant entre les deux derniers états de simulation. Il servira à interpoler les transformations au rendu.
- Une tolérance relative de 10⁻⁶ sur la comparaison au pas absorbe les erreurs d'arrondi : trois frames d'un tiers de pas donnent bien un pas.
- Ordre des phases d'une frame : `processInput`, `simulate` (de zéro à quatre fois), `extract(alpha)`, `render`, `present`. Si `processInput` renvoie `false`, rien d'autre ne s'exécute et `runFrame` renvoie `false`.
- `simulate` ne reçoit que le pas fixe (`FixedStep` : numéro de pas à partir de 0 et durée du pas), jamais le temps réel de la frame (garde-fou G-05).
- `runFrame` prend la durée de frame en paramètre et se teste sans horloge. `run` mesure le temps réel avec `std::chrono::steady_clock` et appelle `runFrame` jusqu'à la demande d'arrêt.
- Aucun état global : la boucle et son accumulateur appartiennent à l'objet `MainLoop` (garde-fou G-06).

## Dépendances

Bibliothèque standard et GLM uniquement. Aucun autre module du moteur, aucune bibliothèque tierce.
