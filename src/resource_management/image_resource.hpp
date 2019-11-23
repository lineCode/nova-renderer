#pragma once

#include "nova_renderer/shaderpack_data.hpp"

namespace nova::renderer {
    /*!
     * \brief Data for an image that's been loaded form disk
     */
    struct ImageData {
        /*!
         * \brief Width of the image, in pixels
         */
        uint64_t width = 1;

        /*!
         * \brief Height of the image, in pixels
         */
        uint64_t height = 1;

        /*!
         * \brief The format of the pixels in this texture
         */
        shaderpack::PixelFormatEnum pixel_format = shaderpack::PixelFormatEnum::RGBA8;

        /*!
         * \brief Pointers to the data for each mip level
         *
         * A texture on disk may or may not have mip levels, so this array may or may not have data for smaller mips
         * than level 0
         */
        std::vector<void*> mips;
    };
} // namespace nova::renderer
