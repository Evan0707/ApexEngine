# 0018 — Ajout de Jolt Physics

**Date :** 20 novembre 2026
**Statut :** acceptée (plan de la phase 6 validé par le développeur le 20 novembre 2026)

## Contexte

La phase 6 introduit le module `Physics` : corps rigides, colliders, requêtes et personnage (cahier des charges, sections 5.7 et 5.8). Le cahier des charges retient Jolt Physics 5.x, dans `Physics` (section 3.2), et impose `CharacterVirtual` pour le personnage (section 5.8).

## Décision

- Ajouter `joltphysics` à `vcpkg.json` : version 5.6.0 dans la baseline, à confirmer à l'installation. Aucune fonctionnalité optionnelle du port (`debugrenderer`, `profiler`, `rtti`) n'est demandée.
- La bibliothèque est liée par sa cible CMake (`Jolt::Jolt`), en privé : ses options de compilation et ses définitions viennent de la cible, jamais d'une copie à la main. Jolt vérifie à l'exécution que ces définitions sont les mêmes des deux côtés.
- Seuls les `.cpp` de `Physics` et un en-tête interne du module incluent Jolt. Aucun en-tête public n'expose de type Jolt : le module rend des structures C++ et des handles opaques ; les conversions entre GLM et Jolt y sont internes (section 5.7).
- Jolt en simple précision, comme `WorldPosition` (G-07). Le passage en double précision reste l'optimisation reportée de la section 4.5.
- Un seul thread : Jolt reçoit `JobSystemSingleThreaded`. Le multithreading est une optimisation reportée (section 4.5), et un seul thread rend les tests reproductibles.
- État global de la bibliothèque : voir la décision 0019, point 1.

## Alternatives écartées

- **PhysX, Bullet :** le cahier des charges a tranché (section 3.2) ; Jolt a le même repère que le moteur (section 3.3) et fournit le contrôleur de personnage demandé.
- **Physique maison :** hors de proportion, et la section 5.8 interdit de réécrire un contrôleur de personnage.
- **`JobSystemThreadPool` :** plus rapide sur de grandes scènes, sans intérêt pour quelques dizaines de corps, et moins simple à tester.

## Conséquences

- Module touché : `Physics` (nouveau), seul à inclure Jolt (section 4.2).
- Garde-fous : G-05 (pas fixe), G-06 (voir 0019), G-10 (confinement).
- Licence : MIT, ajoutée à `ThirdPartyLicenses.md`.
- La première compilation de Jolt allonge la configuration de quelques minutes, en local et en CI ; le cache binaire de vcpkg l'évite ensuite.
- Retour en arrière : local à `Physics`, puisque aucun type Jolt n'en sort.
