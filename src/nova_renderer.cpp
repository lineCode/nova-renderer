#include "nova_renderer/nova_renderer.hpp"

#include <array>
#include <future>

#pragma warning(push, 0)
#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glslang/MachineIndependent/Initialize.h>
#include <minitrace.h>
#include <spirv_glsl.hpp>
#pragma warning(pop)

#include "nova_renderer/constants.hpp"
#include "nova_renderer/frontend/procedural_mesh.hpp"
#include "nova_renderer/frontend/rendergraph.hpp"
#include "nova_renderer/frontend/ui_renderer.hpp"
#include "nova_renderer/loading/shaderpack_loading.hpp"
#include "nova_renderer/memory/block_allocation_strategy.hpp"
#include "nova_renderer/memory/bump_point_allocation_strategy.hpp"
#include "nova_renderer/rhi/command_list.hpp"
#include "nova_renderer/rhi/swapchain.hpp"
#include "nova_renderer/util/logger.hpp"
#include "nova_renderer/util/platform.hpp"

#include "debugging/renderdoc.hpp"
#include "filesystem/shaderpack/render_graph_builder.hpp"
#include "render_objects/uniform_structs.hpp"

// D3D12 MUST be included first because the Vulkan include undefines FAR, yet the D3D12 headers need FAR
// Windows considered harmful
#if defined(NOVA_WINDOWS) && defined(NOVA_D3D12_RHI)
#include "render_engine/dx12/dx12_render_engine.hpp"
#endif

#if defined(NOVA_VULKAN_RHI)
#include "render_engine/vulkan/vulkan_render_engine.hpp"
#endif
#if defined(NOVA_OPENGL_RHI)
#include "render_engine/gl3/gl3_render_engine.hpp"
#endif

using namespace nova::mem;
using namespace operators;
using namespace fmt;

const Bytes global_memory_pool_size = 1_gb;

namespace nova::renderer {
    std::unique_ptr<NovaRenderer> NovaRenderer::instance;

    bool FullMaterialPassName::operator==(const FullMaterialPassName& other) const {
        return material_name == other.material_name && pass_name == other.pass_name;
    }

    std::size_t FullMaterialPassNameHasher::operator()(const FullMaterialPassName& name) const {
        const std::size_t material_name_hash = std::hash<std::string>()(name.material_name);
        const std::size_t pass_name_hash = std::hash<std::string>()(name.pass_name);

        return material_name_hash ^ pass_name_hash;
    }

    NovaRenderer::NovaRenderer(const NovaSettings& settings) : render_settings(settings) {
        create_global_allocators();

        mtr_init("trace.json");

        MTR_META_PROCESS_NAME("NovaRenderer");
        MTR_META_THREAD_NAME("Main");

        MTR_SCOPE("Init", "nova_renderer::nova_renderer");

        window = std::make_shared<NovaWindow>(settings);

        if(settings.debug.renderdoc.enabled) {
            MTR_SCOPE("Init", "LoadRenderdoc");
            auto rd_load_result = load_renderdoc(settings.debug.renderdoc.renderdoc_dll_path);

            rd_load_result
                .map([&](RENDERDOC_API_1_3_0* api) {
                    render_doc = api;

                    render_doc->SetCaptureFilePathTemplate(settings.debug.renderdoc.capture_path);

                    RENDERDOC_InputButton capture_key[] = {eRENDERDOC_Key_F12, eRENDERDOC_Key_PrtScrn};
                    render_doc->SetCaptureKeys(capture_key, 2);

                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_AllowFullscreen, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_AllowVSync, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_VerifyMapWrites, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_SaveAllInitials, 1U);
                    render_doc->SetCaptureOptionU32(eRENDERDOC_Option_APIValidation, 1U);

                    NOVA_LOG(INFO) << "Loaded RenderDoc successfully";

                    return 0;
                })
                .on_error([](const ntl::NovaError& error) { NOVA_LOG(ERROR) << error.to_string().c_str(); });
        }

        switch(settings.api) {
            case GraphicsApi::D3D12:
#if defined(NOVA_WINDOWS) && defined(NOVA_D3D12_RHI)
            {
                MTR_SCOPE("Init", "InitDirect3D12RenderEngine");
                rhi = std::make_unique<rhi::D3D12RenderEngine>(render_settings, window, *global_allocator);
            } break;
#endif

#if defined(NOVA_VULKAN_RHI)
            case GraphicsApi::Vulkan: {
                MTR_SCOPE("Init", "InitVulkanRenderEngine");
                rhi = std::make_unique<rhi::VulkanRenderEngine>(render_settings, window, *global_allocator);
            } break;
#endif

#if defined(NOVA_OPENGL_RHI)
            case GraphicsApi::NvGl4: {
                MTR_SCOPE("Init", "InitGL3RenderEngine");
                rhi = std::make_unique<rhi::Gl4NvRenderEngine>(render_settings, window, *global_allocator);
            } break;
#endif
            default: {
                // TODO: Deal with it in a better way, this will crash soon
                NOVA_LOG(FATAL) << "Selected graphics API was not enabled!";
            } break;
        }

        swapchain = rhi->get_swapchain();

        create_global_gpu_pools();

        create_global_sync_objects();

        create_resource_storage();

        create_builtin_textures();

        create_uniform_buffers();

        create_builtin_renderpasses();
    }

    NovaRenderer::~NovaRenderer() { mtr_shutdown(); }

    NovaSettingsAccessManager& NovaRenderer::get_settings() { return render_settings; }

    AllocatorHandle<>* NovaRenderer::get_global_allocator() const { return global_allocator.get(); }

    void NovaRenderer::execute_frame() {
        MTR_SCOPE("RenderLoop", "execute_frame");
        frame_count++;

        AllocatorHandle<>& frame_allocator = frame_allocators[frame_count % NUM_IN_FLIGHT_FRAMES];

        cur_frame_idx = rhi->get_swapchain()->acquire_next_swapchain_image(frame_allocator);

        NOVA_LOG(DEBUG) << "\n***********************\n        FRAME START        \n***********************";

        rhi->reset_fences({frame_fences.at(cur_frame_idx)});

        // TODO: Figure out wtf I'm supposed to do about UI

        rhi::CommandList* cmds = rhi->create_command_list(frame_allocator, 0, rhi::QueueType::Graphics);

        // This may or may not work well lmao
        for(auto& [id, proc_mesh] : proc_meshes) {
            proc_mesh.record_commands_to_upload_data(cmds, cur_frame_idx);
        }

        FrameContext ctx = {};
        ctx.frame_count = frame_count;
        ctx.nova = this;
        ctx.allocator = &frame_allocator;
        ctx.swapchain_framebuffer = swapchain->get_framebuffer(cur_frame_idx);
        ctx.swapchain_image = swapchain->get_image(cur_frame_idx);

        for(const auto& renderpass : renderpasses) {
            renderpass->render(cmds, ctx);
        }

        // Record the UI pass

        rhi->submit_command_list(cmds, rhi::QueueType::Graphics, frame_fences.at(cur_frame_idx));

        // Wait for the GPU to finish before presenting. This destroys pipelining and throughput, however at this time I'm not sure how best
        // to say "when GPU finishes this task, CPU should do something"
        rhi->wait_for_fences({frame_fences.at(cur_frame_idx)});

        rhi->get_swapchain()->present(cur_frame_idx);

        mtr_flush();
    }

    void NovaRenderer::set_num_meshes(const uint32_t num_meshes) { meshes.reserve(num_meshes); }

    MeshId NovaRenderer::create_mesh(const MeshData& mesh_data) {
        rhi::BufferCreateInfo vertex_buffer_create_info;
        vertex_buffer_create_info.buffer_usage = rhi::BufferUsage::VertexBuffer;
        vertex_buffer_create_info.size = mesh_data.vertex_data.size() * sizeof(FullVertex);

        rhi::Buffer* vertex_buffer = rhi->create_buffer(vertex_buffer_create_info, *mesh_memory, *global_allocator);

        // TODO: Try to get staging buffers from a pool

        {
            rhi::BufferCreateInfo staging_vertex_buffer_create_info = vertex_buffer_create_info;
            staging_vertex_buffer_create_info.buffer_usage = rhi::BufferUsage::StagingBuffer;

            rhi::Buffer* staging_vertex_buffer = rhi->create_buffer(staging_vertex_buffer_create_info,
                                                                    *staging_buffer_memory,
                                                                    *global_allocator);
            rhi->write_data_to_buffer(mesh_data.vertex_data.data(),
                                      mesh_data.vertex_data.size() * sizeof(FullVertex),
                                      0,
                                      staging_vertex_buffer);

            rhi::CommandList* vertex_upload_cmds = rhi->create_command_list(*global_allocator, 0, rhi::QueueType::Transfer);
            vertex_upload_cmds->copy_buffer(vertex_buffer, 0, staging_vertex_buffer, 0, vertex_buffer_create_info.size);

            rhi::ResourceBarrier vertex_barrier = {};
            vertex_barrier.resource_to_barrier = vertex_buffer;
            vertex_barrier.old_state = rhi::ResourceState::CopyDestination;
            vertex_barrier.new_state = rhi::ResourceState::Common;
            vertex_barrier.access_before_barrier = rhi::ResourceAccess::CopyWrite;
            vertex_barrier.access_after_barrier = rhi::ResourceAccess::VertexAttributeRead;
            vertex_barrier.buffer_memory_barrier.offset = 0;
            vertex_barrier.buffer_memory_barrier.size = vertex_buffer->size;

            vertex_upload_cmds->resource_barriers(rhi::PipelineStage::Transfer, rhi::PipelineStage::VertexInput, {vertex_barrier});

            rhi->submit_command_list(vertex_upload_cmds, rhi::QueueType::Transfer);

            // TODO: Barrier on the mesh's first usage
        }

        rhi::BufferCreateInfo index_buffer_create_info;
        index_buffer_create_info.buffer_usage = rhi::BufferUsage::IndexBuffer;
        index_buffer_create_info.size = mesh_data.indices.size() * sizeof(uint32_t);

        rhi::Buffer* index_buffer = rhi->create_buffer(index_buffer_create_info, *mesh_memory, *global_allocator);

        {
            rhi::BufferCreateInfo staging_index_buffer_create_info = index_buffer_create_info;
            staging_index_buffer_create_info.buffer_usage = rhi::BufferUsage::StagingBuffer;
            rhi::Buffer* staging_index_buffer = rhi->create_buffer(staging_index_buffer_create_info,
                                                                   *staging_buffer_memory,
                                                                   *global_allocator);
            rhi->write_data_to_buffer(mesh_data.indices.data(), mesh_data.indices.size() * sizeof(uint32_t), 0, staging_index_buffer);

            rhi::CommandList* indices_upload_cmds = rhi->create_command_list(*global_allocator, 0, rhi::QueueType::Transfer);
            indices_upload_cmds->copy_buffer(index_buffer, 0, staging_index_buffer, 0, index_buffer_create_info.size);

            rhi::ResourceBarrier index_barrier = {};
            index_barrier.resource_to_barrier = index_buffer;
            index_barrier.old_state = rhi::ResourceState::CopyDestination;
            index_barrier.new_state = rhi::ResourceState::Common;
            index_barrier.access_before_barrier = rhi::ResourceAccess::CopyWrite;
            index_barrier.access_after_barrier = rhi::ResourceAccess::IndexRead;
            index_barrier.buffer_memory_barrier.offset = 0;
            index_barrier.buffer_memory_barrier.size = index_buffer->size;

            indices_upload_cmds->resource_barriers(rhi::PipelineStage::Transfer, rhi::PipelineStage::VertexInput, {index_barrier});

            rhi->submit_command_list(indices_upload_cmds, rhi::QueueType::Transfer);

            // TODO: Barrier on the mesh's first usage
        }

        // TODO: Clean up staging buffers

        Mesh mesh;
        mesh.vertex_buffer = vertex_buffer;
        mesh.index_buffer = index_buffer;
        mesh.num_indices = static_cast<uint32_t>(mesh_data.indices.size());

        MeshId new_mesh_id = next_mesh_id;
        next_mesh_id++;
        meshes.emplace(new_mesh_id, mesh);

        return new_mesh_id;
    }

    ProceduralMeshAccessor NovaRenderer::create_procedural_mesh(const uint64_t vertex_size, const uint64_t index_size) {
        MeshId our_id = next_mesh_id;
        next_mesh_id++;

        proc_meshes.emplace(our_id, ProceduralMesh(vertex_size, index_size, rhi.get()));

        return ProceduralMeshAccessor(&proc_meshes, our_id);
    }

    void NovaRenderer::load_shaderpack(const std::string& shaderpack_name) {
        MTR_SCOPE("ShaderpackLoading", "load_shaderpack");
        glslang::InitializeProcess();

        shaderpack::ShaderpackData data = shaderpack::load_shaderpack_data(fs::path(shaderpack_name.c_str()));

        if(shaderpack_loaded) {
            destroy_dynamic_resources();

            destroy_renderpasses();
            NOVA_LOG(DEBUG) << "Resources from old shaderpacks destroyed";
        }

        data.graph_data.passes = order_passes(data.graph_data.passes).value; // TODO: Handle errors somehow

        create_dynamic_textures(data.resources.render_targets);
        NOVA_LOG(DEBUG) << "Dynamic textures created";

        create_render_passes(data.graph_data.passes, data.pipelines, data.materials);

        create_pipelines(data.pipelines);

        NOVA_LOG(DEBUG) << "Created render passes";

        // Add builtin passes at the end of the submission
        // Currently the only builtin pass we have is the UI pass
        // As we add more passes, we'll probably need to keep more of the create info at runtime to be better able to insert passes wherever
        // we want
        for(const std::string& builtin_pass_name : data.graph_data.builtin_passes) {
            if(const auto& itr = builtin_renderpasses.find(builtin_pass_name); itr != builtin_renderpasses.end()) {
                renderpasses.emplace_back(itr->second);

            } else {
                NOVA_LOG(ERROR) << "Could not find builtin pass with name " << builtin_pass_name;
            }
        }

        shaderpack_loaded = true;

        NOVA_LOG(INFO) << "Shaderpack " << shaderpack_name.c_str() << " loaded successfully";
    }

    void NovaRenderer::set_ui_renderpass(const std::shared_ptr<Renderpass>& ui_renderpass) {
        std::lock_guard l(ui_function_mutex);
        builtin_renderpasses["NovaUI"] = ui_renderpass;
    }

    std::optional<shaderpack::RenderPassCreateInfo> NovaRenderer::get_renderpass_metadata(const std::string& renderpass_name) {
        if(const auto itr = renderpass_metadatas.find(renderpass_name); itr != renderpass_metadatas.end()) {
            return std::make_optional(itr->second.data);

        } else {
            return std::nullopt;
        }
    }

    void NovaRenderer::create_dynamic_textures(const std::pmr::vector<shaderpack::TextureCreateInfo>& texture_create_infos) {
        for(const shaderpack::TextureCreateInfo& create_info : texture_create_infos) {
            const auto size = create_info.format.get_size_in_pixels(rhi->get_swapchain()->get_size());

            const auto render_target = resource_storage->create_render_target(create_info.name,
                                                                              size.x,
                                                                              size.y,
                                                                              to_rhi_pixel_format(create_info.format.pixel_format),
                                                                              *renderpack_allocator);

            dynamic_texture_infos.emplace(create_info.name, create_info);
        }
    }

    void NovaRenderer::create_render_passes(const std::pmr::vector<shaderpack::RenderPassCreateInfo>& pass_create_infos,
                                            const std::pmr::vector<shaderpack::PipelineCreateInfo>& pipelines,
                                            const std::pmr::vector<shaderpack::MaterialData>& materials) {

        rhi->set_num_renderpasses(static_cast<uint32_t>(pass_create_infos.size()));

        uint32_t total_num_descriptors = 0;
        for(const shaderpack::MaterialData& material_data : materials) {
            for(const shaderpack::MaterialPass& material_pass : material_data.passes) {
                total_num_descriptors += static_cast<uint32_t>(material_pass.bindings.size());
            }
        }

        rhi::DescriptorPool* descriptor_pool = rhi->create_descriptor_pool(total_num_descriptors,
                                                                           5,
                                                                           total_num_descriptors,
                                                                           *renderpack_allocator);

        for(const shaderpack::RenderPassCreateInfo& create_info : pass_create_infos) {
            std::shared_ptr<Renderpass> renderpass = std::make_shared<Renderpass>();
            add_render_pass(create_info, pipelines, materials, descriptor_pool, renderpass);
        }
    }

    void NovaRenderer::add_render_pass(const shaderpack::RenderPassCreateInfo& create_info,
                                       const std::pmr::vector<shaderpack::PipelineCreateInfo>& pipelines,
                                       const std::pmr::vector<shaderpack::MaterialData>& materials,
                                       rhi::DescriptorPool* descriptor_pool,
                                       const std::shared_ptr<Renderpass>& renderpass) {
        RenderpassMetadata metadata;
        metadata.data = create_info;

        std::pmr::vector<rhi::Image*> color_attachments;
        color_attachments.reserve(create_info.texture_outputs.size());

        glm::uvec2 framebuffer_size(0);

        const auto num_attachments = create_info.depth_texture ? create_info.texture_outputs.size() + 1 :
                                                                 create_info.texture_outputs.size();
        std::pmr::vector<std::string> attachment_errors;
        attachment_errors.reserve(num_attachments);

        bool writes_to_backbuffer = false;

        for(const shaderpack::TextureAttachmentInfo& attachment_info : create_info.texture_outputs) {
            if(attachment_info.name == BACKBUFFER_NAME) {
                writes_to_backbuffer = true;

                if(create_info.texture_outputs.size() == 1) {
                    renderpass->writes_to_backbuffer = true;
                    renderpass->framebuffer = nullptr; // Will be resolved when rendering

                } else {
                    attachment_errors.push_back(format(
                        fmt("Pass {:s} writes to the backbuffer and {:d} other textures, but that's not allowed. If a pass writes to the backbuffer, it can't write to any other textures"),
                        create_info.name,
                        create_info.texture_outputs.size() - 1));
                }

            } else {
                const auto render_target_opt = resource_storage->get_render_target(attachment_info.name);
                if(render_target_opt) {
                    const auto& render_target = *render_target_opt;

                    color_attachments.push_back(render_target->image);

                    const shaderpack::TextureCreateInfo& info = dynamic_texture_infos.at(attachment_info.name);
                    const glm::uvec2 attachment_size = {render_target->width, render_target->height};
                    if(framebuffer_size.x > 0) {
                        if(attachment_size.x != framebuffer_size.x || attachment_size.y != framebuffer_size.y) {
                            attachment_errors.push_back(format(
                                fmt("Attachment {:s} has a size of {:d}x{:d}, but the framebuffer for pass {:s} has a size of {:d}x{:d} - these must match! All attachments of a single renderpass must have the same size"),
                                attachment_info.name,
                                attachment_size.x,
                                attachment_size.y,
                                create_info.name,
                                framebuffer_size.x,
                                framebuffer_size.y));
                        }

                    } else {
                        framebuffer_size = attachment_size;
                    }
                } else {
                    NOVA_LOG(ERROR) << "No render target named " << attachment_info.name;
                }
            }
        }

        // Can't combine these if statements and I don't want to `.find` twice
        const auto depth_attachment = [&]() -> std::optional<rhi::Image*> {
            if(create_info.depth_texture) {
                if(const auto depth_tex = resource_storage->get_render_target(create_info.depth_texture->name); depth_tex) {
                    auto* image = (*depth_tex)->image;
                    return std::make_optional<rhi::Image*>(image);
                }
            }

            return {};
        }();

        if(!attachment_errors.empty()) {
            for(const auto& err : attachment_errors) {
                NOVA_LOG(ERROR) << err;
            }

            NOVA_LOG(ERROR) << "Could not create renderpass " << create_info.name
                            << " because there were errors in the attachment specification. Look above this message for details";
            return;
        }

        ntl::Result<rhi::Renderpass*> renderpass_result = rhi->create_renderpass(create_info, framebuffer_size, *renderpack_allocator);
        if(renderpass_result) {
            renderpass->renderpass = renderpass_result.value;

        } else {
            NOVA_LOG(ERROR) << "Could not create renderpass " << create_info.name << ": " << renderpass_result.error.to_string();
            return;
        }

        // Backbuffer framebuffers are owned by the swapchain, not the renderpass that writes to them, so if the
        // renderpass writes to the backbuffer then we don't need to create a framebuffer for it
        if(!writes_to_backbuffer) {
            renderpass->framebuffer = rhi->create_framebuffer(renderpass->renderpass,
                                                              color_attachments,
                                                              depth_attachment,
                                                              framebuffer_size,
                                                              *renderpack_allocator);
        }

        renderpass->pipeline_names.reserve(pipelines.size());
        for(const shaderpack::PipelineCreateInfo& pipeline_create_info : pipelines) {
            if(pipeline_create_info.pass == create_info.name) {
                renderpass->pipeline_names.emplace_back(pipeline_create_info.name);
            }
        }
        renderpass->id = static_cast<uint32_t>(renderpass_metadatas.size());

        renderpasses.push_back(renderpass);
        renderpass_metadatas.emplace(create_info.name, metadata);
    }

    void NovaRenderer::create_pipelines(const std::pmr::vector<shaderpack::PipelineCreateInfo>& pipeline_create_infos) {
        for(const auto& pipeline_create_info : pipeline_create_infos) {
            pipeline_storage->add_pipeline_from_shaderpack(pipeline_create_info);
        }
    }

    void NovaRenderer::create_materials_for_pipeline(
        Pipeline& pipeline,
        std::unordered_map<FullMaterialPassName, MaterialPassMetadata, FullMaterialPassNameHasher>& material_metadatas,
        const std::pmr::vector<shaderpack::MaterialData>& materials,
        const std::string& pipeline_name,
        const rhi::PipelineInterface* pipeline_interface,
        rhi::DescriptorPool* descriptor_pool,
        const MaterialPassKey& template_key) {

        // Determine the pipeline layout so the material can create descriptors for the pipeline

        // Large overestimate, but that's fine
        pipeline.passes.reserve(materials.size());

        for(const shaderpack::MaterialData& material_data : materials) {
            for(const shaderpack::MaterialPass& pass_data : material_data.passes) {
                if(pass_data.pipeline == pipeline_name) {
                    MaterialPass pass = {};
                    pass.pipeline_interface = pipeline_interface;

                    pass.descriptor_sets = rhi->create_descriptor_sets(pipeline_interface, descriptor_pool, *renderpack_allocator);

                    bind_data_to_material_descriptor_sets(pass, pass_data.bindings, pipeline_interface->bindings);

                    FullMaterialPassName full_pass_name = {pass_data.material_name, pass_data.name};

                    MaterialPassMetadata pass_metadata{};
                    pass_metadata.data = pass_data;
                    material_metadatas.emplace(full_pass_name, pass_metadata);

                    MaterialPassKey key = template_key;
                    key.material_pass_index = static_cast<uint32_t>(pipeline.passes.size());

                    material_pass_keys.emplace(full_pass_name, key);

                    pipeline.passes.push_back(pass);
                }
            }
        }

        pipeline.passes.shrink_to_fit();
    }

    void NovaRenderer::bind_data_to_material_descriptor_sets(
        const MaterialPass& material,
        const std::unordered_map<std::string, std::string>& bindings,
        const std::unordered_map<std::string, rhi::ResourceBindingDescription>& descriptor_descriptions) {

        std::pmr::vector<rhi::DescriptorSetWrite> writes;
        writes.reserve(bindings.size());

        for(const auto& [descriptor_name, resource_name] : bindings) {
            const rhi::ResourceBindingDescription& binding_desc = descriptor_descriptions.at(descriptor_name);
            rhi::DescriptorSet* descriptor_set = material.descriptor_sets.at(binding_desc.set);

            rhi::DescriptorSetWrite write = {};
            write.set = descriptor_set;
            write.binding = binding_desc.binding;
            write.resources.emplace_back();
            rhi::DescriptorResourceInfo& resource_info = write.resources[0];

            if(const auto dyn_tex = resource_storage->get_render_target(resource_name); dyn_tex) {
                rhi::Image* image = (*dyn_tex)->image;

                resource_info.image_info.image = image;
                resource_info.image_info.sampler = point_sampler;
                resource_info.image_info.format = dynamic_texture_infos.at(resource_name).format;

                write.type = rhi::DescriptorType::CombinedImageSampler;

                writes.push_back(write);

            } else if(const auto builtin_buffer_itr = builtin_buffers.find(resource_name); builtin_buffer_itr != builtin_buffers.end()) {
                rhi::Buffer* buffer = builtin_buffer_itr->second;

                resource_info.buffer_info.buffer = buffer;
                write.type = rhi::DescriptorType::UniformBuffer;

                writes.push_back(write);

            } else {
                NOVA_LOG(ERROR) << "Resource " << resource_name.c_str() << " is not known to Nova";
            }
        }

        rhi->update_descriptor_sets(writes);
    }

    void NovaRenderer::destroy_dynamic_resources() {
        if(loaded_renderpack) {
            for(const auto& tex_data : loaded_renderpack->resources.render_targets) {
                resource_storage->destroy_render_target(tex_data.name, *renderpack_allocator);
            }
            NOVA_LOG(DEBUG) << "Deleted all dynamic textures from renderpack " << loaded_renderpack->name;
        }
    }

    void NovaRenderer::destroy_renderpasses() {
        for(const auto& renderpass : renderpasses) {
            if(!renderpass->is_builtin) {
                rhi->destroy_renderpass(renderpass->renderpass, *renderpack_allocator);
                rhi->destroy_framebuffer(renderpass->framebuffer, *renderpack_allocator);

                for(Pipeline& pipeline : renderpass->pipeline_names) {
                    rhi->destroy_pipeline(pipeline.pipeline, *renderpack_allocator);

                    for(MaterialPass& material_pass : pipeline.passes) {
                        (void) material_pass;
                        // TODO: Destroy descriptors for material
                        // TODO: Have a way to save mesh data somewhere outside of the render graph, then process it cleanly here
                    }
                }
            }
        }
    }

    rhi::Buffer* NovaRenderer::get_builtin_buffer(const std::string& buffer_name) const { return builtin_buffers.at(buffer_name); }

    rhi::Sampler* NovaRenderer::get_point_sampler() const { return point_sampler; }

    RenderableId NovaRenderer::add_renderable_for_material(const FullMaterialPassName& material_name,
                                                           const StaticMeshRenderableData& renderable) {
        const RenderableId id = next_renderable_id.load();
        next_renderable_id.fetch_add(1);

        const auto pos = material_pass_keys.find(material_name);
        if(pos == material_pass_keys.end()) {
            NOVA_LOG(ERROR) << "No material named " << material_name.material_name << " for pass " << material_name.pass_name;
            return std::numeric_limits<uint64_t>::max();
        }

        MaterialPass material = {};

        StaticMeshRenderCommand command = make_render_command(renderable, id);

        if(const auto itr = meshes.find(renderable.mesh); itr != meshes.end()) {
            const Mesh& mesh = itr->second;

            if(renderable.is_static) {
                bool need_to_add_batch = true;

                for(MeshBatch<StaticMeshRenderCommand>& batch : material.static_mesh_draws) {
                    if(batch.vertex_buffer == mesh.vertex_buffer) {
                        batch.commands.emplace_back(command);

                        need_to_add_batch = false;
                        break;
                    }
                }

                if(need_to_add_batch) {
                    MeshBatch<StaticMeshRenderCommand> batch;
                    batch.vertex_buffer = mesh.vertex_buffer;
                    batch.index_buffer = mesh.index_buffer;
                    batch.commands.emplace_back(command);

                    material.static_mesh_draws.emplace_back(batch);
                }
            }

        } else if(const auto proc_itr = proc_meshes.find(renderable.mesh); proc_itr != proc_meshes.end()) {
            if(renderable.is_static) {
                bool need_to_add_batch = false;

                for(ProceduralMeshBatch<StaticMeshRenderCommand>& batch : material.static_procedural_mesh_draws) {
                    if(batch.mesh.get_key() == renderable.mesh) {
                        batch.commands.emplace_back(command);

                        need_to_add_batch = false;
                        break;
                    }
                }

                if(need_to_add_batch) {
                    ProceduralMeshBatch<StaticMeshRenderCommand> batch(&proc_meshes, renderable.mesh);
                    batch.commands.emplace_back(command);

                    material.static_procedural_mesh_draws.emplace_back(batch);
                }
            }
        } else {
            NOVA_LOG(ERROR) << "Could not find a mesh with ID " << renderable.mesh;
        }

        // Figure out where to put the renderable
        const MaterialPassKey& pass_key = pos->second;
        auto& passes = passes_by_pipeline[pass_key.pipeline_name];
        passes.emplace_back(material);

        return id;
    }

    rhi::RenderEngine* NovaRenderer::get_engine() const { return rhi.get(); }

    NovaWindow* NovaRenderer::get_window() const { return window.get(); }

    DeviceResources* NovaRenderer::get_resource_manager() const { return resource_storage.get(); }

    PipelineStorage* NovaRenderer::get_pipeline_storage() { return pipeline_storage.get(); }

    NovaRenderer* NovaRenderer::get_instance() { return instance.get(); }

    NovaRenderer* NovaRenderer::initialize(const NovaSettings& settings) {
        return (instance = std::make_unique<NovaRenderer>(settings)).get();
    }

    void NovaRenderer::deinitialize() { instance.reset(); }

    void NovaRenderer::create_global_allocators() {
        global_allocator = std::make_unique<AllocatorHandle<>>(std::pmr::new_delete_resource());

        renderpack_allocator = std::make_unique<AllocatorHandle<>>(*global_allocator->create_suballocator());

        frame_allocators.reserve(NUM_IN_FLIGHT_FRAMES);
        for(size_t i = 0; i < NUM_IN_FLIGHT_FRAMES; i++) {
            void* ptr = global_allocator->allocate(PER_FRAME_MEMORY_SIZE.b_count());
            auto* mem = global_allocator->new_other_object<std::pmr::monotonic_buffer_resource>(ptr, PER_FRAME_MEMORY_SIZE.b_count());
            frame_allocators.emplace_back(mem);
        }
    }

    void NovaRenderer::create_global_gpu_pools() {
        const uint64_t mesh_memory_size = 512000000;
        ntl::Result<rhi::DeviceMemory*> memory_result = rhi->allocate_device_memory(mesh_memory_size,
                                                                                    rhi::MemoryUsage::DeviceOnly,
                                                                                    rhi::ObjectType::Buffer,
                                                                                    *global_allocator);
        const ntl::Result<DeviceMemoryResource*> mesh_memory_result = memory_result.map([&](rhi::DeviceMemory* memory) {
            auto* allocator = global_allocator->new_other_object<BlockAllocationStrategy>(global_allocator.get(),
                                                                                          Bytes(mesh_memory_size),
                                                                                          64_b);
            return global_allocator->new_other_object<DeviceMemoryResource>(memory, allocator);
        });

        if(mesh_memory_result) {
            mesh_memory = std::make_unique<DeviceMemoryResource>(*mesh_memory_result.value);

        } else {
            NOVA_LOG(ERROR) << "Could not create mesh memory pool: " << mesh_memory_result.error.to_string().c_str();
        }

        // Assume 65k things, plus we need space for the builtin ubos
        const uint64_t ubo_memory_size = sizeof(PerFrameUniforms) + sizeof(glm::mat4) * 0xFFFF;
        const ntl::Result<DeviceMemoryResource*>
            ubo_memory_result = rhi->allocate_device_memory(ubo_memory_size,
                                                            rhi::MemoryUsage::DeviceOnly,
                                                            rhi::ObjectType::Buffer,
                                                            *global_allocator)
                                    .map([=](rhi::DeviceMemory* memory) {
                                        auto* allocator = global_allocator
                                                              ->new_other_object<BumpPointAllocationStrategy>(Bytes(ubo_memory_size),
                                                                                                              Bytes(sizeof(glm::mat4)));
                                        return global_allocator->new_other_object<DeviceMemoryResource>(memory, allocator);
                                    });

        if(ubo_memory_result) {
            ubo_memory = std::make_unique<DeviceMemoryResource>(*ubo_memory_result.value);

        } else {
            NOVA_LOG(ERROR) << "Could not create mesh memory pool: " << ubo_memory_result.error.to_string().c_str();
        }

        // Staging buffers will be pooled, so we don't need a _ton_ of memory for them
        const Bytes staging_memory_size = 256_kb;
        const ntl::Result<DeviceMemoryResource*>
            staging_memory_result = rhi->allocate_device_memory(staging_memory_size.b_count(),
                                                                rhi::MemoryUsage::StagingBuffer,
                                                                rhi::ObjectType::Buffer,
                                                                *global_allocator)
                                        .map([=](rhi::DeviceMemory* memory) {
                                            auto* allocator = global_allocator
                                                                  ->new_other_object<BumpPointAllocationStrategy>(staging_memory_size,
                                                                                                                  64_b);
                                            return global_allocator->new_other_object<DeviceMemoryResource>(memory, allocator);
                                        });

        if(staging_memory_result) {
            staging_buffer_memory = std::make_unique<DeviceMemoryResource>(*staging_memory_result.value);

        } else {
            NOVA_LOG(ERROR) << "Could not create staging buffer memory pool: " << staging_memory_result.error.to_string();
        }
    }

    void NovaRenderer::create_global_sync_objects() {
        const std::pmr::vector<rhi::Fence*>& fences = rhi->create_fences(*global_allocator, NUM_IN_FLIGHT_FRAMES, true);
        for(uint32_t i = 0; i < NUM_IN_FLIGHT_FRAMES; i++) {
            frame_fences[i] = fences.at(i);
        }
    }

    void NovaRenderer::create_resource_storage() { resource_storage = std::make_unique<DeviceResources>(*this); }

    void NovaRenderer::create_builtin_textures() {}

    void NovaRenderer::create_uniform_buffers() {
        // Buffer for per-frame uniform data
        rhi::BufferCreateInfo per_frame_data_create_info = {};
        per_frame_data_create_info.size = sizeof(PerFrameUniforms);
        per_frame_data_create_info.buffer_usage = rhi::BufferUsage::UniformBuffer;

        auto* per_frame_data_buffer = rhi->create_buffer(per_frame_data_create_info, *ubo_memory, *global_allocator);
        builtin_buffers.emplace(PER_FRAME_DATA_NAME, per_frame_data_buffer);

        // Buffer for each drawcall's model matrix
        rhi::BufferCreateInfo model_matrix_buffer_create_info = {};
        model_matrix_buffer_create_info.size = sizeof(glm::mat4) * 0xFFFF;
        model_matrix_buffer_create_info.buffer_usage = rhi::BufferUsage::UniformBuffer;

        auto* model_matrix_buffer = rhi->create_buffer(model_matrix_buffer_create_info, *ubo_memory, *global_allocator);
        builtin_buffers.emplace(MODEL_MATRIX_BUFFER_NAME, model_matrix_buffer);
    }

    void NovaRenderer::create_builtin_renderpasses() {
        // UI render pass
        {
            const std::shared_ptr<Renderpass> ui_renderpass = std::make_shared<NullUiRenderpass>(rhi.get(),
                                                                                                 rhi->get_swapchain()->get_size());

            shaderpack::RenderPassCreateInfo ui = {};
            ui.name = UI_RENDER_PASS_NAME;
            ui.texture_inputs = {BACKBUFFER_NAME};
            ui.texture_outputs = {{BACKBUFFER_NAME, shaderpack::PixelFormatEnum::RGBA8, false}};

            add_render_pass(ui, {}, {}, nullptr, ui_renderpass);

            ui_renderpass->is_builtin = true;

            builtin_renderpasses[UI_RENDER_PASS_NAME] = ui_renderpass;
        }
    }
} // namespace nova::renderer
