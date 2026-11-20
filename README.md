# Apex Engine

Prototype de moteur 3D en C++23 : Vulkan, Windows x64. Première fondation d'un futur moteur orienté monde ouvert.

**État : phase 6 en cours (Jolt, corps rigides, personnage, caméra troisième personne).** `ApexDemo` charge sa scène dans `Assets/Scenes/demo.json` et sa configuration dans `Config/`, simule ses corps rigides avec Jolt Physics, puis dessine par Vulkan 1.3 ce que la scène contient : un terrain de test avec rampes, plateformes, mur et caisses à pousser. La vue démarre en caméra libre ; F3 donne les commandes à un personnage suivi par une caméra troisième personne. Un panneau de debug Dear ImGui s'affiche par F1. Il fait tourner la boucle à pas fixe, logue les actions et se ferme proprement.

## Prérequis

| Outil | Version | Remarque |
|---|---|---|
| Visual Studio 2026 | 18.12 ou plus récent | Charge de travail « Développement Desktop en C++ ». Fournit MSVC 14.52, CMake et Ninja |
| Git | récente | |
| vcpkg | cloné à part | Voir ci-dessous |
| SDK Vulkan (LunarG) | 1.4.363.1 | Fournit `glslc`, qui compile les shaders pendant le build, et les validation layers utilisées en Debug. Rouvrir le terminal après l'installation, pour que `VULKAN_SDK` soit défini |

Installation de vcpkg, une seule fois :

```
git clone https://github.com/microsoft/vcpkg C:\dev\vcpkg
C:\dev\vcpkg\bootstrap-vcpkg.bat
setx VCPKG_ROOT C:\dev\vcpkg
```

Rouvrir le terminal après `setx`.

## Compiler et tester

Depuis un « x64 Native Tools Command Prompt for VS 2026 » :

```
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
build\windows-debug\bin\ApexDemo.exe
```

Remplacer `windows-debug` par `windows-release` pour la configuration Release, ou par `windows-profile` pour une Release instrumentée par Tracy (voir `Engine/Core/README.md`).

## La démonstration

`ApexDemo` ouvre une fenêtre de 1920 × 1080 et écrit ses logs dans la console et dans `Logs/apex.log`, relatif au dossier de lancement. Il lit sa scène et ses modèles dans le dossier `Assets`, et sa configuration dans le dossier `Config`, tous deux placés à côté de lui : le build les y copie à chaque compilation.

Pour modifier la scène sans recompiler (AC-04), lancer la démo depuis la racine du dépôt en lui désignant les dossiers sources, puis éditer `Assets/Scenes/demo.json` et relancer :

```
build\windows-debug\bin\ApexDemo.exe --assets Assets --config Config
```

Le format de la scène et de la configuration est décrit dans `Engine/Serialization/README.md`. Un champ inconnu ou invalide arrête la démo avec une erreur qui nomme le fichier, l'entité et le champ.

| Option | Effet |
|---|---|
| `--frames N` | Quitte seul après N frames |
| `--no-vsync` | Désactive la synchronisation verticale, pour les mesures |
| `--assets DIR` | Dossier des assets ; par défaut, le dossier `Assets` copié à côté de l'exécutable par le build |
| `--config DIR` | Dossier de `engine.json` (taille de la fenêtre, caméra libre, gravité, personnage, caméra troisième personne) et `input.json` (touches) ; par défaut, le dossier `Config` copié à côté de l'exécutable. Un fichier absent donne les valeurs par défaut, avec un avertissement |
| `--scene PATH` | Scène à charger, chemin relatif au dossier des assets ; `Scenes/demo.json` par défaut |
| `--model PATH` | Ajoute un modèle, chemin relatif au dossier des assets, posé au sol à droite du personnage, sans collider. Un modèle introuvable ou invalide est remplacé par un cube en damier magenta, et l'erreur est logée avec le chemin (AC-11) |
| `--benchmark S` | Mesure de performance (section 7.1) : la caméra suit un cercle autour de la scène ; après 2 s de mise en route, S secondes de frames sont mesurées, le résultat est logué avec la mémoire GPU de fin de mesure, puis l'application quitte. À lancer en Release avec `--no-vsync` (voir `Docs/benchmark.md`) |

| Code de sortie | Signification |
|---|---|
| 0 | Fermeture normale |
| 1 | Échec : fenêtre, GPU, option invalide, scène ou configuration invalide, ou erreur pendant le rendu (voir le log) |
| 2 | Fermeture normale, mais les validation layers ont émis au moins un message |

| Touche (AZERTY) | Effet |
|---|---|
| F3 | Bascule entre la caméra libre (au lancement) et le personnage suivi par la caméra troisième personne |
| Z / S, Q / D | Caméra libre : avance, recule, va à gauche ou à droite dans la direction du regard (5 m/s). Personnage : marche à 4 m/s dans la direction de la caméra, et se tourne vers où il va |
| E / A | Caméra libre : monte ou descend à la verticale |
| Maj gauche | Caméra libre : vitesse multipliée par 4. Personnage : court à 7 m/s |
| Espace | Personnage : saute de 1,2 m, depuis le sol seulement |
| Échap | Capture ou libère le curseur |
| Souris | Quand le curseur est capturé : oriente la caméra libre, ou fait tourner la caméra troisième personne autour du personnage |
| F1 | Affiche ou masque le panneau de debug, où se règle le champ de vision (50° à 100°) |
| F2, F4 | Logue l'appui ; sans effet avant la phase 7 |
| Croix, Alt + F4 | Ferme l'application, code de sortie 0 |

### Parcours de validation (cahier des charges, section 6.2)

Au lancement, la caméra libre regarde le cube de référence ; le personnage (capsule orange, visière sombre sur le visage) est juste devant elle. Appuyer sur Échap pour capturer la souris.

| Étape | Où | Ce qu'on doit voir |
|---|---|---|
| 1. Lancer | | La scène se charge seule ; la pile de trois caisses, à droite, ne bouge pas |
| 2. Caméra libre | Z, Q, S, D, E, A, souris | Déplacement sur trois axes |
| 3. F3 | | La caméra passe derrière le personnage, à 4 m ; la souris la fait tourner autour de lui |
| 4. Marcher, courir, monter une rampe | À gauche du départ : rampe beige de 15°, plateforme grise de 1,5 m, rampe de 35°, plateforme de 3 m | Le personnage accélère et freine progressivement, se tourne vers où il va, et monte les deux rampes |
| 5. Sauter sur la plateforme de 1 m | Plateforme jaune, derrière le point de départ, à gauche | Elle arrête le personnage qui marche ; Espace en avançant le pose dessus |
| 6. Courir contre le mur de 10 cm | Mur rouge, à droite | Le personnage s'arrête contre le mur, sans le traverser ni trembler |
| 7. Pousser une caisse, faire tomber la pile | Caisses à droite ; pile de trois devant à droite | Une caisse seule glisse devant le personnage ; la pile tombe quand on marche dedans |
| 8. F1 | | Le panneau donne les FPS, les entités (32), les corps physiques (27) et ceux qui ne dorment pas |
| 9. Redimensionner, réduire, restaurer | | Aucun accroc, aucune erreur dans le panneau |
| 10. Fermer | Croix ou Alt + F4 | Code de sortie 0 |

La caméra troisième personne ne doit jamais passer sous le sol ni derrière un mur : en levant la souris au maximum ou en reculant contre le mur rouge, elle se rapproche du personnage au lieu de traverser.

Avec Visual Studio : ouvrir le dossier du dépôt, les presets sont détectés automatiquement.

La première configuration télécharge et compile les dépendances vcpkg (une dizaine de minutes, dont Jolt Physics) ; les suivantes sont rapides. Les presets désignent le triplet vcpkg du dépôt, `cmake/triplets/x64-windows.cmake` : Jolt y est compilé en flottants précis, parce que sa version Release par défaut est instable avec MSVC 19.52 (décision 0020).

## Organisation

```text
Engine/      bibliothèque statique du moteur, un dossier par module
Game/        exécutable de démonstration
Tests/       tests automatisés (Catch2), sans fenêtre ni GPU
Assets/      scène, modèles et textures de la démonstration, licences dans Assets/LICENSES.md
Config/      engine.json (fenêtre, caméras, gravité, personnage) et input.json (touches)
Docs/        cahier des charges, architecture, décisions, suivi des heures
Tools/       scripts Python (génération des assets), voir Tools/README.md
cmake/       options de compilation communes, triplet vcpkg du dépôt (décision 0020)
```

## Documentation

- `Docs/cahier_des_charges.md` : spécification du prototype (v0.3), source de vérité.
- `Docs/architecture.md` : architecture et revue des garde-fous.
- `Docs/decisions/` : notes de décision.
- `Docs/benchmark.md` : machine de référence, protocole et résultats des mesures de performance.
- `Docs/suivi_heures.md` : heures prévues et réelles par phase.
- `AGENTS.md` : règles pour les agents IA.
- `ThirdPartyLicenses.md` : licences des dépendances.

## Intégration continue

À chaque push, GitHub Actions vérifie le formatage, compile en Debug, en Release et avec le preset `windows-profile`, puis lance les tests (`.github/workflows/ci.yml`).
