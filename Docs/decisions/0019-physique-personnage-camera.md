# 0019 — Physique, personnage et caméra troisième personne

**Date :** 20 novembre 2026
**Statut :** acceptée (plan de la phase 6 validé par le développeur le 20 novembre 2026)

## Contexte

La phase 6 rend le parcours de validation jouable (cahier des charges, section 6.2) : corps rigides, personnage, caméra troisième personne (sections 5.3, 5.7 et 5.8). Le cahier des charges fixe l'interface de `Physics` et le flux de données (section 4.3), mais plusieurs règles restent à choisir.

## Décision

1. **État global de Jolt.** Jolt impose un état global : allocateur, fabrique de types, table des types. Il est créé et détruit par un objet `physics::Runtime`, possédé par l'application et passé à `PhysicsWorld::create`. Le moteur n'ajoute aucune variable statique : un second `Runtime` simultané est une erreur de programmation, détectée par une assertion sur l'état de Jolt lui-même. Les fonctions de trace et d'assertion de Jolt gardent leur valeur par défaut, parce qu'un pointeur de fonction global ne peut pas recevoir le logger sans variable statique.
2. **Personnage dans `Physics`.** La section 5.7 ne liste pas le personnage dans l'interface, mais la section 5.8 impose `CharacterVirtual` et aucun type Jolt ne sort du module. `Physics` expose donc : créer et détruire un personnage, lui donner une vitesse voulue, lire sa position et son état au sol. Jolt résout les collisions, les marches, la pente et la poussée des corps dynamiques ; rien n'est réécrit.
3. **Composants ajoutés à `World`.** En plus de la liste de la section 5.6 : `PhysicsBody` (handle du corps créé pour l'entité), `CharacterBody` (dimensions et limites du personnage), `CharacterMotion` (vitesse voulue : l'intention de la section 4.3) et `CharacterGround` (état au sol relu après le pas). Ce sont des données du moteur ; les règles du personnage (vitesses, saut, orientation) restent dans `Game`.
4. **Déroulement d'un pas fixe.** `rememberTransforms`, systèmes de `Game` (qui écrivent `CharacterMotion`), `world::stepPhysics` (pousse les intentions, avance `Physics`, recopie les transformations), puis caméra troisième personne, qui suit ainsi la position du pas courant.
5. **Collider centré sur l'origine de l'entité.** Le format de scène ne reçoit pas de champ de décalage. Un modèle dont l'origine n'est pas au centre de sa forme est corrigé à la source : `crate.gltf` est régénéré centré, comme dans l'exemple de la section 5.6.
6. **Échelle.** `Physics` ne connaît pas l'échelle : `World` multiplie les dimensions du collider par l'échelle de l'entité à la création du corps, une fois. Une sphère demande une échelle uniforme, une capsule la même échelle sur X et Z ; sinon la création échoue avec une erreur qui nomme l'entité.
7. **Personnage reconnu par son identifiant.** L'entité `player` de la scène reçoit de `Game` ses composants, comme `camera` et `spinner` (décision 0017). Son origine est à ses pieds. Elle n'a ni `collider` ni `rigid_body` : le personnage n'est pas un corps rigide. Le format de scène ne change pas.
8. **Configuration.** `Config/engine.json` reçoit trois sections facultatives : `physics` (gravité), `character` (valeurs de la section 5.8, force de poussée) et `third_person_camera` (distance, hauteur de visée, section 5.3).
9. **Caméras.** La démo démarre en caméra libre (section 6.2). F3 bascule ; chaque caméra reprend l'orientation de l'autre. En caméra libre, le personnage ne reçoit pas d'entrées mais reste simulé.
10. **Caméra troisième personne.** Balayage d'une sphère de 0,2 m de rayon entre la cible et la position voulue, contre les couches statique et dynamique. La caméra se rapproche d'un coup quand un obstacle s'interpose et s'éloigne progressivement ; la cible est lissée. Le lissage avance par pas fixes (G-05, décision 0011).
11. **Couches.** Statique : touche dynamique et personnage. Dynamique : touche tout. Personnage : touche statique et dynamique. Les requêtes reçoivent un masque de couches.
12. **Scène.** Le terrain du parcours est ajouté dès maintenant : sol de 100 m, deux rampes, trois plateformes dont une à 1 m, mur de 10 cm, cinq caisses dont une pile de trois. Les 10 à 20 objets statiques texturés de la section 6.1 restent pour la phase 8.
13. **Affichage des colliders (F2).** Il reste en phase 7 : c'est du P1 (section 2.1).

## Alternatives écartées

- **Compteur de références statique pour l'état de Jolt :** une variable statique modifiable dans le moteur, contraire à G-06.
- **`Game` qui appelle `Physics` pour déplacer le personnage :** contredit le flux de la section 4.3, où `World` pousse les intentions.
- **Champ `offset` dans `collider` :** un changement de format pour un seul modèle généré par le projet.
- **Échelle suivie à chaque pas :** recréer une forme par pas pour un cas que la démo n'a pas.
- **Champ `character` dans la scène :** `Serialization` devrait connaître un composant dont les règles sont dans `Game`.
- **Spherecast contre la couche statique seule :** la caméra entrerait dans les caisses.

## Conséquences

- Modules touchés : `Physics` (nouveau), `World`, `Serialization` (configuration), `Debug` (nombre de corps), `Game`.
- Garde-fous : G-05 (personnage, corps et caméra par pas fixes), G-06 (point 1 : l'état global est celui de Jolt, borné par un objet possédé par l'application), G-07 (les positions échangées avec `Physics` sont des `WorldPosition`), G-10 (Jolt dans `Physics` seulement).
- Limite connue : une assertion de Jolt, en Debug, arrête le programme sans passer par le logger.
- Retour en arrière : les points 5 à 8 touchent des fichiers de données ; un champ de décalage ou un champ `character` pourront être ajoutés par une version 2 du format.
