///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - Headless OpenGL Context Header      //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>

namespace mesh2splat {

/// Platform-independent headless OpenGL context
/// Uses EGL on Linux and CGL on macOS
class HeadlessContext {
public:
    /// Constructor - creates a headless OpenGL context
    /// @param majorVersion OpenGL major version (default 4)
    /// @param minorVersion OpenGL minor version (default 1 for macOS compatibility)
    HeadlessContext(int majorVersion = 4, int minorVersion = 1);
    
    /// Destructor - destroys the context
    ~HeadlessContext();
    
    /// Make this context current on the calling thread
    /// @return true if successful
    bool makeCurrent();
    
    /// Release the context from the calling thread
    void release();
    
    /// Check if context is valid and usable
    bool isValid() const;
    
    /// Get OpenGL version string
    std::string getGLVersion() const;
    
    /// Get OpenGL renderer string
    std::string getGLRenderer() const;
    
    /// Get any error message from context creation
    std::string getErrorMessage() const;
    
    /// Check if headless GPU is available on this system
    /// This attempts to create a temporary context to verify
    static bool isAvailable();

    // Non-copyable
    HeadlessContext(const HeadlessContext&) = delete;
    HeadlessContext& operator=(const HeadlessContext&) = delete;
    
    // Movable
    HeadlessContext(HeadlessContext&& other) noexcept;
    HeadlessContext& operator=(HeadlessContext&& other) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

/// RAII helper to make context current and release on scope exit
class ContextGuard {
public:
    explicit ContextGuard(HeadlessContext& ctx) : ctx_(ctx), owned_(ctx.makeCurrent()) {}
    ~ContextGuard() { if (owned_) ctx_.release(); }
    
    bool isValid() const { return owned_; }
    
    // Non-copyable, non-movable
    ContextGuard(const ContextGuard&) = delete;
    ContextGuard& operator=(const ContextGuard&) = delete;
    
private:
    HeadlessContext& ctx_;
    bool owned_;
};

} // namespace mesh2splat
