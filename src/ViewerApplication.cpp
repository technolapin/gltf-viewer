#include "ViewerApplication.hpp"

#include <iostream>
#include <numeric>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/io.hpp>

#include "utils/cameras.hpp"

#include <stb_image_write.h>
#include <tiny_gltf.h>

#include "utils/gltf.hpp"

#include <math.h> 

#include <unordered_map>

const GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
const GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
const GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;
const GLuint VERTEX_ATTRIB_TANGENT_IDX = 3;


















#include <chrono>
#include <unistd.h> // usleep
struct Delayer
{
    std::chrono::time_point<std::chrono::steady_clock> clock;
    int delay;

    Delayer(int microseconds):
        clock(std::chrono::steady_clock::now()),
        delay(microseconds)
    {}


    
    ~Delayer()
    {
        auto elapsed = std::chrono::steady_clock::now() - clock;
        auto rest = delay - std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
        if (rest > 0)
        {
            usleep(rest);
        }
    }
    
};





void keyCallback(
    GLFWwindow *window, int key, int scancode, int action, int mods)
{
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, 1);
  }
}



int ViewerApplication::run()
{
  tinygltf::Model model;
  // DONE Loading the glTF file

  loadGltfFile(model);


// Loader shaders
  const auto glslProgram =
      compileProgram({m_ShadersRootPath / m_vertexShader,
          m_ShadersRootPath / m_fragmentShader});

  const auto modelViewProjMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelViewProjMatrix");
  const auto modelViewMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelViewMatrix");
  const auto modelMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uModelMatrix");
  const auto normalMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");

  const auto lightDirLocation =
      glGetUniformLocation(glslProgram.glId(), "uLightDir");
  const auto lightColLocation =
      glGetUniformLocation(glslProgram.glId(), "uLightCol");


  const auto baseColorTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uBaseColorTexture");

  const auto baseColorFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uBaseColorFactor");

  const auto metallicFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uMetallicFactor");
  const auto roughnessFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uRoughnessFactor");
  const auto metallicRoughnessTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uMetallicRoughnessTexture");

  const auto emissiveFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uEmissiveFactor");
  const auto emissiveTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uEmissiveTexture");

  const auto occlusionFactorLocation =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionFactor");
  const auto occlusionTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uOcclusionTexture");

  const auto normalTextureLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalTexture");
  const auto useNormalLocation =
      glGetUniformLocation(glslProgram.glId(), "uUseNormal");
  const auto renderModeLocation =
      glGetUniformLocation(glslProgram.glId(), "uRenderMode");


  // bounding box
  glm::vec3 bboxMin, bboxMax, bboxCenter, bboxDiag;
  computeSceneBounds(model, bboxMin, bboxMax);
  bboxCenter = (bboxMin + bboxMax)*0.5f;
  bboxDiag = bboxMax - bboxMin;

  // Build projection matrix
  const auto maxDistance = std::max(100.f, glm::length(bboxDiag));
  const auto projMatrix =
      glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  const auto camera_speed_percentage = 0.1f;
  
  // DONE Implement a new CameraController model and use it instead. Propose the
  // choice from the GUI
//  FirstPersonCameraController cameraController{m_GLFWHandle.window(), camera_speed_percentage * maxDistance};
  std::vector<std::unique_ptr<CameraController>> cameras;

  cameras.push_back(std::make_unique<TrackballCameraController>(m_GLFWHandle.window(),
                                                                camera_speed_percentage * maxDistance));
  cameras.push_back(std::make_unique<FirstPersonCameraController>(m_GLFWHandle.window(),
                                                                camera_speed_percentage * maxDistance));

  auto camera_index = 0;
  
  
  if (m_hasUserCamera)
  {
      cameras[camera_index]->setCamera(m_userCamera);
  }
  else
  {
      const auto up = glm::vec3(0, 1, 0);
      
      const auto eye = (bboxDiag.z >= 0.0001)? bboxCenter + bboxDiag: bboxCenter + 2.f * glm::cross(bboxDiag, up);

      cameras[camera_index]->setCamera(Camera{eye, bboxCenter, up});
  }
  

  // DONE creation of Textures
  const auto textures = createTextureObjects(model);

  // DONE creation of a white default texture
  GLuint whiteTexture;
  {
      const float white[] = {1, 1, 1, 1};
  
      // Generate the texture object:
      glGenTextures(1, &whiteTexture);
  
      // Bind texture object to target GL_TEXTURE_2D:
      glBindTexture(GL_TEXTURE_2D, whiteTexture);
      // Set image data:
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                   GL_RGB, GL_FLOAT, white);
      // Set sampling parameters:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      // Set wrapping parameters:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);

      glBindTexture(GL_TEXTURE_2D, 0);
  }
  GLuint defaultNormalMapTexture;
  {
      const float normal[] = {0, 0, 1, 0};
  
      // Generate the texture object:
      glGenTextures(1, &defaultNormalMapTexture);
  
      // Bind texture object to target GL_TEXTURE_2D:
      glBindTexture(GL_TEXTURE_2D, defaultNormalMapTexture);
      // Set image data:
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                   GL_RGB, GL_FLOAT, normal);
      // Set sampling parameters:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      // Set wrapping parameters:
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, GL_REPEAT);

      glBindTexture(GL_TEXTURE_2D, 0);
  }



  
  // DONE Creation of Buffer Objects
  const auto vbos = createBufferObjects(model);

  // DONE Creation of Vertex Array Objects
  std::vector<VaoRange> meshIndexToVaoRange;
  const auto vbas = createVertexArrayObjects(model,
                                             vbos,
                                             meshIndexToVaoRange);


  
  // Setup OpenGL state for rendering
  glEnable(GL_DEPTH_TEST);
  glslProgram.use();

  glm::vec3 light_col(1, 0.7, 0.2);

  bool light_is_camera = false;
  
  float light_theta = 0;
  float light_phi = 0;
  float light_intensity = 1.f;
  bool apply_occlusion = true;
  bool apply_normal_map = true;
  bool normal_option_unsigned = true;
  bool normal_option_2chan = false;
  bool normal_option_greenup = false;
  bool normal_compute_on_fly = false;
  int render_mode = 0;
  
  
  const auto bind_texture = [&](const auto tex, const auto texture_slot, const auto location)
  {
      glActiveTexture(GL_TEXTURE0 + texture_slot);
      glBindTexture(GL_TEXTURE_2D, tex);
      glUniform1i(location, texture_slot);
  };

  const auto load_texture = [&](const auto index, const auto texture_slot, const auto location, const auto default_texture)
  {
      auto texture_obj = default_texture;
      if (index >= 0)
      {
          const auto & texture = model.textures[index];
          if (texture.source >= 0)
          {
              texture_obj = textures[texture.source];
          }
      }

      bind_texture(texture_obj, texture_slot, location);
  };


  
  const auto bindMaterial = [&] (const auto materialIndex)
  {
      if (materialIndex >= 0)
      {
          // only valid is materialIndex >= 0
          const auto & material = model.materials[materialIndex];
          const auto & pbrMetallicRoughness = material.pbrMetallicRoughness;

          glUniform4f(baseColorFactorLocation,
                      (float)pbrMetallicRoughness.baseColorFactor[0],
                      (float)pbrMetallicRoughness.baseColorFactor[1],
                      (float)pbrMetallicRoughness.baseColorFactor[2],
                      (float)pbrMetallicRoughness.baseColorFactor[3]);

          load_texture(pbrMetallicRoughness.baseColorTexture.index,
                       0, baseColorTextureLocation, whiteTexture);

          glUniform1f(metallicFactorLocation,
                      (float)pbrMetallicRoughness.metallicFactor);
          glUniform1f(roughnessFactorLocation,
                      (float)pbrMetallicRoughness.roughnessFactor);


          load_texture(pbrMetallicRoughness.metallicRoughnessTexture.index,
                       1, metallicRoughnessTextureLocation, 0);


          glUniform3f(emissiveFactorLocation,
                      (float)material.emissiveFactor[0],
                      (float)material.emissiveFactor[1],
                      (float)material.emissiveFactor[2]);

          load_texture(material.emissiveTexture.index,
                       2, emissiveTextureLocation, 0);

          if (apply_occlusion)
          {
              glUniform1f(occlusionFactorLocation,
                          (float) material.occlusionTexture.strength);
          }
          else
          {
              glUniform1f(occlusionFactorLocation, 0.f);
          }

          load_texture(material.occlusionTexture.index,
                       3, occlusionTextureLocation, whiteTexture);

          // we do not use a default normal map
          if (apply_normal_map && material.normalTexture.index >= 0)
          {

              glUniform1i(useNormalLocation, 1
                          + (((GLuint) normal_option_unsigned) << 1)
                          + (((GLuint) normal_option_2chan) << 2)
                          + (((GLuint) normal_option_greenup) << 3)
                          + (((GLuint) normal_compute_on_fly) << 4));
              load_texture(material.normalTexture.index,
                           4, normalTextureLocation, defaultNormalMapTexture);
          }
          else
          {

              glUniform1i(useNormalLocation, 0);
              glActiveTexture(GL_TEXTURE4);
              glBindTexture(GL_TEXTURE_2D, defaultNormalMapTexture);
              glUniform1i(normalTextureLocation, 4);

          }
      }
      else
      {
          std::cout << "no material found \n";
          glUniform4f(baseColorFactorLocation,
                      1.f,
                      1.f,
                      1.f,
                      1.f);

          bind_texture(whiteTexture, 0, baseColorTextureLocation);

          glUniform1f(metallicFactorLocation, 1.f);
          glUniform1f(roughnessFactorLocation, 1.f);

          bind_texture(whiteTexture, 1, metallicRoughnessTextureLocation);

          glUniform3f(emissiveFactorLocation, 0.f, 0.f, 0.f);

          bind_texture(0, 2, emissiveTextureLocation);

          glUniform1f(occlusionFactorLocation, 0.f);

          bind_texture(0, 3, occlusionTextureLocation);
          
          bind_texture(0, 4, normalTextureLocation);
          glUniform1i(useNormalLocation, 0);

          
      }

  };


  
  // Lambda function to draw the scene
  const auto drawScene = [&](const Camera &camera)
  {
      glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      glUniform1i(renderModeLocation, render_mode);
      
      const auto viewMatrix = camera.getViewMatrix();
      
      const auto sin_phi = std::sin(light_phi);
      const auto cos_phi = std::cos(light_phi);
      const auto sin_the = std::sin(light_theta);
      const auto cos_the = std::cos(light_theta);
    
      const auto light_dir = glm::vec3(sin_the*cos_phi, cos_the, sin_the*sin_phi);
      
      glm::vec3 light_viewspace_dir;
      
      if (light_is_camera)
      {
          light_viewspace_dir = glm::vec3(0, 0, 1);
      }
      else
      {
          light_viewspace_dir = glm::normalize(glm::vec3(viewMatrix * glm::vec4(light_dir, 0)));
      }
      
      const auto light_intensity_color = light_intensity * light_col;
      
      glUniform3fv(lightDirLocation, 1, glm::value_ptr(light_viewspace_dir));
      glUniform3fv(lightColLocation, 1, glm::value_ptr(light_intensity_color));


    // The recursive function that should draw a node
    // We use a std::function because a simple lambda cannot be recursive
    const std::function<void(int, const glm::mat4 &)> drawNode =
        [&](int nodeIdx, const glm::mat4 &parentMatrix)
        {
            // DOING The drawNode function
            const auto & node = model.nodes[nodeIdx];
            const auto modelMatrix = getLocalToWorldMatrix(node, parentMatrix); 
            if (node.mesh >= 0)
            { 
                const auto modelViewMatrix = viewMatrix * modelMatrix;
                const auto modelViewProjMatrix = projMatrix * modelViewMatrix;
                const auto normalMatrix = glm::transpose(glm::inverse(modelViewMatrix));

                glUniformMatrix4fv(modelMatrixLocation,
                                   1, GL_FALSE,
                                   glm::value_ptr(modelMatrix));
                glUniformMatrix4fv(modelViewMatrixLocation,
                                   1, GL_FALSE,
                                   glm::value_ptr(modelViewMatrix));
                glUniformMatrix4fv(modelViewProjMatrixLocation,
                                   1, GL_FALSE,
                                   glm::value_ptr(modelViewProjMatrix));
                glUniformMatrix4fv(normalMatrixLocation,
                                   1, GL_FALSE,
                                   glm::value_ptr(normalMatrix));


                const auto & mesh = model.meshes[node.mesh];
                const auto & vaoRange = meshIndexToVaoRange[node.mesh];
                auto primIdx = 0;

                for (const auto & prim: mesh.primitives)
                {
                    bindMaterial(prim.material);

                    
                    
                    const auto & vao = vbas[vaoRange.begin + primIdx];

                    glBindVertexArray(vao);

                    if (prim.indices >= 0)
                    { // indices case
                        const auto & accessor = model.accessors[prim.indices];
                        const auto & bufferView = model.bufferViews[accessor.bufferView];
                        const auto byteOffset = bufferView.byteOffset + accessor.byteOffset;

                        glDrawElements(prim.mode,
                                       accessor.count,
                                       accessor.componentType,
                                       (GLvoid*) byteOffset);

                    }
                    else
                    { // no indices case
                        const auto accessorIdx = (*begin(prim.attributes)).second;
                        const auto & accessor = model.accessors[accessorIdx];
                        glDrawArrays(prim.mode, 0, accessor.count);
                    }
                    //glBindVertexArray(0);
                    primIdx++;
                }


            
            
            }
            for (const auto & child: node.children)
            {
                drawNode(child, modelMatrix);
            }

            
        };


    
    // Draw the scene referenced by gltf file
    if (model.defaultScene >= 0)
    {
        // DONE Draw all nodes
        for (const auto & node: model.scenes[model.defaultScene].nodes)
        {
            // node is an int
            drawNode(node, glm::mat4(1));
        }
        
    }
  };

  if (!m_OutputPath.empty())
  {
      const auto w = m_nWindowWidth;
      const auto h = m_nWindowHeight;
      const auto channels = 3;
      const auto n = w*h*channels;
      std::vector<unsigned char> test_image(n);
      renderToImage(w, h, channels, test_image.data(),
                    [&]() {drawScene(cameras[camera_index]->getCamera());});
      flipImageYAxis(w, h, channels, test_image.data());
      const auto strPath = m_OutputPath.string();
      stbi_write_png(
          strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, test_image.data(), 0);
      return 0;
  }


  // Loop until the user closes the window
  for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
       ++iterationCount)
  {
      Delayer _delayer(1000000/120);
      const auto seconds = glfwGetTime();

      const auto camera = cameras[camera_index]->getCamera();
      drawScene(camera);

      // GUI code:
      imguiNewFrame();

      {
          ImGui::Begin("GUI");
          ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                      1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
          if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {

              const auto prev_camera_index = camera_index;
              auto cam_changed = ImGui::RadioButton("Trackball", &camera_index, 0);
              ImGui::SameLine(); // I want to keep that
              cam_changed = cam_changed || ImGui::RadioButton("FPS", &camera_index, 1);
              if (cam_changed)
              {
                  cameras[camera_index]->setCamera(cameras[prev_camera_index]->getCamera());
              }

              ImGui::Text("eye: %.3f %.3f %.3f", camera.eye().x, camera.eye().y,
                          camera.eye().z);
              ImGui::Text("center: %.3f %.3f %.3f", camera.center().x,
                          camera.center().y, camera.center().z);
              ImGui::Text(
                  "up: %.3f %.3f %.3f", camera.up().x, camera.up().y, camera.up().z);

              ImGui::Text("front: %.3f %.3f %.3f", camera.front().x, camera.front().y,
                          camera.front().z);
              ImGui::Text("left: %.3f %.3f %.3f", camera.left().x, camera.left().y,
                          camera.left().z);

              if (ImGui::Button("CLI camera args to clipboard"))
              {
                  std::stringstream ss;
                  ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
                     << camera.eye().z << "," << camera.center().x << ","
                     << camera.center().y << "," << camera.center().z << ","
                     << camera.up().x << "," << camera.up().y << "," << camera.up().z;
                  const auto str = ss.str();
                  glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
              }

              if (ImGui::CollapsingHeader("Lighting"))
              {
                  ImGui::SliderFloat("theta", &light_theta, 0, glm::pi<float>());
                  ImGui::SliderFloat("phi", &light_phi, 0, glm::pi<float>());
                  ImGui::ColorEdit3("color", (float *)&light_col);
                  ImGui::SliderFloat("intensity", &light_intensity, 0, 10.f); 
                  ImGui::Checkbox("Light from Camera", &light_is_camera);
                  ImGui::Checkbox("Occlusion", &apply_occlusion);
                  ImGui::Checkbox("Normal Maps", &apply_normal_map);

                  if (apply_normal_map)
                  {
                      ImGui::Checkbox("Compute tangents on the fly", &normal_compute_on_fly);
                      ImGui::Checkbox("unsigned", &normal_option_unsigned);
                      ImGui::SameLine();
                      ImGui::Checkbox("2 channels", &normal_option_2chan);
                      ImGui::SameLine();
                      ImGui::Checkbox("green up", &normal_option_greenup);
                  }
                  
              }
          }

          if (ImGui::CollapsingHeader("Render Mode"))
          {
              ImGui::RadioButton("Full", &render_mode, 0);
              ImGui::RadioButton("Show Normals", &render_mode, 1);
              ImGui::RadioButton("Normal Map", &render_mode, 2);
              ImGui::RadioButton("Pos Variation X", &render_mode, 3);
              ImGui::RadioButton("Pos Variation Y", &render_mode, 4);
              ImGui::RadioButton("UV variation X", &render_mode, 5);
              ImGui::RadioButton("UV variation Y", &render_mode, 6);
              ImGui::RadioButton("UV", &render_mode, 7);
              ImGui::RadioButton("WorldSpace Position", &render_mode, 8);
              ImGui::RadioButton("Viewspace Position", &render_mode, 9);
          }

          ImGui::End();
      }

      imguiRenderFrame();

      glfwPollEvents(); // Poll for and process events

      auto ellapsedTime = glfwGetTime() - seconds;
      auto guiHasFocus =
          ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
      if (!guiHasFocus) {
          cameras[camera_index]->update(float(ellapsedTime));
      }

      m_GLFWHandle.swapBuffers(); // Swap front and back buffers
  }

  // TODO clean up allocated GL data

  return 0;
}



ViewerApplication::ViewerApplication(const fs::path &appPath, uint32_t width,
                                     uint32_t height, const fs::path &gltfFile,
                                     const std::vector<float> &lookatArgs, const std::string &vertexShader,
                                     const std::string &fragmentShader, const fs::path &output) :
    m_nWindowWidth(width),
    m_nWindowHeight(height),
    m_AppPath{appPath},
    m_AppName{m_AppPath.stem().string()},
    m_ImGuiIniFilename{m_AppName + ".imgui.ini"},
    m_ShadersRootPath{m_AppPath.parent_path() / "shaders"},
    m_gltfFilePath{gltfFile},
    m_OutputPath{output}
{
    if (!lookatArgs.empty()) {
        m_hasUserCamera = true;
        m_userCamera =
            Camera{glm::vec3(lookatArgs[0], lookatArgs[1], lookatArgs[2]),
            glm::vec3(lookatArgs[3], lookatArgs[4], lookatArgs[5]),
            glm::vec3(lookatArgs[6], lookatArgs[7], lookatArgs[8])};
    }

    if (!vertexShader.empty()) {
        m_vertexShader = vertexShader;
    }

    if (!fragmentShader.empty()) {
        m_fragmentShader = fragmentShader;
    }

    ImGui::GetIO().IniFilename =
        m_ImGuiIniFilename.c_str(); // At exit, ImGUI will store its windows
    // positions in this file

    glfwSetKeyCallback(m_GLFWHandle.window(), keyCallback);

    printGLVersion();
}



bool
ViewerApplication::loadGltfFile(tinygltf::Model & model)
{
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_gltfFilePath.string());
    //bool ret = loader.LoadBinaryFromFile(&model, &err, &warn, m_gltfFilePath.string()); // for binary glTF(.glb)

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
    }
    
    return ret;
    
}

// checked
std::vector<GLuint> ViewerApplication::createBufferObjects(
    const tinygltf::Model &model) const
{
    std::vector<GLuint> bufferObjects(model.buffers.size(), 0); // Assuming buffers is a std::vector of Buffer
    
    glGenBuffers(model.buffers.size(), bufferObjects.data());
    
    for (size_t i = 0; i < model.buffers.size(); ++i)
    {
        glBindBuffer(GL_ARRAY_BUFFER, bufferObjects[i]);
        glBufferStorage(GL_ARRAY_BUFFER,
                        model.buffers[i].data.size(), // Assume a Buffer has a data member variable of type std::vector
                        model.buffers[i].data.data(), 0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0); // Cleanup the binding point after the loop only

    return bufferObjects;
}





std::vector<GLuint>
ViewerApplication::createVertexArrayObjects(const tinygltf::Model &model,
                                            const std::vector<GLuint> &bufferObjects,
                                            std::vector<VaoRange> & meshIndexToVaoRange) const
{
    std::vector<GLuint> vertexArrayObjects;
    for (auto const & mesh: model.meshes)
    {
        const auto vaoOffset = (GLsizei) vertexArrayObjects.size();
        const auto n_primitives = (GLsizei) mesh.primitives.size();
        vertexArrayObjects.resize(vaoOffset + n_primitives);
        meshIndexToVaoRange.push_back(VaoRange{vaoOffset, n_primitives}); // Will be used during rendering

        // vectors are contiguous
        glGenVertexArrays(n_primitives, vertexArrayObjects.data() + vaoOffset);

        auto prim_off = vaoOffset;
        auto pIdx = 0;
        for (auto const & primitive: mesh.primitives)
        {
            auto vao = vertexArrayObjects[prim_off];

            glBindVertexArray(vao);

            const std::vector<std::pair<std::string, GLuint>> attributes =
                {
                    {"POSITION", VERTEX_ATTRIB_POSITION_IDX},
                    {"NORMAL", VERTEX_ATTRIB_NORMAL_IDX},
                    {"TEXCOORD_0", VERTEX_ATTRIB_TEXCOORD0_IDX},
                    {"TANGENT", VERTEX_ATTRIB_TANGENT_IDX},
                };

            for (const auto name_and_attrib: attributes)
            {
                const auto iterator = primitive.attributes.find(name_and_attrib.first);
                const auto attrib = name_and_attrib.second;
                
                if (iterator != end(primitive.attributes))
                { // If "POSITION" has been found in the map
                    const auto accessorIdx = (*iterator).second;
                    const auto &accessor = model.accessors[accessorIdx];
                    const auto &bufferView = model.bufferViews[accessor.bufferView];
                    const auto bufferIdx = bufferView.buffer;
                    
                    const auto bufferObject = bufferObjects[bufferIdx]; 
                    
                    glEnableVertexAttribArray(attrib);
                    //glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX);
                    
                    glBindBuffer(GL_ARRAY_BUFFER, bufferObject);
                    const auto byteOffset = bufferView.byteOffset + accessor.byteOffset;
                    glVertexAttribPointer(attrib,
                                          accessor.type,
                                          accessor.componentType,
                                          GL_FALSE,
                                          GLsizei(bufferView.byteStride),
                                          (const GLvoid*) byteOffset);

                }
                                                                                                                                                                                                    
            }

            if (primitive.indices >= 0)
            {
                const auto & accessor = model.accessors[primitive.indices];
                const auto & bufferView = model.bufferViews[accessor.bufferView];
                glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX);
                const auto bufferObject = bufferObjects[bufferView.buffer];
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bufferObject);
            }

            glBindBuffer(GL_ARRAY_BUFFER, 0); // Cleanup the binding point after the loop only
        
            glBindVertexArray(0);

            prim_off++;
        }
    }

    return vertexArrayObjects;
}




std::vector<GLuint>
ViewerApplication::createTextureObjects(const tinygltf::Model &model) const
{
    std::vector<GLuint> textures(model.textures.size(), 0);
    
    tinygltf::Sampler default_sampler;
    default_sampler.minFilter = GL_LINEAR;
    default_sampler.magFilter = GL_LINEAR;
    default_sampler.wrapS = GL_REPEAT;
    default_sampler.wrapT = GL_REPEAT;
    default_sampler.wrapR = GL_REPEAT;


    glActiveTexture(GL_TEXTURE0);

    glGenTextures((GLsizei) model.textures.size(), textures.data());

    auto i = 0;
    for (const auto & tex: model.textures)
    {
        const GLuint & tex_obj = textures[i++];
        
        assert(tex.source >= 0);

        const auto & image = model.images[tex.source];

        const auto & sampler = tex.sampler >= 0 ? model.samplers[tex.sampler] : default_sampler;


        glBindTexture(GL_TEXTURE_2D, tex_obj);
        
        // texture generation
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     image.width, image.height,
                     0, GL_RGBA, image.pixel_type,
                     image.image.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        sampler.minFilter != -1 ? sampler.minFilter : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                        sampler.magFilter != -1 ? sampler.magFilter : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, sampler.wrapS);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, sampler.wrapT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, sampler.wrapR);
        
        
        if (sampler.minFilter == GL_NEAREST_MIPMAP_NEAREST ||
            sampler.minFilter == GL_NEAREST_MIPMAP_LINEAR ||
            sampler.minFilter == GL_LINEAR_MIPMAP_NEAREST ||
            sampler.minFilter == GL_LINEAR_MIPMAP_LINEAR)
        {
            glGenerateMipmap(GL_TEXTURE_2D);
        }

    }
    glBindTexture(GL_TEXTURE_2D, 0);

    return textures;

}

