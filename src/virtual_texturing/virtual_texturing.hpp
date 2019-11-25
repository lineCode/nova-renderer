#pragma once
#include "nova_renderer/shaderpack_data.hpp"

namespace nova::renderer {
    /*!
     * \brief Retrieves the render pass definition for the virtual texture ID pass
     *
     * The virtual texture ID pass 
     */
    shaderpack::RenderPassCreateInfo& get_virtual_texture_id_pass_definition();

    /*!
     * \brief Retrieves the material definition for the material that the virtual texture ID pass will use
     */
    shaderpack::MaterialData& get_virtual_texture_material_definition();

}
