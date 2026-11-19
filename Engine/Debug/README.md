# Debug

**Rôle :** outils de débogage : panneau ImGui affiché par F1, console des derniers avertissements et erreurs, statistiques de frame (cahier des charges, section 5.9).

**État (phase 2, tâche 10) :** statistiques de temps de frame, historique des logs et intégration de Dear ImGui (note de décision 0008). Le panneau de la section 5.9 arrive avec la tâche 11 ; pour l'instant, une petite fenêtre affiche la version d'ImGui et les FPS.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Debug/FrameTimeStats.h` | `FrameTimeStats` : `addFrame()`, `averageSeconds()`, `maxSeconds()`, `framesPerSecond()`, `frameCount()` |
| `Debug/LogHistory.h` | `LogEntry`, `LogHistory` : `add()`, `snapshot()`, `capacity()` |
| `Debug/LogHistorySink.h` | `LogHistorySink` : sink du logger qui alimente une `LogHistory` |
| `Debug/DebugUi.h` | `DebugUi` : `create()`, `buildFrame()`, `wantsKeyboard()`, `wantsMouse()` ; implémente `graphics::RenderOverlay` |

En-tête interne : `Debug/ImGuiBackend.h` (`ImGuiBackend`), les backends SDL3 et Vulkan de Dear ImGui.

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

## Interface de debug

- `DebugUi` possède le contexte Dear ImGui. Un seul par processus : ImGui garde un pointeur global vers son contexte courant, un état interne à la bibliothèque (G-06, note 0008).
- `create()` initialise ImGui et ses backends, puis s'enregistre comme overlay du renderer. Le destructeur se retire du renderer, attend le GPU, arrête les backends et détruit le contexte. `DebugUi` est donc créé après le renderer et détruit avant lui.
- `buildFrame()` est appelé une fois par frame, après la phase d'entrées et avant `Renderer::renderFrame()`. Il construit l'interface ; le renderer la dessine à la fin de sa passe principale, par `record()`.
- `Game` n'inclut jamais ImGui : `DebugUi` construit lui-même ses fenêtres.
- Pas de fichier `imgui.ini` ni de log ImGui : rien n'est écrit dans le dossier de lancement.
- Piège : les couleurs du thème ImGui sont en sRGB et la swapchain est sRGB. Elles sont converties une fois en linéaire au démarrage ; sans cela, l'interface est encodée deux fois et paraît délavée.

## Backends (`ImGuiBackend.cpp`)

- Seul fichier du moteur à inclure à la fois SDL, Vulkan et les backends d'ImGui (note 0009). Il reconvertit les handles opaques de `Window` (`nativeHandle()`, observateur d'événements) et lit les handles Vulkan par `Graphics/VulkanBackendAccess.h`.
- Backend Vulkan en dynamic rendering, avec le format de la swapchain (G-09). Si une recréation change ce format, le pipeline d'ImGui est recréé ; le GPU est alors déjà inactif.
- Le backend crée son propre pool de descripteurs et envoie lui-même la texture de la police au GPU.

## Dépendances

Autorisé : tous les modules du moteur. Interdit : `Game`. Dear ImGui 1.93.0, avec ses backends SDL3 et Vulkan, lié en `PRIVATE` à `ApexEngine` et inclus seulement dans `Debug` (note 0008).
