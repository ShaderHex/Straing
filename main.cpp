#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#include "stb/stb_image.h"
#include "tinygltf/tiny_obj_loader.h"
#include "cgltf.h"

#include "fastgltf/core.hpp"
#include "fastgltf/types.hpp"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <filesystem>

GLuint LoadTextureFromFile(const char* filepath);
GLuint LoadGLTFTexture(cgltf_data* data, const char* basePath, int imageIndex);


const int WINDOW_WIDTH = 1600;
const int WINDOW_HEIGHT = 800;
float yaw   = -90.0f;
float pitch = 0.0f;
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;
bool firstMouse = true;
float cameraSpeed = 0.1f;

glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f, 5.0f);
glm::vec3 cameraTarget= glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, 1.0f, 0.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);

struct Color {
    float r;
    float g;
    float b;
    float a;
};

struct Camera {
    glm::mat4 view;
    glm::mat4 projection;
};

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

    bool operator==(const Vertex& other) const {
        return Position == other.Position &&
               Normal == other.Normal &&
               TexCoords == other.TexCoords;
    }
};

struct Texture {
    unsigned int id;
    std::string type;
    std::string path;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<Texture> textures;
    GLuint VAO, VBO, EBO;

    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, const std::vector<Texture>& textures)
        : vertices(vertices), indices(indices), textures(textures)
    {
        setupMesh();
    }

    Mesh() : VAO(0), VBO(0), EBO(0) {}

    void Draw(GLuint& shader, const glm::mat4& view, const glm::mat4& projection, glm::vec3 position, float rotationAngleX, float rotationAngleY, float rotationAngleZ, glm::vec3 scale);

private:
    void setupMesh();
};

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(const Vertex& v) const {
            size_t h1 = hash<float>()(v.Position.x) ^ (hash<float>()(v.Position.y) << 1) ^ (hash<float>()(v.Position.z) << 2);
            size_t h2 = hash<float>()(v.Normal.x) ^ (hash<float>()(v.Normal.y) << 1) ^ (hash<float>()(v.Normal.z) << 2);
            size_t h3 = hash<float>()(v.TexCoords.x) ^ (hash<float>()(v.TexCoords.y) << 1);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void Mesh::setupMesh()
{
    glGenVertexArrays(1, &this->VAO);
    glGenBuffers(1, &this->VBO);
    glGenBuffers(1, &this->EBO);
  
    glBindVertexArray(this->VAO);
    glBindBuffer(GL_ARRAY_BUFFER, this->VBO);

    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                 &indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);   
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);   
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
    glEnableVertexAttribArray(2);   
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    glBindVertexArray(0);
}  

Mesh LoadMeshFromOBJ(const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str());

    if (!warn.empty()) std::cout << "WARN: " << warn << std::endl;
    if (!err.empty()) std::cerr << "ERR: " << err << std::endl;
    if (!ret) throw std::runtime_error("Failed to load OBJ: " + path);

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    std::unordered_map<Vertex, unsigned int> uniqueVertices;

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            Vertex vertex{};

            vertex.Position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            if (index.normal_index >= 0) {
                vertex.Normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            } else {
                vertex.Normal = glm::vec3(0.0f, 0.0f, 1.0f);
            }

            if (index.texcoord_index >= 0) {
                vertex.TexCoords = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
                };
            } else {
                vertex.TexCoords = glm::vec2(0.0f, 0.0f);
            }

            if (uniqueVertices.count(vertex) == 0) {
                uniqueVertices[vertex] = static_cast<unsigned int>(vertices.size());
                vertices.push_back(vertex);
            }
            indices.push_back(uniqueVertices[vertex]);
        }
    }

    std::vector<Texture> textures;
    // OBJ files typically reference textures in .mtl files.
    // This example assumes you'll load the texture separately as you are doing for skull.obj
    // If you need full .mtl parsing, that would be a more involved addition.

    return Mesh(vertices, indices, textures);
}

Mesh LoadMeshFromGLTF(const std::string& path)
{
    cgltf_options options = {};
    cgltf_data* data = nullptr;

    cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
    if (result != cgltf_result_success) {
        std::cerr << "Failed to parse glTF file: " << path << "\n";
        return Mesh({}, {}, {});
    }

    std::string baseDir = path.substr(0, path.find_last_of("/\\") + 1);
    if (baseDir.empty() && path.find_last_of("/\\") == std::string::npos) {
        baseDir = "./";
    }

    result = cgltf_load_buffers(&options, data, baseDir.c_str());
    if (result != cgltf_result_success) {
        std::cerr << "Failed to load buffers for glTF file: " << path << "\n";
        cgltf_free(data);
        return Mesh({}, {}, {});
    }

    if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0) {
        std::cerr << "No mesh or primitive found in glTF file: " << path << "\n";
        cgltf_free(data);
        return Mesh({}, {}, {});
    }

    const cgltf_mesh& gltfMesh = data->meshes[0];
    const cgltf_primitive& prim = gltfMesh.primitives[0];

    const cgltf_accessor* posAcc = nullptr;
    const cgltf_accessor* normAcc = nullptr;
    const cgltf_accessor* uvAcc = nullptr;
    const cgltf_accessor* idxAcc = prim.indices;

    for (size_t i = 0; i < prim.attributes_count; ++i) {
        const cgltf_attribute& attr = prim.attributes[i];
        if (strcmp(attr.name, "POSITION") == 0) posAcc = attr.data;
        else if (strcmp(attr.name, "NORMAL") == 0) normAcc = attr.data;
        else if (strcmp(attr.name, "TEXCOORD_0") == 0) uvAcc = attr.data;
    }

    if (!posAcc || !idxAcc) {
        std::cerr << "POSITION or INDICES missing in mesh.\n";
        cgltf_free(data);
        return Mesh({}, {}, {});
    }

    const float* positions = (const float*)((uint8_t*)posAcc->buffer_view->buffer->data + posAcc->buffer_view->offset + posAcc->offset);
    const float* normals = normAcc ? (const float*)((uint8_t*)normAcc->buffer_view->buffer->data + normAcc->buffer_view->offset + normAcc->offset) : nullptr;
    const float* uvs = uvAcc ? (const float*)((uint8_t*)uvAcc->buffer_view->buffer->data + uvAcc->buffer_view->offset + uvAcc->offset) : nullptr;

    std::vector<Vertex> vertices;
    for (size_t i = 0; i < posAcc->count; ++i) {
        Vertex vertex{};
        vertex.Position = glm::vec3(
            positions[i * 3 + 0],
            positions[i * 3 + 1],
            positions[i * 3 + 2]
        );

        vertex.Normal = normals ? glm::vec3(
            normals[i * 3 + 0],
            normals[i * 3 + 1],
            normals[i * 3 + 2]
        ) : glm::vec3(0.0f, 0.0f, 1.0f);

        // glTF UVs often need to be flipped vertically
        vertex.TexCoords = uvs ? glm::vec2(
            uvs[i * 2 + 0],
            1.0f - uvs[i * 2 + 1] // Flip V coordinate
        ) : glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
    }

    std::vector<unsigned int> indices;
    void* indexData = (uint8_t*)idxAcc->buffer_view->buffer->data + idxAcc->buffer_view->offset + idxAcc->offset;
    for (size_t i = 0; i < idxAcc->count; ++i) {
        switch (idxAcc->component_type) {
            case cgltf_component_type_r_16u:
                indices.push_back(((uint16_t*)indexData)[i]);
                break;
            case cgltf_component_type_r_32u:
                indices.push_back(((uint32_t*)indexData)[i]);
                break;
            case cgltf_component_type_r_8u:
                indices.push_back(((uint8_t*)indexData)[i]);
                break;
            default:
                std::cerr << "Unsupported index component type: " << idxAcc->component_type << "\n";
                break;
        }
    }

    std::vector<Texture> meshTextures;

    if (prim.material && prim.material->has_pbr_metallic_roughness) {
        const cgltf_pbr_metallic_roughness& pbr = prim.material->pbr_metallic_roughness;

        if (pbr.base_color_texture.texture) {
            const cgltf_texture* gltfTexture = pbr.base_color_texture.texture;
            if (gltfTexture->image) {
                const cgltf_image* gltfImage = gltfTexture->image;
                
                int imageIndex = -1;
                for (int i = 0; i < data->images_count; ++i) {
                    if (&data->images[i] == gltfImage) {
                        imageIndex = i;
                        break;
                    }
                }

                if (imageIndex != -1) {
                    GLuint textureID = LoadGLTFTexture(data, baseDir.c_str(), imageIndex);
                    if (textureID != 0) {
                        std::string texturePath = gltfImage->uri ? (baseDir + gltfImage->uri) : "embedded_texture";
                        meshTextures.push_back({textureID, "diffuse", texturePath});
                        std::cout << "Loaded texture: " << texturePath << std::endl;
                    }
                }
            }
        }
    }

    cgltf_free(data);
    return Mesh(vertices, indices, meshTextures);
}



void Mesh::Draw(GLuint& shader, const glm::mat4& view, const glm::mat4& projection, glm::vec3 position, float rotationAngleX, float rotationAngleY, float rotationAngleZ, glm::vec3 scale)
{
    glUseProgram(shader);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotationAngleX, glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, rotationAngleY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, rotationAngleZ, glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, scale);

    glm::mat4 mvp = projection * view * model;
    GLint mvpLoc = glGetUniformLocation(shader, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(model)));
    GLint normalMatrixLoc = glGetUniformLocation(shader, "uNormalMatrix");
    if (normalMatrixLoc != -1) {
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }

    GLuint useTextureLoc = glGetUniformLocation(shader, "uUseTexture");
    GLint colorLoc = glGetUniformLocation(shader, "uColor");

    if (!textures.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0].id);
        glUniform1i(glGetUniformLocation(shader, "uTexture"), 0);
        glUniform1i(useTextureLoc, 1);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
        glUniform1i(useTextureLoc, 0);
        glUniform4f(colorLoc, 1.0f, 0.5f, 0.2f, 1.0f);
    }

    glBindVertexArray(this->VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
}



void checkShaderCompileErrors(GLuint shader, const std::string& type) {
    GLint success;
    GLchar infoLog[1024];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);
        std::cerr << "| ERROR::SHADER-COMPILATION-ERROR of type: " << type << "\n" << infoLog << std::endl;
    }
}

void checkProgramLinkErrors(GLuint program) {
    GLint success;
    GLchar infoLog[1024];
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);
        std::cerr << "| ERROR::PROGRAM-LINKING-ERROR\n" << infoLog << std::endl;
    }
}

const char* LoadShaderFromFile(const char* fileName) {
    std::ifstream inputFile(fileName);
    if (!inputFile.is_open()) {
        std::cerr << "Failed to load the shader from file: " << fileName << "\n";
        return nullptr;
    }

    std::string source((std::istreambuf_iterator<char>(inputFile)),
                        std::istreambuf_iterator<char>());
    inputFile.close();

    char* shaderSource = new char[source.size() + 1];
    std::copy(source.begin(), source.end(), shaderSource);
    shaderSource[source.size()] = '\0';

    return shaderSource;
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    return shader;
}

GLuint createShaderProgram() {
    const char* vertexShaderSource = LoadShaderFromFile("shaders/vertex.shader");
    const char* fragmentShaderSource = LoadShaderFromFile("shaders/fragment.shader");
    
    if (!vertexShaderSource || !fragmentShaderSource) {
        std::cerr << "Failed to load shader source files.\n";
        return 0;
    }

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    checkShaderCompileErrors(vertexShader, "VERTEX");
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    checkShaderCompileErrors(fragmentShader, "FRAGMENT");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);
    checkProgramLinkErrors(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    delete[] vertexShaderSource;
    delete[] fragmentShaderSource;

    return shaderProgram;
}

void drawTriangle3D(GLuint shaderProgram, 
                    glm::vec3 pos, 
                    glm::vec3 scale,
                    float rotation,
                    Color color, 
                    const glm::mat4& view, 
                    const glm::mat4& projection) {

    static GLuint VAO = 0, VBO = 0;

    if (VAO == 0) {
        float vertices[] = {
            -0.5f, 0.0f, -0.5f,
            0.5f, 0.0f, -0.5f,
            -0.5f, 0.0f,  0.5f,

            0.5f, 0.0f, -0.5f,
            0.5f, 0.0f,  0.5f,
            -0.5f, 0.0f,  0.5f,

            -0.5f, 0.0f, -0.5f,
            0.5f, 0.0f, -0.5f,
            0.0f, 0.8f, 0.0f,

            0.5f, 0.0f, -0.5f,
            0.5f, 0.0f,  0.5f,
            0.0f, 0.8f, 0.0f,

            0.5f, 0.0f, 0.5f,
            -0.5f, 0.0f, 0.5f,
            0.0f, 0.8f, 0.0f,

            -0.5f, 0.0f,  0.5f,
            -0.5f, 0.0f, -0.5f,
            0.0f, 0.8f, 0.0f,
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }

    glUseProgram(shaderProgram);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);

    glm::mat4 mvp = projection * view * model;

    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform4f(colorLoc, color.r, color.g, color.b, color.a);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 18);
    glBindVertexArray(0);
}

Camera cameraUpdate() {
    Camera cam;

    cam.view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);

    float aspectRatio = (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT;
    cam.projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    return cam;
}

void inputHandler(GLFWwindow* window) {
    bool isSprint = false;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos.y -= cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos.y += cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        isSprint = !isSprint;

    if (isSprint) {
        cameraSpeed = 0.2f;
        cameraSpeed += 0.1f;
    } else {
        cameraSpeed = 0.1f;
    }
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    static float sensitivity = 0.1f;

    int rightButtonState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);

    if (rightButtonState == GLFW_PRESS) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        if (firstMouse) {
            lastX = xpos;
            lastY = ypos;
            firstMouse = false;
        }

        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;

        lastX = xpos;
        lastY = ypos;

        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw += xoffset;
        pitch += yoffset;

        if (pitch > 89.0f)
            pitch = 89.0f;
        if (pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
    else if (rightButtonState == GLFW_RELEASE) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        firstMouse = true;
    }
}


GLuint LoadTextureFromFile(const char* filepath) {
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << filepath << std::endl;
        return 0;
    }

    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLenum format = 0;
    if (nrChannels == 1)
        format = GL_RED;
    else if (nrChannels == 3)
        format = GL_RGB;
    else if (nrChannels == 4)
        format = GL_RGBA;
    else {
        std::cerr << "Unsupported number of channels for texture: " << nrChannels << " in " << filepath << std::endl;
        stbi_image_free(data);
        return 0;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return textureID;
}


GLuint LoadGLTFTexture(cgltf_data* data, const char* basePath, int imageIndex) {
    cgltf_image* image = &data->images[imageIndex];
    
    if (image->uri && !image->buffer_view) {
        std::filesystem::path fullPath = std::filesystem::path(basePath) / image->uri;
        return LoadTextureFromFile(fullPath.string().c_str());
    }

    if(image->buffer_view && image->buffer_view->buffer->data) {
        const unsigned char* bufferData = (const unsigned char*)image->buffer_view->buffer->data;
        size_t offset = image->buffer_view->offset;
        size_t size = image->buffer_view->size;

        int width, height, channels;
        // Do NOT flip embedded glTF textures vertically; the glTF spec's UVs are already top-left origin.
        stbi_set_flip_vertically_on_load(false); 
        unsigned char* pixels = stbi_load_from_memory(bufferData + offset, size, &width, &height, &channels, 0);
        stbi_set_flip_vertically_on_load(true); // Revert to default for other loads

        if (!pixels) {
            std::cerr << "Failed to load embedded image from glTF buffer view\n";
            return 0;
        }

        GLuint textureID;
        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLenum format = 0;
        if (channels == 1)
            format = GL_RED;
        else if (channels == 3)
            format = GL_RGB;
        else if (channels == 4)
            format = GL_RGBA;
        else {
            std::cerr << "Unsupported number of channels for embedded texture: " << channels << "\n";
            stbi_image_free(pixels);
            return 0;
        }

        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(pixels);
        return textureID;
    }
    std::cerr << "Texture format not supported (no URI and no buffer view) for image index: " << imageIndex << "\n";
    return 0;
}

void drawPlane3D(GLuint shaderProgram, 
                    glm::vec3 pos, 
                    glm::vec3 scale,
                    float rotation,
                    Color color, 
                    const glm::mat4& view, 
                    const glm::mat4& projection) {

    static GLuint VAO = 0, VBO = 0;

    if (VAO == 0) {
        float vertices[] = {
            -0.5f, 0.0f, -0.5f,   0.0f, 1.0f, 0.0f, 
             0.5f, 0.0f, -0.5f,   0.0f, 1.0f, 0.0f, 
            -0.5f, 0.0f,  0.5f,   0.0f, 1.0f, 0.0f, 

             0.5f, 0.0f, -0.5f,   0.0f, 1.0f, 0.0f, 
             0.5f, 0.0f,  0.5f,   0.0f, 1.0f, 0.0f, 
            -0.5f, 0.0f,  0.5f,   0.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    glUseProgram(shaderProgram);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);

    glm::mat4 mvp = projection * view * model;

    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform4f(colorLoc, color.r, color.g, color.b, color.a);

    GLint useTextureLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    glUniform1i(useTextureLoc, 0);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void drawSphere3D(GLuint shaderProgram, 
                  glm::vec3 pos, 
                  glm::vec3 scale,
                  float rotation,
                  Color color, 
                  const glm::mat4& view, 
                  const glm::mat4& projection) {

    static GLuint VAO = 0, VBO = 0;
    static int vertexCount = 0;

    if (VAO == 0) {
        const int stacks = 16;
        const int slices = 32;
        const float radius = 0.5f;

        std::vector<float> vertices;

        for (int i = 0; i <= stacks; ++i) {
            float phi1 = glm::pi<float>() * (float)(i) / (float)stacks;
            float phi2 = glm::pi<float>() * (float)(i + 1) / (float)stacks;

            for (int j = 0; j <= slices; ++j) {
                float theta1 = 2.0f * glm::pi<float>() * (float)(j) / (float)slices;
                float theta2 = 2.0f * glm::pi<float>() * (float)(j + 1) / (float)slices;

                vertices.push_back(radius * sin(phi1) * cos(theta1));
                vertices.push_back(radius * cos(phi1));
                vertices.push_back(radius * sin(phi1) * sin(theta1));
                glm::vec3 normal1 = glm::normalize(glm::vec3(radius * sin(phi1) * cos(theta1), radius * cos(phi1), radius * sin(phi1) * sin(theta1)));
                vertices.push_back(normal1.x); vertices.push_back(normal1.y); vertices.push_back(normal1.z);

                vertices.push_back(radius * sin(phi2) * cos(theta1));
                vertices.push_back(radius * cos(phi2));
                vertices.push_back(radius * sin(phi2) * sin(theta1));
                glm::vec3 normal2 = glm::normalize(glm::vec3(radius * sin(phi2) * cos(theta1), radius * cos(phi2), radius * sin(phi2) * sin(theta1)));
                vertices.push_back(normal2.x); vertices.push_back(normal2.y); vertices.push_back(normal2.z);

                vertices.push_back(radius * sin(phi2) * cos(theta2));
                vertices.push_back(radius * cos(phi2));
                vertices.push_back(radius * sin(phi2) * sin(theta2));
                glm::vec3 normal3 = glm::normalize(glm::vec3(radius * sin(phi2) * cos(theta2), radius * cos(phi2), radius * sin(phi2) * sin(theta2)));
                vertices.push_back(normal3.x); vertices.push_back(normal3.y); vertices.push_back(normal3.z);


                vertices.push_back(radius * sin(phi1) * cos(theta1));
                vertices.push_back(radius * cos(phi1));
                vertices.push_back(radius * sin(phi1) * sin(theta1));
                vertices.push_back(normal1.x); vertices.push_back(normal1.y); vertices.push_back(normal1.z);

                vertices.push_back(radius * sin(phi2) * cos(theta2));
                vertices.push_back(radius * cos(phi2));
                vertices.push_back(radius * sin(phi2) * sin(theta2));
                vertices.push_back(normal3.x); vertices.push_back(normal3.y); vertices.push_back(normal3.z);

                vertices.push_back(radius * sin(phi1) * cos(theta2));
                vertices.push_back(radius * cos(phi1));
                vertices.push_back(radius * sin(phi1) * sin(theta2));
                glm::vec3 normal4 = glm::normalize(glm::vec3(radius * sin(phi1) * cos(theta2), radius * cos(phi1), radius * sin(phi1) * sin(theta2)));
                vertices.push_back(normal4.x); vertices.push_back(normal4.y); vertices.push_back(normal4.z);
            }
        }

        vertexCount = vertices.size() / 6;
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    glUseProgram(shaderProgram);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::rotate(model, rotation, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);

    glm::mat4 mvp = projection * view * model;

    GLint mvpLoc = glGetUniformLocation(shaderProgram, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    GLint colorLoc = glGetUniformLocation(shaderProgram, "uColor");
    glUniform4f(colorLoc, color.r, color.g, color.b, color.a);

    GLint useTextureLoc = glGetUniformLocation(shaderProgram, "uUseTexture");
    glUniform1i(useTextureLoc, 0);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount);
    glBindVertexArray(0);
}

void ShadowInit(const unsigned int SHADOW_WIDTH, const unsigned int SHADOW_HEIGHT) {
    GLuint depthMapFBO;
    glGenFramebuffers(1, &depthMapFBO);

    GLuint depthMap;
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

int main() {
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Straing", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return -1;
    }

    GLuint shaderProgram = createShaderProgram();
    if (shaderProgram == 0) {
        glfwTerminate();
        return -1;
    }
    
    float rotation = 0;

    glEnable(GL_DEPTH_TEST);

    cameraPos.x = -3.25266f;
    cameraPos.y = 1.53258f;
    cameraPos.z = 5.0f;

    ShadowInit(SHADOW_WIDTH, SHADOW_HEIGHT);

    Mesh Skull = LoadMeshFromOBJ("models/skull.obj");

    //GLuint skullTexture = LoadTextureFromFile("models/Skull.jpg");
    //Skull.textures.push_back({ skullTexture, "diffuse", "models/Skull.jpg" });

    //Mesh TeaPot = LoadMeshFromGLTF("models/teapot/scene.gltf");
    
    //Mesh BuildingModel = LoadMeshFromGLTF("models/building/scene.gltf");

    Mesh CarModel = LoadMeshFromGLTF("models/car/scene.gltf");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    while (!glfwWindowShouldClose(window)) {
        inputHandler(window);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Camera currentCamera = cameraUpdate();

        //Skull.Draw(shaderProgram, currentCamera.view, currentCamera.projection, 
        //           glm::vec3(0.0f, 0.0f, 0.0f), 0.0f, rotation, 0.0f, glm::vec3(1.0f));
        
        //TeaPot.Draw(shaderProgram, currentCamera.view, currentCamera.projection, 
        //            glm::vec3(2.0f, 0.0f, 0.0f), 0.0f, rotation, 0.0f, glm::vec3(1.0f));

        //BuildingModel.Draw(shaderProgram, currentCamera.view, currentCamera.projection, 
        //                   glm::vec3(-2.0f, 0.0f, 0.0f), 0.0f, rotation, 0.0f, glm::vec3(1.0f));
        
        drawPlane3D(shaderProgram, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(100.0f, 100.0f, 100.0f), 0, {1, 1, 1}, currentCamera.view, currentCamera.projection);
       
        CarModel.Draw(shaderProgram, currentCamera.view, currentCamera.projection, 
                           glm::vec3(-6.0f, 0.0f, 0.0f), 0.0f, rotation, 0.0f, glm::vec3(1.0f));

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        ImGui::Begin("Debug");
            ImGui::Text("CameraSpeed: %f", cameraSpeed);
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
