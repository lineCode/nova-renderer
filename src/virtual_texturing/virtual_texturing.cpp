#include "virtual_texturing.hpp"

namespace nova::renderer {
    shaderpack::RenderPassCreateInfo& get_virtual_texture_id_pass_definition() {
        static shaderpack::RenderPassCreateInfo pass{"NovaVirtualTextureId",
                                                     {},
                                                     {},
                                                     {shaderpack::TextureAttachmentInfo{"NovaVirtualTextureId",
                                                                                        shaderpack::PixelFormatEnum::U32,
                                                                                        true}},
                                                     std::make_optional<shaderpack::TextureAttachmentInfo>(
                                                         {"NovaVirtualTextureDepth", shaderpack::PixelFormatEnum::Depth, false}),
                                                     {},
                                                     {}};

        return pass;
    }

    shaderpack::MaterialData& get_virtual_texture_material_definition() {
        static shaderpack::MaterialData
            mat{"NovaVirtualTextureIdMat",
                {{"NovaVirtualTextureId", "NovaVirtualTextureIdMat", "NovaVirtualTextureIdPipeline", {}, {}, {}}},
                "everything"};

        return mat;
    }
} // namespace nova::renderer
