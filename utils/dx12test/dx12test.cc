
#pragma warning(disable:4100 4005)

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>
#undef VIS_DX11
#undef VIS_GL
#ifndef VIS_DX12
#define VIS_DX12
#endif
#define VIS_IMPLEMENTATION
#include <vis.h>

struct app_assets
{
  vis_resource pipeline;
  vis_resource cmd_list;
  vis_resource vb;
  vis_resource vs;
  vis_resource ps;
  vis_resource vertex_layout;
  vis_resource shader_layout;
};

struct app_vertex
{
  float position[3];
  float color[4];
};

int app_load_assets(vis* vi, app_assets* assets)
{
  // Define the geometry for a triangle.
  app_vertex triangleVertices[] =
  {
    { { 0.0f, 0.25f * vi->aspect_ratio, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } },
    { { 0.25f, -0.25f * vi->aspect_ratio, 0.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
    { { -0.25f, -0.25f * vi->aspect_ratio, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } }
  };

  const char* vsstr = "";
  const char* psstr = "";

  // Resources (data) - not filling data in yet
  assets->vb = vis_create_resource(vi, VIS_TYPE_VERTEXBUFFER, nullptr, VIS_NOFLAGS);
  vis_sbc* sbc=nullptr;
  vis_shader_compile(vi, VIS_LOAD_SOURCE_MEMORY, (void*)vsstr, strlen(vsstr), &sbc);
  assets->vs = vis_create_resource(vi, VIS_TYPE_SHADER, sbc, VIS_NOFLAGS);
  vis_release_sbc(vi, &sbc);
  vis_shader_compile(vi, VIS_LOAD_SOURCE_MEMORY, (void*)psstr, strlen(psstr), &sbc);
  assets->ps = vis_create_resource(vi, VIS_TYPE_SHADER, sbc, VIS_NOFLAGS);
  vis_release_sbc(vi, &sbc);

  // Descriptors (metadata)
  assets->vertex_layout = vis_create_resource(vi, VIS_TYPE_INPUT_LAYOUT, inputLayout, VIS_NOFLAGS);
  assets->shader_layout = vis_create_resource(vi, VIS_TYPE_GPU_LAYOUT, shLayout, VIS_NOFLAGS);

  // Pipeline state
  vis_pipeline_desc pdesc = { 0 };
  pdesc.vertex_layout = assets->vertex_layout;
  pdesc.shader_layout = assets->shader_layout;
  pdesc.vs = assets->vs;
  pdesc.ps = assets->ps;
  assets->pipeline = vis_create_pipeline(vi, &pdesc);

  // Command list
  assets->cmd_list = vis_create_command_list(vi, assets->pipeline);  
  // Record some upload to GPU in the cmd list
  vis_command_list_record(vi, assets->cmd_list);
  vis_staging vbupdate = vis_command_list_resource_update(vi, assets->cmd_list, assets->vb, triangleVertices, sizeof(app_vertex));
  vis_command_list_close(vi, assets->cmd_list );
  vis_command_list_execute(vi, &assets->cmd_list, 1);

  // tell gpu to signal once it finishes and wait for it here
  vis_id signal = vis_sync_gpu_to_signal(vi);
  vis_sync_cpu_wait_for_signal(vi, signal); // vis_sync_cpu_callback_when_signal(vi, signal, cb_func, cb_data);

  // release temporary resources allocated for upload
  vis_command_list_release_update(vi, assets->cmd_list, vbupdate);

  return VIS_OK;
}

void app_unload_assets(vis* vi, app_assets* assets)
{
  vis_release_descriptor(vi, assets->vertex_layout);
  vis_release_descriptor(vi, assets->gpu_layout);
  vis_release_resource(vi, assets->ps);
  vis_release_resource(vi, assets->vs);
  vis_release_resource(vi, assets->vb);

  vis_release_pipeline(vi, assets->pipeline);
  vis_release_command_list(vi, assets->cmd_list);
}

int main(int argc, const char** argv)
{
  // RENDERING
  vis* v;
  vis_opts vopts = { 0 };
  vopts.width = 1024;
  vopts.height = 768;
  vopts.title = "fltview";

  if (vis_init(&v, &vopts) == VIS_OK)
  {
    app_assets assets = { 0 };
    if (app_load_assets(v, &assets) == VIS_OK)
    {
      while (vis_begin_frame(v) == VIS_OK)
      {
        vis_render_frame(v);
        vis_end_frame(v);
      }
      app_unload_assets(v, &assets);
    }
    vis_release(&v);
  }

  return 0;
}