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
| `Resources/TextureData.h` | `ColorSpace`, `TextureData` : image décodée en RGBA 8 bits |
| `Resources/ImageDecoder.h` | `decodeImage()`, `kMaxImageSize` |
| `Resources/FileReader.h` | `readFile()` |
| `Resources/FallbackAssets.h` | `makeCheckerTexture()`, `makeWhiteTexture()`, `makeFallbackMesh()`, `makeDefaultMaterial()` |
| `Resources/ModelData.h` | `ModelImage`, `ModelMaterial`, `ModelPart`, `ModelData` : ce que produit un importeur |
| `Resources/GltfImporter.h` | `importGltf()` |
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

## Textures

- `TextureData` : image décodée, indépendante du format (G-04), en RGBA 8 bits, lignes de haut en bas, avec l'espace de couleur voulu : sRGB pour une couleur, linéaire pour des données (section 3.3).
- `decodeImage(octets, nom, espace)` décode un PNG ou un JPG par stb_image (décision 0013), seul fichier du moteur qui l'inclut. Les dimensions sont lues avant le décodage : au-delà de 4096 pixels de côté, l'image est refusée sans être décodée. L'erreur nomme l'image et dit pourquoi.
- `readFile(chemin)` lit un fichier entier ; `NotFound` s'il n'existe pas, `IoFailure` s'il ne se lit pas, avec le chemin dans le message.

## Import glTF (décision 0012)

- `importGltf(fichier, nom)` lit un `.gltf` (avec ses fichiers externes) ou un `.glb` par cgltf, seul fichier du moteur qui l'inclut, et produit un `ModelData` indépendant du format (G-04). `nom` est le chemin d'asset, cité par toute erreur avec l'élément fautif.
- Refusés : fichier absent (`NotFound`), tronqué, JSON ou glTF invalide, glTF 1.0, extension marquée obligatoire, index hors des sommets, primitive sans `POSITION`, modèle sans aucun triangle.
- Hiérarchie aplatie (section 5.5, décision 0014) : chaque primitive est transformée par la matrice monde de son nœud, puis les primitives d'un même matériau sont fusionnées en une partie. Un nœud qui inverse le repère (échelle négative) inverse aussi l'ordre des sommets de ses triangles, pour garder des faces avant antihoraires.
- Lus : `POSITION`, `NORMAL`, `TANGENT`, `TEXCOORD_0` (entiers normalisés compris), indices 8, 16 ou 32 bits, ou aucun indice. Les points et les lignes sont ignorés.
- Normales absentes : moyenne des faces voisines, pondérée par leur aire. Tangentes absentes : calculées à partir des UV, avec la convention de `Vertex` ; une tangente impossible à calculer (UV dégénérés) devient une perpendiculaire quelconque à la normale.
- Scène lue : la scène par défaut, sinon la première, sinon tous les nœuds sans parent. Une hiérarchie de plus de 64 niveaux est refusée (protection contre un cycle).
- Limite : le chemin du fichier est passé à cgltf en encodage ANSI pour les `.bin` externes ; un dossier dont le nom contient des caractères hors de cet encodage ne fonctionnera pas.

## Assets de secours (section 5.5)

- `makeCheckerTexture()` : damier magenta et noir de 64 × 64 pixels, cases de 8, en sRGB, pour une texture en échec.
- `makeWhiteTexture()` : un texel blanc, pour un matériau sans texture : multiplier par 1 laisse les facteurs inchangés.
- `makeFallbackMesh()` : cube de 1 m centré sur l'origine, pour un modèle en échec ; `makeDefaultMaterial()` : le matériau par défaut.

## Primitives

- `makeCube(size)` : cube centré sur l'origine, `size` mètres de côté, 24 sommets (normales par face), 12 triangles. Chaque face couvre toute la texture, haut de la texture vers le haut de la face (vers −Z pour la face du dessus).
- `makeSphere(radius, segments, rings)` : sphère UV centrée sur l'origine, pôles sur l'axe Y. `u` fait le tour depuis +Z vers +X, `v` va du pôle nord (0) au pôle sud (1). Les triangles dégénérés aux pôles sont omis.

## Dépendances

`Core` (et GLM par `Core/Math.h`). stb_image (décision 0013), dans `ImageDecoder.cpp` uniquement ; cgltf (décision 0012), dans `GltfImporter.cpp` uniquement. Ni Vulkan ni EnTT (section 4.2).
