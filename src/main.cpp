#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <type_traits>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Shader.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <pcl/point_types.h>
#include <pcl/impl/point_types.hpp>
#include <pcl/point_cloud.h>
#include <pcl/register_point_struct.h>
#include <pcl/io/ply_io.h> // loadPLYFile

// Functionsdeclarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window);
void DrawStuff();
GLuint setupSSBO(const GLuint& bindIdx, const auto& bufferData);
void updateSSBO(const auto& bufferData, GLuint ssbo);
glm::vec4 normalizeRotation(glm::vec4& rot);
float sigmoid(float opacity);
glm::vec3 SH2RGB(const glm::vec3& color);
std::vector<uint32_t> sortGaussians(const pcl::PointCloud<struct GaussianData>::Ptr& splatCloud, const glm::mat4& viewMat);

// Type & const
// Define custom point type with exact property names
const float C0 = 0.28209479177387814f;
struct GaussianData {
	PCL_ADD_POINT4D;                  

	float nx, ny, nz;                
	float f_dc_0, f_dc_1, f_dc_2;     
	float opacity;
	float scale_0, scale_1, scale_2;
	float rot_0, rot_1, rot_2, rot_3;

	PCL_MAKE_ALIGNED_OPERATOR_NEW 
};

// Register the custom point type with PCL
POINT_CLOUD_REGISTER_POINT_STRUCT(
	GaussianData,
	(float, x, x) (float, y, y) (float, z, z)
	(float, nx, nx) (float, ny, ny) (float, nz, nz)
	(float, f_dc_0, f_dc_0) (float, f_dc_1, f_dc_1) (float, f_dc_2, f_dc_2)
	(float, opacity, opacity)
	(float, scale_0, scale_0) (float, scale_1, scale_1) (float, scale_2, scale_2)
	(float, rot_0, rot_0) (float, rot_1, rot_1) (float, rot_2, rot_2) (float, rot_3, rot_3)
);

// parameters
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 800;
float vertices[] = {
    -1.0f,  1.0f,
	 1.0f,  1.0f,
	 1.0f, -1.0f,
	-1.0f, -1.0f
};

unsigned int indices[] = {
    0, 1, 2,
	0, 2, 3
};

unsigned int VBO,VAO,EBO;

GLFWwindow* window;

//camera
float znear = 0.01f;
float zfar = 100.f;
glm::vec3 hfov_focal(0.0f);
glm::mat4 viewMat;
glm::vec3 cameraPos   = glm::vec3(0.0f, 0.0f,  3.0f);
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 cameraUp    = glm::vec3(0.0f, -1.0f,  0.0f);

//time
float deltaTime = 0.0f; // delta of frame time
float lastFrame = 0.0f; // last frame time

//Geometry
pcl::PointCloud<GaussianData>::Ptr splatCloud(new pcl::PointCloud<GaussianData>);
std::vector<float> flatGaussianData;

//Mode
bool is_training = false;

//mouse
float lastX = SCR_WIDTH/2, lastY = SCR_HEIGHT/2;
bool firstMouse=true;
bool btn_click=true;
float pitch = 0, yaw = -90;
float fov=45;

int main()
{
    //Read GS with PCL
    // Create a PointCloud with the custom point type
    const std::string plyPath = "model/point_cloud.ply";
    if (pcl::io::loadPLYFile(plyPath, *splatCloud) < 0)
    {
        std::cerr << "Failed to load PLY file!" << std::endl;
        return -1;
    }
    std::cout << "Loaded " << splatCloud->points.size() << " gaussians." << std::endl;

    flatGaussianData.clear();
    flatGaussianData.reserve(splatCloud->points.size() * 14);

    for (const auto& point : splatCloud->points)
    {
        glm::vec4 q(point.rot_0, point.rot_1, point.rot_2, point.rot_3);
        glm::vec4 normQ = normalizeRotation(q);
        glm::vec3 scale = glm::exp(glm::vec3(point.scale_0, point.scale_1, point.scale_2));
        float alpha = sigmoid(point.opacity);
        glm::vec3 color = SH2RGB(glm::vec3(point.f_dc_0, point.f_dc_1, point.f_dc_2));

        // 
        flatGaussianData.push_back(point.x);
        flatGaussianData.push_back(point.y);
        flatGaussianData.push_back(point.z);

        flatGaussianData.push_back(normQ.x);
        flatGaussianData.push_back(normQ.y);
        flatGaussianData.push_back(normQ.z);
        flatGaussianData.push_back(normQ.w);

        flatGaussianData.push_back(scale.x);
        flatGaussianData.push_back(scale.y);
        flatGaussianData.push_back(scale.z);

        flatGaussianData.push_back(alpha);

        flatGaussianData.push_back(color.r);
        flatGaussianData.push_back(color.g);
        flatGaussianData.push_back(color.b);
    }

    std::cout << "Flattened " << flatGaussianData.size() / 14 << " Gaussians to SSBO format." << std::endl;

    
    
    // Calculated outside render loop:
    float htany = tan(glm::radians(fov) / 2);
    float htanx = htany / SCR_HEIGHT * SCR_WIDTH;
    float focal_z = SCR_HEIGHT / (2 * htany);
    hfov_focal = glm::vec3(htanx, htany, focal_z);

    //OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    glm::mat4 model=glm::mat4(1.0f);
    glm::mat4 view=glm::mat4(1.0f);
    glm::mat4 projection=glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(0.0f), glm::vec3(1.0f, 0.0f, 0.0f));

    projection = glm::perspective(glm::radians(45.0f), static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT), znear, zfar);

    window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "GS Render", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        const char* description;
        int code = glfwGetError(&description);
        std::cout << "Error code: " << code << std::endl;
        std::cout << "Error description: " << description << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    // glEnable(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);

    Shader ourShader("shaders/chap01/shader.vert", 
        "shaders/chap01/shader.frag");
    glGenBuffers(1, &VBO);
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    //copy to buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    //set Pointer
    GLint quad_position = 0;
    glVertexAttribPointer(quad_position, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(quad_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, 0); 
    // glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    glBindVertexArray(0);


    GLuint ssbo1 = setupSSBO(1, flatGaussianData);

    viewMat  =  glm::lookAt(cameraPos,
                            glm::vec3(0.0f, 0.0f, 0.0f),
                            cameraUp);
    std::vector<uint32_t> gausIdx = sortGaussians(splatCloud, viewMat);

    GLuint ssbo2 = setupSSBO(2, gausIdx);

    //Render Loop
    while(!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);

        ourShader.use();
        view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
        gausIdx = sortGaussians(splatCloud, view);
        updateSSBO(gausIdx,ssbo2);
        projection  =  glm::perspective(glm::radians(fov), 
                                        static_cast<float>(SCR_WIDTH) / static_cast<float>(SCR_HEIGHT),
                                        znear, zfar);
        htany = tan(glm::radians(fov) / 2);
        htanx = htany / SCR_HEIGHT * SCR_WIDTH;
        focal_z = SCR_HEIGHT / (2 * htany);
        hfov_focal = glm::vec3(htanx, htany, focal_z);
        float timeValue = glfwGetTime();
        float myTime = (sin(timeValue) / 2.0f) + 0.5f;
        ourShader.setFloat("sinesig",myTime);
        ourShader.setFloat("time", timeValue);
        ourShader.setMatrix4("model", model);
        ourShader.setMatrix4("view", view);
        ourShader.setMatrix4("projection",projection);
        ourShader.setVector3("hfov_focal",hfov_focal);
        // int modelLoc = glGetUniformLocation(ourShader.ID, "model");
        // glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        DrawStuff();
    }

    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &ssbo1);
    glDeleteBuffers(1, &ssbo2);
    // glDeleteProgram(shaderProgram);
    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

//Input
void processInput(GLFWwindow *window)
{
    if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS){
        glfwSetWindowShouldClose(window, true);
    }
    float cameraSpeed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        cameraPos += cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        cameraPos -= cameraSpeed * cameraFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
    glm::vec3 cameraRight = glm::normalize(glm::cross(cameraFront, cameraUp));
    glm::vec3 cameraLocalUp = glm::normalize(glm::cross(cameraRight, cameraFront));
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        cameraPos += cameraLocalUp * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        cameraPos -= cameraLocalUp * cameraSpeed;
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos){

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_RELEASE ){
        btn_click=true;
    }
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ){
        if(btn_click)
        {
            lastX = xpos;
            lastY = ypos;
            btn_click = false;
        }
        float xoffset = xpos - lastX;
        float yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        float sensitivity = 0.05;
        xoffset *= sensitivity;
        yoffset *= sensitivity;

        yaw   += xoffset;
        pitch += yoffset;

        if(pitch > 89.0f)
            pitch = 89.0f;
        if(pitch < -89.0f)
            pitch = -89.0f;

        glm::vec3 front;
        front.x =cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y =sin(glm::radians(pitch));
        front.z =sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        cameraFront = glm::normalize(front);
    }
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
  if(fov >= 1.0f && fov <= 45.0f)
    fov -= yoffset;
  if(fov <= 1.0f)
    fov = 1.0f;
  if(fov >= 45.0f)
    fov = 45.0f;
}

void DrawStuff(){
    //Render
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glClear(GL_COLOR_BUFFER_BIT);
    
    glBindVertexArray(VAO);

    GLsizei numInstances = static_cast<GLsizei>(splatCloud->points.size());
    if (numInstances > 0) {
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0, numInstances);
    }

    // glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    
    glBindVertexArray(0);
    
    glfwSwapBuffers(window);
    glfwPollEvents();
}

GLuint setupSSBO(const GLuint& bindIdx, const auto& bufferData){
        using BufferValue = typename std::decay_t<decltype(bufferData)>::value_type;
		GLuint ssbo;
		// Generate SSBO
		glGenBuffers(1, &ssbo);
		// Bind ssbo to GL_SHADER_STORAGE_BUFFER
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		// Populate GL_SHADER_STORAGE_BUFFER with our data
		glBufferData(GL_SHADER_STORAGE_BUFFER, bufferData.size() * sizeof(BufferValue), bufferData.empty() ? nullptr : bufferData.data(), GL_STATIC_DRAW);
		// Specify the index of the binding (bindIdx)
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bindIdx, ssbo);
		// Unbind ssbo to GL_SHADER_STORAGE_BUFFER
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		return ssbo;
};

void updateSSBO(const auto& bufferData, GLuint ssbo){
		// Bind ssbo to GL_SHADER_STORAGE_BUFFER
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		// Populate GL_SHADER_STORAGE_BUFFER with our data
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,0,bufferData.size() * sizeof(uint32_t),bufferData.data());
		// Unbind ssbo to GL_SHADER_STORAGE_BUFFER
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		return;
};

glm::vec4 normalizeRotation(glm::vec4& rot) {
	float sumOfSquares = rot.x * rot.x + rot.y * rot.y + rot.z * rot.z + rot.w * rot.w;
	float invnorm = (sumOfSquares > 0.0f) ? 1.0f / std::sqrt(sumOfSquares) : 1.0f;
    return glm::vec4(rot.x * invnorm, rot.y * invnorm, rot.z * invnorm, rot.w * invnorm);
};

float sigmoid(float opacity) {
	return 1.0 / (1.0 + std::exp(-opacity));
};

glm::vec3 SH2RGB(const glm::vec3& color) {
	// return 0.5f + C0 * color;
    return glm::clamp(0.5f + 0.5f * color, 0.0f, 1.0f);
};

std::vector<uint32_t> sortGaussians(const pcl::PointCloud<GaussianData>::Ptr& splatCloud, const glm::mat4& viewMat) {
	std::vector<std::pair<float, uint32_t>> depthIndex;
	size_t count = 0;
	for (const auto& point : splatCloud->points) {

		const glm::vec3 xyz = glm::vec3(point.x, point.y, point.z);
		glm::vec4 xyzView = viewMat * glm::vec4(xyz, 1.0f);

		float depth = xyzView.z;

		depthIndex.emplace_back(depth, static_cast<int>(count));
		++count;
	}

	std::sort(depthIndex.begin(), depthIndex.end(), [](const std::pair<float, int>& a, const std::pair<float, int>& b) {
		return a.first < b.first;
    });

	std::vector<uint32_t> sortedIndices;
	sortedIndices.reserve(depthIndex.size());
	for (const auto& pair : depthIndex) {
		sortedIndices.push_back(pair.second);
	}
	return sortedIndices;
};

