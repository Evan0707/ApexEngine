# Resources

**Rôle :** données CPU des assets, indépendantes du format de fichier (garde-fou G-04). À terme : cache, handles avec état, import glTF et textures, assets de secours (cahier des charges, section 5.5).

**État (phase 4, en cours) :** structures `MeshData` et `MaterialData`, validation d'un mesh, primitives cube et sphère (décision 0011), chemins d'assets normalisés, handles avec état (décision 0014). L'import, le cache et les assets de secours arrivent dans les tâches suivantes.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Resources/MeshData.h` | `Vertex`, `MeshData`, `triangleCount()`, `validate()` |
| `Resources/MaterialData.h` | `MaterialData` : couleur de base, métal, rugosité |
| `Resources/Primitives.h` | `makeCube(size)`, `makeSphere(radius, segments, rings)` |
| `Resources/AssetPath.h` | `AssetPath` : `parse()`, `resolveSibling()`, `str()` |
| `Resources/Handles.h` | `Handle`, `MeshHandle`, `TextureHandle`, `MaterialHandle`, `ModelHandle`, `ResourceState` |
| `Resources/ResourcePool.h` | `ResourcePool` : ressources d'un type, avec leur état, désignées par handles |

## Données

- Format de sommet unique (section 5.4) : position, normale, tangente, UV, 48 octets. La tangente suit glTF : `xyz` dans le sens des `u` croissants, `w` vaut +1 ou −1, et `cross(normale, tangente) * w` pointe vers le haut de la texture. L'origine des UV est le coin haut gauche (glTF).
- `MeshData` : liste de triangles indexés sur 32 bits, faces avant dans le sens antihoraire (section 3.3).
- `validate()` vérifie ce que `Graphics` suppose : au moins un triangle, indices dans les bornes, positions finies, normales et tangentes unitaires. L'erreur nomme le mesh et l'élément fautif.
- `MaterialData` : modèle PBR metallic-roughness de glTF, simplifié (section 5.5), couleur de base en linéaire. Les valeurs par défaut forment le matériau par défaut du moteur : gris clair, non métallique, rugosité 0,5. Les textures arrivent en phase 4.

## Chemins d'assets

- Relatifs au dossier `Assets/`, séparateur `/` (section 3.3). `AssetPath::parse()` accepte `\`, retire `.` et les segments vides, résout `..`, et refuse un chemin vide, absolu (`/…`, `C:…`) ou qui sort d'`Assets/`. L'erreur cite le chemin.
- Le chemin normalisé est la clé du cache : deux écritures d'un même fichier donnent le même `AssetPath`. La casse est gardée telle quelle : `Models/A.glb` et `models/a.glb` sont deux clés.
- `resolveSibling()` résout un chemin écrit dans un asset (la texture d'un modèle) par rapport au dossier de cet asset.

## Handles et états (G-03)

- Un handle typé (`MeshHandle`, `TextureHandle`, `MaterialHandle`, `ModelHandle`) est un index et une génération. Décharger la ressource rend le handle périmé, même quand l'emplacement est réutilisé. Le handle par défaut n'est jamais valide.
- États (section 5.5) : `Loading`, `Ready`, `Failed`. Un handle périmé se lit comme `Failed` : son utilisateur reçoit l'asset de secours.
- `ResourcePool` range les ressources d'un type. `reserve()` crée une ressource en chargement, `setReady()` et `setFailed()` la terminent, `remove()` et `clear()` la déchargent. `find()` ne renvoie une valeur que pour une ressource prête : un pointeur obtenu ne doit pas être gardé d'une frame à l'autre.
- `revision()` change à chaque modification : un lecteur (le renderer) peut ignorer un pool qui n'a pas changé. `forEach()` parcourt les ressources vivantes avec leur état.
- Pas de verrou : le chargement est synchrone en phase 4.

## Primitives

- `makeCube(size)` : cube centré sur l'origine, `size` mètres de côté, 24 sommets (normales par face), 12 triangles. Chaque face couvre toute la texture, haut de la texture vers le haut de la face (vers −Z pour la face du dessus).
- `makeSphere(radius, segments, rings)` : sphère UV centrée sur l'origine, pôles sur l'axe Y. `u` fait le tour depuis +Z vers +X, `v` va du pôle nord (0) au pôle sud (1). Les triangles dégénérés aux pôles sont omis.

## Dépendances

`Core` (et GLM par `Core/Math.h`). Ni Vulkan ni EnTT (section 4.2).
