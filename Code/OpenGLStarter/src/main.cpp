#undef GLFW_DLL

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

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

// Submission file name: Assignment2_studentID_yourPairID.png
// TODO: replace with your real student ID and pair ID before submitting
const std::string STUDENT_ID = "studentID";
const std::string PAIR_ID = "yourPairID";
const std::string SCREENSHOT_NAME =
    "Assignment2_" + STUDENT_ID + "_" + PAIR_ID + ".png";

std::vector<Mesh *> meshList;
std::vector<Shader *> shaderList;

// Separate depth shader (week13 style)
Shader *depthShader;

// Light indicator shader (week13 style: same as lightShader)
Shader *lightShader;

float fov = 45.0f;
float yaw = 68.5f;
float pitch = 13.5f;
glm::vec3 lightColour = glm::vec3(0.9f, 0.95f, 1.0f); // Cool moonlight
glm::vec3 lightPos =
    glm::vec3(1.63f, 5.4f, -4.03f); // Bottom centre of lamp head (from OBJ vertices)
glm::vec3 cameraPos = glm::vec3(-4.9f, 0.9f, -13.2f);

// Default camera (week13 uses globals for these too)
const glm::vec3 DEFAULT_CAM_POS(-4.9f, 0.9f, -13.2f);
const float DEFAULT_YAW = 68.5f;
const float DEFAULT_PITCH = 13.5f;
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
  fov -= (float)yoffset * 2.0f;
  fov = glm::clamp(fov, 10.0f, 90.0f);
}

void UpdateMouseLook(GLFWwindow *window) {
  double mouseX, mouseY;
  glfwGetCursorPos(window, &mouseX, &mouseY);

  static double lastX = mouseX;
  static double lastY = mouseY;

  float xOffset = static_cast<float>(mouseX - lastX);
  float yOffset = static_cast<float>(mouseY - lastY);
  lastX = mouseX;
  lastY = mouseY;

  if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
    return;

  float sensitivity = 0.2f;
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

// Create a flat horizontal rectangle (week13 CreateMesh style: light panel under lamp)
// Vertex layout: pos(3) + normal(3) + uv(2)  = 8 floats per vertex
void CreateRect()
{
    GLfloat vertices[] = {
        //  pos x     y     z       normal nx   ny   nz    uv u    v
        -0.25f, 0.0f, -0.18f,   0.0f, -1.0f, 0.0f,   0.0f, 0.0f,
         0.25f, 0.0f, -0.18f,   0.0f, -1.0f, 0.0f,   1.0f, 0.0f,
         0.25f, 0.0f,  0.18f,   0.0f, -1.0f, 0.0f,   1.0f, 1.0f,
        -0.25f, 0.0f,  0.18f,   0.0f, -1.0f, 0.0f,   0.0f, 1.0f,
    };
    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0,
    };
    Mesh* rect = new Mesh();
    rect->CreateMesh(vertices, indices, 32, 6); // 4 verts * 8 floats = 32
    meshList.push_back(rect); // index 1 = lamp panel
}

void CreateShaders() {
  // Main lit shader  (week13 style: only handles lit pass with Blinn-Phong +
  // shadow)
  Shader *shader1 = new Shader();
  shader1->CreateFromFiles(shaderDirectory / "shader.vert",
                           shaderDirectory / "shader.frag");
  shaderList.push_back(shader1); // index 0

  // Separate depth shader (week13 style: dedicated shader for shadow/depth
  // pass)
  depthShader = new Shader();
  depthShader->CreateFromFiles(shaderDirectory / "depthShader.vert",
                               shaderDirectory / "depthShader.frag");

  // Light indicator shader (week13 style: flat colour shader for moon sphere)
  lightShader = new Shader();
  lightShader->CreateFromFiles(shaderDirectory / "lightShader.vert",
                               shaderDirectory / "lightShader.frag");
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
  delete depthShader;
  depthShader = nullptr;
  delete lightShader;
  lightShader = nullptr;
}

// -------------------------------------------------------
// main
// -------------------------------------------------------
int main(int argc, char *argv[]) {
  bool autoExit = false;
  for (int i = 1; i < argc; ++i) {
    if (std::string(argv[i]) == "--screenshot-and-exit")
      autoExit = true;
  }

  Window mainWindow(WIDTH, HEIGHT, 3, 3);
  if (mainWindow.initialise() != 0)
    return 1;

  // Load the project model (week13: CreateObj then CreateShaders)
  CreateObj(modelDirectory / "ModelFutureMe.obj");
  CreateRect();  // lamp light panel rectangle (week13 cube equivalent)
  CreateShaders();

  if (meshList.empty() || shaderList.empty()) {
    Cleanup();
    return 1;
  }

  glEnable(GL_DEPTH_TEST);

  GLFWwindow *window = glfwGetCurrentContext();
  glfwSetScrollCallback(window, ScrollCallback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

  // Shadow map framebuffer (week13 style: separate GLuint variables)
  GLuint depthMapFBO;
  glGenFramebuffers(1, &depthMapFBO);

  const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;

  GLuint depthMap;
  glGenTextures(1, &depthMap);
  glBindTexture(GL_TEXTURE_2D, depthMap);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH,
               SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

  glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                         depthMap, 0);
  glDrawBuffer(GL_NONE);
  glReadBuffer(GL_NONE);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  // Camera (week13 style: direction computed from yaw/pitch)
  glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 cameraDirection;

  float lastFrame = (float)glfwGetTime();
  int frameCounter = 0;

  // paintComponent
  while (!mainWindow.getShouldClose()) {
    glfwPollEvents();

    glClearColor(0.55f, 0.65f, 0.78f, 1.0f); // Daylight sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float now = (float)glfwGetTime();
    float deltaTime = now - lastFrame;
    lastFrame = now;

    UpdateMouseLook(window);

    // Reset camera with 'R'
    if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
      yaw = DEFAULT_YAW;
      pitch = DEFAULT_PITCH;
      cameraPos = DEFAULT_CAM_POS;
    }

    // Camera direction from yaw/pitch (week13 style)
    cameraDirection.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraDirection.y = sin(glm::radians(pitch));
    cameraDirection.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraDirection = glm::normalize(cameraDirection);

    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraDirection, up));
    float speed = 4.0f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
      cameraPos += cameraDirection * speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
      cameraPos -= cameraDirection * speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
      cameraPos -= cameraRight * speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
      cameraPos += cameraRight * speed;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
      cameraPos += up * speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
      cameraPos -= up * speed;

    // Build projection
    glm::mat4 projection =
        glm::perspective(glm::radians(fov),
                         (GLfloat)mainWindow.getBufferWidth() /
                             (GLfloat)mainWindow.getBufferHeight(),
                         0.1f, 300.0f);

    // Build view matrix manually (week13 style: cameraRotateMat * cameraPosMat)
    glm::vec3 cameraUp =
        glm::normalize(glm::cross(cameraRight, cameraDirection));

    glm::mat4 cameraPosMat(1.0f);
    cameraPosMat[3][0] = -cameraPos.x;
    cameraPosMat[3][1] = -cameraPos.y;
    cameraPosMat[3][2] = -cameraPos.z;

    glm::mat4 cameraRotateMat(1.0f);
    cameraRotateMat[0] =
        glm::vec4(cameraRight.x, cameraUp.x, -cameraDirection.x, 0.0f);
    cameraRotateMat[1] =
        glm::vec4(cameraRight.y, cameraUp.y, -cameraDirection.y, 0.0f);
    cameraRotateMat[2] =
        glm::vec4(cameraRight.z, cameraUp.z, -cameraDirection.z, 0.0f);

    glm::mat4 view = cameraRotateMat * cameraPosMat;

    // Light/shadow matrices - lamp casts shadow downward below lamp head
    const glm::vec3 sceneTarget(1.63f, 0.0f, -4.03f); // ground directly below lamp
    glm::mat4 lightProjection =
        glm::ortho(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f, 15.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, sceneTarget, up);

    glm::mat4 model = glm::mat4(1.0f);

    // --- Pass 1: depth map (week13 style: depthShader with view+projection
    // separate) ---
    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    depthShader->UseShader();
    glUniformMatrix4fv(depthShader->GetUniformLocation("model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    glUniformMatrix4fv(depthShader->GetUniformLocation("view"), 1, GL_FALSE,
                       glm::value_ptr(lightView));
    glUniformMatrix4fv(depthShader->GetUniformLocation("projection"), 1,
                       GL_FALSE, glm::value_ptr(lightProjection));

    glCullFace(GL_FRONT);
    meshList[0]->RenderMesh();
    glCullFace(GL_BACK);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- Pass 2: lit scene (week13 style: main shader with lightPos + separate
    // lightProjection/lightView) ---
    glViewport(0, 0, mainWindow.getBufferWidth(), mainWindow.getBufferHeight());
    glClearColor(0.55f, 0.65f, 0.78f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    shaderList[0]->UseShader();
    glUniformMatrix4fv(shaderList[0]->GetUniformLocation("model"), 1, GL_FALSE,
                       glm::value_ptr(model));
    glUniformMatrix4fv(shaderList[0]->GetUniformLocation("view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(shaderList[0]->GetUniformLocation("projection"), 1,
                       GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(shaderList[0]->GetUniformLocation("lightProjection"), 1,
                       GL_FALSE, glm::value_ptr(lightProjection));
    glUniformMatrix4fv(shaderList[0]->GetUniformLocation("lightView"), 1,
                       GL_FALSE, glm::value_ptr(lightView));
    glUniform3fv(shaderList[0]->GetUniformLocation("lightColour"), 1,
                 (GLfloat *)&lightColour);
    glUniform3fv(shaderList[0]->GetUniformLocation("lightPos"), 1,
                 (GLfloat *)&lightPos);
    glUniform3fv(shaderList[0]->GetUniformLocation("viewPos"), 1,
                 (GLfloat *)&cameraPos);
    glUniform1i(shaderList[0]->GetUniformLocation("texture_data"), 0);
    glUniform1i(shaderList[0]->GetUniformLocation("shadowMap"), 1);

    // Bind shadow map to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);

    // Render model with all submeshes using their respective materials &
    // textures
    GLint useTextureLoc = shaderList[0]->GetUniformLocation("useTexture");
    GLint textureLoc = shaderList[0]->GetUniformLocation("texture_data");
    GLint objectColourLoc = shaderList[0]->GetUniformLocation("objectColour");

    meshList[0]->RenderWithMaterials(useTextureLoc, textureLoc,
                                     objectColourLoc);

    // Unbind textures
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // --- Lamp panel rect (week13 lightShader style: flat colour rect below lamp head) ---
    lightShader->UseShader();
    glm::mat4 lampModel = glm::mat4(1.0f);
    // Position rect just below the lamp head (0.12 units below lightPos)
    lampModel = glm::translate(lampModel, lightPos + glm::vec3(0.0f, -0.12f, 0.0f));
    glUniformMatrix4fv(lightShader->GetUniformLocation("model"), 1, GL_FALSE,
                       glm::value_ptr(lampModel));
    glUniformMatrix4fv(lightShader->GetUniformLocation("view"), 1, GL_FALSE,
                       glm::value_ptr(view));
    glUniformMatrix4fv(lightShader->GetUniformLocation("projection"), 1,
                       GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(lightShader->GetUniformLocation("lightColour"), 1,
                 (GLfloat *)&lightColour);
    meshList[1]->RenderMesh();

    glUseProgram(0);
    // end draw

    // Screenshot on 'P' key
    frameCounter++;
    if (autoExit && frameCounter == 5) {
      SaveScreenshot(SCREENSHOT_NAME, mainWindow.getBufferWidth(),
                     mainWindow.getBufferHeight());
    }
    static bool pWasDown = false;
    const bool pDown = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
    if (pDown && !pWasDown) {
      SaveScreenshot(SCREENSHOT_NAME, mainWindow.getBufferWidth(),
                     mainWindow.getBufferHeight());
    }
    pWasDown = pDown;

    mainWindow.swapBuffers();

    if (autoExit && frameCounter >= 10)
      break;
  }

  glDeleteFramebuffers(1, &depthMapFBO);
  glDeleteTextures(1, &depthMap);
  Cleanup();
  return 0;
}