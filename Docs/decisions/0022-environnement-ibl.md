# 0022 — Environnement, IBL et skybox

**Date :** 20 novembre 2026
**Statut :** acceptée par délégation (plan de la phase 7 validé par le développeur le 20 novembre 2026, points ouverts confiés à l'agent ; à relire par le développeur)

## Contexte

Section 5.4, P1 : l'environnement est chargé depuis un fichier `.hdr`, préfiltré au chargement (irradiance diffuse, spéculaire par niveau de rugosité, table BRDF) et affiché en skybox. Il remplace le terme ambiant constant. AC-P1-6 : la skybox est visible et les sphères métalliques reflètent l'environnement, de net à flou selon la rugosité.

## Décision

1. **Aucune nouvelle dépendance.** stb_image lit le format Radiance (`.hdr`) ; il est déjà dans `Resources` (décision 0013, qui le prévoyait). Le décodage reste dans `ImageDecoder.cpp`.
2. **Une ressource de plus.** `Resources` reçoit `EnvironmentData` (image équirectangulaire en RGB flottant linéaire, indépendante du format de fichier, G-04) et un handle à état, `EnvironmentHandle`, dans le même cache que les autres ressources : chargé une fois par chemin, libéré au déchargement de la scène. Un fichier manquant ou invalide est écrit dans le log avec son chemin ; la scène est alors éclairée par le terme ambiant constant, sans skybox (G-03). Il n'y a pas d'environnement de secours : l'ambiant en tient lieu.
3. **Préfiltrage partagé entre CPU et GPU.**
   - Irradiance diffuse : neuf coefficients d'harmoniques sphériques, calculés sur le CPU à l'envoi, placés dans `FrameUniforms`.
   - Table BRDF : 64 × 64, calculée une fois sur le CPU à la création du renderer.
   - Spéculaire : une cubemap de 256 × 256 par face, un niveau de mip par rugosité (0 au niveau 0, 1 au dernier), préfiltrée sur le GPU par échantillonnage préférentiel de la GGX, une fois par environnement.
   Les calculs du CPU sont testés sans GPU (un environnement uniforme de radiance L donne une irradiance π·L dans toutes les directions).
4. **Skybox.** Dessinée dans la passe de scène après les objets, au plan lointain, sans écriture de profondeur ; elle lit le niveau 0 de la cubemap.
5. **Scène.** Champ facultatif `environment` : `{ "path": "Environments/sky.hdr", "intensity": 1.0 }`. Le format reste en version 1 : les fichiers existants restent valides. Avec l'IBL, le champ `ambient` n'est plus utilisé ; il sert encore quand l'environnement manque ou que l'IBL est désactivée.
6. **Fichier d'environnement.** Généré par `Tools/generate_assets.py` : ciel en dégradé, halo autour du soleil, sol sombre, quelques panneaux colorés à l'horizon pour lire les reflets. Il ne pose aucune question de licence. Le soleil lui-même n'y est pas : la lumière directionnelle l'éclaire déjà.
7. **Bascule.** L'IBL se désactive dans le panneau. Désactivée, le terme ambiant constant revient et le ciel est remplacé par la couleur d'effacement.

## Alternatives écartées

- **Préfiltrage entièrement sur le GPU :** quatre pipelines au lieu d'un (conversion en cubemap, irradiance, spéculaire, table BRDF), et des résultats qui ne se vérifient qu'à l'œil.
- **Préfiltrage entièrement sur le CPU :** la cubemap spéculaire demande des dizaines de millions d'échantillons, plusieurs secondes en Debug au lancement.
- **HDRI photographique (Poly Haven, CC0) :** plus beau, mais plusieurs Mo de plus dans le dépôt, téléchargés hors du projet.
- **Format de scène en version 2 :** aucun fichier existant n'en a besoin.
- **Environnement de secours (ciel uni) :** l'ambiant constant joue déjà ce rôle, sans asset de plus.

## Conséquences

- Modules touchés : `Resources` (décodage, données, cache), `Graphics` (préfiltrage, cubemap, éclairage, ciel), `World` (`SceneEnvironment`), `Serialization` (champ `environment`).
- Garde-fous : G-03 (handle à état, repli sur l'ambiant), G-04 (`EnvironmentData` indépendante du format), G-08 (les shaders lisent l'éclairage par un fichier d'inclusion commun), G-10 (stb_image reste dans `Resources`).
- Mémoire GPU : l'image source avec ses mipmaps (environ 5 Mio en 1024 × 512, RGBA 16 bits flottants) et la cubemap (environ 4 Mio).
- Retour en arrière : un HDRI photographique se dépose à la place du fichier généré, sans code.
