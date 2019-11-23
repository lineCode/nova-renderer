#pragma once

#include "folder_accessor.hpp"

namespace nova::renderer {
    /*!
     * \brief Allows access to resources in a regular folder
     */
    class RegularFolderAccessor final : public FolderAccessorBase {
    public:
        explicit RegularFolderAccessor(const fs::path& folder);

        std::vector<uint8_t> read_file(const fs::path& resource_path) override;

        std::vector<fs::path> get_all_items_in_folder(const fs::path& folder) override;

    protected:
        bool does_resource_exist_on_filesystem(const fs::path& resource_path) override;
    };
} // namespace nova::renderer
