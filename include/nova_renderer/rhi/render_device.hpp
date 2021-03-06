#pragma once

#include <rx/core/memory/system_allocator.h>

#include "nova_renderer/nova_settings.hpp"
#include "nova_renderer/rhi/command_list.hpp"
#include "nova_renderer/rhi/rhi_types.hpp"
#include "nova_renderer/renderpack_data.hpp"
#include "nova_renderer/util/result.hpp"
#include "nova_renderer/window.hpp"

namespace nova::renderer {
    struct PipelineStateCreateInfo;
    struct DeviceMemoryResource;
} // namespace nova::renderer

namespace nova::renderer::rhi {

    /*!
     * \brief All the GPU architectures that Nova cares about, at whatever granularity is most useful
     */
    enum class DeviceArchitecture {
        Unknown,

        /*!
         * \brief The GPU was made by AMD
         */
        Amd,

        /*!
         * \brief The GPU was made by Nvidia
         */
        Nvidia,

        /*!
         * \brief The GPU was made by Intel
         */
        Intel,
    };

    /*!
     * \brief Information about hte capabilities and limits of the device we're running on
     */
    struct DeviceInfo {
        DeviceArchitecture architecture = DeviceArchitecture::Unknown;

        mem::Bytes max_texture_size = 0;

        bool is_uma = false;

        bool supports_raytracing = false;
        bool supports_mesh_shaders = false;
    };

#define NUM_THREADS 1

    /*!
     * \brief Interface to a logical device which can render to an operating system window
     */
    class RenderDevice {
    public:
        DeviceInfo info;

        NovaSettingsAccessManager& settings;

        RenderDevice(RenderDevice&& other) = delete;
        RenderDevice& operator=(RenderDevice&& other) noexcept = delete;

        RenderDevice(const RenderDevice& other) = delete;
        RenderDevice& operator=(const RenderDevice& other) = delete;

        /*!
         * \brief Needed to make destructor of subclasses called
         */
        virtual ~RenderDevice() = default;

        virtual void set_num_renderpasses(uint32_t num_renderpasses) = 0;

        [[nodiscard]] virtual ntl::Result<RhiDeviceMemory*> allocate_device_memory(mem::Bytes size,
                                                                                MemoryUsage type,
                                                                                ObjectType allowed_objects,
                                                                                rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Creates a renderpass from the provided data
         *
         * Renderpasses are created 100% upfront, meaning that the caller can't change anything about a renderpass
         * after it's been created
         *
         * \param data The data to create a renderpass from
         * \param framebuffer_size The size in pixels of the framebuffer that the renderpass will write to
         * \param allocator The allocator to allocate the renderpass from
         *
         * \return The newly created renderpass
         */
        [[nodiscard]] virtual ntl::Result<RhiRenderpass*> create_renderpass(const renderpack::RenderPassCreateInfo& data,
                                                                         const glm::uvec2& framebuffer_size,
                                                                         rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual RhiFramebuffer* create_framebuffer(const RhiRenderpass* renderpass,
                                                              const rx::vector<RhiImage*>& color_attachments,
                                                              const rx::optional<RhiImage*> depth_attachment,
                                                              const glm::uvec2& framebuffer_size,
                                                              rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual ntl::Result<RhiPipelineInterface*> create_pipeline_interface(
            const rx::map<rx::string, RhiResourceBindingDescription>& bindings,
            const rx::vector<renderpack::TextureAttachmentInfo>& color_attachments,
            const rx::optional<renderpack::TextureAttachmentInfo>& depth_texture,
            rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual RhiDescriptorPool* create_descriptor_pool(const rx::map<DescriptorType, uint32_t>& descriptor_capacity,
                                                                     rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual rx::vector<RhiDescriptorSet*> create_descriptor_sets(const RhiPipelineInterface* pipeline_interface,
                                                                                RhiDescriptorPool* pool,
                                                                                rx::memory::allocator* allocator) = 0;

        virtual void update_descriptor_sets(rx::vector<RhiDescriptorSetWrite>& writes) = 0;

        virtual void reset_descriptor_pool(RhiDescriptorPool* pool) = 0;

        [[nodiscard]] virtual ntl::Result<RhiPipeline*> create_pipeline(RhiPipelineInterface* pipeline_interface,
                                                                     const PipelineStateCreateInfo& data,
                                                                     rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Creates a buffer with undefined contents
         */
        [[nodiscard]] virtual RhiBuffer* create_buffer(const RhiBufferCreateInfo& info,
                                                    DeviceMemoryResource& memory,
                                                    rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Writes data to a buffer
         *
         * This method always writes the data from byte 0 to byte num_bytes. It does not let you use an offset for either reading from
         * the data or writing to the buffer
         *
         * The CPU must be able to write directly to the buffer for this method to work. If the buffer is device local, this method will
         * fail in a horrible way
         *
         * \param data The data to upload
         * \param num_bytes The number of bytes to write
         * \param offset The offset from the start of the buffer to write the data at
         * \param buffer The buffer to write to
         */
        virtual void write_data_to_buffer(const void* data, mem::Bytes num_bytes, mem::Bytes offset, const RhiBuffer* buffer) = 0;

        /*!
         * \brief Creates a new Sampler object
         */
        [[nodiscard]] virtual RhiSampler* create_sampler(const RhiSamplerCreateInfo& create_info, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Creates an empty image
         *
         * The image will start out in the Undefined layout. You must transition it to whatever layout you want to use
         */
        [[nodiscard]] virtual RhiImage* create_image(const renderpack::TextureCreateInfo& info,
                                                  rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual RhiSemaphore* create_semaphore(rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual rx::vector<RhiSemaphore*> create_semaphores(uint32_t num_semaphores,
                                                                       rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual RhiFence* create_fence(bool signaled, rx::memory::allocator* allocator) = 0;

        [[nodiscard]] virtual rx::vector<RhiFence*> create_fences(uint32_t num_fences,
                                                               bool signaled,
                                                               rx::memory::allocator* allocator) = 0;

        /*!
         * \blocks the fence until all fences are signaled
         *
         * Fences are waited on for an infinite time
         *
         * \param fences All the fences to wait for
         */
        virtual void wait_for_fences(rx::vector<RhiFence*> fences) = 0;

        virtual void reset_fences(const rx::vector<RhiFence*>& fences) = 0;

        /*!
         * \brief Clean up any GPU objects a Renderpass may own
         *
         * While Renderpasses are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we still
         * need to clean up their GPU objects
         */
        virtual void destroy_renderpass(RhiRenderpass* pass, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Clean up any GPU objects a Framebuffer may own
         *
         * While Framebuffers are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we still
         * need to clean up their GPU objects
         */
        virtual void destroy_framebuffer(RhiFramebuffer* framebuffer, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Clean up any GPU objects a PipelineInterface may own
         *
         * While PipelineInterfaces are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we
         * still need to clean up their GPU objects
         */
        virtual void destroy_pipeline_interface(RhiPipelineInterface* pipeline_interface, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Clean up any GPU objects a Pipeline may own
         *
         * While Pipelines are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we still
         * need to clean up their GPU objects
         */
        virtual void destroy_pipeline(RhiPipeline* pipeline, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Clean up any GPU objects an Image may own
         *
         * While Images are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we still need
         * to clean up their GPU objects
         */
        virtual void destroy_texture(RhiImage* resource, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Clean up any GPU objects a Semaphores may own
         *
         * While Semaphores are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we still
         * need to clean up their GPU objects
         */
        virtual void destroy_semaphores(rx::vector<RhiSemaphore*>& semaphores, rx::memory::allocator* allocator) = 0;

        /*!
         * \brief Clean up any GPU objects a Fence may own
         *
         * While Fence are per-renderpack objects, and their CPU memory will be cleaned up when a new renderpack is loaded, we still need to
         * clean up their GPU objects
         */
        virtual void destroy_fences(const rx::vector<RhiFence*>& fences, rx::memory::allocator* allocator) = 0;

        [[nodiscard]] Swapchain* get_swapchain() const;

        /*!
         * \brief Allocates a new command list that can be used from the provided thread and has the desired type
         *
         * Ownership of the command list is given to the caller. You can record your commands into it, then submit it
         * to a queue. Submitting it gives ownership back to the render engine, and recording commands into a
         * submitted command list is not supported
         *
         * There is one command list pool per swapchain image per thread. All the pools for one swapchain image are
         * reset at the beginning of a frame that renders to that swapchain image. This means that any command list
         * allocated in one frame will not be valid in the next frame. DO NOT hold on to command lists
         *
         * Command lists allocated by this method are returned ready to record commands into - the caller doesn't need
         * to begin the command list
         */
        virtual CommandList* create_command_list(uint32_t thread_idx,
                                                 QueueType needed_queue_type,
                                                 CommandList::Level level,
                                                 rx::memory::allocator* allocator) = 0;

        virtual void submit_command_list(CommandList* cmds,
                                         QueueType queue,
                                         RhiFence* fence_to_signal = nullptr,
                                         const rx::vector<RhiSemaphore*>& wait_semaphores = {},
                                         const rx::vector<RhiSemaphore*>& signal_semaphores = {}) = 0;

        [[nodiscard]] rx::memory::allocator* get_allocator() const;

    protected:
        rx::memory::allocator* internal_allocator;

        NovaWindow& window;

        glm::uvec2 swapchain_size = {};
        Swapchain* swapchain = nullptr;

        /*!
         * \brief Initializes the engine
         * \param settings The settings passed to nova
         * \param window The OS window that we'll be rendering to
         * \param allocator The allocator to use for internal memory
         *
         * Intentionally does nothing. This constructor serves mostly to ensure that concrete render engines have a
         * constructor that takes in some settings
         *
         * \attention Called by the various render engine implementations
         */
        RenderDevice(NovaSettingsAccessManager& settings,
                     NovaWindow& window,
                     rx::memory::allocator* allocator = &rx::memory::g_system_allocator);

        template <typename ObjectType, typename... Args>
        ObjectType* allocate_object(rx::memory::allocator* local_allocator, Args... args);
    };

    template <typename ObjectType, typename... Args>
    ObjectType* RenderDevice::allocate_object(rx::memory::allocator* local_allocator, Args... args) {
        return [&] {
            if(local_allocator != nullptr) {
                return local_allocator->create<ObjectType>(rx::utility::forward<Args>(args)...);
            } else {
                return internal_allocator->create<ObjectType>(rx::utility::forward<Args>(args)...);
            }
        }();
    }
} // namespace nova::renderer::rhi
