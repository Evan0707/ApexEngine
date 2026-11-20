# Resources

**Rôle :** données CPU des assets, indépendantes du format de fichier (garde-fou G-04). À terme : cache, handles avec état, import glTF et textures, assets de secours (cahier des charges, section 5.5).

**État (phase 7) :** structures CPU des meshes, matériaux, textures, modèles et environnements ; import glTF et GLB (décision 0012) ; décodage PNG, JPG et Radiance HDR (décisions 0013 et 0022) ; chemins d'assets normalisés, handles avec état, cache de ressources et assets de secours (décision 0014) ; primitives cube et sphère.

## Interface publique

| En-tête | Contenu |
|---|---|
| `Resources/MeshData.h` | `Vertex`, `MeshData`, `triangleCount()`, `validate()`, `BoundingSphere`, `boundingSphere()` |
| `Resources/MaterialData.h` | `MaterialData` : couleur de base, métal, rugosité, facteur de la normal map, handles des trois textures |
| `Resources/Primitives.h` | `makeCube(size)`, `makeSphere(radius, segments, rings)` |
| `Resources/AssetPath.h` | `AssetPath` : `parse()`, `resolveSibling()`, `str()` |
| `Resources/Handles.h` | `Handle`, `MeshHandle`, `TextureHandle`, `MaterialHandle`, `ModelHandle`, `EnvironmentHandle`, `ResourceState` |
| `Resources/TextureData.h` | `ColorSpace`, `TextureData` : image décodée en RGBA 8 bits |
| `Resources/EnvironmentData.h` | `EnvironmentData` : image équirectangulaire de radiance en RGB flottant ; `environmentUv()`, `environmentDirection()` |
| `Resources/ImageDecoder.h` | `decodeImage()`, `decodeHdrImage()`, `kMaxImageSize` |
| `Resources/FileReader.h` | `readFile()` |
| `Resources/FallbackAssets.h` | `makeCheckerTexture()`, `makeWhiteTexture()`, `makeFlatNormalTexture()`, `makeFallbackMesh()`, `makeDefaultMaterial()` |
| `Resources/ModelData.h` | `ModelImage`, `ModelMaterial`, `ModelPart`, `ModelData` : ce que produit un importeur |
| `Resources/GltfImporter.h` | `importGltf()` |
| `Resources/ResourceCache.h` | `ModelPartRef`, `Model`, `ResourceCache` : `loadModel()`, `loadTexture()`, `loadEnvironment()`, `addMesh()`, `addMaterial()`, `addModel()`, `state()`, `parts()`, assets de secours, `meshes()`, `textures()`, `materials()`, `environments()`, `unloadAll()`, `filesRead()` |
| `Resources/ResourcePool.h` | `ResourcePool` : ressources d'un type, avec leur état, désignées par handles |

## Données

- Format de sommet unique (section 5.4) : position, normale, tangente, UV, 48 octets. La tangente suit glTF : `xyz` dans le sens des `u` croissants, `w` vaut +1 ou −1, et `cross(normale, tangente) * w` pointe vers le haut de la texture. L'origine des UV est le coin haut gauche (glTF).
- `MeshData` : liste de triangles indexés sur 32 bits, faces avant dans le sens antihoraire (section 3.3).
- `boundingSphere()` : une sphère qui contient tous les sommets, centrée sur leur boîte englobante (pas la plus petite possible, mais proche pour les formes de la démo). Sert au frustum culling de `Graphics` (décision 0021).
- `validate()` vérifie ce que `Graphics` suppose : au moins un triangle, indices dans les bornes, positions finies, normales et tangentes unitaires. L'erreur nomme le mesh et l'élément fautif.
- `MaterialData` : modèle PBR metallic-roughness de glTF, simplifié (section 5.5), couleur de base en linéaire, multipliée par la texture de couleur (sRGB) ; métal et rugosité multipliés par les canaux bleu et vert de la texture métal-rugosité (linéaire). Normal map (P1, décision 0021) : texture linéaire en espace tangent, convention glTF (+X à droite, +Y vers le haut de la texture, +Z hors de la surface), et son facteur `normalScale` (le `scale` de glTF), qui multiplie X et Y. Un handle de texture par défaut signifie « pas de texture » (surface plate pour la normal map). Les valeurs par défaut forment le matériau par défaut du moteur : gris clair, non métallique, rugosité 0,5, sans texture.

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
- Une image Radiance HDR n'est pas une texture : `decodeImage()` la refuse (stb_image la ramènerait sinon en 8 bits).
- `readFile(chemin)` lit un fichier entier ; `NotFound` s'il n'existe pas, `IoFailure` s'il ne se lit pas, avec le chemin dans le message.

## Environnements (section 5.4, décision 0022)

- `EnvironmentData` : la lumière qui arrive de toutes les directions, indépendante du format (G-04). Image équirectangulaire de radiance en RGB linéaire flottant, lignes du haut (+Y) vers le bas (−Y). Le long d'une ligne, la direction tourne autour de +Y ; la colonne du milieu regarde vers −Z.
- `environmentUv(direction)` et `environmentDirection(environnement, x, y)` sont les deux sens de cette convention ; les shaders utilisent la même formule. `u = 0,5 + atan2(x, −z) / 2π`, `v = acos(y) / π`.
- `decodeHdrImage(octets, nom)` décode un fichier Radiance (`.hdr`), compressé par plages ou non, par stb_image. Même limite de taille que les textures. L'erreur nomme l'image.
- Limite de stb_image : un fichier non compressé tronqué n'est pas détecté (les pixels manquants valent zéro) ; un fichier compressé tronqué est refusé. Le ciel de la démonstration est compressé.

## Import glTF (décision 0012)

- `importGltf(fichier, nom)` lit un `.gltf` (avec ses fichiers externes) ou un `.glb` par cgltf, seul fichier du moteur qui l'inclut, et produit un `ModelData` indépendant du format (G-04). `nom` est le chemin d'asset, cité par toute erreur avec l'élément fautif.
- Refusés : fichier absent (`NotFound`), tronqué, JSON ou glTF invalide, glTF 1.0, extension marquée obligatoire, index hors des sommets, primitive sans `POSITION`, modèle sans aucun triangle.
- Hiérarchie aplatie (section 5.5, décision 0014) : chaque primitive est transformée par la matrice monde de son nœud, puis les primitives d'un même matériau sont fusionnées en une partie. Un nœud qui inverse le repère (échelle négative) inverse aussi l'ordre des sommets de ses triangles, pour garder des faces avant antihoraires.
- Lus : `POSITION`, `NORMAL`, `TANGENT`, `TEXCOORD_0` (entiers normalisés compris), indices 8, 16 ou 32 bits, ou aucun indice. Les points et les lignes sont ignorés.
- Normales absentes : moyenne des faces voisines, pondérée par leur aire. Tangentes absentes : calculées à partir des UV, avec la convention de `Vertex` ; une tangente impossible à calculer (UV dégénérés) devient une perpendiculaire quelconque à la normale.
- Matériaux : facteurs metallic-roughness (bornés à [0 ; 1]) et indices des images de couleur de base et de métal-rugosité. Les valeurs absentes du fichier prennent celles de glTF (blanc, métal et rugosité à 1). Une primitive sans matériau reçoit le matériau par défaut du moteur (gris, non métallique), plus lisible sans IBL que celui de glTF. La normal map (`normalTexture` et son `scale`) est lue depuis la phase 7. Seul le premier jeu d'UV existe : `texCoord` est ignoré, comme les samplers, l'occlusion et l'émissif.
- Images : non décodées par l'importeur. Une image est soit un chemin relatif au modèle, déjà décodé des échappements d'URI, soit des octets (GLB, `data:` en base64). Le cache les charge ; une image absente n'empêche pas l'import du modèle.
- Scène lue : la scène par défaut, sinon la première, sinon tous les nœuds sans parent. Une hiérarchie de plus de 64 niveaux est refusée (protection contre un cycle).
- Limite : le chemin du fichier est passé à cgltf en encodage ANSI pour les `.bin` externes ; un dossier dont le nom contient des caractères hors de cet encodage ne fonctionnera pas.

## Cache (section 5.5, décision 0014)

- `ResourceCache` possède toutes les ressources CPU : meshes, textures, matériaux et modèles, chacun dans un `ResourcePool`. Il est construit avec le dossier des assets et le logger.
- `loadModel(chemin)` et `loadTexture(chemin, espace)` normalisent le chemin et le cherchent d'abord dans le cache : un fichier n'est lu qu'une fois, quelle que soit l'écriture du chemin. Un échec est mis en cache comme un succès : le fichier n'est pas relu à chaque demande.
- Un chargement ne renvoie jamais d'erreur à l'appelant : il renvoie un handle dont l'état dit `Failed`, et l'erreur est écrite dans le log avec le chemin (AC-11). Chemin invalide, fichier absent, fichier corrompu : même traitement.
- Un modèle chargé devient un `Model` : une liste de parties (`ModelPartRef` : un mesh et un matériau du cache). Ses textures externes passent par `loadTexture()`, donc par le cache ; ses images intégrées sont décodées sans clé de chemin. Une texture en échec n'empêche pas le modèle d'être prêt.
- Une même image utilisée comme couleur et comme données serait chargée deux fois, une fois par espace de couleur (formats GPU différents).
- `parts(modèle)` renvoie les parties d'un modèle prêt, sinon celles du modèle de secours : le cube de 1 m dans le damier magenta (`fallbackMaterial()`), impossible à manquer à l'écran (G-03, AC-11). `defaultMaterial()` reste le matériau par défaut, sans texture.
- `addMesh()`, `addMaterial()` et `addModel()` ajoutent des ressources faites en code (primitives et matériaux de la démo), sans chemin. Un mesh invalide ou un facteur hors de [0 ; 1] donnent `Failed`, avec une ligne de log.
- `loadEnvironment(chemin)` charge un environnement, une fois par chemin, comme une texture. Il n'y a pas d'environnement de secours : un handle qui n'est pas prêt fait éclairer la scène par le terme ambiant constant (décision 0022).
- `meshes()`, `textures()`, `materials()` et `environments()` donnent accès en lecture aux pools : le renderer y lit ce qui est devenu prêt ou a été déchargé.
- `modelPath(modèle)` et `environmentPath(environnement)` rendent le chemin d'asset d'un modèle ou d'un environnement chargé depuis un fichier (pour l'écriture d'une scène) ; `root()` le dossier des assets.
- `unloadAll()` décharge tout (déchargement de la scène) : chaque handle donné jusque-là devient périmé, et les assets de secours sont recréés.
- Chargement synchrone et sans verrou en phase 4 : l'état `Loading` ne dure que le temps de l'appel.

## Assets de secours (section 5.5)

- `makeCheckerTexture()` : damier magenta et noir de 64 × 64 pixels, cases de 8, en sRGB, pour une texture en échec.
- `makeWhiteTexture()` : un texel blanc, pour un matériau sans texture : multiplier par 1 laisse les facteurs inchangés.
- `makeFlatNormalTexture()` (`flatNormalTexture()` du cache) : un texel (128, 128, 255), linéaire, la normale de la surface elle-même. Une normal map absente, en chargement ou en échec la remplace : le damier, lu comme des normales, tordrait l'éclairage sans signaler l'erreur, que le log signale déjà (décision 0021).
- `makeFallbackMesh()` : cube de 1 m centré sur l'origine, pour un modèle en échec ; `makeDefaultMaterial()` : le matériau par défaut.

## Primitives

- `makeCube(size)` : cube centré sur l'origine, `size` mètres de côté, 24 sommets (normales par face), 12 triangles. Chaque face couvre toute la texture, haut de la texture vers le haut de la face (vers −Z pour la face du dessus).
- `makeSphere(radius, segments, rings)` : sphère UV centrée sur l'origine, pôles sur l'axe Y. `u` fait le tour depuis +Z vers +X, `v` va du pôle nord (0) au pôle sud (1). Les triangles dégénérés aux pôles sont omis.

## Dépendances

`Core` (et GLM par `Core/Math.h`). stb_image (décision 0013), dans `ImageDecoder.cpp` uniquement ; cgltf (décision 0012), dans `GltfImporter.cpp` uniquement. Ni Vulkan ni EnTT (section 4.2).
