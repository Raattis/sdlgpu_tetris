// Modified from https://github.com/libsdl-org/SDL/blob/main/test/testgpu_spinning_cube.c

/*
* Compiling:
*	- Windows (Visual Studio 2022 with cmake project):
*		- right-click folder background, select open with visual studio
*		- wait for cmake configuration to finish
*		- compile and run
*	- Windows (Visual Studio 2022 solution):
*		- start "x64 Native Tools Command Prompt for Visual Studio 2022"
*		- navigate to this folder, run "cmake -S . -B build"
*		- goto the newly created build folder and open the .sln file
*		- compile & run normally
*	- Linux & Mac:
*		- cmake -S . -B build
*		- cd build
*		- make
*/

#include <stdlib.h>

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_assert.h>

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>

/* Regenerate the shaders with testgpu/build-shaders.sh */
#include "testgpu/testgpu_spirv.h"
#include "testgpu/testgpu_dxbc.h"
#include "testgpu/testgpu_dxil.h"
#include "testgpu/testgpu_metallib.h"

#define TESTGPU_SUPPORTED_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB)

#define CHECK_CREATE(var, thing) do { if (!(var)) { SDL_Log("Failed to create %s: %s\n", thing, SDL_GetError()); SDL_assert_always(0 && "CHECK_CREATE for " thing " var:" #var " failed"); } } while(0)

typedef struct RenderState
{
	SDL_GPUBuffer* buf_vertex;
	SDL_GPUGraphicsPipeline* pipeline;
	SDL_GPUSampleCount sample_count;
} RenderState;

typedef struct WindowState
{
	int angle_x, angle_y, angle_z;
	SDL_GPUTexture* tex_depth, * tex_msaa, * tex_resolve;
	Uint32 prev_drawablew, prev_drawableh;
} WindowState;

typedef struct Tetris
{
	Uint64 prev_ns;
	Uint64 drop_timer;
	Uint32 score;
	Uint32 lines;
	Uint8 board[220];
	Uint8 rot;
	Uint8 x;
	Uint8 y;
	Uint8 piece;
	Uint8 input_rot;
	Uint8 input_down;
	Uint8 input_left;
	Uint8 input_right;
	Uint8 input_drop;
} Tetris;

typedef struct AppState
{
	Uint32 frames;
	SDL_GPUDevice* gpu_device;
	RenderState render_state;
	SDLTest_CommonState* state;
	WindowState* window_states;
	Tetris* tetris;
} AppState;

/*
 * Simulates desktop's glRotatef. The matrix is returned in column-major
 * order.
 */
static void
rotate_matrix(float angle, float x, float y, float z, float* r)
{
	float radians, c, s, c1, u[3], length;
	int i, j;

	radians = angle * SDL_PI_F / 180.0f;

	c = SDL_cosf(radians);
	s = SDL_sinf(radians);

	c1 = 1.0f - SDL_cosf(radians);

	length = (float)SDL_sqrt(x * x + y * y + z * z);

	u[0] = x / length;
	u[1] = y / length;
	u[2] = z / length;

	for (i = 0; i < 16; i++) {
		r[i] = 0.0;
	}

	r[15] = 1.0;

	for (i = 0; i < 3; i++) {
		r[i * 4 + (i + 1) % 3] = u[(i + 2) % 3] * s;
		r[i * 4 + (i + 2) % 3] = -u[(i + 1) % 3] * s;
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? c : 0.0f);
		}
	}
}

/*
 * Simulates gluPerspectiveMatrix
 */
static void
perspective_matrix(float fovy, float aspect, float znear, float zfar, float* r)
{
	int i;
	float f;

	f = 1.0f / SDL_tanf(fovy * 0.5f);

	for (i = 0; i < 16; i++) {
		r[i] = 0.0;
	}

	r[0] = f / aspect;
	r[5] = f;
	r[10] = (znear + zfar) / (znear - zfar);
	r[11] = -1.0f;
	r[14] = (2.0f * znear * zfar) / (znear - zfar);
	r[15] = 0.0f;
}

/*
 * Multiplies lhs by rhs and writes out to r. All matrices are 4x4 and column
 * major. In-place multiplication is supported.
 */
static void
multiply_matrix(float* lhs, float* rhs, float* r)
{
	int i, j, k;
	float tmp[16];

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			tmp[j * 4 + i] = 0.0;

			for (k = 0; k < 4; k++) {
				tmp[j * 4 + i] += lhs[k * 4 + i] * rhs[j * 4 + k];
			}
		}
	}

	for (i = 0; i < 16; i++) {
		r[i] = tmp[i];
	}
}

typedef struct VertexData
{
	float x, y, z; /* 3D data. Vertex range -0.5..0.5 in all axes. Z -0.5 is near, 0.5 is far. */
	float red, green, blue;  /* intensity 0 to 1 (alpha is always 1). */
} VertexData;

static const VertexData vertex_data[] = {
	/* Front face. */
	/* Bottom left */
	{ -0.5,  0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
	{  0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */
	{ -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */

	/* Top right */
	{ -0.5, 0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
	{ 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
	{ 0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */

	/* Left face */
	/* Bottom left */
	{ -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
	{ -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
	{ -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

	/* Top right */
	{ -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
	{ -0.5,  0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
	{ -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */

	/* Top face */
	/* Bottom left */
	{ -0.5, 0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
	{  0.5, 0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
	{ -0.5, 0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */

	/* Top right */
	{ -0.5, 0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
	{  0.5, 0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
	{  0.5, 0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */

	/* Right face */
	/* Bottom left */
	{ 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
	{ 0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */
	{ 0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */

	/* Top right */
	{ 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
	{ 0.5,  0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
	{ 0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */

	/* Back face */
	/* Bottom left */
	{  0.5,  0.5, 0.5, 0.0, 0.0, 0.0 }, /* black */
	{ -0.5, -0.5, 0.5, 0.0, 1.0, 1.0 }, /* cyan */
	{  0.5, -0.5, 0.5, 1.0, 0.0, 1.0 }, /* magenta */

	/* Top right */
	{  0.5,  0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
	{ -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
	{ -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

	/* Bottom face */
	/* Bottom left */
	{ -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
	{  0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */
	{ -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

	/* Top right */
	{ -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
	{  0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */
	{  0.5, -0.5,  0.5, 1.0, 0.0, 1.0 } /* magenta */
};

static SDL_GPUTexture*
CreateDepthTexture(AppState* appstate, Uint32 drawablew, Uint32 drawableh)
{
	SDL_GPUTextureCreateInfo createinfo;
	SDL_GPUTexture* result;

	SDL_GPUDevice* gpu_device = appstate->gpu_device;
	SDLTest_CommonState* state = appstate->state;
	RenderState* render_state = &appstate->render_state;

	createinfo.type = SDL_GPU_TEXTURETYPE_2D;
	createinfo.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
	createinfo.width = drawablew;
	createinfo.height = drawableh;
	createinfo.layer_count_or_depth = 1;
	createinfo.num_levels = 1;
	createinfo.sample_count = render_state->sample_count;
	createinfo.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
	createinfo.props = 0;

	result = SDL_CreateGPUTexture(gpu_device, &createinfo);
	CHECK_CREATE(result, "Depth Texture");

	return result;
}

static SDL_GPUTexture*
CreateMSAATexture(AppState* appstate, Uint32 drawablew, Uint32 drawableh)
{
	SDL_GPUTextureCreateInfo createinfo;
	SDL_GPUTexture* result;

	SDL_GPUDevice* gpu_device = appstate->gpu_device;
	SDLTest_CommonState* state = appstate->state;
	RenderState* render_state = &appstate->render_state;

	if (render_state->sample_count == SDL_GPU_SAMPLECOUNT_1) {
		return NULL;
	}

	createinfo.type = SDL_GPU_TEXTURETYPE_2D;
	createinfo.format = SDL_GetGPUSwapchainTextureFormat(gpu_device, state->windows[0]);
	createinfo.width = drawablew;
	createinfo.height = drawableh;
	createinfo.layer_count_or_depth = 1;
	createinfo.num_levels = 1;
	createinfo.sample_count = render_state->sample_count;
	createinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
	createinfo.props = 0;

	result = SDL_CreateGPUTexture(gpu_device, &createinfo);
	CHECK_CREATE(result, "MSAA Texture");

	return result;
}

static SDL_GPUTexture*
CreateResolveTexture(AppState* appstate, Uint32 drawablew, Uint32 drawableh)
{
	SDL_GPUTextureCreateInfo createinfo;
	SDL_GPUTexture* result;

	SDL_GPUDevice* gpu_device = appstate->gpu_device;
	SDLTest_CommonState* state = appstate->state;
	RenderState* render_state = &appstate->render_state;

	if (render_state->sample_count == SDL_GPU_SAMPLECOUNT_1) {
		return NULL;
	}

	createinfo.type = SDL_GPU_TEXTURETYPE_2D;
	createinfo.format = SDL_GetGPUSwapchainTextureFormat(gpu_device, state->windows[0]);
	createinfo.width = drawablew;
	createinfo.height = drawableh;
	createinfo.layer_count_or_depth = 1;
	createinfo.num_levels = 1;
	createinfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
	createinfo.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	createinfo.props = 0;

	result = SDL_CreateGPUTexture(gpu_device, &createinfo);
	CHECK_CREATE(result, "Resolve Texture");

	return result;
}

static void get_piece_coords(Uint8 piece, int x, int y, Uint8 rot, int* xs_out, int* ys_out)
{
	//                  L          J          S          Z          T          O          I
	const int xs[] = { -1,-1, 1,  -1, 1, 1,  -1, 0, 1,  -1, 0, 1,  -1, 0, 1,  -1,-1, 0,  -2,-1, 1 };
	const int ys[] = { -1, 0, 0,   0, 0,-1,  -1,-1, 0,   0,-1,-1,   0,-1, 0,  -1, 0,-1,   0, 0, 0 };
	int o = (piece - 1) * 3;
	switch (rot)
	{
	case 0:
		xs_out[0] = x + xs[o + 0]; xs_out[1] = x + xs[o + 1]; xs_out[2] = x + xs[o + 2];
		ys_out[0] = y + ys[o + 0]; ys_out[1] = y + ys[o + 1]; ys_out[2] = y + ys[o + 2];
		break;
	case 1:
		xs_out[0] = x + ys[o + 0]; xs_out[1] = x + ys[o + 1]; xs_out[2] = x + ys[o + 2];
		ys_out[0] = y - xs[o + 0]; ys_out[1] = y - xs[o + 1]; ys_out[2] = y - xs[o + 2];
		break;
	case 2:
		xs_out[0] = x - xs[o + 0]; xs_out[1] = x - xs[o + 1]; xs_out[2] = x - xs[o + 2];
		ys_out[0] = y - ys[o + 0]; ys_out[1] = y - ys[o + 1]; ys_out[2] = y - ys[o + 2];
		break;
	case 3:
		xs_out[0] = x - ys[o + 0]; xs_out[1] = x - ys[o + 1]; xs_out[2] = x - ys[o + 2];
		ys_out[0] = y + xs[o + 0]; ys_out[1] = y + xs[o + 1]; ys_out[2] = y + xs[o + 2];
		break;
	default:
		SDL_assert_always(!"rot < 4");
	}

	xs_out[3] = x;
	ys_out[3] = y;
}

static int try_move(Tetris* tetris, int dx, int dy, int drot)
{
	//                   L  J  S  Z  T  O  I
	const int rots[] = { 4, 4, 2, 2, 4, 1, 2 };
	int xs[4] = { 0 };
	int ys[4] = { 0 };
	int x = tetris->x + dx;
	int y = tetris->y + dy;
	int rot = (tetris->rot + drot + 4) % rots[tetris->piece - 1];
	get_piece_coords(tetris->piece, x, y, rot, xs, ys);
	if (xs[0] < 0 || xs[0] >= 10 || ys[0] < 0 || ys[0] >= 22 || (tetris->board[xs[0] + ys[0] * 10] != 0) ||
		xs[1] < 0 || xs[1] >= 10 || ys[1] < 0 || ys[1] >= 22 || (tetris->board[xs[1] + ys[1] * 10] != 0) ||
		xs[2] < 0 || xs[2] >= 10 || ys[2] < 0 || ys[2] >= 22 || (tetris->board[xs[2] + ys[2] * 10] != 0) ||
		xs[3] < 0 || xs[3] >= 10 || ys[3] < 0 || ys[3] >= 22 || (tetris->board[xs[3] + ys[3] * 10] != 0))
	{
		return 0;
	}

	tetris->x = x;
	tetris->y = y;
	tetris->rot = rot;
	return 1;
}

void glue(Tetris* tetris)
{
	int xs[4] = { 0 };
	int ys[4] = { 0 };
	get_piece_coords(tetris->piece, tetris->x, tetris->y, tetris->rot, xs, ys);

	tetris->board[xs[0] + ys[0] * 10] = tetris->piece;
	tetris->board[xs[1] + ys[1] * 10] = tetris->piece;
	tetris->board[xs[2] + ys[2] * 10] = tetris->piece;
	tetris->board[xs[3] + ys[3] * 10] = tetris->piece;

	tetris->piece = (tetris->piece % 7) + 1;
	tetris->x = 5;	
	tetris->y = 20;
	tetris->rot = 0;

	int cleared = 0;
	for (int y = 0; y < 22; ++y)
	{
		int count = 0;
		for (int x = 0; x < 10; ++x)
		{
			int old = tetris->board[x + y * 10];
			count += old != 0;
			tetris->board[x + y * 10] = 0;
			tetris->board[x + (y - cleared) * 10] = old;
		}

		cleared += count == 10;
	}

	tetris->score += (tetris->lines / 10 + 1) << cleared;
	tetris->lines += cleared;

	if (!try_move(tetris, 0, 0, 0))
	{
		// Game over
		tetris->piece = 0;
	}
}

static void Render(AppState* appstate, SDL_Window* window, const int windownum)
{
	WindowState* winstate = &appstate->window_states[windownum];
	SDL_GPUTexture* swapchainTexture;
	SDL_GPUColorTargetInfo color_target;
	SDL_GPUDepthStencilTargetInfo depth_target;
	float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16];
	SDL_GPUCommandBuffer* cmd;
	SDL_GPURenderPass* pass;
	SDL_GPUBufferBinding vertex_binding;
	SDL_GPUBlitInfo blit_info;
	int drawablew, drawableh;

	SDL_GPUDevice* gpu_device = appstate->gpu_device;
	SDLTest_CommonState* state = appstate->state;
	RenderState* render_state = &appstate->render_state;

	/* Acquire the swapchain texture */
	cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
	if (!cmd) {
		SDL_Log("Failed to acquire command buffer :%s", SDL_GetError());
		SDL_assert_always(0);
		return;
	}
	if (!SDL_AcquireGPUSwapchainTexture(cmd, state->windows[windownum], &swapchainTexture)) {
		SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
		SDL_assert_always(0);
		return;
	}

	if (swapchainTexture == NULL) {
		/* No swapchain was acquired, probably too many frames in flight */
		SDL_SubmitGPUCommandBuffer(cmd);
		return;
	}

	SDL_GetWindowSizeInPixels(window, &drawablew, &drawableh);

	/*
	* Do some rotation with Euler angles. It is not a fixed axis as
	* quaterions would be, but the effect is cool.
	*/
	rotate_matrix((float)winstate->angle_x, 1.0f, 0.0f, 0.0f, matrix_modelview);
	rotate_matrix((float)winstate->angle_y, 0.0f, 1.0f, 0.0f, matrix_rotate);

	multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

	rotate_matrix((float)winstate->angle_z, 0.0f, 1.0f, 0.0f, matrix_rotate);

	multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

	perspective_matrix(45.0f, (float)drawablew / drawableh, 0.01f, 100.0f, matrix_perspective);

	winstate->angle_x += 3;
	winstate->angle_y += 2;
	winstate->angle_z += 1;

	if (winstate->angle_x >= 360) winstate->angle_x -= 360;
	if (winstate->angle_x < 0) winstate->angle_x += 360;
	if (winstate->angle_y >= 360) winstate->angle_y -= 360;
	if (winstate->angle_y < 0) winstate->angle_y += 360;
	if (winstate->angle_z >= 360) winstate->angle_z -= 360;
	if (winstate->angle_z < 0) winstate->angle_z += 360;

	/* Resize the depth buffer if the window size changed */

	if (winstate->prev_drawablew != drawablew || winstate->prev_drawableh != drawableh) {
		SDL_ReleaseGPUTexture(gpu_device, winstate->tex_depth);
		SDL_ReleaseGPUTexture(gpu_device, winstate->tex_msaa);
		SDL_ReleaseGPUTexture(gpu_device, winstate->tex_resolve);
		winstate->tex_depth = CreateDepthTexture(appstate, drawablew, drawableh);
		winstate->tex_msaa = CreateMSAATexture(appstate, drawablew, drawableh);
		winstate->tex_resolve = CreateResolveTexture(appstate, drawablew, drawableh);
	}
	winstate->prev_drawablew = drawablew;
	winstate->prev_drawableh = drawableh;

	/* Set up the pass */

	SDL_zero(color_target);
	color_target.clear_color.a = 1.0f;
	if (winstate->tex_msaa) {
		color_target.load_op = SDL_GPU_LOADOP_CLEAR;
		color_target.store_op = SDL_GPU_STOREOP_RESOLVE;
		color_target.texture = winstate->tex_msaa;
		color_target.resolve_texture = winstate->tex_resolve;
		color_target.cycle = true;
		color_target.cycle_resolve_texture = true;
	}
	else {
		color_target.load_op = SDL_GPU_LOADOP_CLEAR;
		color_target.store_op = SDL_GPU_STOREOP_STORE;
		color_target.texture = swapchainTexture;
	}

	SDL_zero(depth_target);
	depth_target.clear_depth = 1.0f;
	depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
	depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;
	depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
	depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
	depth_target.texture = winstate->tex_depth;
	depth_target.cycle = true;

	/* Set up the bindings */

	vertex_binding.buffer = render_state->buf_vertex;
	vertex_binding.offset = 0;

	/* Draw the cube(s)! */

	pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, &depth_target);
	SDL_BindGPUGraphicsPipeline(pass, render_state->pipeline);
	SDL_BindGPUVertexBuffers(pass, 0, &vertex_binding, 1);

	matrix_modelview[14] = -22.0f;

	Tetris* tetris = appstate->tetris;
	int piece_xs[4] = { 0 };
	int piece_ys[4] = { 0 };
	get_piece_coords(tetris->piece, tetris->x, tetris->y, tetris->rot, piece_xs, piece_ys);

	for (int i = 0; i < 220; ++i)
	{
		Uint8 color = tetris->board[i];
		int x = i % 10;
		int y = i / 10;
		color = (
			(piece_xs[0] == x && piece_ys[0] == y) ||
			(piece_xs[1] == x && piece_ys[1] == y) ||
			(piece_xs[2] == x && piece_ys[2] == y) ||
			(piece_xs[3] == x && piece_ys[3] == y)
			) ? tetris->piece : color;
		if (color == 0)
		{
			continue;
		}
		matrix_modelview[12] = (float)x - 4.5f;
		matrix_modelview[13] = (float)y - 10.5f;

		float matrix_final[16];
		multiply_matrix(matrix_perspective, matrix_modelview, matrix_final);
		SDL_PushGPUVertexUniformData(cmd, 0, matrix_final, sizeof(matrix_final));
		SDL_DrawGPUPrimitives(pass, 36, 1, 0, 0);
	}

	SDL_EndGPURenderPass(pass);

	/* Blit MSAA resolve target to swapchain, if needed */
	if (render_state->sample_count > SDL_GPU_SAMPLECOUNT_1) {
		SDL_zero(blit_info);
		blit_info.source.texture = winstate->tex_resolve;
		blit_info.source.w = drawablew;
		blit_info.source.h = drawableh;

		blit_info.destination.texture = swapchainTexture;
		blit_info.destination.w = drawablew;
		blit_info.destination.h = drawableh;

		blit_info.load_op = SDL_GPU_LOADOP_DONT_CARE;
		blit_info.filter = SDL_GPU_FILTER_LINEAR;

		SDL_BlitGPUTexture(cmd, &blit_info);
	}

	/* Submit the command buffer! */
	SDL_SubmitGPUCommandBuffer(cmd);

	appstate->frames += 1;
}

static SDL_GPUShader*
load_shader(AppState* appstate, bool is_vertex)
{
	SDL_GPUDevice* gpu_device = appstate->gpu_device;
	SDLTest_CommonState* state = appstate->state;
	RenderState* render_state = &appstate->render_state;

	SDL_GPUShaderCreateInfo createinfo;
	createinfo.num_samplers = 0;
	createinfo.num_storage_buffers = 0;
	createinfo.num_storage_textures = 0;
	createinfo.num_uniform_buffers = is_vertex ? 1 : 0;
	createinfo.props = 0;

	SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(gpu_device);
	if (format & SDL_GPU_SHADERFORMAT_DXBC) {
		createinfo.format = SDL_GPU_SHADERFORMAT_DXBC;
		createinfo.code = is_vertex ? D3D11_CubeVert : D3D11_CubeFrag;
		createinfo.code_size = is_vertex ? SDL_arraysize(D3D11_CubeVert) : SDL_arraysize(D3D11_CubeFrag);
		createinfo.entrypoint = is_vertex ? "VSMain" : "PSMain";
	}
	else if (format & SDL_GPU_SHADERFORMAT_DXIL) {
		createinfo.format = SDL_GPU_SHADERFORMAT_DXIL;
		createinfo.code = is_vertex ? D3D12_CubeVert : D3D12_CubeFrag;
		createinfo.code_size = is_vertex ? SDL_arraysize(D3D12_CubeVert) : SDL_arraysize(D3D12_CubeFrag);
		createinfo.entrypoint = is_vertex ? "VSMain" : "PSMain";
	}
	else if (format & SDL_GPU_SHADERFORMAT_METALLIB) {
		createinfo.format = SDL_GPU_SHADERFORMAT_METALLIB;
		createinfo.code = is_vertex ? cube_vert_metallib : cube_frag_metallib;
		createinfo.code_size = is_vertex ? cube_vert_metallib_len : cube_frag_metallib_len;
		createinfo.entrypoint = is_vertex ? "vs_main" : "fs_main";
	}
	else {
		createinfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
		createinfo.code = is_vertex ? cube_vert_spv : cube_frag_spv;
		createinfo.code_size = is_vertex ? cube_vert_spv_len : cube_frag_spv_len;
		createinfo.entrypoint = "main";
	}

	createinfo.stage = is_vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
	return SDL_CreateGPUShader(gpu_device, &createinfo);
}

static SDL_AppResult
init_render_state(AppState* appstate, int msaa)
{
	SDL_GPUCommandBuffer* cmd;
	SDL_GPUTransferBuffer* buf_transfer;
	void* map;
	SDL_GPUTransferBufferLocation buf_location;
	SDL_GPUBufferRegion dst_region;
	SDL_GPUCopyPass* copy_pass;
	SDL_GPUBufferCreateInfo buffer_desc;
	SDL_GPUTransferBufferCreateInfo transfer_buffer_desc;
	SDL_GPUGraphicsPipelineCreateInfo pipelinedesc;
	SDL_GPUColorTargetDescription color_target_desc;
	Uint32 drawablew, drawableh;
	SDL_GPUVertexAttribute vertex_attributes[2];
	SDL_GPUVertexBufferDescription vertex_buffer_desc;
	SDL_GPUShader* vertex_shader;
	SDL_GPUShader* fragment_shader;

	appstate->gpu_device = SDL_CreateGPUDevice(
		TESTGPU_SUPPORTED_FORMATS,
		true,
		appstate->state->gpudriver
	);
	CHECK_CREATE(appstate->gpu_device, "GPU device");

	/* Claim the windows */
	for (int i = 0; i < appstate->state->num_windows; ++i) {
		SDL_ClaimWindowForGPUDevice(
			appstate->gpu_device,
			appstate->state->windows[i]
		);
	}

	/* Create shaders */

	vertex_shader = load_shader(appstate, true);
	CHECK_CREATE(vertex_shader, "Vertex Shader");
	fragment_shader = load_shader(appstate, false);
	CHECK_CREATE(fragment_shader, "Fragment Shader");

	/* Create buffers */

	buffer_desc.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
	buffer_desc.size = sizeof(vertex_data);
	buffer_desc.props = 0;
	appstate->render_state.buf_vertex = SDL_CreateGPUBuffer(
		appstate->gpu_device,
		&buffer_desc
	);
	CHECK_CREATE(appstate->render_state.buf_vertex, "Static vertex buffer");

#pragma warning(push)
#pragma warning(disable: 4566)
	SDL_SetGPUBufferName(appstate->gpu_device, appstate->render_state.buf_vertex, "космонавт");
#pragma warning(pop)

	transfer_buffer_desc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transfer_buffer_desc.size = sizeof(vertex_data);
	transfer_buffer_desc.props = 0;
	buf_transfer = SDL_CreateGPUTransferBuffer(
		appstate->gpu_device,
		&transfer_buffer_desc
	);
	CHECK_CREATE(buf_transfer, "Vertex transfer buffer");

	/* We just need to upload the static data once. */
	map = SDL_MapGPUTransferBuffer(appstate->gpu_device, buf_transfer, false);
	SDL_memcpy(map, vertex_data, sizeof(vertex_data));
	SDL_UnmapGPUTransferBuffer(appstate->gpu_device, buf_transfer);

	cmd = SDL_AcquireGPUCommandBuffer(appstate->gpu_device);
	copy_pass = SDL_BeginGPUCopyPass(cmd);
	buf_location.transfer_buffer = buf_transfer;
	buf_location.offset = 0;
	dst_region.buffer = appstate->render_state.buf_vertex;
	dst_region.offset = 0;
	dst_region.size = sizeof(vertex_data);
	SDL_UploadToGPUBuffer(copy_pass, &buf_location, &dst_region, false);
	SDL_EndGPUCopyPass(copy_pass);
	SDL_SubmitGPUCommandBuffer(cmd);

	SDL_ReleaseGPUTransferBuffer(appstate->gpu_device, buf_transfer);

	/* Determine which sample count to use */
	appstate->render_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
	if (msaa && SDL_GPUTextureSupportsSampleCount(
		appstate->gpu_device,
		SDL_GetGPUSwapchainTextureFormat(appstate->gpu_device, appstate->state->windows[0]),
		SDL_GPU_SAMPLECOUNT_4)) {
		appstate->render_state.sample_count = SDL_GPU_SAMPLECOUNT_4;
	}

	/* Set up the graphics pipeline */

	SDL_zero(pipelinedesc);
	SDL_zero(color_target_desc);

	color_target_desc.format = SDL_GetGPUSwapchainTextureFormat(appstate->gpu_device, appstate->state->windows[0]);

	pipelinedesc.target_info.num_color_targets = 1;
	pipelinedesc.target_info.color_target_descriptions = &color_target_desc;
	pipelinedesc.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
	pipelinedesc.target_info.has_depth_stencil_target = true;

	pipelinedesc.depth_stencil_state.enable_depth_test = true;
	pipelinedesc.depth_stencil_state.enable_depth_write = true;
	pipelinedesc.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

	pipelinedesc.multisample_state.sample_count = appstate->render_state.sample_count;

	pipelinedesc.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

	pipelinedesc.vertex_shader = vertex_shader;
	pipelinedesc.fragment_shader = fragment_shader;

	vertex_buffer_desc.slot = 0;
	vertex_buffer_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
	vertex_buffer_desc.instance_step_rate = 0;
	vertex_buffer_desc.pitch = sizeof(VertexData);

	vertex_attributes[0].buffer_slot = 0;
	vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertex_attributes[0].location = 0;
	vertex_attributes[0].offset = 0;

	vertex_attributes[1].buffer_slot = 0;
	vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertex_attributes[1].location = 1;
	vertex_attributes[1].offset = sizeof(float) * 3;

	pipelinedesc.vertex_input_state.num_vertex_buffers = 1;
	pipelinedesc.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;
	pipelinedesc.vertex_input_state.num_vertex_attributes = 2;
	pipelinedesc.vertex_input_state.vertex_attributes = (SDL_GPUVertexAttribute*)&vertex_attributes;

	pipelinedesc.props = 0;

	appstate->render_state.pipeline = SDL_CreateGPUGraphicsPipeline(appstate->gpu_device, &pipelinedesc);
	CHECK_CREATE(appstate->render_state.pipeline, "Render Pipeline");

	/* These are reference-counted; once the pipeline is created, you don't need to keep these. */
	SDL_ReleaseGPUShader(appstate->gpu_device, vertex_shader);
	SDL_ReleaseGPUShader(appstate->gpu_device, fragment_shader);

	/* Set up per-window state */
	appstate->window_states = (WindowState*)SDL_calloc(appstate->state->num_windows, sizeof(WindowState));
	if (!appstate->window_states)
	{
		SDL_Log("Out of memory!\n");
		return SDL_APP_FAILURE;
	}

	for (int i = 0; i < appstate->state->num_windows; ++i)
	{
		WindowState* winstate = &appstate->window_states[i];

		/* create a depth texture for the window */
		SDL_GetWindowSizeInPixels(appstate->state->windows[i], (int*)&drawablew, (int*)&drawableh);
		winstate->tex_depth = CreateDepthTexture(appstate, drawablew, drawableh);
		winstate->tex_msaa = CreateMSAATexture(appstate, drawablew, drawableh);
		winstate->tex_resolve = CreateResolveTexture(appstate, drawablew, drawableh);

		/* make each window different */
		winstate->angle_x = (i * 10) % 360;
		winstate->angle_y = (i * 20) % 360;
		winstate->angle_z = (i * 30) % 360;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate_ptr)
{
	AppState* appstate = appstate_ptr;

	{
		Tetris* tetris = appstate->tetris;
		if (tetris->piece == 0)
		{
			SDL_memset(tetris, 0, sizeof(Tetris));
			tetris->piece = 1;
			tetris->x = 5;
			tetris->y = 21;
		}

		Uint64 now = SDL_GetTicksNS();
		Uint64 dt = now - tetris->prev_ns;
		tetris->prev_ns = now;
		if (tetris->drop_timer > dt)
		{
			tetris->drop_timer -= dt;
		}
		else
		{
			if (!try_move(tetris, 0, -1, 0))
			{
				glue(tetris);
			}
			tetris->drop_timer += 1000000000 >> (tetris->lines / 10);
		}
	}


	for (int window_index = 0; window_index < appstate->state->num_windows; ++window_index)
	{
		Render(appstate, appstate->state->windows[window_index], window_index);
	}
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate_ptr, SDL_Event* event)
{
	AppState* appstate = appstate_ptr;
	int done = 0;
	SDLTest_CommonEvent(appstate->state, event, &done);

	if (event->type == SDL_EVENT_KEY_DOWN)
	{
		int rot = event->key.key == SDLK_UP;
		int down = event->key.key == SDLK_DOWN;
		int left = event->key.key == SDLK_LEFT;
		int right = event->key.key == SDLK_RIGHT;
		int drop = event->key.key == SDLK_SPACE;

		Tetris* tetris = appstate->tetris;
		
		if (left)
			try_move(tetris, -1, 0, 0);
		if (right)
			try_move(tetris, 1, 0, 0);
		if (rot)
		{
			int i_nudge = tetris->x == 0 && tetris->piece == 7;
			int d = 1;
			try_move(tetris, 0, 0, d) ||
			try_move(tetris, -1, 0, d) ||
			try_move(tetris, 1, 0, d) ||
			(i_nudge && try_move(tetris, 2, 0, d)) ||
			try_move(tetris, 0, -1, d) ||
			try_move(tetris, 0, 1, d);
		}

		if (drop || down)
		{
			while (drop && try_move(tetris, 0, -1, 0))
			{
				// all the way down
			}

			if (drop || (down && !try_move(tetris, 0, -1, 0)))
			{
				glue(tetris);
			}

			tetris->drop_timer += 1000000000 >> (tetris->lines / 10);
		}
	}
	return done ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppInit(void** appstate_out, int argc, char* argv[])
{
	AppState* appstate = SDL_calloc(1, sizeof(AppState));
	*appstate_out = appstate;

	/* Initialize test framework */
	appstate->state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
	if (!appstate->state) {
		SDL_assert_always(!"SDLTest_CommonCreateState failed to create test framework");
		return SDL_APP_FAILURE;
	}

	int msaa = 0;
	for (int i = 1; i < argc;) {
		int consumed;

		consumed = SDLTest_CommonArg(appstate->state, i);
		if (consumed == 0) {
			if (SDL_strcasecmp(argv[i], "--msaa") == 0) {
				++msaa;
				consumed = 1;
			}
			else {
				consumed = -1;
			}
		}
		if (consumed < 0) {
			static const char* options[] = { "[--msaa]", NULL };
			SDLTest_CommonLogUsage(appstate->state, argv[0], options);
			return SDL_APP_FAILURE;
		}
		i += consumed;
	}

	appstate->state->skip_renderer = 1;
	appstate->state->window_flags |= SDL_WINDOW_RESIZABLE;
	appstate->state->window_w = 200 + 20;
	appstate->state->window_h = 440 + 20;

	if (!SDLTest_CommonInit(appstate->state)) {
		SDL_assert_always(!"SDLTest_CommonInit failed to init test framework");
		return SDL_APP_FAILURE;
	}

	appstate->tetris = SDL_calloc(1, sizeof(Tetris));
	if (!appstate->tetris)
	{
		SDL_assert_always(!"Out of memory. Can't even allocate Tetris.");
		return SDL_APP_FAILURE;
	}

	return init_render_state(appstate, msaa);
}

static void shutdownGPU(AppState* appstate)
{
	if (appstate->window_states) {
		int i;
		for (i = 0; i < appstate->state->num_windows; i++) {
			WindowState* winstate = &appstate->window_states[i];
			SDL_ReleaseGPUTexture(appstate->gpu_device, winstate->tex_depth);
			SDL_ReleaseGPUTexture(appstate->gpu_device, winstate->tex_msaa);
			SDL_ReleaseGPUTexture(appstate->gpu_device, winstate->tex_resolve);
			SDL_ReleaseWindowFromGPUDevice(appstate->gpu_device, appstate->state->windows[i]);
		}
		SDL_free(appstate->window_states);
		appstate->window_states = NULL;
	}

	SDL_ReleaseGPUBuffer(appstate->gpu_device, appstate->render_state.buf_vertex);
	SDL_ReleaseGPUGraphicsPipeline(appstate->gpu_device, appstate->render_state.pipeline);
	SDL_DestroyGPUDevice(appstate->gpu_device);

	SDL_zero(appstate->render_state);
	appstate->gpu_device = NULL;
}

void SDL_AppQuit(void* appstate_ptr)
{
	AppState* appstate = appstate_ptr;
	shutdownGPU(appstate);
	SDLTest_CommonQuit(appstate->state);
	SDL_free(appstate->tetris);
	SDL_free(appstate);
}

/* vi: set ts=4 sw=4 expandtab: */
