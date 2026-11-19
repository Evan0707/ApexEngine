# Licences des assets

Tout fichier ajouté à `Assets/` figure ici, avec son origine et sa licence (cahier des charges, section 6.1).

| Fichier | Origine | Licence |
|---|---|---|
| `Models/reference_cube.glb` | Créé pour le projet par `Tools/generate_assets.py` | Celle du projet |
| `Models/crate.gltf`, `Models/crate.bin`, `Models/crate_color.jpg`, `Models/crate_metal_rough.png` | Créés pour le projet par `Tools/generate_assets.py` | Celle du projet |
| `Models/cube.glb`, `Models/sphere.glb` | Créés pour le projet par `Tools/generate_assets.py` | Celle du projet |
| `Scenes/demo.json` | Écrit pour le projet | Celle du projet |
| `Models/water_bottle.glb` | « Water Bottle », glTF Sample Assets de Khronos (`Models/WaterBottle/glTF-Binary/WaterBottle.glb`, branche `main`, récupéré le 19 novembre 2026), © 2017 Microsoft | CC0 1.0 Universal (https://creativecommons.org/publicdomain/zero/1.0/legalcode) |

Notes :

- Les fichiers générés sont reproductibles : relancer `Tools/generate_assets.py` donne les mêmes octets, avec la version de Pillow de `Tools/requirements.txt`.
- `water_bottle.glb` n'a pas été modifié (SHA-256 `b337e526fd6a162013c2984aeec163f5fbb4f717252724dfc3f3458bd51df94b`). Le modèle utilise aussi une normal map, une texture d'occlusion et une texture émissive, que le moteur ignore (section 5.5).
- Les fichiers de `Tests/Data/` sont générés par le même script, pour les tests automatisés uniquement.
