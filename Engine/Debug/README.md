# Debug

**Rôle :** outils de débogage : panneau ImGui affiché par F1, console des derniers avertissements et erreurs, statistiques de frame (cahier des charges, section 5.9).

**État (phase 3) :** panneau de debug de la section 5.9 (Dear ImGui, note de décision 0008), affiché par F1 : FPS, temps CPU et temps de frame, entités, objets dessinés, triangles, mémoire GPU, réglage du champ de vision (note 0011), console des 100 derniers avertissements et erreurs. Les éléments P1 du panneau arrivent en phase 7.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Debug/FrameTimeStats.h` | `FrameSample`, `FrameTimeStats` : `addFrame()`, `framesPerSecond()`, `averageFrameSeconds()`, `maxFrameSeconds()`, `averageCpuSeconds()`, `maxCpuSeconds()`, `frameCount()` |
| `Debug/LogHistory.h` | `LogEntry`, `LogHistory` : `add()`, `snapshot()`, `capacity()` |
| `Debug/LogHistorySink.h` | `LogHistorySink` : sink du logger qui alimente une `LogHistory` |
| `Debug/DebugUi.h` | `DebugUi` : `create()`, `buildFrame(data, controls)`, `setPanelVisible()`, `panelVisible()`, `setMouseInput()`, `wantsKeyboard()`, `wantsMouse()` ; implémente `graphics::RenderOverlay` |
| `Debug/StatsPanel.h` | `DebugPanelData` : ce que le panneau affiche, rempli par l'application ; `DebugControls` : les réglages que le panneau modifie, appliqués par l'application |
| `Debug/BenchmarkRecorder.h` | `BenchmarkSummary`, `BenchmarkRecorder` : `addFrame()`, `elapsedSeconds()`, `summary()` |
| `Debug/InputCapture.h` | `filterCapturedEvents()` : retire les événements utilisés par l'interface |
| `Debug/ColorSpace.h` | `srgbToLinear()` |

En-tête interne : `Debug/ImGuiBackend.h` (`ImGuiBackend`), les backends SDL3 et Vulkan de Dear ImGui. `drawStatsPanel()`, déclarée dans `StatsPanel.h`, n'est appelée que par `DebugUi`.

## Statistiques de frame

- `FrameTimeStats` garde, pour chaque frame, sa durée et son temps CPU, sur une fenêtre glissante d'une seconde de durée de frame par défaut : FPS, moyenne et maximum « sur une seconde » de la section 5.9.
- Temps CPU d'une frame : sa durée moins le temps passé bloqué sur le GPU ou sur la présentation (`FrameCounters::waitSeconds`, mesuré par le renderer). Avec la vsync, la durée de frame vaut à peu près la période de l'écran ; le temps CPU montre le travail réel. Il est plafonné par la durée de frame.
- Une frame sort de la fenêtre quand les frames plus récentes couvrent à elles seules toute la fenêtre. La frame la plus récente reste toujours, même si elle dure plus que la fenêtre (cas d'un blocage de plusieurs secondes).
- Une durée négative ou non finie compte pour 0. Toutes les valeurs valent 0 avant la première frame.
- Les durées viennent de l'application, qui mesure la frame. Rien ici ne lit l'horloge. Ce temps réel n'alimente que le panneau, jamais la simulation (G-05).

## Mesure de performance

- `BenchmarkRecorder` garde toutes les durées de frame d'une mesure (section 7.1) : nombre de frames, durée, FPS moyen (frames divisées par la durée), durée moyenne, 99e centile et maximum, en millisecondes.
- 99e centile au rang le plus proche : la plus petite durée que 99 % des frames ne dépassent pas, calculée en entiers (`ceil(0,99 n)`) pour ne pas dépendre d'un arrondi.
- Comme `FrameTimeStats`, il ne lit pas l'horloge : l'application lui donne les durées. Une durée négative ou non finie compte pour 0.

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

## Panneau (F1)

- Masqué au démarrage. L'action `ToggleDebug` (F1) l'affiche ou le masque ; son bouton de fermeture le masque aussi.
- Contenu (section 5.9) : FPS ; temps CPU par frame et durée de frame, moyenne et maximum sur une seconde ; entités (0 avant le module `World`, phase 5) ; objets dessinés et triangles de la dernière frame ; mémoire GPU utilisée et budget ; console des 100 derniers avertissements et erreurs, en jaune et en rouge.
- Réglages (note 0011) : `buildFrame(data, controls)` reçoit une `DebugControls` que les widgets modifient : champ de vision vertical, de 50° à 100° (section 5.3). Le panneau ne connaît pas la caméra ; l'application lit la valeur et l'applique. Le même mécanisme servira à l'exposition et aux bascules de la phase 7.
- La console suit les nouvelles lignes tant que la vue est en bas ; remonter permet de lire les plus anciennes.
- Les couleurs ajoutées par le panneau sont données en sRGB et converties en linéaire, comme le thème.

## Entrées

- `filterCapturedEvents()` retire des événements ce que l'interface utilise : les appuis de touche quand ImGui a le clavier (les relâchements restent, pour qu'une touche tenue avant soit relâchée), le mouvement de la souris quand ImGui a la souris. Les événements de fenêtre restent toujours.
- Quand le jeu capture le curseur (mode relatif), `setMouseInput(false)` coupe la souris d'ImGui : le curseur caché ne doit pas survoler le panneau et voler la souris au jeu.

## Backends (`ImGuiBackend.cpp`)

- Seul fichier du moteur à inclure à la fois SDL, Vulkan et les backends d'ImGui (note 0009). Il reconvertit les handles opaques de `Window` (`nativeHandle()`, observateur d'événements) et lit les handles Vulkan par `Graphics/VulkanBackendAccess.h`.
- Backend Vulkan en dynamic rendering, avec le format de la swapchain (G-09). Si une recréation change ce format, le pipeline d'ImGui est recréé ; le GPU est alors déjà inactif.
- Le backend crée son propre pool de descripteurs et envoie lui-même la texture de la police au GPU.

## Dépendances

Autorisé : tous les modules du moteur. Interdit : `Game`. Dear ImGui 1.93.0, avec ses backends SDL3 et Vulkan, lié en `PRIVATE` à `ApexEngine` et inclus seulement dans `Debug` (note 0008).
