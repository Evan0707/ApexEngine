# 0023 — Outils de debug P1 : colliders, temps GPU, réglages

**Date :** 20 novembre 2026
**Statut :** acceptée par délégation (plan de la phase 7 validé par le développeur le 20 novembre 2026, points ouverts confiés à l'agent ; à relire par le développeur)

## Contexte

Sections 2.1, 5.2 et 5.9, P1 : affichage filaire des colliders (F2), temps GPU, bascules des ombres (F4), du tonemapping et de l'IBL, réglage de l'exposition. Critères AC-P1-2 et AC-P1-3.

## Décision

1. **Temps GPU en premier.** Il est livré en tête de phase, et non en dernier comme dans le tableau de la section 2.1 : il mesure le coût de chaque fonction qui suit.
2. **Mesure.** Deux horodatages par frame en vol, au début et à la fin de ses commandes, lus quand la fence du créneau a été attendue : la lecture ne bloque jamais. La valeur affichée est celle de la frame précédente dans ce créneau, moyennée et maximisée sur une seconde comme le temps CPU. Un GPU sans horodatages sur la file graphique affiche « n/a ».
3. **Colliders dessinés par le moteur.** `World` produit des segments, en espace de rendu, dans la `RenderScene` ; `Graphics` les dessine. Les dimensions viennent de `Physics` (la forme que Jolt simule, échelle comprise) ; la pose est celle de l'entité, interpolée comme son dessin. Couleurs : statique en gris vert, dynamique éveillé en orange, dynamique endormi en bleu, personnage en jaune.
4. **Dessin des lignes.** Après le tonemapping, avec le test de profondeur de la scène : un collider caché derrière un mur reste caché. Les sommets sont rapprochés de la caméra d'une fraction de leur distance, pour que les arêtes posées sur une surface ne clignotent pas.
5. **Bascules.** F2 (colliders) et F4 (ombres) existent déjà dans `Config/input.json` ; `Game` les applique. Les autres réglages (tonemapping, exposition, IBL, biais des ombres) passent par `DebugControls`, comme le champ de vision (décision 0011). Aucune touche n'est ajoutée à la table de la section 5.2.

## Alternatives écartées

- **Debug renderer de Jolt :** il faudrait recompiler Jolt avec une option de plus et faire sortir ses types de `Physics` (G-10).
- **Dimensions lues dans les composants de `World` :** elles ne prouvent pas que Jolt simule la même forme, ce que vérifie AC-P1-2.
- **Lignes dans l'image HDR :** leur couleur changerait avec l'exposition.
- **Temps GPU lu en attendant le résultat :** une attente de plus par frame, que la mesure elle-même fausserait.

## Conséquences

- Modules touchés : `Physics` (lecture des formes), `World` (segments), `Graphics` (horodatages, pipeline de lignes), `Debug` (panneau), `Game` (F2 et F4).
- Garde-fous : G-01 (les lignes arrivent par la `RenderScene`, copiées), G-02 (sommets dans un buffer par frame, aucun push constant), G-07 (segments convertis en espace de rendu dans l'extraction), G-10 (aucun type Jolt hors de `Physics`).
- Retour en arrière : local à chaque outil.
