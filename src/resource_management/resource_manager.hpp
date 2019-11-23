#pragma once

#include "nova_renderer/util/filesystem.hpp"
#include "nova_renderer/util/result.hpp"

namespace nova::renderer {
    class ImageResource;
    class FolderAccessorBase;

    struct NovaSettings {
        struct ResourceOptions;
    };

    /*!
     * \brief Class that provides access to various resources
     *
     * The resource manager can read resources from the resource search path, which is an ordered list of one or more directories. When you
     * request a resource, the resource manager looks through the directory in the research search path in the order that you added them,
     * retrieving the first resource that matches the requested path
     *
     * Example:
     *
     * Imagine you have a directory structure like this:
     *
     * /
     * /foo
     * /foo/image.png
     * /bar
     * /bar/image.png
     * /bar/photo.png
     *
     * If you register `/foo` as a resource root, then request the resource `image.png`, the resource manager will return whatever data is
     * stored in file `/foo/image.png`
     *
     * However, if you register `/bar` as a resource root, then register `/foo`, and only then do you request the resource, the resource
     * manager will return whatever data is stored in file `/bar/image.png` - `/bar` was registered first, so the resource manager searches
     * that folder first
     */
    class ResourceManager {
    public:
        ResourceManager() = default;

        ResourceManager(ResourceManager&& old) noexcept = default;
        ResourceManager& operator=(ResourceManager&& old) noexcept = default;

        ResourceManager(const ResourceManager& other) = delete;
        ResourceManager& operator=(const ResourceManager& other) = delete;

        ~ResourceManager() = default;

        /*!
         * \brief Adds a directory to the resource search path. If this directory has already been added, this method has no effect
         *
         * \param new_root The directory to add to the resource search path
         */
        void add_resource_root(const fs::path& new_root);

        /*!
         * \brief Removes a specific directory from the resource search path. If the directory is not in the research search path, this
         * method has no effect
         *
         * \param root_to_remove The directory to remove from the resource search path
         */
        void remove_resource_root(const fs::path& root_to_remove);

        /*!
         * \brief Removes all directories from the resource search path
         */
        void clear_research_search_path();

        /*!
         * \brief Loads an image from disk
         *
         * \param resource_path The path from the resource manager root to the image
         */
        [[nodiscard]] ntl::Result<ImageResource> load_image(const fs::path& resource_path) noexcept;

    private:
        std::vector<std::unique_ptr<FolderAccessorBase>> resource_folders;
    };
} // namespace nova::renderer
