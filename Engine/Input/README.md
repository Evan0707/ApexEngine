# Input

**Rôle :** transformer les événements bruts de `Platform` en actions (cahier des charges, sections 4.3 et 5.2). Aucun système de gameplay ne lit une touche directement : il lit une action.

**État (phase 1, en cours) :** table des actions (`ActionMap`), construite à partir d'associations décrites comme des données. L'état des actions (maintenue, pressée, relâchée, valeur d'axe) arrive dans la tâche suivante.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Input/ActionMap.h` | `ButtonBinding`, `KeyAxisBinding`, `MouseAxisBinding`, `InputBindings` : associations écrites avec des noms ; `ActionId`, `ActionKind`, `MouseAxis` ; `ResolvedButton`, `ResolvedKeyAxis`, `ResolvedMouseAxis` : associations résolues ; `ActionMap` : `create()`, `find()`, `name()`, `kind()`, `actionCount()`, `buttons()`, `keyAxes()`, `mouseAxes()` |

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

## Dépendances

`Core` et `Platform` (type `Key` ; `Event` s'y ajoutera avec l'état des actions). Interdit : toute règle de gameplay, SDL, Vulkan, l'API Windows.
