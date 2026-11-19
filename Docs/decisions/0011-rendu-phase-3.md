# 0011 — Rendu de la phase 3 : données, descripteurs et caméra

**Date :** 19 novembre 2026
**Statut :** acceptée (plan de la phase 3 validé par le développeur le 19 novembre 2026)

## Contexte

La phase 3 dessine des meshes éclairés vus par une caméra libre (cahier des charges, sections 5.3 à 5.5). Plusieurs points ne sont pas tranchés par le cahier des charges, ou le sont pour une phase ultérieure : où vivent les données CPU des meshes avant l'import glTF, comment les matériaux arrivent aux shaders avant les textures, comment régler le champ de vision, et à quel rythme la caméra avance.

## Décision

1. **Module `Resources` dès la phase 3.** Il naît avec les seules structures CPU (`MeshData`, `MaterialData`) et les primitives (cube, sphère). Le cache, les handles avec état, l'import glTF et les assets de secours arrivent en phase 4. `Graphics` ne connaît que ces structures (G-04).
2. **Table des matériaux.** Les paramètres de matériau (couleur de base, métal, rugosité) sont dans un tableau du jeu de descripteurs par frame, lu par l'index de matériau des données de chaque objet (G-02, G-08). Le jeu de descripteurs par matériau de la section 5.4 arrive en phase 4 avec les textures. La liste d'objets reste triée par matériau.
3. **Champ de vision.** Réglé par un curseur du panneau de debug, de 50° à 100° (section 5.3). Le panneau reçoit des réglages de l'application ; le même mécanisme servira à l'exposition et aux bascules de la phase 7. Aucune touche n'est ajoutée à la table de la section 5.2.
4. **Caméra par pas fixes.** Position et orientation de la caméra libre avancent dans la simulation, à pas fixes, et sont interpolées au rendu (section 5.1). L'orientation suit la souris avec au plus un pas de retard (16,7 ms). La caméra troisième personne de la phase 6 suivra la même règle.

Points de réalisation retenus avec ces choix :

- `Graphics` reçoit une `RenderScene` (G-01) remplie par `Game` en phase 3, puis par `World` en phase 5. Les matrices des objets y sont relatives à la caméra (G-07) ; la projection est construite par `Graphics`, à partir du champ de vision et de la taille réelle de la cible.
- Deux passes : la scène (couleur et profondeur), puis l'interface (couleur seule). Le backend ImGui ne change pas, et l'ordre est celui qu'imposera le tonemapping de la phase 7.
- Profondeur : `D32_SFLOAT`, test « inférieur », sans Z inversé.
- Meshes : buffers en mémoire GPU, remplis par un buffer de transit au chargement.
- Le triangle de la phase 2 est retiré quand les cubes le remplacent. Le tag `phase-2` le conserve.

## Alternatives écartées

- **`MeshData` dans `Graphics`, déplacé en phase 4 :** un déplacement de plus, et `Graphics` posséderait un temps des structures qui appartiennent aux importeurs.
- **Un jeu de descripteurs par matériau dès la phase 3 :** sans texture, il ne contiendrait que quatre nombres ; l'index de matériau des données par objet servirait quand même.
- **Touches pour le champ de vision :** elles sortiraient de la table de la section 5.2.
- **Orientation de la caméra à chaque frame :** moins de latence, mais deux règles de mise à jour différentes pour une même caméra, et le personnage de la phase 6 avancera de toute façon par pas fixes.

## Conséquences

- Modules touchés : `Resources` (nouveau), `Graphics`, `Debug`, `Game`.
- Garde-fous : G-01 (`RenderScene`), G-02 (buffer par frame lu par `gl_InstanceIndex`), G-04 (structures CPU), G-05 (caméra par pas fixes), G-07 (matrices relatives à la caméra), G-08 (fichier d'inclusion commun des matériaux), G-09 (formats passés aux pipelines).
- Retour en arrière : le passage à un jeu par matériau (point 2) est prévu en phase 4 et reste local à `Graphics`.
