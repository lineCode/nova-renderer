#pragma once

#include <unordered_map>

#include "nova_renderer/util/filesystem.hpp"

namespace nova::renderer {
    template <typename ResourceType>
    class ResourceHandle {
    public:
        ResourceHandle(const std::unordered_map<fs::path, ResourceType>& resources, const fs::path& key)
            : resources_map(resources), key(key) {}

        ResourceType* operator->() {
            if(resources_map.find(key) != resources_map.end()) {
                return &resources_map.at(key);
            }

            return nullptr;
        }

    private:
        const std::unordered_map<fs::path, ResourceType>& resources_map;
        const fs::path& key;
    };
} // namespace nova::renderer
