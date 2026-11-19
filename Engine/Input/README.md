# Input

**Rôle :** transformer les événements bruts de `Platform` en actions (cahier des charges, sections 4.3 et 5.2). Aucun système de gameplay ne lit une touche directement : il lit une action.

**État (phase 1, en cours) :** table des actions (`ActionMap`), construite à partir d'associations décrites comme des données, et état des actions (`ActionState`) : maintenue, pressée, relâchée, valeur d'axe.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Input/ActionMap.h` | `ButtonBinding`, `KeyAxisBinding`, `MouseAxisBinding`, `InputBindings` : associations écrites avec des noms ; `ActionId`, `ActionKind`, `MouseAxis` ; `ResolvedButton`, `ResolvedKeyAxis`, `ResolvedMouseAxis` : associations résolues ; `ActionMap` : `create()`, `find()`, `name()`, `kind()`, `actionCount()`, `buttons()`, `keyAxes()`, `mouseAxes()` |
| `Input/ActionState.h` | `ActionState` : `apply()`, `accumulate()`, `clearTransient()`, `isHeld()`, `wasPressed()`, `wasReleased()`, `axis()` |

## Associations

- Les associations sont des **données**. En phase 1, le jeu les fournit dans une table C++ ; en phase 5, `Serialization` les lira dans `Config/input.json`. Le module ne connaît aucun nom d'action : il n'y a aucune règle de gameplay ici.
- Une touche est désignée par son nom de `platform::Key` (`"W"`, `"Space"`, `"LeftShift"`), c'est-à-dire une position physique nommée d'après le QWERTY américain (voir `Engine/Platform/README.md`). Le nom respecte la casse : `"space"` est refusé.
- Trois sortes d'associations :
  - **bouton** : une touche déclenche une action de type `Button` ;
  - **axe à deux touches** : une touche négative et une touche positive donnent une action de type `Axis` ;
  - **axe de souris** : le déplacement relatif sur X ou Y, multiplié par `scale`, donne une action de type `Axis`. Une échelle négative inverse l'axe ; le déplacement brut de `MouseMoved` a +Y vers le bas.
- Plusieurs associations peuvent viser la même action : deux touches pour un bouton, ou un axe à deux touches et un axe de souris pour un même axe. Elles partagent alors le même `ActionId`.
- Une même touche peut servir à plusieurs actions.

## Construction et identifiants

`ActionMap::create()` résout les noms une fois pour toutes et renvoie un `Result<ActionMap>`. Il échoue avec `ErrorCode::InvalidArgument` et un message qui nomme l'action et la touche fautives si :

- un nom d'action est vide ;
- un nom de touche est inconnu (`platform::keyFromName()` ne le reconnaît pas) ;
- l'échelle d'un axe de souris n'est pas un nombre fini (NaN ou infini) ;
- une même action est associée à la fois comme bouton et comme axe ;
- il y a plus de 65 536 actions (un `ActionId` tient sur 16 bits).

Les `ActionId` sont des indices denses, de 0 à `actionCount() - 1`, attribués dans l'ordre de première apparition : boutons d'abord, puis axes à deux touches, puis axes de souris. `find()` renvoie `std::nullopt` pour un nom inconnu. `name()` et `kind()` sur un identifiant qui n'appartient pas à la table sont une erreur de programmation (`APEX_ASSERT`).

Le jeu cherche ses identifiants une fois, à l'initialisation, puis ne manipule plus que des `ActionId`.

## Exemple : associations par défaut de la section 5.2

Clavier AZERTY, écrit en positions physiques : la touche AZERTY « Z » est `"W"`, « Q » est `"A"`, « A » est `"Q"`.

```cpp
const apex::input::InputBindings bindings{
    .buttons = {{"Jump", "Space"},
                {"Sprint", "LeftShift"},
                {"ToggleCursor", "Escape"},
                {"ToggleDebug", "F1"},
                {"SwitchCamera", "F3"}},
    .keyAxes = {{"MoveForward", "S", "W"},  // AZERTY S / Z
                {"MoveRight", "A", "D"},    // AZERTY Q / D
                {"MoveUp", "Q", "E"}},      // AZERTY A / E
    .mouseAxes = {{"LookX", apex::input::MouseAxis::X, 1.0f},
                  {"LookY", apex::input::MouseAxis::Y, -1.0f}},
};
auto actions = apex::input::ActionMap::create(bindings);
```

F2 (colliders) et F4 (ombres) sont des fonctions P1 : leurs actions seront ajoutées avec ces fonctions.

## État des actions

Un `ActionState` est construit à partir d'une `ActionMap` : une case par action, toutes relâchées, axes à 0. Il ne garde ni pointeur ni référence vers la table, qu'on lui repasse à chaque `apply()` ; ce doit être la même table.

`apply(map, events)` lit les événements de `Platform` :

- `KeyChanged` met à jour les touches physiques enfoncées. Un événement de répétition automatique (`repeat`) est ignoré, de même qu'un appui sur une touche déjà enfoncée ou un relâchement d'une touche déjà relâchée.
- Une action `Button` est maintenue (`isHeld()`) tant qu'**au moins une** de ses touches est enfoncée. `wasPressed()` indique qu'elle est passée de relâchée à maintenue au moins une fois depuis le dernier `clearTransient()`, `wasReleased()` l'inverse. Appuyer sur une deuxième touche d'une action déjà maintenue n'est pas un nouvel appui. Un appui et un relâchement dans la même frame donnent les deux fronts, et l'action finit relâchée : une frappe brève n'est pas perdue.
- Axe à deux touches : +1 si la touche positive est enfoncée, −1 si la touche négative l'est, donc 0 avec les deux. Les axes à deux touches d'une même action s'additionnent, puis la somme est bornée à [−1, 1].
- `MouseMoved` : le déplacement, multiplié par `scale`, s'ajoute à l'axe depuis le dernier `clearTransient()`. Il est en unités brutes et n'est pas borné.
- `axis()` renvoie la valeur des touches plus le déplacement de souris accumulé.
- `FocusLost` relâche toutes les touches enfoncées, avec les fronts `wasReleased()` correspondants : une touche relâchée hors de la fenêtre ne reste pas bloquée.
- Les autres événements (fermeture, redimensionnement, réduction) ne concernent pas `Input` et sont ignorés.

`isHeld()`, `wasPressed()` et `wasReleased()` sur une action `Axis`, `axis()` sur une action `Button`, ou un identifiant hors de la table sont des erreurs de programmation (`APEX_ASSERT`).

`Input` ne connaît pas le temps (G-05) : il compte des événements, pas des durées.

### Deux états : la frame et la simulation

Une frame exécute de zéro à quatre pas de simulation (cahier des charges, section 5.1). Un seul état ne suffit pas :

- vidé à chaque frame, il perdrait un appui survenu pendant une frame sans pas ;
- vidé à chaque pas, il donnerait le même appui à tous les pas d'une frame et compterait plusieurs fois le déplacement de la souris, ou le perdrait.

Le jeu garde donc deux `ActionState` construits sur la même table : `frame` reçoit les événements, `simulation` est lu par le gameplay. `accumulate(frame)` fusionne une frame dans l'état de simulation : l'état maintenu et la valeur des touches sont copiés, les fronts sont combinés par un OU, les déplacements de souris sont additionnés. `clearTransient()` efface les fronts et les déplacements de souris ; l'état maintenu reste.

Séquence à suivre, sans exception :

1. À chaque frame, dans la phase d'entrées :
   ```cpp
   frame.apply(actions, window.pollEvents());
   simulation.accumulate(frame);
   frame.clearTransient();
   ```
2. À chaque pas fixe : le gameplay lit uniquement `simulation`, puis, après le pas, `simulation.clearTransient()`.

Conséquences :

- le premier pas qui suit un front le voit, les pas suivants de la même frame ne le voient plus ;
- un appui pendant une frame sans pas est vu par le premier pas de la frame suivante ;
- le déplacement de la souris de plusieurs frames sans pas est additionné, puis compté une seule fois, par le pas suivant.

Les actions qui ne relèvent pas de la simulation (capture du curseur, panneau de debug) peuvent lire `frame` dans la phase d'entrées, avant `frame.clearTransient()`.

## Dépendances

`Core` et `Platform` (types `Key` et `Event`). Interdit : toute règle de gameplay, SDL, Vulkan, l'API Windows.
