cmake_minimum_required(VERSION 3.20)
project(Renderer C CXX)
set(THIRDPARTY_DIR ${CMAKE_SOURCE_DIR}/Third_Party)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#excutable files
file(GLOB_RECURSE SOURCE_CPP_FILES src/*.cpp)
add_executable (Renderer ${SOURCE_CPP_FILES})

# include_directories(${CMAKE_SOURCE_DIR}/Tools/include)

#PCL
find_package(PCL CONFIG REQUIRED)

#glm
add_subdirectory(${THIRDPARTY_DIR}/glm-master)

add_subdirectory(${THIRDPARTY_DIR}/glfw-3.4)

add_subdirectory(${THIRDPARTY_DIR}/glad)

find_package(glfw3 3.4 REQUIRED)
find_package(OpenGL REQUIRED)

# MyLib
file(GLOB_RECURSE Tools_FILES ${CMAKE_SOURCE_DIR}/Tools/src/*.cpp)
add_library(MyLib STATIC
    ${THIRDPARTY_DIR}/glad/src
    ${Tools_FILES}
)
target_link_libraries(MyLib PUBLIC glm::glm)

target_include_directories(MyLib PUBLIC 
    ${THIRDPARTY_DIR}/include
    ${THIRDPARTY_DIR}/glad/include
    ${CMAKE_SOURCE_DIR}/Tools/include
    ${PCL_INCLUDE_DIRS}
)

add_definitions(${PCL_DEFINITIONS})

target_include_directories(Renderer PRIVATE ${PCL_INCLUDE_DIRS})

target_link_libraries(Renderer PRIVATE
    glad
    glm::glm
    MyLib
    OpenGL::GL 
    glfw
    ${PCL_LIBRARIES}
)
