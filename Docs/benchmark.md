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
- La caméra suit un chemin fixe, identique à chaque mesure : un cercle de 9 m de rayon à 3 m de hauteur, un tour en 20 s, regard vers le centre de la scène. Le parcours de validation de la section 6.2 est jouable depuis la phase 6, mais la démo ne sait pas encore le rejouer seule : ce chemin le remplace jusqu'à la phase 8 (AC-16). Pendant la mesure, la physique tourne et le personnage reste immobile.
- Ne rien faire d'autre pendant la mesure. Le résultat est écrit dans `Logs/apex.log` (lignes `Benchmark`), avec la résolution, la vsync, le nombre d'objets dessinés et de triangles.
- FPS moyen : nombre de frames divisé par la durée mesurée. 99e centile : la plus petite durée que 99 % des frames ne dépassent pas.

## Mesures

| Date | Phase | Objets | Triangles | Options graphiques | FPS moyen | Frame moyenne | 99e centile | Maximum | Objectifs |
|---|---|---|---|---|---|---|---|---|---|
| 19 novembre 2026 | 3 | 19 | 22 188 | aucune | 1 602 | 0,62 ms | 1,53 ms | 18,12 ms | atteints |
| 19 novembre 2026 | 4 | 20 | 26 698 | textures (7, sans mipmaps) | 1 399 | 0,71 ms | 1,56 ms | 8,97 ms | atteints |
| 19 novembre 2026 | 5 | 20 | 26 698 | textures (7, sans mipmaps) | 1 456 | 0,69 ms | 1,46 ms | 4,72 ms | atteints |
| 20 novembre 2026 | 6 | 31 | 27 842 | textures (7, sans mipmaps) | 1 480 | 0,68 ms | 1,41 ms | 17,74 ms | atteints |
| 20 novembre 2026 | 7 | 26 (31 dans la passe d'ombre) | 27 782 | textures avec mipmaps, anisotropie 8×, ombres 2048, HDR et ACES, IBL et ciel, normal maps, frustum culling | 525 | 1,91 ms | 3,15 ms | 56,03 ms | atteints |

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

### Phase 6 (20 novembre 2026)

Scène du parcours de validation : 32 entités, 27 corps physiques (12 boîtes statiques, 10 sphères, 5 caisses dynamiques) et le personnage, simulés par Jolt à 60 pas par seconde sur un seul thread. 88 784 frames en 60 s.

- La physique ne coûte rien de mesurable ici : 1 480 FPS contre 1 456 en phase 5. Les caisses dorment pendant la mesure et le personnage est immobile ; un pas de physique ne fait alors presque rien.
- Le maximum de 17,74 ms est un accroc isolé, comme en phase 3 : le 99e centile est de 1,41 ms. Cette mesure n'a pas respecté la règle « ne rien faire d'autre » : des modifications de fichiers ont eu lieu sur la machine pendant la minute mesurée. À refaire au calme si ce chiffre doit servir.
- Mémoire GPU : 223 Mio après les envois et 223 Mio en fin de mesure. Aucune dérive sur 60 s.
- Ce que la mesure ne dit pas : le coût de la physique quand des corps bougent (pile renversée, caisses poussées), et le parcours joué au clavier. La mesure de la phase 8 devra rejouer le parcours.

### Phase 7 (20 novembre 2026)

Même scène qu'en phase 6, avec le rendu P1 : passe d'ombre de 2048 × 2048, image HDR de 1920 × 1080 en flottants 16 bits, tonemapping ACES, éclairage par l'environnement et ciel, normal map de la caisse, mipmaps et anisotropie 8×, frustum culling. 31 475 frames en 60 s. Deux mesures de suite ; la seconde est retenue dans le tableau.

- Le coût du rendu P1 se voit sur la carte graphique intégrée : de 1 480 à 525 FPS, de 0,68 à 1,91 ms par frame. Les objectifs restent atteints de très loin (60 FPS, 20 ms).
- Première mesure : 528 FPS, 3,00 ms au 99e centile, mais un maximum de 1 302,78 ms. Une seule frame ; la mesure suivante, lancée aussitôt, donne 56,03 ms au maximum. L'origine n'est pas établie (le chargement de l'environnement était fini avant la mesure, d'après le log) ; le 99e centile n'en est pas affecté. À surveiller en phase 8 avec une capture Tracy si l'accroc revient.
- Le culling réduit la passe principale à 26 objets sur 31 sur le chemin de la caméra ; la passe d'ombre en garde 31 (toute la scène tient dans la zone de 40 m).
- Mémoire GPU : 250 Mio après les envois et 250 Mio en fin de mesure, contre 223 Mio en phase 6 (image HDR, shadow map, cubemap, mipmaps). Aucune dérive sur 60 s.
- Le log du benchmark donne maintenant les options graphiques actives (ligne `Benchmark graphics options`).

