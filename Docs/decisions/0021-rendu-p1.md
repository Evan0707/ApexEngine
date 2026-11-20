# 0021 — Rendu P1 : ombres, HDR, culling, mipmaps, normal maps

**Date :** 20 novembre 2026
**Statut :** acceptée par délégation (plan de la phase 7 validé par le développeur le 20 novembre 2026, points ouverts confiés à l'agent ; à relire par le développeur)

## Contexte

La phase 7 livre les fonctions P1 du rendu (cahier des charges, sections 2.1 et 5.4) : ombres directionnelles, HDR et tonemapping, frustum culling, mipmaps, normal maps. L'IBL fait l'objet de la note 0022, les outils de debug de la note 0023.

Deux règles du cahier des charges se contredisent. La section 2.1 n'engage le P1 que lorsque tous les critères P0 sont validés, alors que la section 9.1 place la phase 7 avant la phase 8, où se valident AC-14 à AC-16. Le plafond de 40 heures de la section 9.3 ne peut pas être appliqué : `Docs/suivi_heures.md` est vide.

## Décision

1. **Ordre des phases.** La phase 7 suit la phase 6, comme le prévoit la section 9.1. Chaque fonction P1 se désactive, au lancement par `Config/engine.json` et pendant l'exécution par le panneau ou une touche : la phase 8 pourra mesurer et valider le P0 avec ou sans elles. Le parcours 6.2 joué à l'écran par le développeur reste à faire (fin de la phase 6).
2. **Périmètre complet, blocs indépendants.** S'il faut couper, les mipmaps puis les normal maps partent en premier : aucun critère d'acceptation ne les vérifie.
3. **Passes d'une frame.** Ombre (profondeur seule, dans la shadow map) ; scène puis ciel, dans une image RGBA 16 bits flottante ; tonemapping vers la swapchain ; lignes des colliders (note 0023) ; interface ImGui. L'interface reste dessinée en dernier et le backend ImGui ne change pas.
4. **Aucun push constant.** Exposition, matrice de la lumière, réglages des ombres et bascules sont dans `FrameUniforms` (G-02).
5. **Cibles paramétrées (G-09).** Le format de l'image HDR et la résolution de la shadow map sont choisis en un seul endroit et passés à la création des cibles et des pipelines. La résolution vient de la configuration (`shadows.map_size`, 2048 par défaut) et ne change qu'au lancement.
6. **Cadrage des ombres.** Une projection orthographique couvre une zone carrée fixe, de 40 m de côté par défaut, centrée sur le personnage (section 5.4). `World` calcule ce cadrage dans l'extraction : il centre la zone sur le personnage, cale ce centre sur la grille des texels en coordonnées monde, pour que l'ombre ne scintille pas quand le personnage avance, puis rend la matrice vue-projection de la lumière relative à la caméra (G-07). Sans personnage, la zone est centrée sous la caméra. `Graphics` reçoit cette matrice comme une donnée.
7. **Filtrage et biais.** PCF 3 × 3 par un sampler de comparaison. Biais constant et biais de pente appliqués par le rasterizer (état dynamique), réglables dans la configuration et dans le panneau. La shadow map est en profondeur 16 bits fixes (`D16_UNORM`) : une unité de biais constant y vaut 1/65536 de la profondeur de la zone, ce qui permet de régler ce biais en mètres (avec une profondeur flottante, l'unité dépend de la profondeur elle-même). Sur 100 m de profondeur, la précision est de 1,5 mm. Valeurs par défaut, choisies sur la scène de démonstration : 2 cm et 2. Les ombres s'estompent sur le dernier dixième de la zone, au lieu de s'arrêter net.
8. **Tonemapping.** Approximation ACES de Narkowicz, appliquée après l'exposition, réglable de −4 à +4 stops (section 5.4). Désactivé, l'exposition reste appliquée et la courbe est remplacée par une simple saturation à 1 : la comparaison du panneau isole la courbe (AC-P1-5).
9. **Frustum culling.** Une sphère englobante par mesh, calculée à l'import dans `Resources` (G-04), transformée par la matrice de chaque objet. La passe principale élimine ce qui sort du champ de la caméra, la passe d'ombre ce qui sort du volume de la lumière. Le compteur « objets dessinés » est celui de la passe principale (AC-P1-4) ; la passe d'ombre a son propre compteur.
10. **Mipmaps et anisotropie.** Mipmaps générés à l'envoi de chaque texture, par blits successifs sur le GPU. Filtrage anisotrope 8× quand le GPU le propose ; sans lui, filtrage trilinéaire. Les samplers des fichiers glTF restent ignorés (décision 0014).
11. **Normal maps.** La texture `normalTexture` de glTF et son facteur `scale` sont lus. Une texture absente, en chargement ou en échec donne une normale plate, et non le damier : un damier lu comme normales tordrait l'éclairage au lieu de signaler l'erreur. L'erreur reste écrite dans le log (G-03).

## Alternatives écartées

- **Phase 8 avant la phase 7 :** contraire à l'ordre de la section 9.1, et la phase 8 mesure le rendu final.
- **Zone d'ombre centrée sur la caméra :** la section 5.4 demande le joueur ; en caméra libre loin du personnage, la scène sous la caméra perdrait ses ombres, ce qui est accepté.
- **Calage sur les texels dans `Graphics` :** `Graphics` ne connaît que des positions relatives à la caméra (G-07), qui bougent avec elle ; l'ombre scintillerait.
- **Push constants pour l'exposition et la lumière :** contraire au choix de la note 0011 de ne pas en utiliser ; un uniform par frame suffit.
- **Mipmaps calculés sur le CPU :** plus lent, plus de code, et rien de plus à tester qu'un nombre de niveaux.
- **Damier pour une normal map manquante :** voir le point 11.

## Conséquences

- Modules touchés : `Graphics` (cibles, pipelines, shaders, culling, mipmaps), `World` (cadrage des ombres), `Resources` (sphère englobante, normal map), `Serialization` (sections `shadows` et `rendering` de la configuration), `Debug` (réglages), `Game` (bascules).
- Garde-fous : G-01 (le cadrage et les réglages arrivent par la `RenderScene`), G-02 (aucun push constant), G-07 (matrice de la lumière relative à la caméra), G-09 (formats et résolution paramétrés).
- Mémoire GPU : une image HDR à la taille de la fenêtre (environ 16 Mio en 1920 × 1080) et une shadow map (8 Mio en 2048 × 2048, profondeur 16 bits). Les mipmaps ajoutent un tiers à chaque texture.
- Retour en arrière : chaque fonction se désactive ; la retirer est local à `Graphics`, sauf le cadrage (`World`) et la normal map (`Resources`).
