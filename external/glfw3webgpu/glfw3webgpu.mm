/**
 * This is an extension of GLFW for WebGPU, abstracting away the details of
 * OS-specific operations.
 * 
 * This file is part of the "Learn WebGPU for C++" book.
 *   https://eliemichel.github.io/LearnWebGPU
 * 
 * Most of this code comes from the wgpu-native triangle example:
 *   https://github.com/gfx-rs/wgpu-native/blob/master/examples/triangle/main.c
 * 
 * MIT License
 * Copyright (c) 2022-2025 Elie Michel and the wgpu-native authors
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "glfw3webgpu.h"

#include <cstdio>

#ifdef __EMSCRIPTEN__
    #define GLFW_EXPOSE_NATIVE_EMSCRIPTEN
    #ifndef GLFW_PLATFORM_EMSCRIPTEN  // not defined in older versions of emscripten
        #define GLFW_PLATFORM_EMSCRIPTEN 0
    #endif
#else  // __EMSCRIPTEN__
    #ifdef _GLFW_X11
        #define GLFW_EXPOSE_NATIVE_X11
    #endif
    #ifdef _GLFW_WAYLAND
        #define GLFW_EXPOSE_NATIVE_WAYLAND
    #endif
    #ifdef _GLFW_COCOA
        #define GLFW_EXPOSE_NATIVE_COCOA
    #endif
    #ifdef _GLFW_WIN32
        #define GLFW_EXPOSE_NATIVE_WIN32
    #endif
#endif  // __EMSCRIPTEN__

#ifdef GLFW_EXPOSE_NATIVE_COCOA
    #include <Foundation/Foundation.h>
    #include <QuartzCore/CAMetalLayer.h>
#endif

#ifndef __EMSCRIPTEN__
    #include <GLFW/glfw3native.h>
#endif

wgpu::Surface glfwCreateWindowWGPUSurface(wgpu::Instance instance, GLFWwindow* window)
{

#ifdef GLFW_EXPOSE_NATIVE_X11
        {
            Display* x11_display = glfwGetX11Display();
            Window x11_window    = glfwGetX11Window(window);

            wgpu::SurfaceSourceXlibWindow fromXlibWindow;
            fromXlibWindow.nextInChain = nullptr;
            fromXlibWindow.sType       = wgpu::SType::SurfaceSourceXlibWindow;
            fromXlibWindow.display     = x11_display;
            fromXlibWindow.window      = x11_window;

            wgpu::SurfaceDescriptor surfaceDescriptor;
            surfaceDescriptor.nextInChain = &fromXlibWindow;
            surfaceDescriptor.label       = (WGPUStringView) {NULL, WGPU_STRLEN};

            return instance.CreateSurface(&surfaceDescriptor);
        }
#endif  // GLFW_EXPOSE_NATIVE_X11

#ifdef GLFW_EXPOSE_NATIVE_WAYLAND
        {
            struct wl_display* wayland_display = glfwGetWaylandDisplay();
            struct wl_surface* wayland_surface = glfwGetWaylandWindow(window);

            wgpu::SurfaceSourceWaylandSurface fromWaylandSurface;
            fromWaylandSurface.nextInChain = nullptr;
            fromWaylandSurface.sType       = wgpu::SType::SurfaceSourceWaylandSurface;
            fromWaylandSurface.display     = wayland_display;
            fromWaylandSurface.surface     = wayland_surface;

            wgpu::SurfaceDescriptor surfaceDescriptor;
            surfaceDescriptor.nextInChain = &fromWaylandSurface;
            surfaceDescriptor.label       = (WGPUStringView) {NULL, WGPU_STRLEN};

            return instance.CreateSurface(&surfaceDescriptor);
        }
#endif  // GLFW_EXPOSE_NATIVE_WAYLAND

#ifdef GLFW_EXPOSE_NATIVE_COCOA
        {
            id metal_layer      = [CAMetalLayer layer];
            NSWindow* ns_window = glfwGetCocoaWindow(window);
            [ns_window.contentView setWantsLayer:YES];
            [ns_window.contentView setLayer:metal_layer];

            wgpu::SurfaceSourceMetalLayer fromMetalLayer;
            fromMetalLayer.nextInChain = nullptr;
            fromMetalLayer.sType = wgpu::SType::SurfaceSourceMetalLayer;
            fromMetalLayer.layer      = metal_layer;

            wgpu::SurfaceDescriptor surfaceDescriptor;
            surfaceDescriptor.nextInChain = &fromMetalLayer;
            surfaceDescriptor.label       = (WGPUStringView) {NULL, WGPU_STRLEN};

            return instance.CreateSurface(&surfaceDescriptor);
        }
#endif  // GLFW_EXPOSE_NATIVE_COCOA

#ifdef GLFW_EXPOSE_NATIVE_WIN32
        {
            HWND hwnd           = glfwGetWin32Window(window);
            HINSTANCE hinstance = GetModuleHandle(NULL);

             wgpu::SurfaceSourceWindowsHWND fromWindowsHWND;
            fromWindowsHWND.nextInChain = nullptr;
            fromWindowsHWND.sType       = wgpu::SType::SurfaceSourceWindowsHWND;
            fromWindowsHWND.hinstance   = hinstance;
            fromWindowsHWND.hwnd        = hwnd;

            wgpu::SurfaceDescriptor surfaceDescriptor;
            surfaceDescriptor.nextInChain = &fromWindowsHWND;
            surfaceDescriptor.label       = (WGPUStringView) {NULL, WGPU_STRLEN};

            return instance.CreateSurface(&surfaceDescriptor);
        }
#endif  // GLFW_EXPOSE_NATIVE_WIN32

#ifdef GLFW_EXPOSE_NATIVE_EMSCRIPTEN
        {
            wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector fromCanvasHTMLSelector;
            fromCanvasHTMLSelector.selector   = (WGPUStringView) {"canvas", WGPU_STRLEN};

            wgpu::SurfaceDescriptor surfaceDescriptor;
            surfaceDescriptor.nextInChain = &fromCanvasHTMLSelector;
            surfaceDescriptor.label = (WGPUStringView) {NULL, WGPU_STRLEN};

            return instance.CreateSurface(&surfaceDescriptor);
        }
#endif  // GLFW_EXPOSE_NATIVE_EMSCRIPTEN

    // Unsupported platform
    printf("No Video driver found...!\n");
    return nullptr;
}
