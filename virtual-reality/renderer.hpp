#pragma once

#ifndef vr_renderer_hpp
#define vr_renderer_hpp

#include "linalg_util.hpp"
#include "geometric.hpp"
#include "geometry.hpp"
#include "renderable.hpp"
#include "camera.hpp"
#include "gpu_timer.hpp"

using namespace avl;

#if defined(ANVIL_PLATFORM_WINDOWS)
	#define ALIGNED(n) __declspec(align(n))
#else
	#define ALIGNED(n) alignas(n)
#endif

namespace uniforms
{
	struct per_scene
	{
		static const int      binding = 0;
		float				  time;
		// todo resolution
	};

	struct per_view
	{
		static const int      binding = 1;
		ALIGNED(16) float4x4  view;
		ALIGNED(16) float4x4  viewProj;
		ALIGNED(16) float3    eyePos;
	};

	struct directional_light
	{
		ALIGNED(16) float3	  color;
		ALIGNED(16) float3	  direction;
		ALIGNED(16) float	  size;
	};

	struct point_light
	{
		ALIGNED(16) float3	  color;
		ALIGNED(16) float3	  position;
		ALIGNED(16) float3	  attenuation; // constant, linear, quadratic
	};

	struct spot_light
	{
		ALIGNED(16) float3	  color;
		ALIGNED(16) float3	  direction;
		ALIGNED(16) float3	  position;
		ALIGNED(16) float3	  attenuation; // constant, linear, quadratic
		ALIGNED(16) float	  cutoff;
	};
}

enum class CameraView : int
{
	LeftEye = 0, 
	RightEye = 1,
	CenterEye = 2
};

struct RenderSet
{
	std::vector<Renderable * > objects;
};

struct LightSet
{
	uniforms::directional_light * directionalLight;
	std::vector<uniforms::point_light *> pointLights;
	std::vector<uniforms::spot_light *> spotLights;
};

class Renderer
{
	std::vector<RenderSet *> renderSets;
	std::vector<LightSet *> lightSets;

	GlCamera * debugCamera;
	float2 renderSize;
	GlGpuTimer renderTimer;

	GlBuffer perScene;
	GlBuffer perView;

	void run_skybox_pass();
	void run_forward_pass();
	void run_forward_wireframe_pass();
	void run_shadow_pass();

	void run_bloom_pass();
	void run_reflection_pass();
	void run_ssao_pass();
	void run_smaa_pass();
	void run_blackout_pass();

	void run_post_pass();

	bool renderWireframe { false };
	bool renderShadows { false };
	bool renderPost { false };
	bool renderBloom { false };
	bool renderReflection { false };
	bool renderSSAO { false };
	bool renderSMAA { false };
	bool renderBlackout { false };

public:

	Renderer(float2 renderSize);
	~Renderer();

	void set_debug_camera(GlCamera * cam);
	GlCamera * get_debug_camera();

	void render_frame();
};

#endif // end vr_renderer_hpp
