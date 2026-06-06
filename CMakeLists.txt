cmake_minimum_required(VERSION 3.20)
project(ShaderViewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

include(FetchContent)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.6
)
FetchContent_MakeAvailable(imgui)

# DirectXTex
FetchContent_Declare(
    directxtex
    GIT_REPOSITORY https://github.com/microsoft/DirectXTex.git
    GIT_TAG sep2024
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(directxtex)

# Assimp (minimal build)
FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

add_executable(ShaderViewer WIN32
    src/main.cpp
    src/RendererD3D11.cpp
    src/ShaderCompiler.cpp
    src/FileWatcher.cpp
    src/AssetScanner.cpp
    src/TextureLoader.cpp
    src/MeshLoader.cpp
    src/MeshGen.cpp
    src/Camera.cpp
    src/Material.cpp
    src/AppMesh.cpp
    src/ImGuiLayer.cpp
    src/UserSettings.cpp
    src/LivePaint.cpp
    src/LivePaintAssets.cpp
    resources/app.rc
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(ShaderViewer PRIVATE
    src
    resources
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(ShaderViewer PRIVATE
    d3d11
    dxgi
    d3dcompiler
    shell32
    DirectXTex
    assimp
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_CONFIGURATION_TYPES)
    target_compile_definitions(ShaderViewer PRIVATE $<$<CONFIG:Debug>:SHADER_VIEWER_DEBUG>)
endif()

# Copy runtime assets next to executable
add_custom_command(TARGET ShaderViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:ShaderViewer>/assets"
    COMMENT "Copying assets to output directory"
)

# Copy shaders next to executable
add_custom_command(TARGET ShaderViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/shaders"
        "$<TARGET_FILE_DIR:ShaderViewer>/shaders"
    COMMENT "Copying shaders to output directory"
)

add_custom_command(TARGET ShaderViewer POST_BUILD
    COMMAND powershell -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tools/update_desktop_shortcut.ps1"
        -TargetExe "$<TARGET_FILE:ShaderViewer>"
        -WorkingDirectory "$<TARGET_FILE_DIR:ShaderViewer>"
        -IconPath "${CMAKE_SOURCE_DIR}/resources/app.ico"
    COMMENT "Updating desktop shortcut"
)

add_custom_command(
    OUTPUT "${CMAKE_SOURCE_DIR}/resources/app.ico"
    COMMAND powershell -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/tools/generate_icon.ps1"
    DEPENDS "${CMAKE_SOURCE_DIR}/assets/app_icon.png"
        "${CMAKE_SOURCE_DIR}/tools/generate_icon.ps1"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Generating application icon"
)
add_custom_target(ShaderViewerIcon DEPENDS "${CMAKE_SOURCE_DIR}/resources/app.ico")
add_dependencies(ShaderViewer ShaderViewerIcon)
