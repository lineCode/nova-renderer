#pragma once

#include <stdint.h> // needed for uint****

#include "nova_renderer/rhi/forward_decls.hpp"
#include "nova_renderer/rhi/rhi_enums.hpp"
#include "nova_renderer/rhi/rhi_types.hpp"

namespace nova::renderer::rhi {
    enum class IndexType {
        Uint16,
        Uint32,
    };

    /*!
     * \brief An API-agnostic command list
     *
     * Command lists are allocated from the render engine. Once allocated, ownership is passed to the callee. You can
     * then record whatever commands you want and submit the command list back to the render engine for execution on
     * the GPU. Once submitted, you may not record any more commands into the command list
     *
     * There is one command list pool per swapchain image per thread. All the pools for one swapchain image are
     * reset at the beginning of a frame that renders to that swapchain image. This means that any command list
     * allocated in one frame will not be valid in the next frame. DO NOT hold on to command lists
     *
     * A command list may only be recorded to from one thread at a time
     *
     * Command lists are fully bound to ChaiScript
     */
    class CommandList {
    public:
        enum class Level {
            Primary,
            Secondary,
        };

        CommandList() = default;

        CommandList(CommandList&& old) noexcept = default;
        CommandList& operator=(CommandList&& old) noexcept = default;

        CommandList(const CommandList& other) = delete;
        CommandList& operator=(const CommandList& other) = delete;

        /*!
         * \brief Sets the debug name of this command list, so that API debugging tools can give you a nice name
         */
        virtual void set_debug_name(const rx::string& name) = 0;

        /*!
         * \brief Inserts a barrier so that all access to a resource before the barrier is resolved before any access
         * to the resource after the barrier
         *
         * \param stages_before_barrier The pipeline stages that should be completed before the barriers take effect
         * \param stages_after_barrier The pipeline stages that must wait for the barrier
         * \param barriers All the resource barriers to use
         */
        virtual void resource_barriers(PipelineStage stages_before_barrier,
                                       PipelineStage stages_after_barrier,
                                       const rx::vector<RhiResourceBarrier>& barriers) = 0;

        /*!
         * \brief Records a command to copy one region of a buffer to another buffer
         *
         * \param destination_buffer The buffer to write data to
         * \param destination_offset The offset in the destination buffer to start writing to. Measured in bytes
         * \param source_buffer The buffer to read data from
         * \param source_offset The offset in the source buffer to start reading from. Measures in bytes
         * \param num_bytes The number of bytes to copy
         *
         * \pre destination_buffer != nullptr
         * \pre destination_buffer is a buffer
         * \pre destination_offset is less than the size of destination_buffer
         * \pre source_buffer != nullptr
         * \pre source_buffer is a buffer
         * \pre source_offset is less than the size of source_buffer
         * \pre destination_offset plus num_bytes is less than the size of destination_buffer
         * \pre destination_offset plus num_bytes is less than the size of source_buffer
         */
        virtual void copy_buffer(RhiBuffer* destination_buffer,
                                 mem::Bytes destination_offset,
                                 RhiBuffer* source_buffer,
                                 mem::Bytes source_offset,
                                 mem::Bytes num_bytes) = 0;

        /*!
         * \brief Uploads data to an image in the most API-optimal way
         *
         * \param image The image to upload the data to. Must be in the CopyDestination state
         * \param width The width of the image in pixels
         * \param height The height of the image in pixels
         * \param bytes_per_pixel The number of bytes that each pixel uses
         * \param staging_buffer The buffer to use to upload the data to the image. This buffer must be host writable, and must be in the
         * CopySource state
         * \param data A pointer to the data to upload to the image
         *
         * \note The image must be in the Common layout prior to uploading data to it
         */
        virtual void upload_data_to_image(
            RhiImage* image, size_t width, size_t height, size_t bytes_per_pixel, RhiBuffer* staging_buffer, const void* data) = 0;

        /*!
         * \brief Executed a number of command lists
         *
         * These command lists should be secondary command lists. Nova doesn't validate this because yolo but you need
         * to be nice - the API-specific validation layers _will_ yell at you
         */
        virtual void execute_command_lists(const rx::vector<CommandList*>& lists) = 0;

        /*!
         * \brief Begins a renderpass
         *
         * \param renderpass The renderpass to begin
         * \param framebuffer The framebuffer to render to
         */
        virtual void begin_renderpass(RhiRenderpass* renderpass, RhiFramebuffer* framebuffer) = 0;

        virtual void end_renderpass() = 0;

        virtual void bind_pipeline(const RhiPipeline* pipeline) = 0;

        virtual void bind_descriptor_sets(const rx::vector<RhiDescriptorSet*>& descriptor_sets,
                                          const RhiPipelineInterface* pipeline_interface) = 0;

        /*!
         * \brief Binds the provided vertex buffers to the command list
         *
         * The buffers are always bound sequentially starting from binding 0. The first buffer in the vector is bound to binding 0, the
         * second is bound to binding 1, etc
         *
         * \param buffers The buffers to bind
         */
        virtual void bind_vertex_buffers(const rx::vector<RhiBuffer*>& buffers) = 0;

        /*!
         * \brief Binds the provided index buffer to the command list
         *
         * The index buffer must use 32-bit indices. This will likely change in the future but for now it's a thing
         */
        virtual void bind_index_buffer(const RhiBuffer* buffer, IndexType index_size) = 0;

        /*!
         * \brief Records rendering instances of an indexed mesh
         *
         * \param num_indices The number of indices to read from the current index buffer
         * \param offset The offset from the beginning of the index buffer to begin reading vertex indices
         * \param num_instances The number of instances to render
         */
        virtual void draw_indexed_mesh(uint32_t num_indices, uint32_t offset = 0, uint32_t num_instances = 1) = 0;

        virtual void set_scissor_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

        virtual ~CommandList() = default;
    };
} // namespace nova::renderer::rhi
