# Mesures de performance

Objectifs du cahier des charges, section 7.1 : en Release, à 1920 × 1080, synchronisation verticale désactivée, une moyenne d'au moins 60 FPS et un 99e centile du temps de frame inférieur ou égal à 20 ms.

## Machine de référence

| Élément | Valeur |
|---|---|
| Processeur | Intel Core i7-1360P (13e génération), 12 cœurs, 16 threads |
| Carte graphique | Intel Iris Xe Graphics (intégrée), Vulkan 1.4.323 |
| Pilote graphique | 32.0.101.7088 |
| Mémoire | 16 Gio |
| Système | Windows 11 Famille, version 10.0.26200 |
| Écran | 2880 × 1800 à 60 Hz |
| Alimentation | Sur secteur |

À mettre à jour si la machine, le pilote ou Windows changent : une mesure n'est comparable qu'à une autre faite sur la même configuration.

## Protocole

```
build\windows-release\bin\ApexDemo.exe --benchmark 60 --no-vsync
```

- Configuration Release, fenêtre de 1920 × 1080 pixels, synchronisation verticale désactivée (mode de présentation mailbox, sinon immédiat).
- Les 2 premières secondes (création du pipeline, première swapchain) ne sont pas mesurées. Viennent ensuite 60 s de frames.
- La caméra suit un chemin fixe, identique à chaque mesure : un cercle de 9 m de rayon à 3 m de hauteur, un tour en 20 s, regard vers le centre de la scène. Le parcours de validation de la section 6.2 n'est jouable qu'à partir de la phase 6 ; d'ici là, ce chemin le remplace.
- Ne rien faire d'autre pendant la mesure. Le résultat est écrit dans `Logs/apex.log` (lignes `Benchmark`), avec la résolution, la vsync, le nombre d'objets dessinés et de triangles.
- FPS moyen : nombre de frames divisé par la durée mesurée. 99e centile : la plus petite durée que 99 % des frames ne dépassent pas.

## Mesures

| Date | Phase | Objets | Triangles | Options graphiques | FPS moyen | Frame moyenne | 99e centile | Maximum | Objectifs |
|---|---|---|---|---|---|---|---|---|---|
| 19 novembre 2026 | 3 | 19 | 22 188 | aucune | 1 602 | 0,62 ms | 1,53 ms | 18,12 ms | atteints |
| 19 novembre 2026 | 4 | 20 | 26 698 | textures (7, sans mipmaps) | 1 399 | 0,71 ms | 1,56 ms | 8,97 ms | atteints |
| 19 novembre 2026 | 5 | 20 | 26 698 | textures (7, sans mipmaps) | 1 456 | 0,69 ms | 1,46 ms | 4,72 ms | atteints |

### Phase 3 (19 novembre 2026)

Première mesure, demandée par la section 7.1 dès la fin de la phase 3. Scène de démonstration de la phase 3 : sol, 9 cubes, 10 sphères de 2 208 triangles. 96 118 frames en 60 s.

- Les objectifs sont largement atteints, mais la scène compte environ 13 fois moins de triangles que le budget de la scène finale (300 000, section 6.1), sans textures ni ombres. La mesure sert de point de départ, pas de preuve pour la phase 8.
- Le maximum de 18,12 ms est un accroc isolé : le 99e centile reste à 1,53 ms. Il reste sous les 20 ms ; s'il se répète, une capture Tracy (preset `windows-profile`) dira d'où il vient.
- Sans synchronisation verticale, l'application rend bien plus d'images que l'écran n'en affiche (60 Hz) : le FPS mesure le rythme de la boucle, pas celui de l'affichage.

### Phase 4 (19 novembre 2026)

Scène de la phase 3 avec des modèles glTF : cube de référence texturé, deux caisses (textures de 512 pixels), bouteille de Khronos (environ 4 500 triangles, textures de 2048 pixels). 83 961 frames en 60 s.

- Le FPS moyen baisse de 13 % (1 602 à 1 399) : échantillonnage de deux textures par fragment, un jeu de descripteurs lié par matériau, 4 500 triangles de plus. Le 99e centile ne bouge presque pas (1,53 à 1,56 ms).
- Le maximum descend à 8,97 ms : l'accroc de 18 ms de la phase 3 ne s'est pas reproduit.
- Mémoire GPU : 223 Mio après les envois (voir `Engine/Graphics/README.md` pour ce qu'elle compte). Cette mesure ne relevait pas la valeur de fin ; le résumé du benchmark la donne désormais (ligne `Benchmark end`), pour suivre la dérive demandée par la section 7.2.

### Phase 5 (19 novembre 2026)

Même contenu qu'en phase 4, mais la scène vient de `Assets/Scenes/demo.json` : 22 entités dans un registre EnTT, extraites vers le rendu à chaque frame. 87 382 frames en 60 s.

- Le passage par le registre et par l'extraction ne coûte rien de mesurable : 1 456 FPS contre 1 399 en phase 4, un écart de 4 % qui tient dans la variation d'une mesure à l'autre sur cette machine.
- Mémoire GPU : 223 Mio après les envois et 223 Mio en fin de mesure. Aucune dérive sur 60 s.

