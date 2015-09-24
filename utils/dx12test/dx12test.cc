
#pragma warning(disable:4100 4005 4297)

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdint.h>
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
  vis_handle pipeline;
  vis_handle cmd_list;
  vis_handle vb;
  vis_handle vs;
  vis_handle ps;
  vis_handle shader_layout;
  vis_handle* render_targets;
  vis_viewport viewport;
};

struct app_vertex
{
  float position[3];
  float color[4];
};

void app_wait_for_gpu(vis* vi)
{
  // tell gpu to signal once it finishes and wait for it here
  vis_handle signal = vis_sync_gpu_to_signal(vi);
  vis_sync_cpu_wait_for_signal(vi, signal);
  vis_release_resource(vi, &signal);
}

int app_load_assets(vis* vi, app_assets* assets)
{
  // Define the geometry for a triangle, the vertex and pixel shaders for it
  app_vertex triangleVertices[] =
  {
    { { 0.0f, 0.25f * vi->aspect_ratio, 0.0f },{ 1.0f, 0.0f, 0.0f, 1.0f } },
    { { 0.25f, -0.25f * vi->aspect_ratio, 0.0f },{ 0.0f, 1.0f, 0.0f, 1.0f } },
    { { -0.25f, -0.25f * vi->aspect_ratio, 0.0f },{ 0.0f, 0.0f, 1.0f, 1.0f } }
  };
  const uint32_t vbSize = sizeof(app_vertex) * 3;

  vis_input_element vinput[] = 
  {
    { "POSITION", 0, VIS_FORMAT_R32G32B32_FLOAT, 0, 0 },
    { "COLOR", 0, VIS_FORMAT_R32G32B32_FLOAT, 0, 12 },
  };
  const char* vsstr = "";
  const char* psstr = "";

  // Resources (data) - not filling data in yet
  {
    // vb for triangle
    assets->vb = vis_create_resource(vi, VIS_TYPE_VERTEXBUFFER, nullptr, vbSize);

    // compile and create vs/ps
    vis_shader_bytecode vsbytecode = { 0 };
    vis_shader_bytecode psbytecode = { 0 };
    vis_shader_compile_desc vsdesc = { (void*)vsstr, strlen(vsstr), "vs_main", "vs_5_0", VIS_COMPILEFLAG_DEBUG };
    vis_shader_compile_desc psdesc = { (void*)psstr, strlen(psstr), "ps_main", "ps_5_0", VIS_COMPILEFLAG_DEBUG };
    vis_shader_compile(vi, VIS_LOAD_SOURCE_MEMORY, &vsdesc, &vsbytecode);
    vis_shader_compile(vi, VIS_LOAD_SOURCE_MEMORY, &psdesc, &psbytecode);
    assets->vs = vis_create_resource(vi, VIS_TYPE_SHADER, &vsbytecode, VIS_STAGE_VS);
    assets->ps = vis_create_resource(vi, VIS_TYPE_SHADER, &psbytecode, VIS_STAGE_PS);

    // create RT with the back buffers
    const uint32_t bbcount = vis_get_back_buffer_count(vi);
    assets->render_targets = (vis_handle*)vis_malloc(sizeof(vis_handle)*bbcount);
    for (uint32_t i = 0; i < bbcount; ++i)
    {
      assets->render_targets[i] = vis_create_resource(vi, VIS_TYPE_RENDER_TARGET, 
        vis_get_back_buffer(vi, i), VIS_NONE);
    }
  }

  // Descriptors (metadata)
  {
    vis_shader_layout shaderLayout = {
      nullptr,  // no shader registers
      0,
      VIS_STAGE_IA | VIS_STAGE_VS | VIS_STAGE_PS
    };
    assets->shader_layout = vis_create_resource(vi, VIS_TYPE_SHADER_LAYOUT, &shaderLayout, VIS_NONE);
  }

  // Pipeline state
  {
    vis_pipeline_state pstate;
    vis_pipeline_state_set_default(vi, &pstate);
    pstate.vertex_layout = vinput;
    pstate.vertex_layout_count = 2;
    pstate.shader_layout = assets->shader_layout;
    pstate.vs = assets->vs;
    pstate.ps = assets->ps;
    assets->pipeline = vis_create_resource(vi, VIS_TYPE_PIPELINE, &pstate, VIS_NONE);
  }

  // Command list. creates and record a VB upload data, then waits for it
  {
    assets->cmd_list = vis_create_resource(vi, VIS_TYPE_COMMAND_LIST, &assets->pipeline, VIS_NONE);
    // Record some upload to GPU in the cmd list
    vis_command_list_record(vi, assets->cmd_list);
    vis_handle vbupdate = vis_command_list_resource_update(vi, assets->cmd_list, assets->vb, triangleVertices, sizeof(app_vertex));
    vis_command_list_close(vi, assets->cmd_list);
    vis_command_list_execute(vi, &assets->cmd_list, 1);

    // tell gpu to signal once it finishes and wait for it here
    app_wait_for_gpu(vi);

    // release temporary resources allocated for upload
    vis_release_resource(vi, &vbupdate);
  }

  // viewport/scissor rect
  vis_rect_make(&assets->viewport.rect, 0.0f, 0.0f, (float)vi->width, (float)vi->height);
  assets->viewport.depth_min = 0.0f; assets->viewport.depth_max = 1.0f;

  return VIS_OK;
}

void app_unload_assets(vis* vi, app_assets* assets)
{
  vis_release_resource(vi, &assets->shader_layout);
  vis_release_resource(vi, &assets->ps);
  vis_release_resource(vi, &assets->vs);
  vis_release_resource(vi, &assets->vb);
  vis_release_resource(vi, &assets->pipeline);
  vis_release_resource(vi, &assets->cmd_list);
  
  const uint32_t bbcount = vis_get_back_buffer_count(vi);
  for (uint32_t i = 0; i < bbcount; ++i)
    vis_release_resource(vi, &assets->render_targets[i]);
  vis_safefree(assets->render_targets);
}

void app_render(vis* vi, app_assets* assets)
{
  // populate command list
  {
    vis_command_list_reset(vi, assets->cmd_list);
    vis_command_list_record(vi, assets->cmd_list);

    // (push) set necessary state
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_SHADER_LAYOUT, &assets->shader_layout, VIS_NONE);
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_VIEWPORTS, &assets->viewport, 1);
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_SCISSORS, &assets->viewport.rect, 1);
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_RENDER_TARGETS, &assets->render_targets[vi->framendx], 1);

    // drawing commands
    vis_cmd_clear clear = { &assets->render_targets[vi->framendx], 1, { 0.0f, 0.2f, 0.4f, 1.0f } };
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_CLEAR_RT, &clear, VIS_NONE);
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_PRIM_TOPOLOGY, nullptr, VIS_PRIMTOPO_TRIANGLE_LIST);
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_VERTEX_BUFFER, &assets->vb, 1);
    vis_cmd_draw draw = { 0, 3, 0, 1 };
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_DRAW, &draw, VIS_NONE);

    // (pop) necessary state
    vis_command_list_set(vi, assets->cmd_list, VIS_CLS_RENDER_TARGETS, &assets->render_targets[vi->framendx], -1);

    // close
    vis_command_list_close(vi, assets->cmd_list);    
  }

  // execute command list
  vis_command_list_execute(vi, &assets->cmd_list, 1);
}

int main(int argc, const char** argv)
{
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
        app_render(v, &assets);
        vis_present_frame(v);
        app_wait_for_gpu(v);
      }
      app_unload_assets(v, &assets);
    }
    vis_release(&v);
  }

  return 0;
}