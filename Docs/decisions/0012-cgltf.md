# 0012 — Ajout de cgltf (import glTF 2.0)

**Date :** 19 novembre 2026
**Statut :** acceptée (plan de la phase 4 validé par le développeur le 19 novembre 2026)

## Contexte

La phase 4 importe des modèles glTF 2.0 et GLB (cahier des charges, section 5.5). Le cahier des charges retient cgltf, dans `Resources` (section 3.2), et l'importeur doit produire des structures CPU indépendantes du format (G-04).

## Décision

- Ajouter `cgltf` à `vcpkg.json` : version 1.15 dans la baseline, à confirmer à l'installation. Bibliothèque en un seul en-tête, sans code compilé à part.
- `Resources/GltfImporter.cpp` est le seul fichier qui inclut `cgltf.h`, et le seul à définir `CGLTF_IMPLEMENTATION`. Il produit un `ModelData` (meshes, matériaux, images) ; aucun type cgltf ne sort de ce fichier.
- Le fichier est lu par cgltf (`cgltf_parse_file`, `cgltf_load_buffers`), puis validé (`cgltf_validate`). Toute erreur devient une `core::Error` qui nomme le fichier et l'élément fautif.
- Pris en charge : maillages triangulaires indexés ou non, attributs `POSITION`, `NORMAL`, `TANGENT`, `TEXCOORD_0`, matériau metallic-roughness (facteurs et textures), images intégrées au GLB ou externes en PNG et JPG. Une extension marquée obligatoire (`extensionsRequired`) refuse le fichier ; les autres sont ignorées.
- Ignorés : animations, peaux, caméras, lumières, morph targets, émissif, occlusion et transparence (section 5.5), normal maps avant P1.

## Alternatives écartées

- **Assimp :** des dizaines de formats dont le prototype n'a pas besoin, une compilation longue, et des structures intermédiaires qui ne correspondent pas exactement à glTF. Le cahier des charges l'a déjà écarté (section 0.2).
- **tinygltf :** dépend de nlohmann/json et de stb_image, et lit le JSON en exceptions ; cgltf est plus petit et sans allocation cachée.

## Conséquences

- Module touché : `Resources` uniquement (confinement de la section 4.2).
- Garde-fous : G-04 (structures CPU), G-10 (confinement).
- Licence : MIT, ajoutée à `ThirdPartyLicenses.md`.
- Retour en arrière : local à `GltfImporter.cpp`.
