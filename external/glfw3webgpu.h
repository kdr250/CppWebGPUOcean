#pragma once

#include <webgpu/webgpu_cpp.h>

#define _GLFW_USE_CONFIG_H
#include <GLFW/glfw3.h>

/*! @brief Creates a WebGPU surface for the specified window.
 *
 *  This function creates a WGPUSurface object for the specified window.
 *
 *  If the surface cannot be created, this function returns `NULL`.
 *
 *  It is the responsibility of the caller to destroy the window surface. The
 *  window surface must be destroyed using `wgpuSurfaceRelease`.
 *
 *  @param[in] instance The WebGPU instance to create the surface in.
 *  @param[in] window The window to create the surface for.
 *  @return The handle of the surface.  This is set to `NULL` if an error
 *  occurred.
 *
 *  @ingroup webgpu
 */
wgpu::Surface glfwCreateWindowWGPUSurface(wgpu::Instance instance, GLFWwindow* window);
