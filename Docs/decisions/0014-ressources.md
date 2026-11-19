# 0014 — Ressources : handles, modèles et assets de la démonstration

**Date :** 19 novembre 2026
**Statut :** acceptée (plan de la phase 4 validé par le développeur le 19 novembre 2026)

## Contexte

La phase 4 ajoute le cache de ressources, les handles avec état et les assets de secours (cahier des charges, section 5.5, garde-fous G-03 et G-04). Plusieurs points ne sont pas tranchés par le cahier des charges : quel module définit les handles, comment un fichier glTF devient un objet dessinable, d'où viennent les assets de la démonstration et où l'exécutable les cherche.

## Décision

1. **Un seul système de handles.** Les handles typés de `Resources` (`MeshHandle`, `TextureHandle`, `MaterialHandle`, `ModelHandle`) désignent les assets partout, `RenderScene` comprise. `Graphics` lit le cache de ressources une fois par frame (`Renderer::syncResources`), envoie au GPU ce qui est devenu prêt, libère ce qui a été déchargé, et résout chaque handle vers son objet GPU ; un handle en chargement, en échec ou périmé donne l'asset de secours. Les fonctions `Renderer::createMesh` et `createMaterial` de la phase 3 disparaissent, ainsi que les handles GPU de `Graphics`.
2. **Transformations des nœuds appliquées aux sommets.** À l'import, la hiérarchie glTF est aplatie (section 5.5) : chaque primitive est transformée par la matrice monde de son nœud, puis les primitives qui partagent un matériau sont fusionnées. Un modèle est une liste de parties (un mesh et un matériau chacune), dessinée avec une seule matrice d'objet. Le composant `MeshRenderer` de la phase 5 désignera un modèle.
3. **Assets de la démonstration générés par script.** `Tools/generate_assets.py` écrit le cube de référence (1 m, faces marquées), une caisse texturée et les fichiers des tests. Les fichiers produits sont versionnés ; le build ne lance jamais le script. Pillow, nécessaire au script, est installé dans un environnement virtuel propre au dépôt (`Tools/.venv`, ignoré par Git) ; ce n'est une dépendance ni du moteur ni du build. Un modèle d'exemple de Khronos, sous licence ouverte, sert en plus de fichier venu d'un vrai outil (risque « assets incompatibles », section 11). Toutes les licences sont dans `Assets/LICENSES.md`.
4. **Dossier `Assets/` copié à côté de l'exécutable.** Le build le copie dans `build/<preset>/bin/Assets`, et l'exécutable le cherche à côté de lui ; l'option `--assets <dossier>` en désigne un autre. Le dossier de l'exécutable est fourni par `Platform`, seul module autorisé à appeler le système (G-10).

Points de réalisation retenus avec ces choix :

- Chargement synchrone en phase 4 : l'état « en chargement » existe, mais ne dure pas. Le chargement asynchrone (section 4.5) restera local à `Resources`.
- Textures en RGBA 8 bits : la couleur de base en sRGB, la texture métal-rugosité en linéaire (section 3.3). Un seul sampler, linéaire et répétant ; les samplers du fichier sont ignorés. Pas de mipmaps avant P1.
- Un jeu de descripteurs par matériau (section 5.4) : ses deux textures. Les facteurs restent dans la table par frame (note 0011).
- Assets de secours intégrés (section 5.5) : texture en damier magenta pour une texture en échec, texture blanche pour une texture absente du matériau, cube de 1 m pour un modèle en échec, matériau par défaut.

## Alternatives écartées

- **Deux systèmes de handles reliés par le jeu :** moins de changements dans le code de la phase 3, mais le chargement asynchrone obligerait `Game` puis `World` à suivre les ressources devenues prêtes : le chantier ne resterait pas limité à `Resources` (section 4.5).
- **Une transformation par primitive :** plus fidèle à la hiérarchie, mais un objet dessiné par primitive, et une hiérarchie que le cahier des charges exclut (section 2.2).
- **Assets téléchargés uniquement :** dépendance aux licences de tiers pour chaque fichier, et pas de cube aux faces marquées pour AC-05.
- **Assets cherchés depuis le dossier de lancement :** l'exécutable ne trouverait rien lancé depuis un autre dossier.

## Conséquences

- Modules touchés : `Resources`, `Graphics` (interface publique : `RenderScene`, `syncResources`), `Platform` (dossier de l'exécutable), `Game`.
- Garde-fous : G-03 (handles avec état, asset de secours, aucun pointeur gardé d'une frame à l'autre), G-04, G-10.
- Retour en arrière : le point 1 change l'interface de `Graphics` ; revenir à deux systèmes reviendrait à refaire la tâche correspondante.
