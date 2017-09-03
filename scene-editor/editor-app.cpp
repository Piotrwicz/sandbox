#include "index.hpp"
#include "editor-app.hpp"
#include "../virtual-reality/material.cpp" // HACK HACK

using namespace avl;

scene_editor_app::scene_editor_app() : GLFWApp(1920, 1080, "Scene Editor")
{
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width, height);

    igm.reset(new gui::ImGuiManager(window));
    gui::make_dark_theme();

    editor.reset(new editor_controller<GameObject>());

    cam.look_at({ 0, 9.5f, -6.0f }, { 0, 0.1f, 0 });
    flycam.set_camera(&cam);

    auto wireframeProgram = GlShader(
        read_file_text("../assets/shaders/wireframe_vert.glsl"), 
        read_file_text("../assets/shaders/wireframe_frag.glsl"),
        read_file_text("../assets/shaders/wireframe_geom.glsl"));
    global_register_asset("wireframe", std::move(wireframeProgram));

     // USE_IMAGE_BASED_LIGHTING
    shaderMonitor.watch(
        "../assets/shaders/renderer/forward_lighting_vert.glsl", 
        "../assets/shaders/renderer/forward_lighting_frag.glsl", 
        "../assets/shaders/renderer", 
        {"TWO_CASCADES", "USE_IMAGE_BASED_LIGHTING", "HAS_ROUGHNESS_MAP", "HAS_METALNESS_MAP", "HAS_ALBEDO_MAP", "HAS_NORMAL_MAP"}, [](GlShader shader)
    {
        auto & asset = AssetHandle<GlShader>("pbr-forward-lighting").assign(std::move(shader));
        /*
        auto reflectedUniforms = asset.reflect();
        for (auto & u : reflectedUniforms) std::cout << u.first << " - " << u.second << std::endl;
        */
    });

    shaderMonitor.watch(
        "../assets/shaders/renderer/shadowcascade_vert.glsl", 
        "../assets/shaders/renderer/shadowcascade_frag.glsl", 
        "../assets/shaders/renderer/shadowcascade_geom.glsl", 
        "../assets/shaders/renderer", {}, 
        [](GlShader shader) {
        auto & asset = AssetHandle<GlShader>("cascaded-shadows").assign(std::move(shader));
    });

    renderer.reset(new PhysicallyBasedRenderer<1>(float2(width, height)));
    renderer->get_shadow_pass()->program = GlShaderHandle("cascaded-shadows");

    auto sky = renderer->get_procedural_sky();
    directionalLight.direction = sky->get_sun_direction();
    directionalLight.color = float3(1.f, 0.0f, 0.0f);
    directionalLight.amount = 1.0f;

    pointLights.push_back(uniforms::point_light{ float3(0.88f, 0.85f, 0.97f), float3(-5, 5, 0), 12.f });
    pointLights.push_back(uniforms::point_light{ float3(0.67f, 1.00f, 0.85f), float3(+5, 5, 0), 12.f });

    global_register_asset("rusted-iron-albedo", load_image("../assets/textures/pbr/rusted_iron_2048/albedo.png", false));
    global_register_asset("rusted-iron-normal", load_image("../assets/textures/pbr/rusted_iron_2048/normal.png", false));
    global_register_asset("rusted-iron-metallic", load_image("../assets/textures/pbr/rusted_iron_2048/metallic.png", false));
    global_register_asset("rusted-iron-roughness", load_image("../assets/textures/pbr/rusted_iron_2048/roughness.png", false));

    auto radianceBinary = read_file_binary("../assets/textures/envmaps/wells_radiance.dds");
    auto irradianceBinary = read_file_binary("../assets/textures/envmaps/wells_irradiance.dds");
    gli::texture_cube radianceHandle(gli::load_dds((char *)radianceBinary.data(), radianceBinary.size()));
    gli::texture_cube irradianceHandle(gli::load_dds((char *)irradianceBinary.data(), irradianceBinary.size()));
    global_register_asset("wells-radiance-cubemap", load_cubemap(radianceHandle));
    global_register_asset("wells-irradiance-cubemap", load_cubemap(irradianceHandle));

    auto ico = make_icosasphere(5);
    global_register_asset("icosphere", make_mesh_from_geometry(ico));
    global_register_asset("icosphere", std::move(ico));

    for (int i = 0; i < 10; ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            std::shared_ptr<MetallicRoughnessMaterial> pbrMaterial;

            pbrMaterial.reset(new MetallicRoughnessMaterial("pbr-forward-lighting"));
            pbrMaterial->set_albedo_texture("rusted-iron-albedo");
            pbrMaterial->set_normal_texture("rusted-iron-normal");
            pbrMaterial->set_metallic_texture("rusted-iron-metallic");
            pbrMaterial->set_roughness_texture("rusted-iron-roughness");
            pbrMaterial->set_radiance_cubemap("wells-radiance-cubemap");
            pbrMaterial->set_irrradiance_cubemap("wells-irradiance-cubemap");

            pbrMaterial->set_roughness(remap<float>(i, 0.0f, 9.0f, 0.0f, 1.f));
            pbrMaterial->set_metallic(remap<float>(j, 0.0f, 9.0f, 0.0f, 1.f));

            StaticMesh mesh(GlMeshHandle("icosphere"), GeometryHandle("icosphere"));
            Pose p;
            p.position = float3((i * 2) - 10, 0, (j * 2) - 10);
            mesh.set_pose(p);
            mesh.set_material(pbrMaterial.get()); // todo - assign default material
            objects.push_back(std::move(mesh));

            materials.push_back(pbrMaterial);
        }
    }

    auto cube = make_cube();
    global_register_asset("cube", make_mesh_from_geometry(cube));
    global_register_asset("cube", std::move(cube));

    StaticMesh floorMesh(GlMeshHandle("cube"), GeometryHandle("cube"));
    floorMesh.set_pose(Pose(float3(0, -2.01f, 0)));
    floorMesh.set_scale(float3(16, 0.1f, 16));
    floorMesh.set_material(materials.back().get());
    objects.push_back(std::move(floorMesh));
}

scene_editor_app::~scene_editor_app()
{ 

}

void scene_editor_app::on_window_resize(int2 size) 
{ 

}

void scene_editor_app::on_input(const InputEvent & event)
{
    igm->update_input(event);
    flycam.handle_input(event);
    editor->on_input(event);

    // Prevent scene editor from responding to input destined for ImGui
    if (!ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard)
    {
        if (event.type == InputEvent::KEY)
        {
            if (event.value[0] == GLFW_KEY_ESCAPE && event.action == GLFW_RELEASE)
            {
                // De-select all objects
                editor->clear();
            }
        }

        if (event.type == InputEvent::MOUSE && event.action == GLFW_PRESS && event.value[0] == GLFW_MOUSE_BUTTON_LEFT)
        {
            int width, height;
            glfwGetWindowSize(window, &width, &height);

            const Ray r = cam.get_world_ray(event.cursor, float2(width, height));

            if (length(r.direction) > 0 && !editor->active())
            {
                std::vector<GameObject *> selectedObjects;
                float best_t = std::numeric_limits<float>::max();
                GameObject * hitObject = nullptr;

                for (auto & obj : objects)
                {
                    RaycastResult result = obj.raycast(r);
                    if (result.hit)
                    {
                        if (result.distance < best_t)
                        {
                            best_t = result.distance;
                            hitObject = &obj;
                        }
                    }
                }

                if (hitObject) selectedObjects.push_back(hitObject);

                // New object was selected
                if (selectedObjects.size() > 0)
                {
                    // Multi-selection
                    if (event.mods & GLFW_MOD_CONTROL)
                    {
                        auto existingSelection = editor->get_selection();
                        for (auto s : selectedObjects)
                        {
                            if (!editor->selected(s)) existingSelection.push_back(s);
                        }
                        editor->set_selection(existingSelection);
                    }
                    // Single Selection
                    else
                    {
                        editor->set_selection(selectedObjects);
                    }
                }
            }
        }
    }

}

void scene_editor_app::on_update(const UpdateEvent & e)
{
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    flycam.update(e.timestep_ms);
    shaderMonitor.handle_recompile();
    editor->on_update(cam, float2(width, height));
}

void scene_editor_app::on_draw()
{
    glfwMakeContextCurrent(window);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    glViewport(0, 0, width, height);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float4x4 projectionMatrix = cam.get_projection_matrix(float(width) / float(height));
    const float4x4 viewMatrix = cam.get_view_matrix();
    const float4x4 viewProjectionMatrix = mul(projectionMatrix, viewMatrix);

    {
        // Single-viewport camera
        CameraData data;
        data.pose = cam.get_pose();
        data.projectionMatrix = projectionMatrix;
        data.viewMatrix = viewMatrix;
        data.viewProjMatrix = viewProjectionMatrix;
        renderer->add_camera(0, data);

        // Lighting
        RenderLightingData sceneLighting;
        sceneLighting.directionalLight = &directionalLight;
        for (auto & ptLight : pointLights) sceneLighting.pointLights.push_back(&ptLight);
        renderer->add_lights(sceneLighting);

        // Objects
        std::vector<Renderable *> sceneObjects;
        for (auto & obj : objects) sceneObjects.push_back(&obj);
        renderer->add_objects(sceneObjects);

        renderer->render_frame();

        glUseProgram(0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, width, height);

        glActiveTexture(GL_TEXTURE0);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, renderer->get_output_texture(0));
        glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);
        glTexCoord2f(1, 0); glVertex2f(+1, -1);
        glTexCoord2f(1, 1); glVertex2f(+1, +1);
        glTexCoord2f(0, 1); glVertex2f(-1, +1);
        glEnd();
        glDisable(GL_TEXTURE_2D);

        gl_check_error(__FILE__, __LINE__);
    }

    // Selected objects as wireframe
    {
        auto & program = AssetHandle<GlShader>("wireframe").get();

        program.bind();

        program.uniform("u_eyePos", cam.get_eye_point());
        program.uniform("u_viewProjMatrix", viewProjectionMatrix);

        for (auto obj : editor->get_selection())
        {
            program.uniform("u_modelMatrix", obj->get_pose().matrix());
            obj->draw();
        }

        program.unbind();
    }

    igm->begin_frame();

    renderer->gather_imgui();

    gui::imgui_menu_stack menu(*this, ImGui::GetIO().KeysDown);
    menu.app_menu_begin();
    {
        menu.begin("File");
        if (menu.item("Open Scene", GLFW_MOD_CONTROL, GLFW_KEY_O)) {}
        if (menu.item("Save Scene", GLFW_MOD_CONTROL, GLFW_KEY_S)) {}
        if (menu.item("New Scene", GLFW_MOD_CONTROL, GLFW_KEY_N)) {}
        if (menu.item("Exit", GLFW_MOD_ALT, GLFW_KEY_F4)) exit();
        menu.end();

        menu.begin("Edit");
        if (menu.item("Clone", GLFW_MOD_CONTROL, GLFW_KEY_D)) {}
        if (menu.item("Delete", 0, GLFW_KEY_DELETE)) 
        {
            auto it = std::remove_if(begin(objects), end(objects), [this](StaticMesh & obj) 
            { 
                return editor->selected(&obj);
            });
            objects.erase(it, end(objects));
            editor->clear();
        }
        if (menu.item("Select All", GLFW_MOD_CONTROL, GLFW_KEY_A)) 
        {
            std::vector<GameObject *> selectedObjects;
            for (auto & obj : objects) selectedObjects.push_back(&obj);
            editor->set_selection(selectedObjects);
        }
        menu.end();
    }
    menu.app_menu_end();

    gui::imgui_fixed_window_begin("Objects", { { width - 320, 17 }, { width, height } });

    for (size_t i = 0; i < objects.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        bool selected = editor->selected(&objects[i]);
        std::string name = std::string(typeid(objects[i]).name());
        std::vector<StaticMesh *> selectedObjects;

        if (ImGui::Selectable(name.c_str(), &selected))
        {
            if (!ImGui::GetIO().KeyCtrl) editor->clear();
            editor->update_selection(&objects[i]);
        }
        ImGui::PopID();
    }

    gui::imgui_fixed_window_end();

    igm->end_frame();

    // Scene editor gizmo
    glClear(GL_DEPTH_BUFFER_BIT);
    editor->on_draw();

    gl_check_error(__FILE__, __LINE__);

    glfwSwapBuffers(window); 
}

IMPLEMENT_MAIN(int argc, char * argv[])
{
    try
    {
        scene_editor_app app;
        app.main_loop();
    }
    catch (...)
    {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
