# 0016 — Ajout de nlohmann/json (scènes et configuration)

**Date :** 19 novembre 2026
**Statut :** acceptée (plan de la phase 5 validé par le développeur le 19 novembre 2026)

## Contexte

Les scènes et la configuration sont en JSON, avec un champ `version` (cahier des charges, section 3.1). Le cahier des charges retient nlohmann/json, dans `Serialization` (section 3.2).

## Décision

- Ajouter `nlohmann-json` à `vcpkg.json` : version 3.12.0 dans la baseline, à confirmer à l'installation. Bibliothèque en en-têtes seuls.
- Les seuls fichiers qui incluent nlohmann/json sont des `.cpp` de `Serialization`, plus un en-tête interne partagé entre eux. Aucun en-tête public n'expose de type JSON : le module rend des structures C++ (`SceneDescription`, `EngineConfig`, `InputConfig`).
- La bibliothèque lève des exceptions sur un JSON mal formé. Elles sont interceptées dans `Serialization` et converties en `core::Error` (section 3.1) ; aucune ne sort du module.
- Les conversions implicites de la bibliothèque sont désactivées (`JSON_USE_IMPLICIT_CONVERSIONS=0`) : chaque champ est lu avec son type attendu, et une erreur nomme le fichier, l'entité et le champ (section 5.6).
- L'écriture garde l'ordre des champs (`nlohmann::ordered_json`), pour un fichier lisible et stable d'une écriture à l'autre.

## Alternatives écartées

- **RapidJSON, simdjson :** plus rapides, mais les fichiers lus ici font quelques kilo-octets, chargés une fois ; la simplicité d'écriture prime.
- **Format maison ou binaire :** le cahier des charges fixe JSON (section 3.1), éditable à la main (AC-04).

## Conséquences

- Module touché : `Serialization` uniquement (confinement de la section 4.2).
- Garde-fous : G-10.
- Licence : MIT, ajoutée à `ThirdPartyLicenses.md`.
- Retour en arrière : local à `Serialization`, puisque aucun type JSON n'en sort.
