/*!
 * \brief Holds all the structs that correspond to the data in a shaderpack
 *
 * \author ddubois
 * \date 23-Aug-18.
 */

#ifndef NOVA_RENDERER_SHADERPACK_DATA_HPP
#define NOVA_RENDERER_SHADERPACK_DATA_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <vulkan/vulkan.h>

#include "util/filesystem.hpp"
#include "util/utils.hpp"

namespace nova::renderer::shaderpack {
    /*!
     * \brief Controls the rasterizer's state
     */
    enum class StateEnum {
        /*!
         * \brief Enable blending for this material state
         */
        Blending,

        /*!
         * \brief Render backfaces and cull frontfaces
         */
        InvertCulling,

        /*!
         * \brief Don't cull backfaces or frontfaces
         */
        DisableCulling,

        /*!
         * \brief Don't write to the depth buffer
         */
        DisableDepthWrite,

        /*!
         * \brief Don't perform a depth test
         */
        DisableDepthTest,

        /*!
         * \brief Perform the stencil test
         */
        EnableStencilTest,

        /*!
         * \brief Write to the stencil buffer
         */
        StencilWrite,

        /*!
         * \brief Don't write to the color buffer
         */
        DisableColorWrite,

        /*!
         * \brief Enable alpha to coverage
         */
        EnableAlphaToCoverage,

        /*!
         * \brief Don't write alpha
         */
        DisableAlphaWrite,
    };

    enum class TextureFilterEnum { TexelAA, Bilinear, Point };

    enum class WrapModeEnum { Repeat, Clamp };

    /*!
     * \brief The kind of data in a vertex attribute
     */
    enum class VertexFieldEnum {
        /*!
         * \brief The vertex position
         *
         * 12 bytes
         */
        Position,

        /*!
         * \brief The vertex color
         *
         * 4 bytes
         */
        Color,

        /*!
         * \brief The UV coordinate of this object
         *
         * Except not really, because Nova's virtual textures means that the UVs for a block or entity or whatever
         * could change on the fly, so this is kinda more of a preprocessor define that replaces the UV with a lookup
         * in the UV table
         *
         * 8 bytes (might try 4)
         */
        UV0,

        /*!
         * \brief The UV coordinate in the lightmap texture
         *
         * This is a real UV and it doesn't change for no good reason
         *
         * 2 bytes
         */
        UV1,

        /*!
         * \brief Vertex normal
         *
         * 12 bytes
         */
        Normal,

        /*!
         * \brief Vertex tangents
         *
         * 12 bytes
         */
        Tangent,

        /*!
         * \brief The texture coordinate of the middle of the quad
         *
         * 8 bytes
         */
        MidTexCoord,

        /*!
         * \brief A uint32_t that's a unique identifier for the texture that this vertex uses
         *
         * This is generated at runtime by Nova, so it may change a lot depending on what resourcepacks are loaded and
         * if they use CTM or random detail textures or whatever
         *
         * 4 bytes
         */
        VirtualTextureId,

        /*!
         * \brief Some information about the current block/entity/whatever
         *
         * 12 bytes
         */
        McEntityId,
    };

    /*!
     * \brief Where the texture comes from
     */
    enum class TextureLocationEnum {
        /*!
         * \brief The texture is written to by a shader
         */
        Dynamic,

        /*!
         * \brief The texture is loaded from the textures/ folder in the current shaderpack
         */
        InUserPackage,

        /*!
         * \brief The texture is provided by Nova or by Minecraft
         */
        InAppPackage
    };

    enum class MsaaSupportEnum { MSAA, Both, None };

    enum class StencilOpEnum { Keep, Zero, Replace, Incr, IncrWrap, Decr, DecrWrap, Invert };

    enum class CompareOpEnum { Never, Less, LessEqual, Greater, GreaterEqual, Equal, NotEqual, Always };

    enum class PrimitiveTopologyEnum { Triangles, Lines };

    enum class BlendFactorEnum {
        One,
        Zero,
        SrcColor,
        DstColor,
        OneMinusSrcColor,
        OneMinusDstColor,
        SrcAlpha,
        DstAlpha,
        OneMinusSrcAlpha,
        OneMinusDstAlpha
    };

    enum class RenderQueueEnum { Transparent, Opaque, Cutout };

    enum class PixelFormatEnum {
        /*
         * \brief Four-component format where each component is an 8-bit unsigned integer. Suitable for non-depth data
         */
        RGBA8,

        /*!
         * \brief Four-component format where each component is a 16-bit signed float. Suitable for non-depth data
         */
        RGBA16F,

        /*!
         * \brief Four-component format where each component is a 32-bit signed float. Suitable for non-depth data
         */
        RGBA32F,

        /*!
         * \brief One-component format where the one component is an unsigned 32-bit integer. Suitable for non-depth data
         */
        U32,

        /*!
         * \brief 32-bit depth
         */
        Depth,

        /*!
         * \brief 24-bit depth, 8-bit stencil
         */
        DepthStencil,
    };

    enum class TextureDimensionTypeEnum { ScreenRelative, Absolute };

    /*!
     * \brief Defines a sampler to use for a texture
     *
     * At the time of writing I'm not sure how this is corellated with a texture, but all well
     */
    struct SamplerCreateInfo {
        std::string name;

        /*!
         * \brief What kind of texture filter to use
         *
         * texel_aa does something that I don't want to figure out right now. Bilinear is your regular bilinear filter,
         * and point is the point filter. Aniso isn't an option and I kinda hope it stays that way
         */
        TextureFilterEnum filter{};

        /*!
         * \brief How the texture should wrap at the edges
         */
        WrapModeEnum wrap_mode{};
    };

    struct StencilOpState {
        StencilOpEnum fail_op;
        StencilOpEnum pass_op;
        StencilOpEnum depth_fail_op;
        CompareOpEnum compare_op;
        uint32_t compare_mask;
        uint32_t write_mask;
    };

    struct ShaderSource {
        fs::path filename;
        std::vector<uint32_t> source;
    };

    struct VertexFieldData {
        std::string semantic_name;
        VertexFieldEnum field{};
    };

    /*!
     * \brief All the data that Nova uses to build a pipeline
     */
    struct PipelineCreateInfo {
        /*!
         * \brief The name of this pipeline
         */
        std::string name;

        /*!
         * \brief The pipeline that this pipeline inherits from
         */
        std::optional<std::string> parent_name;

        /*!
         * \brief The name of the pass that this pipeline belongs to
         */
        std::string pass;

        /*!
         * \brief All of the symbols in the shader that are defined by this state
         */
        std::vector<std::string> defines{};

        /*!
         * \brief Defines the rasterizer state that's active for this pipeline
         */
        std::vector<StateEnum> states{};

        /*!
         * \brief Sets up the vertex fields that Nova will bind to this pipeline
         *
         * The index in the array is the attribute index that the vertex field is bound to
         */
        std::vector<VertexFieldData> vertex_fields{};

        /*!
         * \brief The stencil buffer operations to perform on the front faces
         */
        std::optional<StencilOpState> front_face;

        /*!
         * \brief The stencil buffer operations to perform on the back faces
         */
        std::optional<StencilOpState> back_face;

        /*!
         * \brief The material to use if this one's shaders can't be found
         */
        std::optional<std::string> fallback;

        /*!
         * \brief A bias to apply to the depth
         */
        float depth_bias{};

        /*!
         * \brief The depth bias, scaled by slope I guess?
         */
        float slope_scaled_depth_bias{};

        /*!
         * \brief The reference value to use for the stencil test
         */
        uint32_t stencil_ref{};

        /*!
         * \brief The mask to use when reading from the stencil buffer
         */
        uint32_t stencil_read_mask{};

        /*!
         * \brief The mask to use when writing to the stencil buffer
         */
        uint32_t stencil_write_mask{};

        /*!
         * \brief How to handle MSAA for this state
         */
        MsaaSupportEnum msaa_support{};

        /*!
         * \brief
         */
        PrimitiveTopologyEnum primitive_mode{};

        /*!
         * \brief Where to get the blending factor for the soource
         */
        BlendFactorEnum source_blend_factor{};

        /*!
         * \brief Where to get the blending factor for the destination
         */
        BlendFactorEnum destination_blend_factor{};

        /*!
         * \brief How to get the source alpha in a blend
         */
        BlendFactorEnum alpha_src{};

        /*!
         * \brief How to get the destination alpha in a blend
         */
        BlendFactorEnum alpha_dst{};

        /*!
         * \brief The function to use for the depth test
         */
        CompareOpEnum depth_func{};

        /*!
         * \brief The render queue that this pass belongs to
         *
         * This may or may not be removed depending on what is actually needed by Nova
         */
        RenderQueueEnum render_queue{};

        ShaderSource vertex_shader{};

        std::optional<ShaderSource> geometry_shader;
        std::optional<ShaderSource> tessellation_control_shader;
        std::optional<ShaderSource> tessellation_evaluation_shader;
        std::optional<ShaderSource> fragment_shader;

        /*!
         * \brief Merges this pipeline with the parent, returning the merged pipeline
         */
        [[nodiscard]] PipelineCreateInfo merge_with_parent(const PipelineCreateInfo& parent_pipeline) const;
    };

    struct TextureFormat {
        /*!
         * \brief The format of the texture
         */
        PixelFormatEnum pixel_format;

        /*!
         * \brief How to interpret the dimensions of this texture
         */
        TextureDimensionTypeEnum dimension_type;

        /*!
         * \brief The width, in pixels, of the texture
         */
        float width;
        /*!
         * \brief The height, in pixels, of the texture
         */
        float height;

        [[nodiscard]] glm::uvec2 get_size_in_pixels(const glm::uvec2& screen_size) const;

        bool operator==(const TextureFormat& other) const;
        bool operator!=(const TextureFormat& other) const;
    };

    /*!
     * \brief A texture that a pass can use
     */
    struct TextureCreateInfo {
        /*!
         * \brief The name of the texture
         *
         * Nova implicitly defines a few textures for you to use:
         * - ColorVirtualTexture
         *      - Virtual texture atlas that holds color textures
         *      - Textures which have the exact name as requested by Minecraft are in this atlas
         *      - Things without a color texture get a pure white texture
         *      - Always has a format of R8G8B8A8
         *      - Can only be used as a pass's input
         * - NormalVirtualTexture
         *      - Virtual texture atlas that holds normal textures
         *      - Textures which have `_n` after the name requested by Minecraft are in this atlas
         *      - If no normal texture exists for a given object, a texture with RGBA of (0, 0, 1, 1) is used
         *      - Always has a format of R8G8B8A8
         *      - Can only be used as a pass's input
         * - DataVirtualTexture
         *      - Virtual texture atlas that holds data textures
         *      - Textures which have a `_s` after the name requested by Minecraft are in this atlas
         *      - If no data texture exists for a given object, a texture with an RGBA of (0, 0, 0, 0) is used
         *      - Always has a format of R8G8B8A8
         *      - Can only be used as a pass's input
         * - Lightmap
         *      - Lightmap, loaded from the current resourcepack
         *      - Format of RGB8
         *      - Can only be used as an input
         * - Backbuffer
         *      - The texture that gets presented to the screen
         *      - Always has a format of RGB8
         *      - Can only be used as a pass's output
         *
         * If you use one of the virtual textures, then all fields except the binding are ignored
         * If you use `Backbuffer`, then all fields are ignored since the backbuffer is always bound to output location 0
         */
        std::string name;

        TextureFormat format{};
    };

    struct ShaderpackResourcesData {
        std::vector<TextureCreateInfo> textures;
        std::vector<SamplerCreateInfo> samplers;
    };

    /*!
     * \brief A description of a texture that a render pass outputs to
     */
    struct TextureAttachmentInfo {
        /*!
         * \brief The name of the texture
         */
        std::string name{};

        PixelFormatEnum pixel_format;

        /*!
         * \brief Whether to clear it
         *
         * If the texture is a depth buffer, it gets cleared to 1
         * If the texture is a stencil buffer, it gets cleared to 0xFFFFFFFF
         * If the texture is a color buffer, it gets cleared to (0, 0, 0, 0)
         */
        bool clear = false;

        bool operator==(const TextureAttachmentInfo& other) const;
    };

    /*!
     * \brief A pass over the scene
     *
     * A pass has a few things:
     * - What passes MUST be executed before this one
     * - What inputs this pass's shaders have
     *      - What uniform buffers to use
     *      - What vertex data to use
     *      - Any textures that are needed
     * - What outputs this pass has
     *      - Framebuffer attachments
     *      - Write buffers
     *
     * The inputs and outputs of a pass must be resources declared in the shaderpack's `resources.json` file (or the
     * default resources.json), or a resource that's internal to Nova. For example, Nova provides a UBO of uniforms that
     * change per frame, a UBO for per-model data like the model matrix, and the virtual texture atlases. The default
     * resources.json file sets up sixteen framebuffer color attachments for ping-pong buffers, a depth attachment,
     * some shadow maps, etc
     */
    struct RenderPassCreateInfo {
        /*!
         * \brief The name of this render pass
         */
        std::string name;

        /*!
         * \brief The materials that MUST execute before this one
         */
        std::vector<std::string> dependencies{};

        /*!
         * \brief The textures that this pass will read from
         */
        std::vector<std::string> texture_inputs{};
        /*!
         * \brief The textures that this pass will write to
         */
        std::vector<TextureAttachmentInfo> texture_outputs{};

        /*!
         * \brief The depth texture this pass will write to
         */
        std::optional<TextureAttachmentInfo> depth_texture;

        /*!
         * \brief All the buffers that this renderpass reads from
         */
        std::vector<std::string> input_buffers{};

        /*!
         * \brief All the buffers that this renderpass writes to
         */
        std::vector<std::string> output_buffers{};

        RenderPassCreateInfo() = default;
    };

    struct MaterialPass {
        std::string name;
        std::string material_name;
        std::string pipeline;
        // Ugh why is this constructor explicit
        std::unordered_map<std::string, std::string> bindings = std::unordered_map<std::string, std::string>();

        /*!
         * \brief All the descriptor sets needed to bind everything used by this material to its pipeline
         *
         * All the material's resources get bound to its descriptor sets when the material is created. Updating
         * descriptor sets is allowed, although the result won't show up on screen for a couple frames because Nova
         * (will) copies its descriptor sets to each in-flight frame
         */
        std::vector<VkDescriptorSet> descriptor_sets;

        VkPipelineLayout layout = VK_NULL_HANDLE;
    };

    struct MaterialData {
        std::string name;
        std::vector<MaterialPass> passes;
        std::string geometry_filter;
    };

    /*!
     * \brief All the data that can be in a shaderpack
     */
    struct ShaderpackData {
        std::vector<PipelineCreateInfo> pipelines;

        /*!
         * \brief All the renderpasses that this shaderpack needs, in submission order
         */
        std::vector<RenderPassCreateInfo> passes;

        std::vector<MaterialData> materials;

        ShaderpackResourcesData resources;
    };

    // TODO: Wrap these in to_json/from_json thingies

    [[nodiscard]] PixelFormatEnum pixel_format_enum_from_string(const std::string& str);
    [[nodiscard]] TextureDimensionTypeEnum texture_dimension_type_enum_from_string(const std::string& str);
    [[nodiscard]] TextureFilterEnum texture_filter_enum_from_string(const std::string& str);
    [[nodiscard]] WrapModeEnum wrap_mode_enum_from_string(const std::string& str);
    [[nodiscard]] StencilOpEnum stencil_op_enum_from_string(const std::string& str);
    [[nodiscard]] CompareOpEnum compare_op_enum_from_string(const std::string& str);
    [[nodiscard]] MsaaSupportEnum msaa_support_enum_from_string(const std::string& str);
    [[nodiscard]] PrimitiveTopologyEnum primitive_topology_enum_from_string(const std::string& str);
    [[nodiscard]] BlendFactorEnum blend_factor_enum_from_string(const std::string& str);
    [[nodiscard]] RenderQueueEnum render_queue_enum_from_string(const std::string& str);
    [[nodiscard]] StateEnum state_enum_from_string(const std::string& str);
    [[nodiscard]] VertexFieldEnum vertex_field_enum_from_string(const std::string& str);

    [[nodiscard]] std::string to_string(PixelFormatEnum val);
    [[nodiscard]] std::string to_string(TextureDimensionTypeEnum val);
    [[nodiscard]] std::string to_string(TextureFilterEnum val);
    [[nodiscard]] std::string to_string(WrapModeEnum val);
    [[nodiscard]] std::string to_string(StencilOpEnum val);
    [[nodiscard]] std::string to_string(CompareOpEnum val);
    [[nodiscard]] std::string to_string(MsaaSupportEnum val);
    [[nodiscard]] std::string to_string(PrimitiveTopologyEnum val);
    [[nodiscard]] std::string to_string(BlendFactorEnum val);
    [[nodiscard]] std::string to_string(RenderQueueEnum val);
    [[nodiscard]] std::string to_string(StateEnum val);
    [[nodiscard]] std::string to_string(VertexFieldEnum val);

    [[nodiscard]] uint32_t pixel_format_to_pixel_width(PixelFormatEnum format);
} // namespace nova::renderer::shaderpack

#endif // NOVA_RENDERER_SHADERPACK_DATA_HPP
