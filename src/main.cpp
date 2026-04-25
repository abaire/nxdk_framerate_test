#include "nv2astate.h"
#include "xbox_math_matrix.h"
#ifndef XBOX
#error Must be built with nxdk
#endif

#include <SDL.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <pbkit/pbkit.h>
#include <windows.h>

#include "nxdk_ext.h"
#include "printf.h"
#include "pushbuffer.h"

using namespace PBKitPlusPlus;

#define SET_MASK(mask, val) (((val) << (__builtin_ffs(mask) - 1)) & (mask))

static constexpr int kFramebufferWidth = 640;
static constexpr int kFramebufferHeight = 480;
static constexpr int kBitsPerPixel = 32;
static constexpr int kMaxTextureSize = 512;
static constexpr int kMaxTextureDepth = 3;
static constexpr int kFramebufferPitch = kFramebufferWidth * 4;

extern "C" {
void _putchar(char character) { putchar(character); }
}

static void Initialize(NV2AState &state) {
  state.SetSurfaceFormat(NV2AState::SCF_A8R8G8B8, NV2AState::SZF_Z16, state.GetFramebufferWidth(),
                         state.GetFramebufferHeight());
  {
    Pushbuffer::Begin();
    Pushbuffer::PushF(NV097_SET_EYE_POSITION, 0.0f, 0.0f, 0.0f, 1.0f);
    Pushbuffer::Push(NV097_SET_ZMIN_MAX_CONTROL, NV097_SET_ZMIN_MAX_CONTROL_CULL_NEAR_FAR_EN_TRUE |
                                                     NV097_SET_ZMIN_MAX_CONTROL_ZCLAMP_EN_CULL |
                                                     NV097_SET_ZMIN_MAX_CONTROL_CULL_IGNORE_W_FALSE);
    Pushbuffer::Push(NV097_SET_SURFACE_PITCH, SET_MASK(NV097_SET_SURFACE_PITCH_COLOR, kFramebufferPitch) |
                                                  SET_MASK(NV097_SET_SURFACE_PITCH_ZETA, kFramebufferPitch));
    Pushbuffer::Push(NV097_SET_SURFACE_CLIP_HORIZONTAL, state.GetFramebufferWidth() << 16);
    Pushbuffer::Push(NV097_SET_SURFACE_CLIP_VERTICAL, state.GetFramebufferHeight() << 16);

    Pushbuffer::Push(NV097_SET_LIGHTING_ENABLE, false);
    Pushbuffer::Push(NV097_SET_SPECULAR_ENABLE, false);
    Pushbuffer::Push(NV097_SET_LIGHT_CONTROL, NV097_SET_LIGHT_CONTROL_V_ALPHA_FROM_MATERIAL_SPECULAR |
                                                  NV097_SET_LIGHT_CONTROL_V_SEPARATE_SPECULAR);
    Pushbuffer::Push(NV097_SET_LIGHT_ENABLE_MASK, NV097_SET_LIGHT_ENABLE_MASK_LIGHT0_OFF);
    Pushbuffer::Push(NV097_SET_COLOR_MATERIAL, NV097_SET_COLOR_MATERIAL_ALL_FROM_MATERIAL);
    Pushbuffer::Push(NV097_SET_SCENE_AMBIENT_COLOR, 0x0, 0x0, 0x0);
    Pushbuffer::Push(NV097_SET_MATERIAL_EMISSION, 0x0, 0x0, 0x0);
    Pushbuffer::PushF(NV097_SET_MATERIAL_ALPHA, 1.0f);
    Pushbuffer::PushF(NV097_SET_BACK_MATERIAL_ALPHA, 1.f);

    Pushbuffer::Push(NV097_SET_LIGHT_TWO_SIDE_ENABLE, false);
    Pushbuffer::Push(NV097_SET_FRONT_POLYGON_MODE, NV097_SET_FRONT_POLYGON_MODE_V_FILL);
    Pushbuffer::Push(NV097_SET_BACK_POLYGON_MODE, NV097_SET_FRONT_POLYGON_MODE_V_FILL);

    Pushbuffer::Push(NV097_SET_POINT_PARAMS_ENABLE, false);
    Pushbuffer::Push(NV097_SET_POINT_SMOOTH_ENABLE, false);
    Pushbuffer::Push(NV097_SET_POINT_SIZE, 8);

    Pushbuffer::Push(NV097_SET_DOT_RGBMAPPING, 0);

    Pushbuffer::Push(NV097_SET_SHADE_MODEL, NV097_SET_SHADE_MODEL_SMOOTH);
    Pushbuffer::Push(NV097_SET_FLAT_SHADE_OP, NV097_SET_FLAT_SHADE_OP_VERTEX_LAST);
    Pushbuffer::Push(NV097_SET_FOG_ENABLE, false);

    Pushbuffer::Push(NV097_SET_FRONT_FACE, NV097_SET_FRONT_FACE_V_CW);
    Pushbuffer::Push(NV097_SET_CULL_FACE, NV097_SET_CULL_FACE_V_BACK);
    Pushbuffer::Push(NV097_SET_CULL_FACE_ENABLE, true);

    Pushbuffer::Push(NV097_SET_COLOR_MASK,
                     NV097_SET_COLOR_MASK_BLUE_WRITE_ENABLE | NV097_SET_COLOR_MASK_GREEN_WRITE_ENABLE |
                         NV097_SET_COLOR_MASK_RED_WRITE_ENABLE | NV097_SET_COLOR_MASK_ALPHA_WRITE_ENABLE);

    Pushbuffer::Push(NV097_SET_DEPTH_TEST_ENABLE, false);
    Pushbuffer::Push(NV097_SET_DEPTH_MASK, true);
    Pushbuffer::Push(NV097_SET_DEPTH_FUNC, NV097_SET_DEPTH_FUNC_V_LESS);
    Pushbuffer::Push(NV097_SET_STENCIL_TEST_ENABLE, false);
    Pushbuffer::Push(NV097_SET_STENCIL_MASK, 0xFF);
    // If the stencil comparison fails, leave the value in the stencil buffer alone.
    Pushbuffer::Push(NV097_SET_STENCIL_OP_FAIL, NV097_SET_STENCIL_OP_V_KEEP);
    // If the stencil comparison passes but the depth comparison fails, leave the stencil buffer alone.
    Pushbuffer::Push(NV097_SET_STENCIL_OP_ZFAIL, NV097_SET_STENCIL_OP_V_KEEP);
    // If the stencil comparison passes and the depth comparison passes, leave the stencil buffer alone.
    Pushbuffer::Push(NV097_SET_STENCIL_OP_ZPASS, NV097_SET_STENCIL_OP_V_KEEP);
    Pushbuffer::Push(NV097_SET_STENCIL_FUNC_REF, 0x7F);

    Pushbuffer::Push(NV097_SET_NORMALIZATION_ENABLE, false);
    Pushbuffer::End();
  }

  NV2AState::SetWindowClipExclusive(false);
  // Note, setting the first clip region will cause the hardware to also set all subsequent regions.
  NV2AState::SetWindowClip(state.GetFramebufferWidth(), state.GetFramebufferHeight());

  state.SetBlend();

  state.ClearInputColorCombiners();
  state.ClearInputAlphaCombiners();
  state.ClearOutputColorCombiners();
  state.ClearOutputAlphaCombiners();

  state.SetCombinerControl(1);
  state.SetInputColorCombiner(0, NV2AState::SRC_DIFFUSE, false, NV2AState::MAP_UNSIGNED_IDENTITY, NV2AState::SRC_ZERO,
                              false, NV2AState::MAP_UNSIGNED_INVERT);
  state.SetInputAlphaCombiner(0, NV2AState::SRC_DIFFUSE, true, NV2AState::MAP_UNSIGNED_IDENTITY, NV2AState::SRC_ZERO,
                              false, NV2AState::MAP_UNSIGNED_INVERT);

  state.SetOutputColorCombiner(0, NV2AState::DST_DISCARD, NV2AState::DST_DISCARD, NV2AState::DST_R0);
  state.SetOutputAlphaCombiner(0, NV2AState::DST_DISCARD, NV2AState::DST_DISCARD, NV2AState::DST_R0);

  for (auto i = 0; i < 4; ++i) {
    auto &stage = state.GetTextureStage(i);
    stage.SetUWrap(TextureStage::WRAP_CLAMP_TO_EDGE, false);
    stage.SetVWrap(TextureStage::WRAP_CLAMP_TO_EDGE, false);
    stage.SetPWrap(TextureStage::WRAP_CLAMP_TO_EDGE, false);
    stage.SetQWrap(false);

    stage.SetEnabled(false);
    stage.SetCubemapEnable(false);
    stage.SetFilter(0);
    stage.SetAlphaKillEnable(false);
    stage.SetColorKeyMode(TextureStage::CKM_DISABLE);
    stage.SetLODClamp(0, 4095);

    stage.SetTextureMatrixEnable(false);
  }

  state.SetXDKDefaultViewportAndFixedFunctionMatrices();
  state.SetDepthBufferFloatMode(false);

  state.SetVertexShaderProgram(nullptr);

  const auto &texture_format = GetTextureFormatInfo(NV097_SET_TEXTURE_FORMAT_COLOR_SZ_X8R8G8B8);
  state.SetTextureFormat(texture_format, 0);
  state.SetDefaultTextureParams(0);
  state.SetTextureFormat(texture_format, 1);
  state.SetDefaultTextureParams(1);
  state.SetTextureFormat(texture_format, 2);
  state.SetDefaultTextureParams(2);
  state.SetTextureFormat(texture_format, 3);
  state.SetDefaultTextureParams(3);

  state.SetTextureStageEnabled(0, false);
  state.SetTextureStageEnabled(1, false);
  state.SetTextureStageEnabled(2, false);
  state.SetTextureStageEnabled(3, false);
  state.SetShaderStageProgram(NV2AState::STAGE_NONE);
  state.SetShaderStageInput(0, 0);

  state.ClearAllVertexAttributeStrideOverrides();
}

void WasteTime(NV2AState &state, uint32_t draw_iterations) {
  state.Clear();
  state.SetDiffuse(0xFFFF00FF);
  for (auto i = 0; i < draw_iterations; ++i) {
    state.DrawScreenQuad(0, 0, state.GetFramebufferWidthF(), state.GetFramebufferHeightF(), 1.0);
  }
}

void RenderScene(NV2AState &state, uint32_t program_start, uint32_t last_frame_start, uint32_t frame_start,
                 uint32_t draw_iterations, float current_fps, const VIDEO_MODE &video_mode) {
  static constexpr float kMarginHorizontal = 10.f;
  static constexpr float kMarginTop = 112.f;
  static constexpr uint32_t kOn60FPS = 0xFFFF0000;
  static constexpr uint32_t kOn30FPS = 0xFF00FF00;
  static constexpr uint32_t kOn15FPS = 0xFF0000FF;
  static constexpr uint32_t kOff = 0xFF222222;
  static constexpr uint32_t kMaxAllowedFrameDelta = 20;  // Ideally 16.666
  static uint32_t frame_drops = 0;

  float quad_width = floorf((state.GetFramebufferWidthF() - (kMarginHorizontal * 2.f)) / 60.f);
  float quad_height = floorf((state.GetFramebufferHeightF() - kMarginTop) / 3.f);

  static constexpr float kQuadZ = 1.f;

  state.SetFinalCombiner0Just(NV2AState::SRC_DIFFUSE);
  state.SetFinalCombiner1Just(NV2AState::SRC_ZERO, true, true);

  auto draw_quad = [&state, quad_height](float x, float y, float width, uint32_t color) {
    state.SetDiffuse(color);
    state.DrawScreenQuad(x, y, x + width, y + quad_height, kQuadZ);
  };

  auto elapsed = frame_start - program_start;
  auto frame_elapsed = frame_start - last_frame_start;

  frame_drops <<= 1;
  if (frame_elapsed > kMaxAllowedFrameDelta) {
    ++frame_drops;
  }

  auto index_60 = (elapsed * 60 / 1000) % 60;
  auto index_30 = (elapsed * 30 / 1000) % 30;
  auto index_15 = (elapsed * 15 / 1000) % 15;

  const float y_60 = kMarginTop;
  const float y_30 = y_60 + quad_height;
  const float y_15 = y_30 + quad_height;

  float left = kMarginHorizontal;
  for (auto x = 0; x < 60; ++x) {
    draw_quad(left, y_60, quad_width, x == index_60 ? kOn60FPS : kOff);
    left += quad_width;
  }

  left = kMarginHorizontal;
  for (auto x = 0; x < 30; ++x) {
    draw_quad(left, y_30, quad_width * 2, x == index_30 ? kOn30FPS : kOff);
    left += quad_width * 2;
  }

  left = kMarginHorizontal;
  for (auto x = 0; x < 15; ++x) {
    draw_quad(left, y_15, quad_width * 4, x == index_15 ? kOn15FPS : kOff);
    left += quad_width * 4;
  }

  // Leverage flicker fusion to make lower framerates clearly apparent
  {
    static constexpr float kColorSwatchWidth = 16.f;
    static constexpr float kColorSwatchHeight = 64.f;
    static constexpr uint32_t kReferenceSwatchColor = 0xFF00BABA;
    static constexpr float kColorSwatchTop = 16.f;

    const float kSwatchLeft = state.GetFramebufferWidthF() - kColorSwatchWidth * 2.f;

    state.SetDiffuse(kReferenceSwatchColor);
    state.DrawScreenQuad(kSwatchLeft, kColorSwatchTop, kSwatchLeft + kColorSwatchWidth,
                         kColorSwatchTop + kColorSwatchHeight, kQuadZ);

    state.SetDiffuse(index_60 & 0x01 ? 0xFF00FF00 : 0xFF0000FF);
    state.DrawScreenQuad(kSwatchLeft + kColorSwatchWidth, kColorSwatchTop, kSwatchLeft + kColorSwatchWidth * 2.f,
                         kColorSwatchTop + kColorSwatchHeight, kQuadZ);
  }

  pb_print("Back: exit, A/X/up inc, B/Y/down dec  Mode: %dx%d %dHz\n", video_mode.width, video_mode.height,
           video_mode.refresh);
  char fps[32];
  if (current_fps == 0) {
    snprintf(fps, sizeof(fps), "<WHITE>");
  } else if (current_fps < 0) {
    snprintf(fps, sizeof(fps), "<CALCULATING>");
  } else {
    snprintf_(fps, sizeof(fps), "%0.4f", current_fps);
  }

  pb_print("Elapsed ms %3u  FPS %s\n", frame_elapsed, fps);
  pb_print("Iterations %u\n", draw_iterations);

  for (auto i = 0; i < 32; ++i) {
    if (frame_drops & (1 << i)) {
      pb_printat(3, i, "D");
    }
  }

  pb_draw_text_screen();
}

/* Main program function */
int main() {
  debugPrint("Set video mode");
  if (!XVideoSetMode(kFramebufferWidth, kFramebufferHeight, kBitsPerPixel, REFRESH_DEFAULT)) {
    debugPrint("Failed to set video mode\n");
    Sleep(2000);
    return 1;
  }

  int status = pb_init();
  if (status) {
    debugPrint("pb_init Error %d\n", status);
    Sleep(2000);
    return 1;
  }

  debugPrint("Initializing...");
  pb_show_debug_screen();

  if (SDL_Init(SDL_INIT_GAMECONTROLLER)) {
    debugPrint("Failed to initialize SDL_GAMECONTROLLER.");
    debugPrint("%s", SDL_GetError());
    pb_show_debug_screen();
    Sleep(2000);
    return 1;
  }

  pb_show_front_screen();
  debugClearScreen();

  NV2AState state(kFramebufferWidth, kFramebufferHeight, kMaxTextureSize, kMaxTextureSize, kMaxTextureDepth);
  Initialize(state);

  bool running = true;
  auto program_start = GetTickCount();
  auto last_frame_start = program_start;

  static constexpr auto kMaxDrawIterations = 0xFFFF;
  static constexpr auto kDrawIterationIncTiny = 1;
  static constexpr auto kDrawIterationIncSmall = 100;
  static constexpr auto kDrawIterationIncLarge = 1000;
  uint32_t draw_iterations = 0;

  static constexpr auto kFPSCounterFrames = 60;
  int32_t fps_counter_frames = -1;

  LARGE_INTEGER perf_freq;
  QueryPerformanceFrequency(&perf_freq);

  LARGE_INTEGER start_counter;
  float current_fps = 0.0f;

  VIDEO_MODE video_mode = XVideoGetMode();

  auto handle_button = [&running, &draw_iterations, &fps_counter_frames, &start_counter,
                        &current_fps](const SDL_ControllerButtonEvent &event) {
    switch (event.button) {
      case SDL_CONTROLLER_BUTTON_BACK:
        running = false;
        break;

      case SDL_CONTROLLER_BUTTON_A:
        if (draw_iterations + kDrawIterationIncSmall < kMaxDrawIterations) {
          draw_iterations += kDrawIterationIncSmall;
        } else {
          draw_iterations = kMaxDrawIterations;
        }
        break;

      case SDL_CONTROLLER_BUTTON_B:
        if (draw_iterations >= kDrawIterationIncSmall) {
          draw_iterations -= kDrawIterationIncSmall;
        } else {
          draw_iterations = 0;
        }
        break;

      case SDL_CONTROLLER_BUTTON_X:
        if (draw_iterations + kDrawIterationIncLarge < kMaxDrawIterations) {
          draw_iterations += kDrawIterationIncLarge;
        } else {
          draw_iterations = kMaxDrawIterations;
        }
        break;

      case SDL_CONTROLLER_BUTTON_Y:
        if (draw_iterations >= kDrawIterationIncLarge) {
          draw_iterations -= kDrawIterationIncLarge;
        } else {
          draw_iterations = 0;
        }
        break;

      case SDL_CONTROLLER_BUTTON_DPAD_UP:
        if (draw_iterations + kDrawIterationIncTiny < kMaxDrawIterations) {
          draw_iterations += kDrawIterationIncTiny;
        } else {
          draw_iterations = kMaxDrawIterations;
        }
        break;

      case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
        if (draw_iterations >= kDrawIterationIncTiny) {
          draw_iterations -= kDrawIterationIncTiny;
        } else {
          draw_iterations = 0;
        }
        break;

      case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
        if (fps_counter_frames < 0) {
          QueryPerformanceCounter(&start_counter);
          fps_counter_frames = kFPSCounterFrames;
          current_fps = -1.f;
        }

      default:
        break;
    }
  };

  while (running) {
    auto frame_start = GetTickCount();

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
        case SDL_CONTROLLERDEVICEADDED: {
          SDL_GameController *controller = SDL_GameControllerOpen(event.cdevice.which);
          if (!controller) {
            debugPrint("Failed to handle controller add event.");
            debugPrint("%s", SDL_GetError());
            running = false;
          }
        } break;

        case SDL_CONTROLLERDEVICEREMOVED: {
          SDL_GameController *controller = SDL_GameControllerFromInstanceID(event.cdevice.which);
          SDL_GameControllerClose(controller);
        } break;

        case SDL_CONTROLLERBUTTONUP:
          handle_button(event.cbutton);
          break;

        default:
          break;
      }
    }

    pb_wait_for_vbl();
    pb_reset();
    pb_target_back_buffer();

    /* Clear depth & stencil buffers */
    pb_erase_depth_stencil_buffer(0, 0, kFramebufferWidth, kFramebufferHeight);
    state.Clear();
    state.EraseText();

    while (pb_busy()) {
      /* Wait for completion... */
    }

    WasteTime(state, draw_iterations);
    RenderScene(state, program_start, last_frame_start, frame_start, draw_iterations, current_fps, video_mode);
    last_frame_start = frame_start;

    while (pb_busy()) {
    }
    while (pb_finished()) {
    }

    if (fps_counter_frames > 0) {
      if (--fps_counter_frames == 0) {
        fps_counter_frames = -1;
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);

        int64_t elapsed_ticks = now.QuadPart - start_counter.QuadPart;
        current_fps = static_cast<float>(kFPSCounterFrames * perf_freq.QuadPart) / static_cast<float>(elapsed_ticks);
      }
    }
  }

  pb_kill();
  return 0;
}
