#pragma once

#include <miniz.h>

#include "folder_accessor.hpp"
#include <memory>

namespace nova::renderer {
    struct FileTreeNode {
        std::string name;
        std::vector<std::unique_ptr<FileTreeNode>> children;
        FileTreeNode* parent = nullptr;

        [[nodiscard]] std::string get_full_path() const;
    };

    /*!
     * \brief Allows access to a zip folder
     */
    class ZipFolderAccessor final : public FolderAccessorBase {
    public:
        explicit ZipFolderAccessor(const fs::path& folder);

        ZipFolderAccessor(ZipFolderAccessor&& other) noexcept = default;
        ZipFolderAccessor& operator=(ZipFolderAccessor&& other) noexcept = default;

        ZipFolderAccessor(const ZipFolderAccessor& other) = delete;
        ZipFolderAccessor& operator=(const ZipFolderAccessor& other) = delete;

        ~ZipFolderAccessor() override;

        std::vector<uint8_t> read_file(const fs::path& resource_path) override;

        std::vector<fs::path> get_all_items_in_folder(const fs::path& folder) override;

    private:
        /*!
         * \brief Map from filename to its index in the zip folder. Miniz seems to like indexes
         */
        std::unordered_map<std::string, uint32_t> resource_indexes;

        mz_zip_archive zip_archive = {};

        std::unique_ptr<FileTreeNode> files = nullptr;

        void delete_file_tree(std::unique_ptr<FileTreeNode>& node);

        void build_file_tree();

        bool does_resource_exist_on_filesystem(const fs::path& resource_path) override;
    };

    /*!
     * \brief Prints out the nodes in a depth-first fashion
     */
    void print_file_tree(const std::unique_ptr<FileTreeNode>& folder, uint32_t depth);
} // namespace nova::renderer
