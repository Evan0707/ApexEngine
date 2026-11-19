# Resources

**Rôle :** données CPU des assets, indépendantes du format de fichier (garde-fou G-04). À terme : cache, handles avec état, import glTF et textures, assets de secours (cahier des charges, section 5.5).

**État (phase 3) :** structures `MeshData` et `MaterialData`, validation d'un mesh, primitives cube et sphère (décision 0011). Le cache, les handles, l'import et les assets de secours arrivent en phase 4.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Resources/MeshData.h` | `Vertex`, `MeshData`, `triangleCount()`, `validate()` |
| `Resources/MaterialData.h` | `MaterialData` : couleur de base, métal, rugosité |
| `Resources/Primitives.h` | `makeCube(size)`, `makeSphere(radius, segments, rings)` |

## Données

- Format de sommet unique (section 5.4) : position, normale, tangente, UV, 48 octets. La tangente suit glTF : `xyz` dans le sens des `u` croissants, `w` vaut +1 ou −1, et `cross(normale, tangente) * w` pointe vers le haut de la texture. L'origine des UV est le coin haut gauche (glTF).
- `MeshData` : liste de triangles indexés sur 32 bits, faces avant dans le sens antihoraire (section 3.3).
- `validate()` vérifie ce que `Graphics` suppose : au moins un triangle, indices dans les bornes, positions finies, normales et tangentes unitaires. L'erreur nomme le mesh et l'élément fautif.
- `MaterialData` : modèle PBR metallic-roughness de glTF, simplifié (section 5.5), couleur de base en linéaire. Les valeurs par défaut forment le matériau par défaut du moteur : gris clair, non métallique, rugosité 0,5. Les textures arrivent en phase 4.

## Primitives

- `makeCube(size)` : cube centré sur l'origine, `size` mètres de côté, 24 sommets (normales par face), 12 triangles. Chaque face couvre toute la texture, haut de la texture vers le haut de la face (vers −Z pour la face du dessus).
- `makeSphere(radius, segments, rings)` : sphère UV centrée sur l'origine, pôles sur l'axe Y. `u` fait le tour depuis +Z vers +X, `v` va du pôle nord (0) au pôle sud (1). Les triangles dégénérés aux pôles sont omis.

## Dépendances

`Core` (et GLM par `Core/Math.h`). Ni Vulkan ni EnTT (section 4.2).
