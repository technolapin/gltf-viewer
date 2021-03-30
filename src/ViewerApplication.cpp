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
const GLuint VERTEX_ATTRIB_POSITION_IDX = 0;
const GLuint VERTEX_ATTRIB_NORMAL_IDX = 1;
const GLuint VERTEX_ATTRIB_TEXCOORD0_IDX = 2;






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
  const auto normalMatrixLocation =
      glGetUniformLocation(glslProgram.glId(), "uNormalMatrix");



  // bounding box
  glm::vec3 bboxMin, bboxMax, bboxCenter, bboxDiag;
  computeSceneBounds(model, bboxMin, bboxMax);
  bboxCenter = (bboxMin + bboxMax)*0.5f;
  bboxDiag = glm::normalize(bboxMin - bboxMax);

  // Build projection matrix
  const auto maxDistance = std::max(100.f, glm::length(bboxDiag));
  const auto projMatrix =
      glm::perspective(70.f, float(m_nWindowWidth) / m_nWindowHeight,
          0.001f * maxDistance, 1.5f * maxDistance);

  const auto camera_speed_percentage = 0.04f;
  
  // TODO Implement a new CameraController model and use it instead. Propose the
  // choice from the GUI
//  FirstPersonCameraController cameraController{m_GLFWHandle.window(), camera_speed_percentage * maxDistance};
  std::unique_ptr<CameraController> cameraController =
      std::make_unique<TrackballCameraController>(m_GLFWHandle.window(),
                                                  camera_speed_percentage * maxDistance);
  
  if (m_hasUserCamera)
  {
      cameraController->setCamera(m_userCamera);
  }
  else
  {
      const auto up = glm::vec3(0, 1, 0);
      
      const auto eye = (bboxMax[2] - bboxMin[2] >= 0.0001)? bboxCenter + bboxDiag: bboxCenter + 2.f * glm::cross(bboxDiag, up);

      cameraController->setCamera(Camera{bboxCenter, eye, up});
  }
  


  
  // TODO Creation of Buffer Objects
  const auto vbos = createBufferObjects(model);

  // TODO Creation of Vertex Array Objects
  std::vector<VaoRange> meshIndexToVaoRange;
  const auto vbas = createVertexArrayObjects(model,
                                             vbos,
                                             meshIndexToVaoRange);


  
  // Setup OpenGL state for rendering
  glEnable(GL_DEPTH_TEST);
  glslProgram.use();

  
  
  // Lambda function to draw the scene
  const auto drawScene = [&](const Camera &camera) {
    glViewport(0, 0, m_nWindowWidth, m_nWindowHeight);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const auto viewMatrix = camera.getViewMatrix();

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


                for (const auto & child: node.children)
                {
                    drawNode(child, modelMatrix);
                }

            
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
                    [&]() {drawScene(cameraController->getCamera());});
      flipImageYAxis(w, h, channels, test_image.data());
      const auto strPath = m_OutputPath.string();
      stbi_write_png(
          strPath.c_str(), m_nWindowWidth, m_nWindowHeight, 3, test_image.data(), 0);
      return 0;
  }
  
  


  // Loop until the user closes the window
  for (auto iterationCount = 0u; !m_GLFWHandle.shouldClose();
       ++iterationCount) {
    const auto seconds = glfwGetTime();

    const auto camera = cameraController->getCamera();
    drawScene(camera);

    // GUI code:
    imguiNewFrame();

    {
      ImGui::Begin("GUI");
      ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
          1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
      if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
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

        if (ImGui::Button("CLI camera args to clipboard")) {
          std::stringstream ss;
          ss << "--lookat " << camera.eye().x << "," << camera.eye().y << ","
             << camera.eye().z << "," << camera.center().x << ","
             << camera.center().y << "," << camera.center().z << ","
             << camera.up().x << "," << camera.up().y << "," << camera.up().z;
          const auto str = ss.str();
          glfwSetClipboardString(m_GLFWHandle.window(), str.c_str());
        }
      }
      ImGui::End();
    }

    imguiRenderFrame();

    glfwPollEvents(); // Poll for and process events

    auto ellapsedTime = glfwGetTime() - seconds;
    auto guiHasFocus =
        ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    if (!guiHasFocus) {
      cameraController->update(float(ellapsedTime));
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

#include <typeinfo>  




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
        for (auto const & primitive: mesh.primitives)
        {
            auto vao = vertexArrayObjects[prim_off];

            glBindVertexArray(vao);

            const std::vector<std::pair<std::string, GLuint>> attributes =
                {
                    {"POSITION", VERTEX_ATTRIB_POSITION_IDX},
                    {"NORMAL", VERTEX_ATTRIB_NORMAL_IDX},
                    {"TEXCOORD_0", VERTEX_ATTRIB_TEXCOORD0_IDX},
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
                    glEnableVertexAttribArray(VERTEX_ATTRIB_POSITION_IDX);
                    
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

