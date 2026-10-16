#include "Mesh.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <map>
#include <filesystem>
#include <cstring>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

// ----------------------------
// Texture loader helper
// ----------------------------
static GLuint LoadGLTexture(const std::string& filepath) 
{
    stbi_set_flip_vertically_on_load(true);
    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);
    if (!data) 
    {
        std::cerr << "[Mesh] Failed to load image: " << filepath 
                  << " (" << stbi_failure_reason() << ")\n";
        return 0;
    }

    GLenum format = GL_RGB;
    if (channels == 4) format = GL_RGBA;
    else if (channels == 3) format = GL_RGB;
    else if (channels == 2) format = GL_RG;
    else if (channels == 1) format = GL_RED;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    // Tiny textures are pixel art (e.g. the Minecraft skeleton): keep texels crisp
    const bool pixelArt = (width <= 128 && height <= 128);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pixelArt ? GL_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    pixelArt ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

// ----------------------------
// Mesh: lifecycle
// ----------------------------
Mesh::Mesh() {}

Mesh::~Mesh() { ClearMesh(); }

// ----------------------------
// Create from raw arrays (8 floats per vertex)
// ----------------------------
void Mesh::CreateMesh(const GLfloat* vertices,
                    const unsigned int* indices,
                    unsigned int numOfFloats,
                    unsigned int numOfIndices) 
{
    std::vector<VertexPNU> vtx;

    if (numOfFloats % 8 == 0) 
    {
        unsigned int numVerts = numOfFloats / 8;
        vtx.reserve(numVerts);
        for (unsigned int i = 0; i < numVerts; ++i) 
        {
            const GLfloat* base = vertices + i * 8;
            VertexPNU v{};
            v.position = { base[0], base[1], base[2] };
            v.normal   = { base[3], base[4], base[5] };
            v.uv       = { base[6], base[7] };
            vtx.push_back(v);
        }
    }
    else if (numOfFloats % 5 == 0) 
    {
        unsigned int numVerts = numOfFloats / 5;
        vtx.reserve(numVerts);
        for (unsigned int i = 0; i < numVerts; ++i) 
        {
            const GLfloat* base = vertices + i * 5;
            VertexPNU v{};
            v.position = { base[0], base[1], base[2] };
            v.normal   = { 0.0f, 0.0f, 1.0f };
            v.uv       = { base[3], base[4] };
            vtx.push_back(v);
        }
    }
    else 
    {
        std::cerr << "[Mesh] Error: unsupported vertex float count " << numOfFloats << "\n";
        return;
    }

    std::vector<unsigned int> idx(indices, indices + numOfIndices);
    CreateMesh(vtx, idx);
}

// ----------------------------
// Create from std::vector
// ----------------------------
void Mesh::CreateMesh(const std::vector<VertexPNU>& vertices,
                    const std::vector<unsigned int>& indices) 
{
    subMeshes.clear();
    SubMesh sub;
    sub.materialName = "default";
    sub.indexStart = 0;
    sub.indexCount = static_cast<unsigned int>(indices.size());
    subMeshes.push_back(sub);

    uploadInterleaved_(vertices, indices);
}

// ----------------------------
// Upload helper (interleaved VBO + EBO)
// ----------------------------
void Mesh::uploadInterleaved_(const std::vector<VertexPNU>& vertices,
                            const std::vector<unsigned int>& indices) 
{
    if (VAO != 0) 
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO != 0) 
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (IBO != 0) 
    {
        glDeleteBuffers(1, &IBO);
        IBO = 0;
    }

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(vertices.size() * sizeof(VertexPNU)),
                vertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                indices.data(), GL_STATIC_DRAW);

    const GLsizei stride = static_cast<GLsizei>(sizeof(VertexPNU));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexPNU, position));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexPNU, uv));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)offsetof(VertexPNU, normal));
    glEnableVertexAttribArray(2);

    indexCount = static_cast<GLsizei>(indices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// ----------------------------
// MTL Loader
// ----------------------------
void Mesh::loadMTL_(const std::string& mtlPath, const std::string& textureDir) 
{
    std::ifstream in(mtlPath);
    if (!in) 
    {
        std::cerr << "[Mesh] Warning: Could not open MTL file: " << mtlPath << "\n";
        return;
    }

    std::string line;
    Material* currentMat = nullptr;

    while (std::getline(in, line)) 
    {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "newmtl") 
        {
            std::string matName;
            ss >> matName;
            materials[matName] = Material{};
            currentMat = &materials[matName];
            currentMat->name = matName;
        } 
        else if (tag == "Kd" && currentMat) 
        {
            ss >> currentMat->diffuseColor.r >> currentMat->diffuseColor.g >> currentMat->diffuseColor.b;
        } 
        else if (tag == "map_Kd" && currentMat) 
        {
            std::string rest;
            std::getline(ss, rest);
            size_t first = rest.find_first_not_of(" \t\r\n");
            if (first != std::string::npos) 
            {
                size_t last = rest.find_last_not_of(" \t\r\n");
                std::string texFile = rest.substr(first, last - first + 1);

                std::filesystem::path p(texFile);
                std::string filename = p.filename().string();

                std::filesystem::path fullTexPath;
                if (!textureDir.empty() && std::filesystem::exists(std::filesystem::path(textureDir) / filename)) 
                {
                    fullTexPath = std::filesystem::path(textureDir) / filename;
                } 
                else if (std::filesystem::exists(std::filesystem::path(mtlPath).parent_path() / filename)) 
                {
                    fullTexPath = std::filesystem::path(mtlPath).parent_path() / filename;
                } 
                else if (std::filesystem::exists(texFile)) 
                {
                    fullTexPath = texFile;
                }

                if (!fullTexPath.empty()) 
                {
                    GLuint tid = LoadGLTexture(fullTexPath.string());
                    if (tid != 0) 
                    {
                        currentMat->textureID = tid;
                        currentMat->hasTexture = true;
                        std::cout << "[Mesh] Loaded texture for material '" << currentMat->name 
                                  << "': " << fullTexPath.filename() << "\n";
                    }
                } 
                else 
                {
                    std::cerr << "[Mesh] Texture not found: " << filename << "\n";
                }
            }
        }
    }
}

// ----------------------------
// .OBJ loader with materials
// ----------------------------
namespace 
{
    struct Triple 
    { 
        int v, vt, vn;
        bool operator==(const Triple& o) const { return v==o.v && vt==o.vt && vn==o.vn; }
    };
    struct TripleHash 
    {
        size_t operator()(const Triple& t) const 
        {
            return (size_t)((t.v+1)*73856093) ^ (size_t)((t.vt+1)*19349663) ^ (size_t)((t.vn+1)*83492791);
        }
    };
}

bool Mesh::CreateMeshFromOBJ(const std::string& path, const std::string& textureDir, bool flipV) 
{
    ClearMesh();

    std::ifstream in(path);
    if (!in) 
    {
        std::cerr << "[Mesh] Failed to open OBJ: " << path << "\n";
        return false;
    }

    std::filesystem::path objFsPath(path);
    std::filesystem::path objDir = objFsPath.parent_path();

    std::vector<glm::vec3> V; V.reserve(20000);
    std::vector<glm::vec2> VT; VT.reserve(20000);
    std::vector<glm::vec3> VN; VN.reserve(20000);

    std::vector<VertexPNU> interleaved; interleaved.reserve(40000);
    std::unordered_map<Triple, unsigned int, TripleHash> dedup;

    // Map each material name to its index list in the order encountered
    std::vector<std::string> materialOrder;
    std::unordered_map<std::string, std::vector<unsigned int>> materialIndices;

    std::string currentMaterial = "default";
    materialOrder.push_back(currentMaterial);

    std::string line;
    while (std::getline(in, line)) 
    {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream ss(line);
        std::string tag; ss >> tag;

        if (tag == "mtllib") 
        {
            std::string mtlName;
            ss >> mtlName;
            std::filesystem::path mtlFullPath = objDir / mtlName;
            loadMTL_(mtlFullPath.string(), textureDir);
        } 
        else if (tag == "usemtl") 
        {
            std::string matName;
            ss >> matName;
            currentMaterial = matName;
            if (materialIndices.find(currentMaterial) == materialIndices.end()) 
            {
                materialOrder.push_back(currentMaterial);
            }
        } 
        else if (tag == "v") 
        {
            glm::vec3 p{}; ss >> p.x >> p.y >> p.z; V.push_back(p);
        } 
        else if (tag == "vt") 
        {
            glm::vec2 t{}; ss >> t.x >> t.y; 
            if (flipV) t.y = 1.0f - t.y; 
            VT.push_back(t);
        } 
        else if (tag == "vn") 
        {
            glm::vec3 n{}; ss >> n.x >> n.y >> n.z; VN.push_back(n);
        } 
        else if (tag == "f") 
        {
            std::vector<std::string> fverts; fverts.reserve(8);
            std::string tok;
            while (ss >> tok) { fverts.push_back(tok); }
            if (fverts.size() < 3) continue;

            auto parseOne = [&](const std::string& s, int& iv, int& ivt, int& ivn) -> bool 
            {
                iv = ivt = ivn = -1;
                int parts[3] = {0,0,0}, p = 0;
                std::string cur;
                for (size_t i = 0; i <= s.size(); ++i) 
                {
                    if (i == s.size() || s[i] == '/') 
                    {
                        if (!cur.empty()) 
                        { 
                            try { parts[p] = std::stoi(cur); } catch (...) { parts[p] = 0; } 
                        }
                        cur.clear(); 
                        if (i != s.size() && p < 2) ++p;
                    } 
                    else cur.push_back(s[i]);
                }
                auto fixIndex = [](int idx, int count){ if (idx > 0) return idx - 1; if (idx < 0) return count + idx; return -1; };
                iv  = fixIndex(parts[0], (int)V.size());
                ivt = (p >= 1) ? fixIndex(parts[1], (int)VT.size()) : -1;
                ivn = (p >= 2) ? fixIndex(parts[2], (int)VN.size()) : -1;
                return (iv >= 0);
            };

            auto addFaceVertex = [&](int iv, int ivt, int ivn) -> unsigned int
            {
                Triple key{iv, ivt, ivn};
                auto it = dedup.find(key);
                if (it != dedup.end()) 
                {
                    return it->second;
                }

                VertexPNU out{};
                out.position = V[iv];
                if (ivt >= 0 && ivt < (int)VT.size()) out.uv = VT[ivt];
                if (ivn >= 0 && ivn < (int)VN.size()) out.normal = VN[ivn];

                unsigned int id = static_cast<unsigned int>(interleaved.size());
                interleaved.push_back(out);
                dedup.emplace(key, id);
                return id;
            };

            struct FaceV { int v, vt, vn; };
            std::vector<FaceV> fv; fv.reserve(fverts.size());
            for (const auto& s : fverts) 
            {
                int iv = 0, ivt = 0, ivn = 0;
                parseOne(s, iv, ivt, ivn);
                fv.push_back({iv, ivt, ivn});
            }

            auto& currentIdxList = materialIndices[currentMaterial];
            for (size_t i = 1; i + 1 < fv.size(); ++i) 
            {
                currentIdxList.push_back(addFaceVertex(fv[0].v, fv[0].vt, fv[0].vn));
                currentIdxList.push_back(addFaceVertex(fv[i].v, fv[i].vt, fv[i].vn));
                currentIdxList.push_back(addFaceVertex(fv[i+1].v, fv[i+1].vt, fv[i+1].vn));
            }
        }
    }

    if (interleaved.empty()) 
    {
        std::cerr << "[Mesh] OBJ produced no geometry: " << path << "\n";
        return false;
    }

    // Assemble continuous index buffer grouped by material batches
    std::vector<unsigned int> allIndices;
    allIndices.reserve(interleaved.size() * 3);
    subMeshes.clear();

    for (const auto& matName : materialOrder) 
    {
        auto it = materialIndices.find(matName);
        if (it != materialIndices.end() && !it->second.empty()) 
        {
            SubMesh sub;
            sub.materialName = matName;
            sub.indexStart = static_cast<unsigned int>(allIndices.size());
            sub.indexCount = static_cast<unsigned int>(it->second.size());
            allIndices.insert(allIndices.end(), it->second.begin(), it->second.end());
            subMeshes.push_back(sub);
        }
    }

    uploadInterleaved_(interleaved, allIndices);
    std::cout << "[Mesh] Successfully loaded OBJ with " << subMeshes.size() 
              << " material submeshes, " << interleaved.size() << " vertices, " 
              << allIndices.size() << " indices.\n";
    return true;
}

// ----------------------------
// Render whole mesh (for shadow depth pass)
// ----------------------------
void Mesh::RenderMesh() const 
{
    if (VAO == 0 || indexCount == 0) return;
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

// ----------------------------
// Render with per-material textures and colors (for lit pass)
// ----------------------------
void Mesh::RenderWithMaterials(GLint useTextureLoc, GLint textureLoc, GLint objectColourLoc) const 
{
    if (VAO == 0 || indexCount == 0) return;
    glBindVertexArray(VAO);

    for (const auto& sub : subMeshes) 
    {
        auto it = materials.find(sub.materialName);

        // Always send this material's Kd so transparent texels fall back to
        // the right colour instead of whatever the previous submesh left behind.
        glm::vec3 col(0.8f);
        if (it != materials.end())
        {
            col = it->second.diffuseColor;
        }
        glUniform3fv(objectColourLoc, 1, glm::value_ptr(col));

        if (it != materials.end() && it->second.hasTexture && it->second.textureID != 0)
        {
            glUniform1i(useTextureLoc, GL_TRUE);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, it->second.textureID);
            glUniform1i(textureLoc, 0);
        }
        else
        {
            glUniform1i(useTextureLoc, GL_FALSE);
        }

        glDrawElements(GL_TRIANGLES, sub.indexCount, GL_UNSIGNED_INT, 
                       (const void*)(uintptr_t)(sub.indexStart * sizeof(unsigned int)));
    }

    glBindVertexArray(0);
}

// ----------------------------
// Clear GL resources
// ----------------------------
void Mesh::ClearMesh() 
{
    for (auto& pair : materials) 
    {
        if (pair.second.textureID != 0) 
        {
            glDeleteTextures(1, &pair.second.textureID);
            pair.second.textureID = 0;
        }
    }
    materials.clear();
    subMeshes.clear();

    if (IBO != 0) 
    {
        glDeleteBuffers(1, &IBO);
        IBO = 0;
    }
    if (VBO != 0) 
    {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (VAO != 0) 
    {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    indexCount = 0;
}