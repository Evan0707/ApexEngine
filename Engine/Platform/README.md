# Platform

**Rôle :** frontière avec le système d'exploitation. À terme : fenêtre, événements bruts du clavier et de la souris, capture du curseur (cahier des charges, section 5.2). Seul module autorisé à appeler l'API Windows et à inclure SDL.

**État (phase 1, en cours) :** types des touches et des événements, en C++ pur. La fenêtre et SDL3 arrivent dans la tâche suivante (note de décision `0002-sdl3.md`).

## Interface publique

| En-tête | Contenu |
|---|---|
| `Platform/Key.h` | `Key`, `allKeys()`, `keyFromName()`, `keyName()` |
| `Platform/Event.h` | `Event`, variante de `CloseRequested`, `WindowResized`, `WindowMinimized`, `WindowRestored`, `FocusLost`, `KeyChanged`, `MouseMoved` |

Aucun en-tête public n'inclura SDL : le module expose ses propres types, et la conversion depuis SDL reste dans ses fichiers `.cpp`.

## Touches

- Une `Key` désigne une **position physique** sur le clavier, pas le caractère imprimé dessus (cahier des charges, section 3.3). Ses valeurs sont les codes USB HID du clavier (HID Usage Tables, page 0x07), qui sont aussi les valeurs des scancodes SDL.
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

- `WindowResized` donne la taille de la zone de dessin en pixels.
- `KeyChanged` indique si la touche est enfoncée ou relâchée, et si l'événement vient de la répétition automatique du système.
- `MouseMoved` donne un déplacement relatif en pixels, Y vers le bas comme le rapporte le système : l'inversion vers la convention +Y en haut revient au code qui consomme l'événement.
- Aucune règle de gameplay ici : la traduction des touches en actions appartient au module `Input`.

## Dépendances

Bibliothèque standard uniquement pour l'instant. Autorisé : `Core`. Interdit : Vulkan, Jolt, EnTT. SDL3 sera ajouté dans la tâche suivante, confiné aux fichiers `.cpp` du module.
