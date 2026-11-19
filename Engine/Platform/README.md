# Platform

**Rôle :** frontière avec le système d'exploitation : fenêtre, événements bruts du clavier et de la souris, capture du curseur (cahier des charges, section 5.2). Seul module autorisé à appeler l'API Windows et à inclure SDL.

**État (phase 2) :** types des touches et des événements, fenêtre SDL3 (notes de décision `0002-sdl3.md` et `0005-precisions-sdl3.md`), pont vers `Graphics` et vers le backend ImGui de `Debug` par handles opaques (note `0009-frontieres-platform-graphics-debug.md`).

## Interface publique

| En-tête | Contenu |
|---|---|
| `Platform/Key.h` | `Key`, `allKeys()`, `keyFromName()`, `keyName()` |
| `Platform/Event.h` | `Event`, variante de `CloseRequested`, `WindowResized`, `WindowMinimized`, `WindowRestored`, `FocusLost`, `KeyChanged`, `MouseMoved` |
| `Platform/Paths.h` | `executableDirectory()` : dossier de l'exécutable, où le build copie `Assets/` (décision 0014) |
| `Platform/Window.h` | `WindowConfig`, `PixelSize`, `NativeEventObserver`, `Window` : `create()`, `pollEvents()`, `isMinimized()`, `sizeInPixels()`, `setCursorCaptured()`, `isCursorCaptured()`, `vulkanInstanceExtensions()`, `createVulkanSurface()`, `nativeHandle()`, `setNativeEventObserver()` |

Aucun en-tête public n'inclut SDL : le module expose ses propres types, et la conversion depuis SDL reste dans ses fichiers `.cpp`. `Window.h` déclare seulement `struct SDL_Window;`, sans rien inclure.

## Dossier de l'exécutable

- `executableDirectory()` renvoie le dossier qui contient l'exécutable en cours, par `SDL_GetBasePath()` (chemin UTF-8 converti en `std::filesystem::path`). Utilisable avant la création de la fenêtre. Échec : `PlatformFailure` avec le message de SDL.

## Fenêtre

- `Window::create(config, logger)` initialise le sous-système vidéo de SDL, puis crée la fenêtre (1920 × 1080 et redimensionnable par défaut). Échec : `ErrorCode::PlatformFailure` avec le message de `SDL_GetError()`, ou `ErrorCode::InvalidArgument` pour une largeur ou une hauteur nulle ou supérieure à `INT_MAX`. L'échec est aussi écrit dans le log.
- La `Window` possède le sous-système vidéo : le destructeur détruit la fenêtre puis arrête le sous-système. Elle est déplaçable, pas copiable ; un objet déplacé ne fait plus rien.
- **Une seule `Window` par processus**, créée et utilisée sur le thread principal. Aucun état global ne le vérifie (G-06) : c'est une règle d'utilisation.
- Le `Logger` passé à `create()` doit vivre plus longtemps que la fenêtre.
- `Game` garde un `main` standard : `Window.cpp` inclut `SDL_main.h` avec `SDL_MAIN_HANDLED`, uniquement pour déclarer `SDL_SetMainReady()`, appelé avant l'initialisation. `main` n'est pas redéfini.
- `pollEvents()` vide la file d'événements du système et renvoie les événements traduits. Le `span` reste valide jusqu'au prochain appel.
- `sizeInPixels()` renvoie la taille de la zone de dessin, `{0, 0}` quand la fenêtre est réduite ou que le système ne sait pas la donner. C'est la taille à utiliser pour la swapchain.
- `setCursorCaptured(true)` active le mode souris relatif de SDL : curseur caché et retenu dans la fenêtre, `MouseMoved` continue d'arriver. SDL relâche le curseur quand la fenêtre perd le focus et le reprend au retour du focus. En cas d'échec, un avertissement est écrit et l'état ne change pas.

## Pont vers Vulkan et ImGui

Décision 0009 : `Platform` n'inclut aucun en-tête Vulkan ni ImGui, et aucun type SDL ou Vulkan n'apparaît dans `Window.h`. Les handles traversent la frontière sous forme opaque.

- La fenêtre est créée avec `SDL_WINDOW_VULKAN`.
- `vulkanInstanceExtensions()` : extensions d'instance demandées par SDL pour présenter dans la fenêtre. Les chaînes appartiennent à SDL et restent valides tant que le sous-système vidéo tourne.
- `createVulkanSurface(void* vulkanInstance)` : reçoit un `VkInstance`, renvoie un `VkSurfaceKHR` converti en `std::uint64_t`. `Window.cpp` inclut `SDL_vulkan.h`, qui déclare lui-même ces handles. `Graphics` possède la surface et la détruit avant l'instance, donc avant la fenêtre.
- `nativeHandle()` : le `SDL_Window*`, pour le backend SDL3 d'ImGui.
- `setNativeEventObserver(observer)` : `pollEvents()` appelle l'observateur avec un `const SDL_Event*` pour chaque événement du système, avant sa traduction, y compris ceux que `Platform` ignore. Le pointeur n'est valide que pendant l'appel. Un observateur vide le retire.

Seuls `Graphics` (surface) et `Debug/ImGuiBackend.cpp` (fenêtre et événements) reconvertissent ces handles.

## Touches

- Une `Key` désigne une **position physique** sur le clavier, pas le caractère imprimé dessus (cahier des charges, section 3.3). Ses valeurs sont les codes USB HID du clavier (HID Usage Tables, page 0x07), qui sont aussi les valeurs des scancodes SDL. `Window.cpp` le vérifie par `static_assert` pour les 63 touches nommées.
- Piège : les noms sont ceux de la disposition **QWERTY américaine**. La touche marquée « Z » sur un clavier AZERTY est donc `Key::W`. C'est ce qui rend les associations indépendantes de la disposition : la même position physique donne la même `Key` en AZERTY et en QWERTY.
- Le nom d'une touche est le nom de son énumérateur : `"A"`, `"Digit1"`, `"Space"`, `"LeftShift"`, `"F1"`. C'est ce nom qu'utilisera `Config/input.json`.
- `keyFromName()` respecte la casse et renvoie `std::nullopt` pour un nom inconnu, vide ou égal à `"Unknown"`. `keyName()` renvoie `"Unknown"` pour `Key::Unknown` et pour toute valeur sans énumérateur.
- Touches nommées : lettres A à Z, chiffres `Digit0` à `Digit9`, `Return`, `Escape`, `Backspace`, `Tab`, `Space`, F1 à F12, flèches (`Left`, `Right`, `Up`, `Down`), `LeftCtrl`, `LeftShift`, `LeftAlt`, `RightCtrl`, `RightShift`, `RightAlt`.

Associations par défaut de la section 5.2, écrites en `Key` :

| Action | Touche AZERTY | `Key` |
|---|---|---|
| Avancer / reculer | Z / S | `W` / `S` |
| Gauche / droite | Q / D | `A` / `D` |
| Monter / descendre (caméra libre) | E / A | `E` / `Q` |
| Sauter | Espace | `Space` |
| Courir | Maj gauche | `LeftShift` |
| Capturer ou libérer le curseur | Échap | `Escape` |
| Panneau de debug, colliders, caméra, ombres | F1 à F4 | `F1` à `F4` |

## Événements

| Événement SDL | `Event` |
|---|---|
| `SDL_EVENT_QUIT`, `SDL_EVENT_WINDOW_CLOSE_REQUESTED` (croix, Alt + F4) | `CloseRequested` |
| `SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED` | `WindowResized` |
| `SDL_EVENT_WINDOW_MINIMIZED` | `WindowMinimized` |
| `SDL_EVENT_WINDOW_RESTORED`, seulement après une réduction | `WindowRestored` |
| `SDL_EVENT_WINDOW_FOCUS_LOST` | `FocusLost` |
| `SDL_EVENT_KEY_DOWN`, `SDL_EVENT_KEY_UP`, scancode d'une `Key` nommée | `KeyChanged` |
| `SDL_EVENT_MOUSE_MOTION` | `MouseMoved` |

Tout autre événement est ignoré, de même que les touches sans `Key` nommée.

- `WindowResized` donne la taille de la zone de dessin en pixels. SDL en envoie un dès la création de la fenêtre.
- Une fermeture produit en général deux `CloseRequested` (demande de fermeture de la fenêtre, puis fin de l'application) : le code qui les consomme doit le tolérer.
- `WindowMinimized` et `WindowRestored` vont par paires et `isMinimized()` suit le même état. Sous Windows, SDL envoie `SDL_EVENT_WINDOW_RESTORED` en sortie de réduction, même quand la fenêtre revient à l'état agrandi ; le même événement envoyé en quittant l'état agrandi n'est pas transmis.
- `KeyChanged` indique si la touche est enfoncée ou relâchée, et si l'événement vient de la répétition automatique du système.
- `MouseMoved` donne un déplacement relatif en pixels, Y vers le bas comme le rapporte le système : l'inversion vers la convention +Y en haut revient au code qui consomme l'événement.
- Aucune règle de gameplay ici : la traduction des touches en actions appartient au module `Input`.

## Dépendances

Autorisé : `Core`. SDL3 (3.4.20, lié en `PRIVATE` à `ApexEngine`), inclus seulement dans les fichiers `.cpp` du module. Interdit : Vulkan (aucun en-tête, aucun lien), Jolt, EnTT. Le port SDL est compilé avec sa fonctionnalité `vulkan` (décision 0006).

`ApexEngine` étant une seule cible CMake, la compilation n'empêche pas un autre module d'inclure SDL : le confinement se vérifie en cherchant `#include <SDL3/` hors de `Engine/Platform/`.
