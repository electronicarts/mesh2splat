# Unit tests for the parts of Mesh2Splat that are pure CPU-side math.
#
# These link against nothing but the vendored, header-only glm, so they build
# and run on a machine with no GPU, no window server and no network access.

add_executable(Mesh2SplatTests coordinateSystemTests.cpp)

target_include_directories(Mesh2SplatTests PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)
target_include_directories(Mesh2SplatTests SYSTEM PRIVATE
    ${GLM_DIR}
)

if(MSVC)
    target_compile_options(Mesh2SplatTests PRIVATE /W4)
else()
    target_compile_options(Mesh2SplatTests PRIVATE -Wall -Wextra)
endif()

add_test(NAME coordinateSystem COMMAND Mesh2SplatTests)
