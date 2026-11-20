# Tools

Scripts Python du projet. Ils ne font pas partie du build : leurs sorties sont versionnées.

## `generate_assets.py`

Écrit les assets de la démonstration et les fichiers des tests de l'importeur (note de décision 0014) :

- `Assets/Models/reference_cube.glb` : cube de référence de 1 m (section 6.1), chaque face marquée de son axe (`+X`, `-X`…) avec une barre blanche le long du bord haut de la texture. Rouge pour X, vert pour Y, bleu pour Z ; couleur sombre pour les faces négatives.
- `Assets/Models/crate.gltf` : caisse en bois de 1 m aux coins métalliques, en glTF avec fichiers externes (`crate.bin`, couleur en JPG, métal-rugosité en PNG). La caisse est centrée sur son origine, comme son collider (décision 0019). Le mesh fait 0,5 m et son nœud le double, ce que l'import applique aux sommets.
- `Assets/Models/cube.glb` et `sphere.glb` : cube de 1 m et sphère de 1 m de diamètre, blancs et sans texture. Les scènes leur donnent une taille par `transform.scale` et une couleur par `material` (décision 0017).
- `Assets/Models/character.glb` : le personnage de la démonstration (section 5.8), une capsule de 1,8 m de haut et 0,3 m de rayon, l'origine aux pieds, avec une visière sombre sur l'avant (−Z) qui montre où il regarde.
- `Tests/Data/Gltf/` et `Tests/Data/Images/` : petits fichiers aux valeurs connues pour les tests automatisés.

Lancement, depuis la racine du dépôt :

```
py -m venv Tools/.venv
Tools/.venv/Scripts/python -m pip install -r Tools/requirements.txt
Tools/.venv/Scripts/python Tools/generate_assets.py
```

`Assets/Scenes/demo.json` et `Tests/Data/Scenes/` ne sont pas générés : ils sont écrits à la main.

L'environnement `Tools/.venv` est ignoré par Git. Avec la version de Pillow de `requirements.txt`, le script réécrit les mêmes octets : `git status` ne doit rien montrer s'il n'a pas changé.
