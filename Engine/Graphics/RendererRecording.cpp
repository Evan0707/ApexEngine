#include "Core/Profiling.h"
#include "Graphics/RendererState.h"
#include "Graphics/VulkanBackendAccess.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace apex::graphics {

namespace {

// Linear HDR color, before exposure and tonemapping: a dark blue, easy to tell from a black window.
constexpr VkClearColorValue kClearColor{{0.02f, 0.04f, 0.10f, 1.0f}};

struct ImageBarrier {
    VkImage image;
    VkImageAspectFlags aspect;
    VkImageLayout from;
    VkImageLayout to;
    VkPipelineStageFlags2 srcStage;
    VkAccessFlags2 srcAccess;
    VkPipelineStageFlags2 dstStage;
    VkAccessFlags2 dstAccess;
};

void transitionImage(VkCommandBuffer commands, const ImageBarrier& transition) {
    const VkImageMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = transition.srcStage,
        .srcAccessMask = transition.srcAccess,
        .dstStageMask = transition.dstStage,
        .dstAccessMask = transition.dstAccess,
        .oldLayout = transition.from,
        .newLayout = transition.to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = transition.image,
        .subresourceRange = {transition.aspect, 0, 1, 0, 1},
    };
    const VkDependencyInfo dependency{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };
    vkCmdPipelineBarrier2(commands, &dependency);
}

} // namespace

std::pair<const GpuMesh*, std::uint32_t>
Renderer::State::resolveMesh(const RenderObject& object) const noexcept {
    if (const GpuMesh* mesh = meshes.find(object.mesh)) {
        return {mesh, object.mesh.index};
    }
    return {meshes.find(fallbackMesh), fallbackMesh.index};
}

void Renderer::State::prepareFrame(const RenderScene& scene) {
    drawItems.clear();
    for (std::uint32_t i = 0; i < scene.objects.size(); ++i) {
        const RenderObject& object = scene.objects[i];
        const auto [mesh, meshIndex] = resolveMesh(object);
        if (mesh == nullptr) {
            continue;
        }
        drawItems.push_back(DrawItem{
            .objectIndex = i, .materialIndex = materials.gpuIndex(object.material), .meshIndex = meshIndex});
    }
    if (drawItems.size() > kMaxObjects) {
        if (!warnedTooManyObjects) {
            logger->warning("{} objects to draw, only the first {} are (logged once)", drawItems.size(),
                            kMaxObjects);
            warnedTooManyObjects = true;
        }
        drawItems.resize(kMaxObjects);
    }
    sortDrawItems(drawItems);

    objectData.clear();
    drawMeshes.clear();
    drawMaterialSets.clear();
    for (const DrawItem& item : drawItems) {
        const RenderObject& object = scene.objects[item.objectIndex];
        drawMeshes.push_back(resolveMesh(object).first);
        const VkDescriptorSet* materialSet = materialSetMirror.find(object.material);
        drawMaterialSets.push_back(materialSet != nullptr ? *materialSet
                                                          : defaultMaterialSet.value_or(VK_NULL_HANDLE));
        objectData.push_back(
            makeObjectGpu(scene.objects[item.objectIndex].modelRelativeToCamera, item.materialIndex));
    }
    const VkExtent2D extent = swapchain.extent();
    const float aspectRatio = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    frameShadow = scene.shadow;
    frameBuffers.write(frameIndex, makeFrameUniforms(scene, aspectRatio, shadowMap.size()), objectData,
                       materials.gpuData());
}

void Renderer::State::recordFrame(VkCommandBuffer commands, std::uint32_t imageIndex) {
    gpuTimer.begin(commands, frameIndex);
    const VkImage image = swapchain.image(imageIndex);
    recordShadowPass(commands);

    // One HDR image for both frames in flight: the previous frame's tonemapping must be done reading it.
    // UNDEFINED: the previous content is cleared anyway.
    transitionImage(commands,
                    {sceneColor.image(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
    // One depth buffer for both frames in flight: the previous frame's depth writes must be done.
    constexpr VkPipelineStageFlags2 kDepthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    transitionImage(commands, {depth.image(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, kDepthStages,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, kDepthStages,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});

    recordScenePass(commands);

    transitionImage(commands,
                    {sceneColor.image(), VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                     VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
    // UNDEFINED: the tonemapping pass writes every pixel. Waits for the acquire semaphore (same stage).
    transitionImage(commands, {image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                               VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
    recordTonemapPass(commands, imageIndex);
    // Last pass, without depth: the interface is drawn over the tonemapped image (decisions 0008 and 0021).
    if (overlay != nullptr) {
        transitionImage(
            commands,
            {image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT});
        recordOverlayPass(commands, imageIndex);
    }

    transitionImage(commands,
                    {image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                     VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, VK_ACCESS_2_NONE});
    gpuTimer.end(commands, frameIndex);
}

void Renderer::State::recordShadowPass(VkCommandBuffer commands) {
    constexpr VkPipelineStageFlags2 kDepthStages =
        VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    // One shadow map for both frames in flight: the previous frame's scene pass must be done reading it.
    transitionImage(commands, {shadowMap.image(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                               VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                               VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE, kDepthStages,
                               VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                   VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT});
    const VkExtent2D extent{shadowMap.size(), shadowMap.size()};
    // Cleared even when shadows are off: the scene shader's descriptor must hold a valid image.
    const VkRenderingAttachmentInfo depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = shadowMap.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.depthStencil = {1.0f, 0}},
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .pDepthAttachment = &depthAttachment,
    };
    vkCmdBeginRendering(commands, &rendering);
    counters.shadowDrawCalls = 0;
    if (frameShadow.enabled) {
        const VkViewport viewport{.x = 0.0f,
                                  .y = 0.0f,
                                  .width = static_cast<float>(extent.width),
                                  .height = static_cast<float>(extent.height),
                                  .minDepth = 0.0f,
                                  .maxDepth = 1.0f};
        const VkRect2D scissor{{0, 0}, extent};
        vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.handle());
        vkCmdSetViewport(commands, 0, 1, &viewport);
        vkCmdSetScissor(commands, 0, 1, &scissor);
        // 16-bit depth over the area's depth range: one unit of constant bias is depthRange / 65536 meters.
        constexpr float kDepthSteps = 65536.0f;
        vkCmdSetDepthBias(commands, frameShadow.constantBias * kDepthSteps / frameShadow.depthRange, 0.0f,
                          frameShadow.slopeBias);
        const VkDescriptorSet frameSet = frameBuffers.descriptorSet(frameIndex);
        vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.layout(), 0, 1,
                                &frameSet, 0, nullptr);
        const GpuMesh* boundMesh = nullptr;
        for (std::uint32_t drawIndex = 0; drawIndex < drawItems.size(); ++drawIndex) {
            const GpuMesh* mesh = drawMeshes[drawIndex];
            if (mesh != boundMesh) {
                const VkDeviceSize offset = 0;
                vkCmdBindVertexBuffers(commands, 0, 1, &mesh->vertices.buffer, &offset);
                vkCmdBindIndexBuffer(commands, mesh->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
                boundMesh = mesh;
            }
            vkCmdDrawIndexed(commands, mesh->indexCount, 1, 0, 0, drawIndex);
            ++counters.shadowDrawCalls;
        }
    }
    vkCmdEndRendering(commands);
    transitionImage(commands,
                    {shadowMap.image(), VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                     VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                     VK_ACCESS_2_SHADER_SAMPLED_READ_BIT});
}

void Renderer::State::recordScenePass(VkCommandBuffer commands) {
    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = sceneColor.view(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = {.color = kClearColor},
    };
    // Depth is not needed after the pass.
    const VkRenderingAttachmentInfo depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depth.view(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {1.0f, 0}},
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, swapchain.extent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
        .pDepthAttachment = &depthAttachment,
    };
    vkCmdBeginRendering(commands, &rendering);

    const VkExtent2D extent = swapchain.extent();
    const VkViewport viewport{.x = 0.0f,
                              .y = 0.0f,
                              .width = static_cast<float>(extent.width),
                              .height = static_cast<float>(extent.height),
                              .minDepth = 0.0f,
                              .maxDepth = 1.0f};
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.handle());
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    const VkDescriptorSet frameSet = frameBuffers.descriptorSet(frameIndex);
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.layout(), 0, 1,
                            &frameSet, 0, nullptr);

    counters.drawCalls = 0;
    counters.triangles = 0;
    const GpuMesh* boundMesh = nullptr;
    VkDescriptorSet boundMaterial = VK_NULL_HANDLE;
    for (std::uint32_t drawIndex = 0; drawIndex < drawItems.size(); ++drawIndex) {
        const GpuMesh* mesh = drawMeshes[drawIndex];
        const VkDescriptorSet materialSet = drawMaterialSets[drawIndex];
        if (materialSet == VK_NULL_HANDLE) {
            continue;
        }
        // Draws are sorted by material: one bind per material (spec 5.4).
        if (materialSet != boundMaterial) {
            vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipeline.layout(), 1, 1,
                                    &materialSet, 0, nullptr);
            boundMaterial = materialSet;
        }
        if (mesh != boundMesh) {
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commands, 0, 1, &mesh->vertices.buffer, &offset);
            vkCmdBindIndexBuffer(commands, mesh->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
            boundMesh = mesh;
        }
        // firstInstance = place in the object buffer: the shader reads it through gl_InstanceIndex (G-02).
        vkCmdDrawIndexed(commands, mesh->indexCount, 1, 0, 0, drawIndex);
        ++counters.drawCalls;
        counters.triangles += mesh->indexCount / 3;
    }

    vkCmdEndRendering(commands);
}

void Renderer::State::recordTonemapPass(VkCommandBuffer commands, std::uint32_t imageIndex) {
    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkExtent2D extent = swapchain.extent();
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, extent},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    vkCmdBeginRendering(commands, &rendering);
    const VkViewport viewport{.x = 0.0f,
                              .y = 0.0f,
                              .width = static_cast<float>(extent.width),
                              .height = static_cast<float>(extent.height),
                              .minDepth = 0.0f,
                              .maxDepth = 1.0f};
    const VkRect2D scissor{{0, 0}, extent};
    vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipeline.handle());
    vkCmdSetViewport(commands, 0, 1, &viewport);
    vkCmdSetScissor(commands, 0, 1, &scissor);
    const std::array sets{frameBuffers.descriptorSet(frameIndex), sceneColorInput.set()};
    vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipeline.layout(), 0,
                            static_cast<std::uint32_t>(sets.size()), sets.data(), 0, nullptr);
    // A full-screen triangle made by the vertex shader.
    vkCmdDraw(commands, 3, 1, 0, 0);
    vkCmdEndRendering(commands);
}

void Renderer::State::recordOverlayPass(VkCommandBuffer commands, std::uint32_t imageIndex) {
    const VkRenderingAttachmentInfo colorAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = swapchain.imageView(imageIndex),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };
    const VkRenderingInfo rendering{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {{0, 0}, swapchain.extent()},
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachment,
    };
    vkCmdBeginRendering(commands, &rendering);
    overlay->record(OverlayCommands{.commandBuffer = commands, .colorFormat = swapchain.format()});
    vkCmdEndRendering(commands);
}

} // namespace apex::graphics
