#define STB_IMAGE_IMPLEMENTATION
#define TINYOBJLOADER_IMPLEMENTATION
#include "stb/stb_image.h"
#include "tinyobjloader/tiny_obj_loader.h"

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

const int WINDOW_WIDTH = 1600;
const int WINDOW_HEIGHT = 800;
float yaw   = -90.0f;
float pitch = 0.0f;
float lastX = WINDOW_WIDTH / 2.0f;
float lastY = WINDOW_HEIGHT / 2.0f;
bool firstMouse = true;

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
    std::string name;
};

class Mesh {
    public:
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        std::vector<Texture> textures;

        Mesh(std::vector<Vertex> vertices, std::vector<unsigned int> indices, std::vector<Texture> textures)
        {
            this->vertices = vertices;
            this->indices = indices;
            this->textures = textures;

            setupMesh();
        }
        void Draw(GLuint &shader, const glm::mat4& view, const glm::mat4& projection, glm::vec3 position, float rotationAngle, glm::vec3 scale);

    private:
        unsigned int VAO, VBO, EBO;

        void setupMesh();

};

void Mesh::setupMesh()
{
    GLuint VAO = 0, VBO = 0, EBO = 0;
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

    //std::unordered_map<Vertex, unsigned int> uniqueVertices;

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
            }

            if (index.texcoord_index >= 0) {
                vertex.TexCoords = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            vertices.push_back(vertex);
            indices.push_back(indices.size());
        }
    }

    std::vector<Texture> textures;

    return Mesh(vertices, indices, textures);
}

void Mesh::Draw(GLuint &shader, const glm::mat4& view, const glm::mat4& projection, glm::vec3 position, float rotationAngle, glm::vec3 scale)
{
    glUseProgram(shader);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, scale);

    glm::mat4 mvp = projection * view * model;
    GLint mvpLoc = glGetUniformLocation(shader, "uMVP");
    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));

    GLint colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, 1.0f, 0.5f, 0.2f, 1.0f);

    if (!textures.empty()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[0].id);
        glUniform1i(glGetUniformLocation(shader, "uTexture"), 0);
    }

    glBindVertexArray(this->VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
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
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
    checkShaderCompileErrors(vertexShader, "VERTEX");
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    checkShaderCompileErrors(fragmentShader, "FRAGMENT");

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

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
            -0.5f, 0.0f, -0.5f,  // triangle 1
            0.5f, 0.0f, -0.5f,
            -0.5f, 0.0f,  0.5f,

            0.5f, 0.0f, -0.5f,  // triangle 2
            0.5f, 0.0f,  0.5f,
            -0.5f, 0.0f,  0.5f,

            // Side 1
            -0.5f, 0.0f, -0.5f,
            0.5f, 0.0f, -0.5f,
            0.0f, 0.8f, 0.0f,

            // Side 2
            0.5f, 0.0f, -0.5f,
            0.5f, 0.0f,  0.5f,
            0.0f, 0.8f, 0.0f,

            // Side 3
            0.5f, 0.0f, 0.5f,
            -0.5f, 0.0f, 0.5f,
            0.0f, 0.8f, 0.0f,

            // Side 4
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
    float cameraSpeed = 0.1f;

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

    if (nrChannels == 3)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    else if (nrChannels == 4)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    else {
        std::cerr << "Unsupported number of channels: " << nrChannels << std::endl;
        stbi_image_free(data);
        return 0;
    }

    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
    return textureID;
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
            -0.5f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f, 
             0.5f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f, 
            -0.5f, 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 

             0.5f, 0.0f, -0.5f, 0.0f, 1.0f, 0.0f, 
             0.5f, 0.0f,  0.5f, 0.0f, 1.0f, 0.0f, 
            -0.5f, 0.0f,  0.5f, 0.0f, 1.0f, 0.0f,
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

                // First triangle
                vertices.push_back(radius * sin(phi1) * cos(theta1));
                vertices.push_back(radius * cos(phi1));
                vertices.push_back(radius * sin(phi1) * sin(theta1));

                vertices.push_back(radius * sin(phi2) * cos(theta1));
                vertices.push_back(radius * cos(phi2));
                vertices.push_back(radius * sin(phi2) * sin(theta1));

                vertices.push_back(radius * sin(phi2) * cos(theta2));
                vertices.push_back(radius * cos(phi2));
                vertices.push_back(radius * sin(phi2) * sin(theta2));

                // Second triangle
                vertices.push_back(radius * sin(phi1) * cos(theta1));
                vertices.push_back(radius * cos(phi1));
                vertices.push_back(radius * sin(phi1) * sin(theta1));

                vertices.push_back(radius * sin(phi2) * cos(theta2));
                vertices.push_back(radius * cos(phi2));
                vertices.push_back(radius * sin(phi2) * sin(theta2));

                vertices.push_back(radius * sin(phi1) * cos(theta2));
                vertices.push_back(radius * cos(phi1));
                vertices.push_back(radius * sin(phi1) * sin(theta2));
            }
        }

        vertexCount = vertices.size() / 3;

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

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
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Straing", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, mouse_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD!" << std::endl;
        return -1;
    }

    GLuint shaderProgram = createShaderProgram();

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*) 0);
    glEnableVertexAttribArray(0);
    float rotation = 0;

    glEnable(GL_DEPTH_TEST);

    cameraPos.x = -3.25266;
    cameraPos.y = 1.53258;

    ShadowInit(SHADOW_WIDTH, SHADOW_HEIGHT);

    Mesh Skull = LoadMeshFromOBJ("models/skull.obj");

    GLuint skullTexture = LoadTextureFromFile("models/Skull.jpg");
    Skull.textures.push_back({ skullTexture, "diffuse" });
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    float sphereX = 0.0f, sphereY = 1.0f, sphereZ = 0.0f;
    while (!glfwWindowShouldClose(window)) {

        inputHandler(window);

        glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        Camera cam = cameraUpdate();
        rotation += 0.1;
        GLint lightDirLoc = glGetUniformLocation(shaderProgram, "lightDir");
        GLint viewPosLoc  = glGetUniformLocation(shaderProgram, "viewPos");

        glm::vec3 lightDir = glm::vec3(-0.2f, -1.0f, -0.3f);

        glUseProgram(shaderProgram);
        
        glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));
        glUniform3fv(viewPosLoc, 1, glm::value_ptr(cameraPos));
        //drawPlane3D(shaderProgram, 
        //            glm::vec3(0.0f, 0.0f, 0.0f),
        //            glm::vec3(10.0f, 10.0f, 10.0f),
        //            0,
        //            {1.0f, 1.0f, 1.0f, 1.0f},
        //            cam.view, cam.projection
        //);
        //drawTriangle3D(shaderProgram, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(10.0, 10.0, 10.0), 2.0f, {1.0f, 1.0f, 1.0f, 1.0f}, cam.view, cam.projection);
        Skull.Draw(shaderProgram,
            cam.view,
            cam.projection,
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::radians(45.0f),
            glm::vec3(0.5f)
        );

        drawSphere3D(shaderProgram, 
             glm::vec3(sphereX, sphereY, sphereZ), 
             glm::vec3(1.0f, 1.0f, 1.0f), 
             rotation, 
             {0.0f, 1.0f, 1.0f, 1.0f}, 
             cam.view, cam.projection);
        
        ImGui::Begin("Inspector");
            ImGui::DragFloat("X", &sphereX, 0.3);
            ImGui::DragFloat("Y", &sphereY, 0.3);
            ImGui::DragFloat("Z", &sphereZ, 0.3);
        ImGui::End();
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
