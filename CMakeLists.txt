cmake_minimum_required(VERSION 3.20)
project(SoftRenderer VERSION 0.1.0)

set(RESOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Editor/resources)
set(EDITOR_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Editor/src)
set(RENDERER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Renderer/src)
set(WALNUT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Walnut/src)
set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty)


find_package(spdlog CONFIG REQUIRED)

find_package(Vulkan REQUIRED)

find_package(glm REQUIRED)

find_package(OpenMP REQUIRED)

find_package(Assimp)

find_package(imguizmo CONFIG REQUIRED)

find_package(embree 3 CONFIG REQUIRED)

find_package(TBB CONFIG REQUIRED)


# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${THIRD_PARTY_DIR}/glfw)

# find_package(glfw3 CONFIG REQUIRED)

# Dear ImGui
set(IMGUI_DIR ${THIRD_PARTY_DIR}/imgui)
file(GLOB imgui_src ${IMGUI_DIR}/*.cpp)
set(imgui_backend ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)
add_library(imgui STATIC ${imgui_src} ${imgui_backend})
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui glfw)



# Source files
file(GLOB_RECURSE walnut_src ${WALNUT_SOURCE_DIR}/*.cpp )
file(GLOB_RECURSE renderer_src ${RENDERER_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE editor_src ${EDITOR_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE resources ${RESOURCE_DIR}/*.rc)

# Walnut: Simple App Framework by The Cherno
add_library(Walnut ${walnut_src})
target_include_directories(Walnut PUBLIC ${WALNUT_SOURCE_DIR})
target_link_libraries(Walnut PUBLIC Renderer Vulkan::Vulkan imgui glfw glm::glm)
target_include_directories(Walnut PRIVATE ${THIRD_PARTY_DIR}/glfw/include ${THIRD_PARTY_DIR}/stb)
target_compile_features(Walnut PRIVATE cxx_std_20)

# Renderer Runtime for Editor
add_library(Renderer ${renderer_src})
target_include_directories(Renderer PUBLIC ${RENDERER_SOURCE_DIR} ${THIRD_PARTY_DIR}/tinyobjloader ${THIRD_PARTY_DIR}/stb)
target_link_libraries(Renderer PUBLIC Walnut spdlog::spdlog glm::glm assimp::assimp glfw)
target_link_libraries(Renderer PRIVATE embree)
target_link_libraries(Renderer PRIVATE OpenMP::OpenMP_CXX)
target_link_libraries(Renderer PRIVATE TBB::tbb TBB::tbbmalloc TBB::tbbmalloc_proxy)
target_compile_features(Renderer PRIVATE cxx_std_20)

# Editor
add_executable(Editor ${editor_src} ${resources})
target_include_directories(Editor PRIVATE ${EDITOR_SOURCE_DIR})
target_include_directories(Editor PRIVATE ${THIRD_PARTY_DIR}/glfw/include ${THIRD_PARTY_DIR}/stb)
target_link_libraries(Editor PRIVATE Walnut imgui glm::glm glfw)
target_link_libraries(Editor PRIVATE OpenMP::OpenMP_CXX)


target_compile_features(Editor PRIVATE cxx_std_20)


set_property(TARGET OpenMP::OpenMP_CXX
                 PROPERTY INTERFACE_COMPILE_OPTIONS ${OpenMP_CXX_FLAGS})
    # Only works if the same flag is passed to the linker; use CMake 3.9+ otherwise (Intel, AppleClang)
set_property(TARGET OpenMP::OpenMP_CXX
                 PROPERTY INTERFACE_LINK_LIBRARIES ${OpenMP_CXX_FLAGS} Threads::Threads)

