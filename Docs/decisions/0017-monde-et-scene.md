# 0017 — Monde, format de scène et configuration

**Date :** 19 novembre 2026
**Statut :** acceptée (plan de la phase 5 validé par le développeur le 19 novembre 2026)

## Contexte

La phase 5 fait venir toute la scène de `Assets/Scenes/demo.json` (cahier des charges, sections 5.6 et 6.1) et la configuration de `Config/` (section 5.2). L'exemple de format de la section 5.6 ne montre ni la caméra, ni la lumière, ni le terme ambiant, et plusieurs règles restent à fixer.

## Décision

1. **Matériau dans la scène.** Une entité peut porter un champ facultatif `material` (couleur de base, métal, rugosité, sans texture) qui remplace le matériau de toutes les parties de son modèle. Deux modèles suffisent ainsi au sol, aux boîtes colorées et aux sphères de référence. Le composant `MeshRenderer` désigne un modèle et, au besoin, ce matériau.
2. **Champs inconnus refusés.** Un champ que le format ne connaît pas est une erreur qui le nomme, à tous les niveaux du fichier : une faute de frappe se voit au lancement.
3. **Configuration.** `Config/engine.json` (taille de la fenêtre, réglages de la caméra libre) et `Config/input.json` (table d'actions de la section 5.2) sont copiés à côté de l'exécutable comme `Assets/` ; l'option `--config <dossier>` en désigne un autre. Un fichier absent donne les valeurs par défaut et un avertissement ; un fichier invalide arrête l'application avec le code de sortie 1.
4. **Composants propres au jeu attachés par identifiant.** `Serialization` ne connaît pas `Game` (section 4.2). Après le chargement, `Game` attache ses composants aux entités qu'il reconnaît par leur identifiant : `camera` reçoit le contrôleur de caméra libre, `spinner` tourne sur elle-même. Le personnage de la phase 6 suivra la même règle.
5. **`Serialization` ne dépend pas d'`Input`.** `Serialization` rend sa propre structure (`InputConfig`, des chaînes) ; `Game` la convertit en table d'actions. La table de dépendances de la section 4.2 reste inchangée.
6. **`Collider` et `RigidBody` dès la phase 5.** Les deux composants sont définis, lus et validés, sans effet avant la physique de la phase 6.

Format de scène, version 1, complété :

- Racine : `version` (obligatoire, 1), `ambient` (trois nombres, terme ambiant linéaire), `entities`.
- Entité : `id` (obligatoire, unique, non vide), `name`, `transform`, `mesh`, `material`, `collider`, `rigid_body`, `camera`, `light`.
- `transform` : `position`, `rotation_deg`, `scale`, chacun facultatif. `rotation_deg` donne les angles en degrés autour de X, Y et Z, appliqués dans l'ordre Y, X, Z : lacet, tangage, roulis (section 3.3 : degrés dans les fichiers, radians en interne).
- `mesh` : chemin d'asset d'un modèle. `material` : `base_color` (trois nombres), `metallic`, `roughness` (dans [0 ; 1]) ; demande un `mesh`.
- `collider` : `shape` (`box`, `sphere`, `capsule`), avec `half_extents` pour la boîte, `radius` pour la sphère, `radius` et `half_height` pour la capsule. `rigid_body` : `type` (`static`, `dynamic`), `mass` en kilogrammes pour un corps dynamique ; demande un `collider`.
- `camera` : `fov_deg` (vertical), `near`, `far`. `light` : `direction` (le sens dans lequel la lumière voyage), `color`, `intensity`.
- Une erreur nomme le fichier, l'entité et le champ (section 5.6).

Points de réalisation retenus avec ces choix :

- La lecture se fait en deux temps : le texte devient une `SceneDescription` (données pures, entièrement validées), puis celle-ci peuple le registre. Un fichier invalide ne crée donc aucune entité.
- `Serialization` sait aussi écrire une scène, pour le test d'aller-retour demandé par la section 7.4. La démonstration ne s'en sert pas.
- L'extraction (G-07) passe de `Game` à `World`. L'interpolation s'appuie sur un composant `PreviousTransform`, mis à jour au début de chaque pas fixe.
- Une seule lumière directionnelle est dessinée (section 5.5) : la première trouvée. La caméra de vue est choisie par `Game`.
- La scène à charger est `Scenes/demo.json` ; l'option `--scene <chemin>` en désigne une autre, relative au dossier des assets.

## Alternatives écartées

- **Un fichier GLB par variante de matériau :** une quinzaine de fichiers pour deux formes.
- **Champs inconnus ignorés :** une faute de frappe (`postion`) laisserait l'objet à l'origine sans rien dire.
- **Configuration cherchée depuis le dossier de lancement :** même défaut que pour `Assets/` (décision 0014).
- **Format extensible par des lecteurs de composants fournis par `Game` :** un mécanisme d'enregistrement de plus, pour deux composants aujourd'hui.
- **`Serialization` dépendant d'`Input` :** un écart à la section 4.2 pour éviter dix lignes de conversion.
- **`Collider` et `RigidBody` en phase 6 :** le format changerait après que `demo.json` a été écrit.

## Conséquences

- Modules touchés : `World` et `Serialization` (nouveaux), `Resources` (chemin d'un modèle, dossier racine), `Game`.
- Garde-fous : G-01, G-05 (transformations par pas fixes, interpolées à l'extraction), G-06, G-07 (extraction dans `World`), G-10.
- Retour en arrière : le format porte un champ `version` ; un changement incompatible passera par une version 2.
