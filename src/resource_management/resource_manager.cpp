#include "resource_manager.hpp"

#include "../loading/folder_accessor.hpp"
#include "../loading/loading_utils.hpp"
#include "../loading/regular_folder_accessor.hpp"
#include "../loading/zip_folder_accessor.hpp"

namespace nova::renderer {
    void ResourceManager::add_resource_root(const fs::path& new_root) {
        const auto root_location = std::find_if(resource_folders.begin(),
                                                resource_folders.end(),
                                                [&](const std::unique_ptr<FolderAccessorBase>& folder) {
                                                    return folder->get_root() == new_root;
                                                });

        const bool root_already_added = root_location != resource_folders.end();
        if(!root_already_added) {
            if(is_zip_folder(new_root)) {
                resource_folders.emplace_back(std::make_unique<ZipFolderAccessor>(new_root));
            } else {
                resource_folders.emplace_back(std::make_unique<RegularFolderAccessor>(new_root));
            }
        }
    }

    void ResourceManager::remove_resource_root(const fs::path& root_to_remove) {
        const auto remove_itr = std::remove_if(resource_folders.begin(),
                                               resource_folders.end(),
                                               [&](const std::unique_ptr<FolderAccessorBase>& folder) {
                                                   return folder->get_root() == root_to_remove;
                                               });
        resource_folders.erase(remove_itr);
    }

    void ResourceManager::clear_research_search_path() { resource_folders.clear(); }

    ImageData ResourceManager::load_image(const fs::path& resource_path) noexcept {
        for(const std::unique_ptr<FolderAccessorBase>& root : resource_folders) {
            if(root->does_resource_exist(resource_path)) {
                const std::vector<uint8_t> image_data = root->read_file(resource_path);
            }
        }

        // TODO
        return {};
    }
} // namespace nova::renderer
