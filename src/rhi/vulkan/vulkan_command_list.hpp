#pragma once

#include <vulkan/vulkan.h>

#include "nova_renderer/rhi/command_list.hpp"
#include "nova_renderer/rhi/rhi_enums.hpp"
#include "nova_renderer/rhi/rhi_types.hpp"

namespace nova::renderer::rhi {
    class VulkanRenderDevice;

    /*!
     * \brief Vulkan implementation of `command_list`
     */
    class VulkanCommandList final : public CommandList {
    public:
        VkCommandBuffer cmds;

        VulkanCommandList(VkCommandBuffer cmds, const VulkanRenderDevice* render_device);

        ~VulkanCommandList() override = default;

        void set_debug_name(const rx::string& name) override;

        void resource_barriers(PipelineStage stages_before_barrier,
                               PipelineStage stages_after_barrier,
                               const rx::vector<RhiResourceBarrier>& barriers) override;

        void copy_buffer(RhiBuffer* destination_buffer,
                         mem::Bytes destination_offset,
                         RhiBuffer* source_buffer,
                         mem::Bytes source_offset,
                         mem::Bytes num_bytes) override;

        void execute_command_lists(const rx::vector<CommandList*>& lists) override;

        void begin_renderpass(RhiRenderpass* renderpass, RhiFramebuffer* framebuffer) override;

        void end_renderpass() override;

        void bind_pipeline(const RhiPipeline* pipeline) override;

        void bind_descriptor_sets(const rx::vector<RhiDescriptorSet*>& descriptor_sets, const RhiPipelineInterface* pipeline_interface) override;

        void bind_vertex_buffers(const rx::vector<RhiBuffer*>& buffers) override;

        void bind_index_buffer(const RhiBuffer* buffer, IndexType index_type) override;

        void draw_indexed_mesh(uint32_t num_indices, uint32_t offset, uint32_t num_instances) override;

        void set_scissor_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;

        void upload_data_to_image(
            RhiImage* image, size_t width, size_t height, size_t bytes_per_pixel, RhiBuffer* staging_buffer, const void* data) override;

    private:
        const VulkanRenderDevice& render_device;
    };
} // namespace nova::renderer::rhi
