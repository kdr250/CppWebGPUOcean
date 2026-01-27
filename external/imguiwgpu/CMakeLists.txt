add_library(imguiwgpu STATIC imgui_impl_wgpu.cpp)
target_include_directories(imguiwgpu PUBLIC .)

if (NOT EMSCRIPTEN)
    add_compile_definitions(IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
endif()

target_link_libraries(imguiwgpu PUBLIC dawn::webgpu_dawn)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
