# 0013 — Ajout de stb_image (décodage PNG et JPG)

**Date :** 19 novembre 2026
**Statut :** acceptée (plan de la phase 4 validé par le développeur le 19 novembre 2026)

## Contexte

Les textures des modèles sont en PNG ou en JPG (cahier des charges, section 5.5). Le cahier des charges retient stb_image, dans `Resources` (section 3.2).

## Décision

- Ajouter le port `stb` à `vcpkg.json` : version du 2 août 2026 dans la baseline, à confirmer à l'installation. Le port installe toutes les bibliothèques stb ; seul `stb_image.h` est utilisé.
- `Resources/ImageDecoder.cpp` est le seul fichier qui inclut `stb_image.h` et définit `STB_IMAGE_IMPLEMENTATION`. Seuls les décodeurs PNG et JPEG sont compilés (`STBI_ONLY_PNG`, `STBI_ONLY_JPEG`), sans accès direct aux fichiers (`STBI_NO_STDIO`) : le moteur lit les octets, stb_image les décode.
- Toute image est décodée en RGBA 8 bits, dans un `TextureData` qui porte aussi l'espace de couleur voulu (sRGB pour la couleur de base, linéaire pour les données). Une image de plus de 4096 pixels de côté est refusée (section 6.1 : 2048 au maximum dans la scène).
- La fonction de décodage est appelée depuis un seul thread à la fois : stb_image n'a pas d'état global modifiable sauf ses options, qui ne sont jamais changées.

## Alternatives écartées

- **WIC (Windows Imaging Component) :** une API Windows, interdite hors de `Platform` (G-10).
- **libpng et libjpeg-turbo :** deux bibliothèques et leurs API pour deux formats, sans gain pour des textures chargées une fois.

## Conséquences

- Module touché : `Resources` uniquement (section 4.2).
- Garde-fous : G-04, G-06 (aucune option globale de stb_image modifiée), G-10.
- Licence : MIT ou domaine public, au choix ; le projet retient la licence MIT. Ajoutée à `ThirdPartyLicenses.md`.
- Retour en arrière : local à `ImageDecoder.cpp`. Le format HDR de l'IBL (P1) passera aussi par ce fichier.
