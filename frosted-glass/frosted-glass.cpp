#include "index.hpp"
#include "frosted-glass.hpp"

using namespace avl;

constexpr const char skybox_vert[] = R"(#version 330
    layout(location = 0) in vec3 vertex;
    layout(location = 1) in vec3 normal;
    uniform mat4 u_viewProj;
    uniform mat4 u_modelMatrix;
    out vec3 v_normal;
    out vec3 v_world;
    void main()
    {
        vec4 worldPosition = u_modelMatrix * vec4(vertex, 1);
        gl_Position = u_viewProj * worldPosition;
        v_world = worldPosition.xyz;
        v_normal = normal;
    }
)";

constexpr const char skybox_frag[] = R"(#version 330
    in vec3 v_normal, v_world;
    in float u_time;
    out vec4 f_color;
    uniform vec3 u_bottomColor;
    uniform vec3 u_topColor;
    void main()
    {
        float h = normalize(v_world).y;
        f_color = vec4( mix( u_bottomColor, u_topColor, max( pow( max(h, 0.0 ), 0.8 ), 0.0 ) ), 1.0 );
    }
)";

constexpr const char basic_textured_vert[] = R"(#version 450
    layout(location = 0) in vec3 vertex;
    layout(location = 3) in vec2 inTexcoord;
    uniform mat4 u_mvp;
    out vec2 v_texcoord;
    void main()
    {
        gl_Position = u_mvp * vec4(vertex.xyz, 1);
        v_texcoord = inTexcoord;
    }
)";

constexpr const char basic_textured_frag[] = R"(#version 450
    in vec2 v_texcoord;
    out vec4 f_color;
    uniform sampler2D s_texture;
    void main()
    {
        vec4 t = texture(s_texture, v_texcoord);
        f_color = vec4(t.xyz, 1);
    }
)";

shader_workbench::shader_workbench() : GLFWApp(1280, 720, "Doom 2k16 Frosted Glass")
{

    std::cout << "hfov to fov" << to_degrees(hfov_to_dfov(to_radians(100.f), 0.9f)) << std::endl;

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::imgui_wrapper(window));
    gui::make_light_theme();

    post.reset(new blur_chain(float2(width, height)));

    shaderMonitor.watch("../assets/shaders/prototype/frosted_glass_vert.glsl", "../assets/shaders/prototype/frosted_glass_frag.glsl", [&](GlShader & shader)
    {
        glassShader = std::move(shader);
    });

    skyShader = GlShader(skybox_vert, skybox_frag);
    texturedShader = GlShader(basic_textured_vert, basic_textured_frag);

    glassTex = load_image("../assets/textures/glass-dirty.png", true);
    cubeTex = load_image("../assets/textures/uv_checker_map/uvcheckermap_01.png", true);
    floorTex = load_image("../assets/textures/uv_checker_map/uvcheckermap_02.png", false);

    glassSurface = make_plane_mesh(3, 3, 8, 8, false);
    floorMesh = make_plane_mesh(12, 12, 8, 8, false);
    cube = make_cube_mesh();
    skyMesh = make_sphere_mesh(1.0f);

    sceneColor.setup(width, height, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    sceneDepth.setup(width, height, GL_DEPTH_COMPONENT32, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sceneColor, 0);
    glNamedFramebufferTexture2DEXT(sceneFramebuffer, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, sceneDepth, 0);
    sceneFramebuffer.check_complete();

    // Setup Debug visualizations
    uiSurface.bounds = { 0, 0, (float)width, (float)height };
    uiSurface.add_child({ { 0.0000f, +20 },{ 0, +20 },{ 0.1667f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.1667f, +20 },{ 0, +20 },{ 0.3334f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.3334f, +20 },{ 0, +20 },{ 0.5009f, -10 },{ 0.133f, +10 } });
    uiSurface.add_child({ { 0.5000f, +20 },{ 0, +20 },{ 0.6668f, -10 },{ 0.133f, +10 } });
    uiSurface.layout();

    for (int i = 0; i < 4; ++i)
    {
        auto view = std::make_shared<GLTextureView>(true);
        views.push_back(view);
    }
    gizmo.reset(new GlGizmo());

    cubemapCam.reset(new CubemapCamera(1024));
    cam.look_at({ 0, 2.f, 4.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    gl_check_error(__FILE__, __LINE__);
}

shader_workbench::~shader_workbench() { }

void shader_workbench::on_window_resize(int2 size) 
{ 
    uiSurface.bounds = { 0, 0, (float)size.x, (float)size.y };
    uiSurface.layout();
}

void shader_workbench::on_input(const InputEvent & event)
{
    igm->update_input(event);
    flycam.handle_input(event);

    if (event.type == InputEvent::KEY)
    {
        if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE) exit();
        if (event.value[0] == GLFW_KEY_F1 && event.action == GLFW_RELEASE) cubemapCam->export_pngs();
    }

    if (gizmo) gizmo->handle_input(event);
}

void shader_workbench::on_update(const UpdateEvent & e)
{
    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    angle += 0.0025f;
}

void shader_workbench::on_draw()
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    gpuTimer.start();

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);
    if (gizmo) gizmo->update(cam, float2(width, height));

    auto render_scene = [&](const float3 eye, const float4x4 & viewMatrix, const float4x4 & projMatrix)
    {
        const float4x4 viewProjMatrix = mul(projMatrix, viewMatrix);

        // Largest non-clipped sphere
        const float4x4 world = mul(make_translation_matrix(eye), scaling_matrix(float3(cam.farclip * .99f)));

        skyShader.bind();
        skyShader.uniform("u_viewProj", viewProjMatrix);
        skyShader.uniform("u_modelMatrix", world);
        skyShader.uniform("u_bottomColor", float3(52.0f / 255.f, 62.0f / 255.f, 82.0f / 255.f));
        skyShader.uniform("u_topColor", float3(81.0f / 255.f, 101.0f / 255.f, 142.0f / 255.f));
        skyMesh.draw_elements();
        skyShader.unbind();

        texturedShader.bind();
        float4x4 cubeModel = make_translation_matrix({ 0, 0, -3 });
        if (animateCube) cubeModel = mul(cubeModel, make_rotation_matrix({ 0, 1, 0 }, angle * ANVIL_TAU));
        texturedShader.uniform("u_mvp", mul(viewProjMatrix, cubeModel));
        texturedShader.texture("s_texture", 0, cubeTex, GL_TEXTURE_2D);
        cube.draw_elements();

        float4x4 floorModel = mul(make_translation_matrix({ 0, -2, 0 }), make_rotation_matrix({ 1, 0, 0 }, ANVIL_PI / 2.f));
        texturedShader.uniform("u_mvp", mul(viewProjMatrix, floorModel));
        texturedShader.texture("s_texture", 0, floorTex, GL_TEXTURE_2D);
        floorMesh.draw_elements();
        texturedShader.unbind();
    };

    cubemapCam->render = render_scene;
    cubemapCam->update(float3(0, 0, 0));

    {
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        {
            glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
            glViewport(0, 0, width, height);
            glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            render_scene(cam.get_eye_point(), viewMatrix, projectionMatrix);

            post->execute(sceneColor);
        }

        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, width, height);
            glClearColor(0.6f, 0.6f, 0.6f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            render_scene(cam.get_eye_point(), viewMatrix, projectionMatrix);

            glassShader.bind();

            float4x4 glassModel = Identity4x4;

            glassShader.uniform("u_eye", cam.get_eye_point());
            glassShader.uniform("u_viewProj", viewProjectionMatrix);
            glassShader.uniform("u_modelMatrix", glassModel);
            glassShader.uniform("u_modelMatrixIT", inverse(transpose(glassModel)));
            glassShader.texture("s_mip1", 0, post->targets[0].colorAttachment1, GL_TEXTURE_2D);
            glassShader.texture("s_mip2", 1, post->targets[1].colorAttachment1, GL_TEXTURE_2D);
            glassShader.texture("s_mip3", 2, post->targets[2].colorAttachment1, GL_TEXTURE_2D);
            glassShader.texture("s_mip4", 3, post->targets[3].colorAttachment1, GL_TEXTURE_2D);
            glassShader.texture("s_mip5", 4, post->targets[4].colorAttachment1, GL_TEXTURE_2D);

            glassShader.texture("s_frosted", 5, glassTex, GL_TEXTURE_2D);

            glassSurface.draw_elements();
            glassShader.unbind();
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    gpuTimer.stop();

    igm->begin_frame();

    static const char * glassTextures[] = { "glass-debug-gradient.png", "glass-dirty.png", "glass-pattern.png" };
    std::vector<int> blendModes = { GL_ZERO, GL_ONE, GL_SRC_COLOR, GL_ONE_MINUS_SRC_COLOR, GL_DST_COLOR, GL_ONE_MINUS_DST_COLOR, GL_SRC_ALPHA, GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA };
    if (ImGui::ListBox("Glass Texture", &glassTextureSelection, glassTextures, IM_ARRAYSIZE(glassTextures), 4))
    {
        glassTex = load_image("../assets/textures/" + std::string(glassTextures[glassTextureSelection]), true);
    }

    ImGui::Text("Render Time %f ms", gpuTimer.elapsed_ms());
    ImGui::Checkbox("Animate", &animateCube);
    ImGui::Checkbox("Show Debug", &showDebug);

    igm->end_frame();
    if (gizmo) gizmo->draw();

    // Debug Views
    if (showDebug)
    {
        glViewport(0, 0, width, height);
        glDisable(GL_DEPTH_TEST);
        views[0]->draw(uiSurface.children[0]->bounds, float2(width, height), post->targets[0].colorAttachment1);
        views[1]->draw(uiSurface.children[1]->bounds, float2(width, height), post->targets[1].colorAttachment1);
        views[2]->draw(uiSurface.children[2]->bounds, float2(width, height), post->targets[2].colorAttachment1);
        views[3]->draw(uiSurface.children[3]->bounds, float2(width, height), post->targets[3].colorAttachment1);
        glEnable(GL_DEPTH_TEST);
    }
 
    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window);
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        shader_workbench app;
        app.main_loop();
    }
    catch (const std::exception & e)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
