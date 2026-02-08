add_library(glfw3webgpu STATIC glfw3webgpu.mm)
target_include_directories(glfw3webgpu PUBLIC .)

target_link_libraries(glfw3webgpu PUBLIC glfw dawn::webgpu_dawn)

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
