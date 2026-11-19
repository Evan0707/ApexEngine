# Debug

**Rôle :** outils de débogage : panneau ImGui affiché par F1, console des derniers avertissements et erreurs, statistiques de frame (cahier des charges, section 5.9).

**État (phase 2, tâche 9) :** statistiques de temps de frame et historique des logs, sans ImGui. L'interface ImGui arrive avec les tâches 10 et 11 (note de décision 0008).

## Interface publique

| En-tête | Contenu |
|---|---|
| `Debug/FrameTimeStats.h` | `FrameTimeStats` : `addFrame()`, `averageSeconds()`, `maxSeconds()`, `framesPerSecond()`, `frameCount()` |
| `Debug/LogHistory.h` | `LogEntry`, `LogHistory` : `add()`, `snapshot()`, `capacity()` |
| `Debug/LogHistorySink.h` | `LogHistorySink` : sink du logger qui alimente une `LogHistory` |

## Statistiques de frame

- `FrameTimeStats` garde les durées de frame sur une fenêtre glissante, d'une seconde par défaut : FPS, moyenne et maximum « sur une seconde » de la section 5.9.
- Une frame sort de la fenêtre quand les frames plus récentes couvrent à elles seules toute la fenêtre. La frame la plus récente reste toujours, même si elle dure plus que la fenêtre (cas d'un blocage de plusieurs secondes).
- Une durée négative ou non finie compte pour 0. Toutes les valeurs valent 0 avant la première frame.
- Les durées viennent de l'application, qui mesure la frame. Rien ici ne lit l'horloge.

## Historique des logs

- `LogHistory` garde les 100 derniers messages d'avertissement ou d'erreur, par défaut ; la capacité et le niveau minimum sont des paramètres. Au-delà de la capacité, le plus ancien est retiré.
- Le message est copié : la vue d'un `LogRecord` n'est valide que pendant `LogSink::write`.
- `snapshot()` renvoie une copie, du plus ancien au plus récent. Elle est protégée par un mutex propre à l'historique : le panneau peut la lire pendant qu'un autre thread logue.
- Le `Logger` possède ses sinks. L'application possède donc la `LogHistory` et donne au logger un `LogHistorySink` qui y renvoie. L'historique doit vivre plus longtemps que le logger : il est déclaré avant lui.

## Dépendances

Autorisé : tous les modules du moteur. Interdit : `Game`. Aucune bibliothèque tierce pour l'instant ; Dear ImGui y sera confiné (note 0008), et `Debug/ImGuiBackend.cpp` sera le seul fichier du module à inclure SDL et Vulkan (note 0009).
