#include "loading_utils.hpp"

namespace nova::renderer {
    bool is_zip_folder(const fs::path& path_to_folder) {
        if(path_to_folder.has_extension()) {
            const auto extension = path_to_folder.extension();
            return extension == ".zip";

        } else {
            return false;
        }
    }
} // namespace nova::renderer
