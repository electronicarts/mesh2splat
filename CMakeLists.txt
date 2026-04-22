cmake_minimum_required(VERSION 3.15)
project(Mesh2Splat)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options
option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" OFF)
option(MESH2SPLAT_ENABLE_GPU "Enable GPU backend for Python bindings (requires OpenGL)" ON)
option(MESH2SPLAT_STANDALONE_PYTHON "Build only Python bindings (for pip package)" OFF)

set(THIRD_PARTY_DIR         ${CMAKE_SOURCE_DIR}/thirdParty)
set(GLFW_DIR                ${THIRD_PARTY_DIR}/glfw)
set(GLEW_DIR                ${THIRD_PARTY_DIR}/glew)
set(GLM_DIR                 ${THIRD_PARTY_DIR}/glm)
set(IMGUI_DIR               ${THIRD_PARTY_DIR}/imgui)
set(IGFD_DIR                ${THIRD_PARTY_DIR}/ImGuiFileDialog)
set(IMGUIZMO_DIR            ${THIRD_PARTY_DIR}/imguizmo)
set(XATLAS_DIR              ${THIRD_PARTY_DIR}/xatlas)

# Standalone Python build mode (for pip package) - skip main executable
if(NOT MESH2SPLAT_STANDALONE_PYTHON)
    include_directories(${CMAKE_SOURCE_DIR}/src)
    include_directories(SYSTEM
        ${GLFW_DIR}/include
        ${GLEW_DIR}/include
        ${GLM_DIR}
        ${IMGUI_DIR}
        ${IGFD_DIR}
        ${IMGUIZMO_DIR}
        ${XATLAS_DIR}
        ${THIRD_PARTY_DIR}
    )


    file(GLOB_RECURSE SOURCES 
            ${CMAKE_SOURCE_DIR}/src/*.cpp
            ${CMAKE_SOURCE_DIR}/src/*.hpp
            ${CMAKE_SOURCE_DIR}/src/*.h
    )

    file(GLOB IMGUI_SOURCES
            ${IMGUI_DIR}/*.cpp
            ${IMGUI_DIR}/*.hpp
            ${IMGUI_DIR}/*.h
    )
    list(APPEND SOURCES ${IMGUI_SOURCES})

    file(GLOB IMGUIZMO_SOURCES
            ${IMGUIZMO_DIR}/*.cpp
            ${IMGUIZMO_DIR}/*.hpp
            ${IMGUIZMO_DIR}/*.h
    )
    list(APPEND SOURCES ${IMGUIZMO_SOURCES})

    file(GLOB IGFD_SOURCE
            ${IGFD_DIR}/*.cpp
            ${IGFD_DIR}/*.hpp
            ${IGFD_DIR}/*.h
    )
    list(APPEND SOURCES ${IGFD_SOURCE})

    file(GLOB XATLAS_SOURCE
            ${XATLAS_DIR}/*.cpp
            ${XATLAS_DIR}/*.hpp
            ${XATLAS_DIR}/*.h
    )
    list(APPEND SOURCES ${XATLAS_SOURCE})

    set(THIRD_PARTY_SOURCES ${IMGUI_SOURCES} ${IMGUIZMO_SOURCES} ${IGFD_SOURCE} ${XATLAS_SOURCE})
    if(MSVC)
        set_source_files_properties(${THIRD_PARTY_SOURCES} PROPERTIES COMPILE_FLAGS "/W0")
    else()
        set_source_files_properties(${THIRD_PARTY_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
    endif()

    add_executable(Mesh2Splat ${SOURCES})

    target_compile_definitions(Mesh2Splat PRIVATE GLEW_STATIC)

    if(MSVC)
        target_compile_options(Mesh2Splat PRIVATE
            /wd4244   # conversion, possible loss of data
            /wd4267   # size_t to smaller integer conversion
            /wd4018   # signed/unsigned mismatch
            /wd4101   # unreferenced local variable
        )
        target_link_options(Mesh2Splat PRIVATE
            /ignore:4098  # defaultlib runtime library conflict (glew32s.lib)
            /ignore:4099  # PDB not found for glew32s.lib
        )
    endif()

    if (WIN32)
        target_link_libraries(Mesh2Splat 
            ${GLFW_DIR}/lib-vc2022/glfw3.lib 
            ${GLEW_DIR}/lib/Release/x64/glew32s.lib 
            opengl32)
    else()
        target_link_libraries(Mesh2Splat glfw GLEW GL X11 Xrandr Xinerama Xcursor Xi Xxf86vm pthread dl) #todo
    endif()

    set_target_properties(Mesh2Splat PROPERTIES 
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/bin/Debug
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/bin/Release
        ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/lib/Debug
        ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/lib/Release
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/lib/Debug
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${CMAKE_SOURCE_DIR}/lib/Release
    )
endif()

# Python bindings
if(BUILD_PYTHON_BINDINGS OR MESH2SPLAT_STANDALONE_PYTHON)
    add_subdirectory(thirdParty/pybind11)
    add_subdirectory(python)
endif()
