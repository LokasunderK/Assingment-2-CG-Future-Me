#undef GLFW_DLL

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <vector>
#include <iostream>
#include <string>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "Libs/Mesh.h"
#include "Libs/Shader.h"
#include "Libs/Window.h"
#include "ProjectPaths.h"

const GLint WIDTH = 800;
const GLint HEIGHT = 600;

std::vector<Mesh *> meshList;
std::vector<Shader *> shaderList;

float fov = 45.0f;
float yaw = 86.0f;
float pitch = 5.0f;

// Default camera setup: framed to show the sitting skeleton on cardboard in foreground
// and the Microsoft building with corporate logo in background
const glm::vec3 DEFAULT_CAM_POS(-2.8f, 2.3f, -12.5f);
const float DEFAULT_YAW = 86.0f;
const float DEFAULT_PITCH = 5.0f;
const float DEFAULT_FOV = 45.0f;

// -------------------------------------------------------
// Screenshot helper
// -------------------------------------------------------
void SaveScreenshot(const std::string &filename, int width, int height) {
  std::vector<unsigned char> pixels(width * height * 3);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

  std::vector<unsigned char> flipped(width * height * 3);
  for (int y = 0; y < height; ++y) {
    memcpy(&flipped[y * width * 3], &pixels[(height - 1 - y) * width * 3],
           width * 3);
  }
  stbi_write_png(filename.c_str(), width, height, 3, flipped.data(), width * 3);
  std::cout << "[Screenshot] Saved image to: " << filename << "\n";
}

// -------------------------------------------------------
// Input helpers
// -------------------------------------------------------
void ScrollCallback(GLFWwindow *window, double /*xoffset*/, double yoffset) {
  fov -= static_cast<float>(yoffset) * 2.0f;
  fov = glm::clamp(fov, 10.0f, 90.0f);
}

void UpdateMouseLook(GLFWwindow *window) {
  double mouseX, mouseY;
  glfwGetCursorPos(window, &mouseX, &mouseY);

  static double lastX = mouseX;
  static double lastY = mouseY;

  const float xOffset = static_cast<float>(mouseX - lastX);
  const float yOffset = static_cast<float>(mouseY - lastY);
  lastX = mouseX;
  lastY = mouseY;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
    return;

  const float sensitivity = 0.2f;
  yaw += xOffset * sensitivity;
  pitch -= yOffset * sensitivity;
  pitch = glm::clamp(pitch, -89.0f, 89.0f);
}

// -------------------------------------------------------
// Paths
// -------------------------------------------------------
const std::filesystem::path shaderDirectory =
    std::filesystem::u8path(OPENGL_STARTER_SHADER_DIR);
const std::filesystem::path textureDirectory =
    std::filesystem::u8path(OPENGL_STARTER_TEXTURE_DIR);
const std::filesystem::path modelDirectory =
    std::filesystem::u8path(OPENGL_STARTER_MODEL_DIR);

// -------------------------------------------------------
// Asset creation
// -------------------------------------------------------
void CreateObj(const std::filesystem::path &modelPath) {
  Mesh *obj = new Mesh();
  if (!obj->CreateMeshFromOBJ(modelPath.string(), textureDirectory.string())) {
    std::cerr << "Failed to load model: " << modelPath << "\n";
    delete obj;
    return;
  }
  meshList.push_back(obj);
}

void CreateShaders() {
  // Unified shader handling both Depth pass and Lit pass
  Shader *shader = new Shader();
  shader->CreateFromFiles(shaderDirectory / "shader.vert",
                          shaderDirectory / "shader.frag");
  shaderList.push_back(shader); // index 0
}

// -------------------------------------------------------
// Scene transform helpers
// -------------------------------------------------------
glm::mat4 MakeModelMatrix() {
  return glm::mat4(1.0f);
}

// -------------------------------------------------------
// Render passes
// -------------------------------------------------------
void RenderDepthPass(Shader &shader, const glm::mat4 &lightSpaceMatrix) {
  shader.UseShader();
  glUniform1i(shader.GetUniformLocation("isDepthPass"), GL_TRUE);
  glUniformMatrix4fv(shader.GetUniformLocation("lightSpaceMatrix"), 1,
                     GL_FALSE, glm::value_ptr(lightSpaceMatrix));

  const glm::mat4 model = MakeModelMatrix();
  glUniformMatrix4fv(shader.GetUniformLocation("model"), 1, GL_FALSE,
                     glm::value_ptr(model));
  meshList[0]->RenderMesh();
}

void RenderLitPass(Shader &shader, const glm::mat4 &view,
                   const glm::mat4 &projection,
                   const glm::mat4 &lightSpaceMatrix,
                   const glm::vec3 &lightDirection, const glm::vec3 &cameraPos,
                   const glm::vec3 &lightColour,
                   GLuint shadowMap) {
  shader.UseShader();
  glUniform1i(shader.GetUniformLocation("isDepthPass"), GL_FALSE);

  glUniformMatrix4fv(shader.GetUniformLocation("view"), 1, GL_FALSE,
                     glm::value_ptr(view));
  glUniformMatrix4fv(shader.GetUniformLocation("projection"), 1, GL_FALSE,
                     glm::value_ptr(projection));
  glUniformMatrix4fv(shader.GetUniformLocation("lightSpaceMatrix"), 1, GL_FALSE,
                     glm::value_ptr(lightSpaceMatrix));
  glUniform3fv(shader.GetUniformLocation("lightColour"), 1,
               glm::value_ptr(lightColour));
  glUniform3fv(shader.GetUniformLocation("lightDirection"), 1,
               glm::value_ptr(lightDirection));
  glUniform3fv(shader.GetUniformLocation("viewPos"), 1,
               glm::value_ptr(cameraPos));
  glUniform1i(shader.GetUniformLocation("isLight"), GL_FALSE);
  glUniform1i(shader.GetUniformLocation("texture_data"), 0);
  glUniform1i(shader.GetUniformLocation("shadowMap"), 1);

  // Bind shadow map to texture unit 1
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, shadowMap);

  const glm::mat4 model = MakeModelMatrix();
  glUniformMatrix4fv(shader.GetUniformLocation("model"), 1, GL_FALSE,
                     glm::value_ptr(model));

  // Render model with all submeshes using their respective materials & textures
  GLint useTextureLoc = shader.GetUniformLocation("useTexture");
  GLint textureLoc = shader.GetUniformLocation("texture_data");
  GLint objectColourLoc = shader.GetUniformLocation("objectColour");

  meshList[0]->RenderWithMaterials(useTextureLoc, textureLoc, objectColourLoc);

  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
}

// -------------------------------------------------------
// Cleanup
// -------------------------------------------------------
void Cleanup() {
  for (Mesh *m : meshList)
    delete m;
  meshList.clear();
  for (Shader *s : shaderList)
    delete s;
  shaderList.clear();
}

// -------------------------------------------------------
// main
// -------------------------------------------------------
int main(int argc, char *argv[]) {
  bool autoExit = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--screenshot-and-exit") {
      autoExit = true;
    }
  }

  Window mainWindow(WIDTH, HEIGHT, 3, 3);
  if (mainWindow.initialise() != 0)
    return 1;

  // Load the project model and its materials & textures
  CreateObj(modelDirectory / "ModelFutureMe.obj");
  CreateShaders();

  if (meshList.empty() || shaderList.empty()) {
    Cleanup();
    return 1;
  }

  glEnable(GL_DEPTH_TEST);
  glClearColor(0.55f, 0.65f, 0.78f, 1.0f); // Pleasant daylight sky color

  GLFWwindow *window = glfwGetCurrentContext();
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  // Shadow map framebuffer
  constexpr GLuint SHADOW_W = 2048, SHADOW_H = 2048;
  GLuint depthMapFBO = 0, depthMap = 0;
  glGenFramebuffers(1, &depthMapFBO);
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_W, SHADOW_H, 0,
               GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
  const GLfloat border[] = {1.f, 1.f, 1.f, 1.f};
  glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  const GLenum fbStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  if (fbStatus != GL_FRAMEBUFFER_COMPLETE) {
    std::cerr << "Shadow framebuffer incomplete (0x" << std::hex << fbStatus
              << ")\n";
    glDeleteFramebuffers(1, &depthMapFBO);
    glDeleteTextures(1, &depthMap);
    Cleanup();
    return 1;
  }

  // Camera setup
  glm::vec3 cameraPos = DEFAULT_CAM_POS;
  const glm::vec3 up(0.0f, 1.0f, 0.0f);
  yaw = DEFAULT_YAW;
  pitch = DEFAULT_PITCH;
  fov = DEFAULT_FOV;

  // Directional sun light coming from front-right (+X, +Y, -Z) shining towards building
  const glm::vec3 lightPos(4.0f, 16.0f, -16.0f);
  const glm::vec3 sceneTarget(-1.5f, 2.5f, -4.5f);
  const glm::vec3 lightDirection = glm::normalize(lightPos - sceneTarget);
  const glm::vec3 lightColour(1.0f, 0.98f, 0.92f); // Warm sunlight

  float lastFrame = static_cast<float>(glfwGetTime());
  int frameCounter = 0;

  while (!mainWindow.getShouldClose()) {
    glfwPollEvents();
    const float now = static_cast<float>(glfwGetTime());
    const float deltaTime = now - lastFrame;
    lastFrame = now;

    UpdateMouseLook(window);

    // Reset camera with 'R'
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
      yaw = DEFAULT_YAW;
      pitch = DEFAULT_PITCH;
      fov = DEFAULT_FOV;
      cameraPos = DEFAULT_CAM_POS;
    }

    // Camera direction from yaw/pitch
    glm::vec3 camDir;
    camDir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    camDir.y = sin(glm::radians(pitch));
    camDir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    camDir = glm::normalize(camDir);

    const glm::vec3 camRight = glm::normalize(glm::cross(camDir, up));
    const float speed = 4.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      cameraPos += camDir * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      cameraPos -= camDir * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      cameraPos -= camRight * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      cameraPos += camRight * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      cameraPos += up * speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
      cameraPos -= up * speed;

    const GLint bufW = mainWindow.getBufferWidth();
    const GLint bufH = mainWindow.getBufferHeight();
    if (bufW <= 0 || bufH <= 0)
      continue;

    const glm::mat4 view = glm::lookAt(cameraPos, cameraPos + camDir, up);
    const glm::mat4 proj = glm::perspective(
        glm::radians(fov), static_cast<float>(bufW) / static_cast<float>(bufH),
        0.1f, 300.0f);

    // --- Pass 1: shadow depth ---
    const glm::mat4 lightProj =
        glm::ortho(-20.f, 20.f, -20.f, 20.f, 0.1f, 80.f);
    const glm::mat4 lightView = glm::lookAt(lightPos, sceneTarget, up);
    const glm::mat4 lightSpace = lightProj * lightView;

    glViewport(0, 0, SHADOW_W, SHADOW_H);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    RenderDepthPass(*shaderList[0], lightSpace);

    // --- Pass 2: lit scene ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, bufW, bufH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    RenderLitPass(*shaderList[0], view, proj, lightSpace, lightDirection,
                  cameraPos, lightColour, depthMap);

    // Capture screenshot on 'P' key press (or during --screenshot-and-exit)
    frameCounter++;
    if (autoExit && frameCounter == 5) {
      SaveScreenshot("Assignment2_screenshot.png", bufW, bufH);
    }
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS) {
      SaveScreenshot("Assignment2_screenshot.png", bufW, bufH);
    }

    mainWindow.swapBuffers();

    if (autoExit && frameCounter >= 10) {
      break;
    }
  }

  glDeleteFramebuffers(1, &depthMapFBO);
  glDeleteTextures(1, &depthMap);
  Cleanup();
  return 0;
}