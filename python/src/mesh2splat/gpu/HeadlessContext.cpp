///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Headless OpenGL Context Impl        //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#include "HeadlessContext.hpp"
#include <cstring>

// Platform detection
#if defined(__APPLE__)
    #define M2S_PLATFORM_MACOS 1
#elif defined(__linux__)
    #define M2S_PLATFORM_LINUX 1
#else
    #define M2S_PLATFORM_UNSUPPORTED 1
#endif

//------------------------------------------------------------------------------
// macOS CGL Implementation
//------------------------------------------------------------------------------
#if M2S_PLATFORM_MACOS

#include <OpenGL/OpenGL.h>
#include <OpenGL/gl3.h>

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    CGLContextObj context_ = nullptr;
    CGLPixelFormatObj pixelFormat_ = nullptr;
    std::string errorMessage_;
    int majorVersion_ = 4;
    int minorVersion_ = 1;
    
    Impl(int majorVersion, int minorVersion)
        : majorVersion_(majorVersion), minorVersion_(minorVersion) {
        
        // macOS only supports up to OpenGL 4.1 (Legacy profile)
        // Request OpenGL 3.2+ Core Profile which gives us 4.1 on modern Macs
        CGLPixelFormatAttribute attributes[] = {
            kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
            kCGLPFAColorSize, (CGLPixelFormatAttribute)24,
            kCGLPFADepthSize, (CGLPixelFormatAttribute)24,
            kCGLPFAAccelerated,
            kCGLPFAAllowOfflineRenderers, // Allow headless GPU
            (CGLPixelFormatAttribute)0
        };
        
        GLint numPixelFormats = 0;
        CGLError error = CGLChoosePixelFormat(attributes, &pixelFormat_, &numPixelFormats);
        
        if (error != kCGLNoError || numPixelFormats == 0) {
            errorMessage_ = "Failed to find suitable pixel format: ";
            errorMessage_ += CGLErrorString(error);
            return;
        }
        
        error = CGLCreateContext(pixelFormat_, nullptr, &context_);
        if (error != kCGLNoError) {
            errorMessage_ = "Failed to create CGL context: ";
            errorMessage_ += CGLErrorString(error);
            CGLDestroyPixelFormat(pixelFormat_);
            pixelFormat_ = nullptr;
            return;
        }
    }
    
    ~Impl() {
        if (context_) {
            CGLSetCurrentContext(nullptr);
            CGLDestroyContext(context_);
        }
        if (pixelFormat_) {
            CGLDestroyPixelFormat(pixelFormat_);
        }
    }
    
    bool makeCurrent() {
        if (!context_) return false;
        CGLError error = CGLSetCurrentContext(context_);
        return error == kCGLNoError;
    }
    
    void release() {
        CGLSetCurrentContext(nullptr);
    }
    
    bool isValid() const {
        return context_ != nullptr;
    }
    
    std::string getGLVersion() const {
        if (!context_) return "N/A";
        CGLSetCurrentContext(context_);
        const char* version = (const char*)glGetString(GL_VERSION);
        return version ? version : "Unknown";
    }
    
    std::string getGLRenderer() const {
        if (!context_) return "N/A";
        CGLSetCurrentContext(context_);
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        return renderer ? renderer : "Unknown";
    }
};

bool HeadlessContext::isAvailable() {
    HeadlessContext test(4, 1);
    return test.isValid();
}

} // namespace mesh2splat

//------------------------------------------------------------------------------
// Linux EGL Implementation
//------------------------------------------------------------------------------
#elif M2S_PLATFORM_LINUX

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include "GLLoader.hpp"

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLConfig config_ = nullptr;
    std::string errorMessage_;
    int majorVersion_ = 4;
    int minorVersion_ = 1;
    bool glFunctionsLoaded_ = false;
    
    Impl(int majorVersion, int minorVersion)
        : majorVersion_(majorVersion), minorVersion_(minorVersion) {
        
        // Try to get EGL display
        // First try the default display (works with Mesa)
        display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        
        if (display_ == EGL_NO_DISPLAY) {
            // Try platform-specific device enumeration
            PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT = 
                (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
            PFNEGLGETPLATFORMDISPLAYEXTPROC eglGetPlatformDisplayEXT = 
                (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
            
            if (eglQueryDevicesEXT && eglGetPlatformDisplayEXT) {
                EGLDeviceEXT devices[8];
                EGLint numDevices = 0;
                
                if (eglQueryDevicesEXT(8, devices, &numDevices) && numDevices > 0) {
                    // Use first available device
                    display_ = eglGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, devices[0], nullptr);
                }
            }
        }
        
        if (display_ == EGL_NO_DISPLAY) {
            errorMessage_ = "Failed to get EGL display";
            return;
        }
        
        EGLint major, minor;
        if (!eglInitialize(display_, &major, &minor)) {
            errorMessage_ = "Failed to initialize EGL";
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Choose EGL config
        EGLint configAttribs[] = {
            EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
        };
        
        EGLint numConfigs;
        if (!eglChooseConfig(display_, configAttribs, &config_, 1, &numConfigs) || numConfigs == 0) {
            errorMessage_ = "Failed to choose EGL config";
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Bind OpenGL API (not OpenGL ES)
        if (!eglBindAPI(EGL_OPENGL_API)) {
            errorMessage_ = "Failed to bind OpenGL API";
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Create context with requested version
        EGLint contextAttribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, majorVersion_,
            EGL_CONTEXT_MINOR_VERSION, minorVersion_,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE
        };
        
        context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, contextAttribs);
        if (context_ == EGL_NO_CONTEXT) {
            errorMessage_ = "Failed to create EGL context";
            eglTerminate(display_);
            display_ = EGL_NO_DISPLAY;
            return;
        }
        
        // Create a 1x1 pbuffer surface (required for some operations)
        EGLint pbufferAttribs[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };
        
        surface_ = eglCreatePbufferSurface(display_, config_, pbufferAttribs);
        // Surface is optional for compute, so don't fail if it's not created
    }
    
    ~Impl() {
        if (display_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            
            if (context_ != EGL_NO_CONTEXT) {
                eglDestroyContext(display_, context_);
            }
            if (surface_ != EGL_NO_SURFACE) {
                eglDestroySurface(display_, surface_);
            }
            eglTerminate(display_);
        }
    }
    
    bool makeCurrent() {
        if (display_ == EGL_NO_DISPLAY || context_ == EGL_NO_CONTEXT) {
            return false;
        }
        // Try with surface first, then without
        bool success = false;
        if (surface_ != EGL_NO_SURFACE) {
            success = eglMakeCurrent(display_, surface_, surface_, context_) == EGL_TRUE;
        } else {
            success = eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, context_) == EGL_TRUE;
        }
        
        // Load GL functions on first successful makeCurrent
        if (success && !glFunctionsLoaded_) {
            if (!gl::loadGLFunctions()) {
                errorMessage_ = "Failed to load OpenGL functions";
                return false;
            }
            glFunctionsLoaded_ = true;
        }
        
        return success;
    }
    
    void release() {
        if (display_ != EGL_NO_DISPLAY) {
            eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
    }
    
    bool isValid() const {
        return display_ != EGL_NO_DISPLAY && context_ != EGL_NO_CONTEXT;
    }
    
    std::string getGLVersion() const {
        if (!isValid()) return "N/A";
        // Context must be current to call glGetString
        // Note: This temporarily makes context current; caller may need to restore
        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            return "Unknown (context switch failed)";
        }
        const char* version = (const char*)glGetString(GL_VERSION);
        return version ? version : "Unknown";
    }
    
    std::string getGLRenderer() const {
        if (!isValid()) return "N/A";
        // Context must be current to call glGetString
        if (eglMakeCurrent(display_, surface_, surface_, context_) != EGL_TRUE) {
            return "Unknown (context switch failed)";
        }
        const char* renderer = (const char*)glGetString(GL_RENDERER);
        return renderer ? renderer : "Unknown";
    }
};

bool HeadlessContext::isAvailable() {
    HeadlessContext test(4, 1);
    return test.isValid();
}

} // namespace mesh2splat

//------------------------------------------------------------------------------
// Unsupported Platform
//------------------------------------------------------------------------------
#else

namespace mesh2splat {

class HeadlessContext::Impl {
public:
    std::string errorMessage_ = "Headless GPU not supported on this platform";
    
    Impl(int, int) {}
    bool makeCurrent() { return false; }
    void release() {}
    bool isValid() const { return false; }
    std::string getGLVersion() const { return "N/A"; }
    std::string getGLRenderer() const { return "N/A"; }
};

bool HeadlessContext::isAvailable() {
    return false;
}

} // namespace mesh2splat

#endif

//------------------------------------------------------------------------------
// Common Implementation
//------------------------------------------------------------------------------

namespace mesh2splat {

HeadlessContext::HeadlessContext(int majorVersion, int minorVersion)
    : impl_(std::make_unique<Impl>(majorVersion, minorVersion)) {
}

HeadlessContext::~HeadlessContext() = default;

HeadlessContext::HeadlessContext(HeadlessContext&& other) noexcept = default;
HeadlessContext& HeadlessContext::operator=(HeadlessContext&& other) noexcept = default;

bool HeadlessContext::makeCurrent() {
    return impl_->makeCurrent();
}

void HeadlessContext::release() {
    impl_->release();
}

bool HeadlessContext::isValid() const {
    return impl_->isValid();
}

std::string HeadlessContext::getGLVersion() const {
    return impl_->getGLVersion();
}

std::string HeadlessContext::getGLRenderer() const {
    return impl_->getGLRenderer();
}

std::string HeadlessContext::getErrorMessage() const {
    return impl_->errorMessage_;
}

} // namespace mesh2splat
