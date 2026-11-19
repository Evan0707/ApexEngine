# 0005 — Précisions sur l'intégration de SDL3

**Date :** 19 novembre 2026
**Statut :** acceptée. Corrige deux points de la note 0002, qui reste en vigueur pour le reste.

## Contexte

L'intégration de SDL3 3.4.20 (phase 1, tâche 5) a montré que deux points de la note 0002 ne peuvent pas être appliqués à la lettre :

- la note demande d'appeler `SDL_SetMainReady()` sans inclure `SDL_main.h`, alors que cette fonction n'est déclarée que dans cet en-tête ;
- la note prévoit de recopier le texte exact de la licence dans `ThirdPartyLicenses.md`, alors que le fichier `copyright` installé par vcpkg fait plus de 9 000 lignes : il couvre SDL et tout le code tiers qu'il embarque (hidapi, stb, etc.).

## Décision

- `Engine/Platform/Window.cpp` inclut `SDL_main.h`, en définissant `SDL_MAIN_HANDLED` juste avant. Dans ce cas, SDL ne redéfinit pas `main` et n'inclut pas `SDL_main_impl.h` : `Game` garde un `main` standard, ce qui était l'intention de la note 0002.
- `ThirdPartyLicenses.md` donne pour SDL la version, la licence (Zlib, et la déclaration du port vcpkg pour le code embarqué) et renvoie au fichier `share/sdl3/copyright` installé par vcpkg, sans en recopier le texte.

## Alternatives écartées

- **Redéclarer `SDL_SetMainReady()` à la main :** évite l'inclusion, mais casse sans bruit si SDL change la déclaration.
- **Ne pas appeler `SDL_SetMainReady()` :** SDL fonctionne sous Windows sans cet appel, mais la documentation de SDL le demande quand l'application garde son propre `main`.
- **Recopier les 9 000 lignes :** rend `ThirdPartyLicenses.md` illisible pour un texte déjà fourni par vcpkg avec le paquet. La règle de la section 7.3 du cahier des charges vise le code tiers copié hors de vcpkg, ce qui n'est pas le cas.

## Conséquences

- La même règle de renvoi s'applique aux prochaines dépendances dont le fichier de licence est long ; une licence courte reste recopiée ou résumée.
- Garde-fou G-10 inchangé : `SDL_main.h` n'est inclus que dans un fichier `.cpp` de `Platform`.
- Coût de retour en arrière : nul.
