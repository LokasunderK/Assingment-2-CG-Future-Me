#ifndef MESH____H
#define MESH____H

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <GL/glew.h>

// Interleaved vertex: position (3), normal (3), uv (2)
struct VertexPNU 
{
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec2 uv{};
};

struct Material 
{
    std::string name;
    glm::vec3 diffuseColor{0.8f, 0.8f, 0.8f};
    GLuint textureID{0};
    bool hasTexture{false};
};

struct SubMesh 
{
    std::string materialName;
    unsigned int indexStart{0};
    unsigned int indexCount{0};
};

class Mesh 
{
public:
    Mesh();
    ~Mesh();

    // Create from client-provided interleaved vertex array (8 floats per vertex)
    void CreateMesh(const GLfloat* vertices,
                    const unsigned int* indices,
                    unsigned int numOfFloats,
                    unsigned int numOfIndices);

    // Create from interleaved vector of VertexPNU
    void CreateMesh(const std::vector<VertexPNU>& vertices,
                    const std::vector<unsigned int>& indices);

    // Load from .obj. If textureDir is provided, automatically loads materials and textures!
    bool CreateMeshFromOBJ(const std::string& path, 
                           const std::string& textureDir = "", 
                           bool flipV = false);

    // Render entire mesh (for shadow depth pass)
    void RenderMesh() const;

    // Render with materials and textures (for lit pass)
    void RenderWithMaterials(GLint useTextureLoc, 
                             GLint textureLoc, 
                             GLint objectColourLoc) const;

    void ClearMesh();

    // Accessors
    inline GLsizei GetIndexCount() const { return indexCount; }
    inline GLuint GetVAO() const { return VAO; }
    const std::vector<SubMesh>& GetSubMeshes() const { return subMeshes; }
    const std::unordered_map<std::string, Material>& GetMaterials() const { return materials; }

private:
    GLuint VAO{0};
    GLuint VBO{0};
    GLuint IBO{0};

    GLsizei indexCount{0};

    std::vector<SubMesh> subMeshes;
    std::unordered_map<std::string, Material> materials;

    void uploadInterleaved_(const std::vector<VertexPNU>& vertices,
                            const std::vector<unsigned int>& indices);
    void loadMTL_(const std::string& mtlPath, const std::string& textureDir);
};

#endif