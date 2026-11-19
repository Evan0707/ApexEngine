# Graphics

**Rôle :** renderer Vulkan 1.3, seul module qui émet des appels GPU (cahier des charges, sections 4.3 et 5.4, garde-fou G-01).

**État (phase 2, tâche 3) :** instance, surface de la fenêtre et périphérique logique, avec la validation en Debug. Rien n'est encore dessiné : swapchain et première frame arrivent avec la tâche 5. Notes de décision `0006-chaine-vulkan.md` et `0009-frontieres-platform-graphics-debug.md`.

## Interface publique

Aucun en-tête public n'inclut Vulkan : `Game` peut les inclure tous.

| En-tête | Contenu |
|---|---|
| `Graphics/ValidationLog.h` | `ValidationSeverity`, `ValidationLog` : `report()`, `messageCount()` |
| `Graphics/Renderer.h` | `RendererConfig`, `Renderer` : `create()` |

En-tête interne, inclus uniquement par les fichiers `.cpp` de `Graphics` : `Graphics/Device.h` (`DeviceConfig`, `Device`), qui inclut Vulkan et vk-bootstrap.

## Renderer

- `Renderer::create(window, logger, validation, config)` crée le périphérique. Échec : `ErrorCode::GpuFailure` (ou `PlatformFailure` si SDL ne peut pas créer la surface), avec un message qui dit ce qui manque. Le message est aussi écrit dans le log.
- La fenêtre, le logger et le `ValidationLog` doivent vivre plus longtemps que le renderer. Le renderer n'est ni copiable ni déplaçable : il est tenu par un `std::unique_ptr`.
- Le destructeur attend la fin du travail du GPU, puis détruit le périphérique, la surface et l'instance, dans cet ordre.

## Périphérique

- Instance Vulkan 1.3. Ses extensions sont celles que SDL demande pour présenter dans la fenêtre (`Window::vulkanInstanceExtensions()`), vk-bootstrap ne choisit pas les siennes.
- GPU retenu : le premier qui accepte Vulkan 1.3, `dynamicRendering`, `synchronization2` et la présentation dans la surface, un GPU dédié de préférence. Le nom du GPU et sa version de Vulkan sont écrits dans le log.
- Une file graphique et une file de présentation (souvent la même).

## Validation

- Demandée par `RendererConfig::enableValidation`, que `Game` active en Debug. Si les layers ne sont pas installées (SDK Vulkan absent), un avertissement est écrit et le renderer continue sans elles.
- Messages reçus : avertissements et erreurs, de type général, validation et performance. Chaque message est écrit dans le log, préfixé par `[vulkan]`, et compté par `ValidationLog`. Aucun message n'est filtré ni ignoré (`AGENTS.md`, section 7).
- Le `ValidationLog` est créé par l'application avant le renderer et lu après sa destruction : les messages de la fermeture sont comptés. `ApexDemo` renvoie le code de sortie 2 si au moins un message a été émis.
- `report()` peut être appelé depuis n'importe quel thread : les layers appellent depuis le thread qui a fait l'appel Vulkan fautif.

## Dépendances

Autorisé : `Core`, `Platform`, `Resources`. Interdit : EnTT, Jolt, `World`. Bibliothèques : Vulkan-Headers, Vulkan-Loader et vk-bootstrap 1.4.363 (liés en `PRIVATE` à `ApexEngine`). Les en-têtes Vulkan ne sont inclus que dans `Graphics` et dans `Debug/ImGuiBackend.cpp`.
