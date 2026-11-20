#include "Graphics/GpuEnvironment.h"

#include "Core/Profiling.h"
#include "Graphics/MipLevels.h"

#include <format>
#include <span>
#include <vector>

namespace apex::graphics {

namespace {

constexpr std::uint32_t kCubeFaces = 6;
constexpr VkFormat kBrdfTableFormat = VK_FORMAT_R16G16_SFLOAT;

[[nodiscard]] std::unexpected<core::Error> viewError(std::string_view name, VkResult result) {
    return core::makeError(
        core::ErrorCode::GpuFailure,
        std::format("Cannot create a view of '{}': VkResult {}", name, static_cast<int>(result)));
}

[[nodiscard]] VkResult createView(VkDevice device, VkImage image, VkImageViewType type, VkFormat format,
                                  VkImageSubresourceRange range, VkImageView& view) {
    const VkImageViewCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                     .image = image,
                                     .viewType = type,
                                     .format = format,
                                     .subresourceRange = range};
    return vkCreateImageView(device, &info, nullptr, &view);
}

// Destroys what an environment's creation made in between, whatever the outcome.
class Scratch {
public:
    Scratch(VkDevice device, GpuAllocator& allocator) noexcept : m_device(device), m_allocator(&allocator) {}
    ~Scratch() {
        for (const VkImageView view : views) {
            vkDestroyImageView(m_device, view, nullptr);
        }
        if (image.image != VK_NULL_HANDLE) {
            m_allocator->destroyImage(image);
        }
    }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    GpuImage image;
    std::vector<VkImageView> views;

private:
    VkDevice m_device;
    GpuAllocator* m_allocator;
};

} // namespace

core::Result<GpuEnvironment> createGpuEnvironment(VkDevice device, GpuAllocator& allocator,
                                                  UploadContext& upload, EnvironmentPrefilter& prefilter,
                                                  LightingSets& lightingSets, VkImageView brdfTable,
                                                  std::string_view name,
                                                  const resources::EnvironmentData& environment) {
    APEX_PROFILE_SCOPE("Graphics::createGpuEnvironment");
    GpuEnvironment result{.irradiance = projectIrradiance(environment)};

    // The equirectangular image, in 16-bit floats, with its mips: read only by the prefilter.
    Scratch source(device, allocator);
    const std::uint32_t sourceLevels = mipLevelCount(environment.width, environment.height);
    const VkImageCreateInfo sourceInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = kEnvironmentFormat,
        .extent = {environment.width, environment.height, 1},
        .mipLevels = sourceLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    core::Result<GpuImage> sourceImage = allocator.createImage(name, sourceInfo, false);
    if (!sourceImage) {
        return std::unexpected(sourceImage.error());
    }
    source.image = *sourceImage;
    std::vector<std::uint64_t> halves;
    halves.reserve(environment.texels.size());
    for (const glm::vec3& texel : environment.texels) {
        halves.push_back(glm::packHalf4x16(glm::vec4(texel, 1.0f)));
    }
    const ImageCopy copy{.destination = source.image.image,
                         .width = environment.width,
                         .height = environment.height,
                         .data = std::as_bytes(std::span(halves)),
                         .mipLevels = sourceLevels};
    if (core::Result<void> uploaded = upload.uploadImage(allocator, name, copy); !uploaded) {
        return std::unexpected(uploaded.error());
    }
    VkImageView sourceView = VK_NULL_HANDLE;
    if (const VkResult created =
            createView(device, source.image.image, VK_IMAGE_VIEW_TYPE_2D, kEnvironmentFormat,
                       {VK_IMAGE_ASPECT_COLOR_BIT, 0, sourceLevels, 0, 1}, sourceView);
        created != VK_SUCCESS) {
        return viewError(name, created);
    }
    source.views.push_back(sourceView);

    // The cubemap, and one view per level and face to draw into.
    const VkImageCreateInfo cubeInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = kEnvironmentFormat,
        .extent = {kSpecularCubeSize, kSpecularCubeSize, 1},
        .mipLevels = kSpecularCubeLevels,
        .arrayLayers = kCubeFaces,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    core::Result<GpuImage> cube = allocator.createImage(std::format("{} (specular)", name), cubeInfo, false);
    if (!cube) {
        return std::unexpected(cube.error());
    }
    result.specular.image = *cube;
    Scratch faces(device, allocator);
    for (std::uint32_t level = 0; level < kSpecularCubeLevels; ++level) {
        for (std::uint32_t face = 0; face < kCubeFaces; ++face) {
            VkImageView view = VK_NULL_HANDLE;
            if (const VkResult created =
                    createView(device, result.specular.image.image, VK_IMAGE_VIEW_TYPE_2D, kEnvironmentFormat,
                               {VK_IMAGE_ASPECT_COLOR_BIT, level, 1, face, 1}, view);
                created != VK_SUCCESS) {
                allocator.destroyImage(result.specular.image);
                return viewError(name, created);
            }
            faces.views.push_back(view);
        }
    }
    if (const VkResult created = createView(
            device, result.specular.image.image, VK_IMAGE_VIEW_TYPE_CUBE, kEnvironmentFormat,
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, kSpecularCubeLevels, 0, kCubeFaces}, result.specular.view);
        created != VK_SUCCESS) {
        allocator.destroyImage(result.specular.image);
        return viewError(name, created);
    }

    const PrefilterParams params{.faceSize = kSpecularCubeSize,
                                 .levels = kSpecularCubeLevels,
                                 .samples = kPrefilterSamples,
                                 .sourceTexelSolidAngle = 4.0f * glm::pi<float>() /
                                                          (static_cast<float>(environment.width) *
                                                           static_cast<float>(environment.height))};
    if (core::Result<void> drawn = upload.run(
            name,
            [&](VkCommandBuffer commands) {
                prefilter.record(commands, sourceView, result.specular.image.image, faces.views, params);
            });
        !drawn) {
        destroyGpuTexture(device, allocator, result.specular);
        return std::unexpected(drawn.error());
    }

    const std::optional<VkDescriptorSet> set = lightingSets.allocate(result.specular.view, brdfTable);
    if (!set) {
        destroyGpuTexture(device, allocator, result.specular);
        return core::makeError(core::ErrorCode::GpuFailure,
                               std::format("No room for the lighting descriptor set of '{}'", name));
    }
    result.lightingSet = *set;
    return result;
}

void destroyGpuEnvironment(VkDevice device, GpuAllocator& allocator, LightingSets& lightingSets,
                           const GpuEnvironment& environment) noexcept {
    lightingSets.free(environment.lightingSet);
    destroyGpuTexture(device, allocator, environment.specular);
}

core::Result<GpuTexture> createBrdfTable(VkDevice device, GpuAllocator& allocator, UploadContext& upload) {
    const std::vector<glm::vec2> table = computeBrdfTable(kBrdfTableSize, kBrdfTableSamples);
    std::vector<std::uint32_t> halves;
    halves.reserve(table.size());
    for (const glm::vec2 entry : table) {
        halves.push_back(glm::packHalf2x16(entry));
    }
    const VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = kBrdfTableFormat,
        .extent = {kBrdfTableSize, kBrdfTableSize, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    constexpr std::string_view kName = "BRDF table";
    core::Result<GpuImage> image = allocator.createImage(kName, info, false);
    if (!image) {
        return std::unexpected(image.error());
    }
    GpuTexture texture{.image = *image};
    const ImageCopy copy{.destination = texture.image.image,
                         .width = kBrdfTableSize,
                         .height = kBrdfTableSize,
                         .data = std::as_bytes(std::span(halves))};
    if (core::Result<void> uploaded = upload.uploadImage(allocator, kName, copy); !uploaded) {
        allocator.destroyImage(texture.image);
        return std::unexpected(uploaded.error());
    }
    if (const VkResult created =
            createView(device, texture.image.image, VK_IMAGE_VIEW_TYPE_2D, kBrdfTableFormat,
                       {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}, texture.view);
        created != VK_SUCCESS) {
        allocator.destroyImage(texture.image);
        return viewError(kName, created);
    }
    return texture;
}

core::Result<GpuTexture> createBlackCube(VkDevice device, GpuAllocator& allocator, UploadContext& upload) {
    const VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = kEnvironmentFormat,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = kCubeFaces,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };
    constexpr std::string_view kName = "black environment";
    core::Result<GpuImage> image = allocator.createImage(kName, info, false);
    if (!image) {
        return std::unexpected(image.error());
    }
    GpuTexture texture{.image = *image};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, kCubeFaces};
    const core::Result<void> cleared = upload.run(kName, [&](VkCommandBuffer commands) {
        const auto barrier = [&](VkImageLayout from, VkImageLayout to, VkPipelineStageFlags2 srcStage,
                                 VkAccessFlags2 srcAccess, VkPipelineStageFlags2 dstStage,
                                 VkAccessFlags2 dstAccess) {
            const VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                                     .srcStageMask = srcStage,
                                                     .srcAccessMask = srcAccess,
                                                     .dstStageMask = dstStage,
                                                     .dstAccessMask = dstAccess,
                                                     .oldLayout = from,
                                                     .newLayout = to,
                                                     .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                     .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                     .image = texture.image.image,
                                                     .subresourceRange = range};
            const VkDependencyInfo dependency{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                              .imageMemoryBarrierCount = 1,
                                              .pImageMemoryBarriers = &imageBarrier};
            vkCmdPipelineBarrier2(commands, &dependency);
        };
        barrier(VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE,
                VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        const VkClearColorValue black{{0.0f, 0.0f, 0.0f, 1.0f}};
        vkCmdClearColorImage(commands, texture.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1,
                             &range);
        barrier(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_PIPELINE_STAGE_2_CLEAR_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT,
                VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    });
    if (!cleared) {
        allocator.destroyImage(texture.image);
        return std::unexpected(cleared.error());
    }
    if (const VkResult created = createView(device, texture.image.image, VK_IMAGE_VIEW_TYPE_CUBE,
                                            kEnvironmentFormat, range, texture.view);
        created != VK_SUCCESS) {
        allocator.destroyImage(texture.image);
        return viewError(kName, created);
    }
    return texture;
}

} // namespace apex::graphics
