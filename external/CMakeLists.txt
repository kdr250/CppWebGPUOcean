add_library(glfw3webgpu STATIC glfw3webgpu.mm)
target_include_directories(glfw3webgpu PUBLIC .)

if (EMSCRIPTEN)
    set(USE_FLAGS "-s USE_GLFW=3")
    set(CMAKE_CXX_FLAGS ${USE_FLAGS})
    target_link_libraries(glfw3webgpu PUBLIC dawn::webgpu_dawn)
else()
    target_link_libraries(glfw3webgpu PUBLIC glfw dawn::webgpu_dawn)
endif()

if (APPLE)
    target_compile_definitions(glfw3webgpu PRIVATE _GLFW_COCOA)
    target_link_libraries(glfw3webgpu PRIVATE "-framework CoreVideo" "-framework IOKit" "-framework QuartzCore")

    if (IOS)
        target_link_libraries(glfw3webgpu PRIVATE "-framework UIKit")
    else()
        target_link_libraries(glfw3webgpu PRIVATE "-framework Cocoa")
    endif()
endif()

if (WIN32)
    target_compile_definitions(glfw3webgpu PRIVATE _GLFW_WIN32)
endif()

# TODO: Linux set _GLFW_X11 or _GLFW_WAYLAND
