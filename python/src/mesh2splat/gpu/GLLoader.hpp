///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - OpenGL Function Loader             //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////
//
// On Linux with EGL, OpenGL core profile functions must be loaded dynamically.
// macOS provides them directly via <OpenGL/gl3.h>.
//

#pragma once

#if defined(__APPLE__)
    // macOS: functions are directly available
    #include <OpenGL/gl3.h>
    
namespace mesh2splat {
namespace gl {
    inline bool loadGLFunctions() { return true; }
    inline bool areGLFunctionsLoaded() { return true; }
} // namespace gl
} // namespace mesh2splat

#elif defined(__linux__)

#include <EGL/egl.h>
#define GL_GLEXT_PROTOTYPES 0  // Don't use gl.h prototypes, we load our own
#include <GL/gl.h>

// OpenGL 2.0+ function pointer types
// Shader functions
typedef GLuint (*PFNGLCREATESHADERPROC_M2S)(GLenum type);
typedef void (*PFNGLSHADERSOURCEPROC_M2S)(GLuint shader, GLsizei count, const GLchar *const* string, const GLint *length);
typedef void (*PFNGLCOMPILESHADERPROC_M2S)(GLuint shader);
typedef void (*PFNGLGETSHADERIVPROC_M2S)(GLuint shader, GLenum pname, GLint *params);
typedef void (*PFNGLGETSHADERINFOLOGPROC_M2S)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLDELETESHADERPROC_M2S)(GLuint shader);

// Program functions
typedef GLuint (*PFNGLCREATEPROGRAMPROC_M2S)(void);
typedef void (*PFNGLATTACHSHADERPROC_M2S)(GLuint program, GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC_M2S)(GLuint program);
typedef void (*PFNGLGETPROGRAMIVPROC_M2S)(GLuint program, GLenum pname, GLint *params);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC_M2S)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLDELETEPROGRAMPROC_M2S)(GLuint program);
typedef void (*PFNGLUSEPROGRAMPROC_M2S)(GLuint program);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC_M2S)(GLuint program, const GLchar *name);

// Uniform functions
typedef void (*PFNGLUNIFORM1IPROC_M2S)(GLint location, GLint v0);
typedef void (*PFNGLUNIFORM1FPROC_M2S)(GLint location, GLfloat v0);
typedef void (*PFNGLUNIFORM2FPROC_M2S)(GLint location, GLfloat v0, GLfloat v1);
typedef void (*PFNGLUNIFORM3FPROC_M2S)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (*PFNGLUNIFORM4FPROC_M2S)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

// VAO functions
typedef void (*PFNGLGENVERTEXARRAYSPROC_M2S)(GLsizei n, GLuint *arrays);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC_M2S)(GLsizei n, const GLuint *arrays);
typedef void (*PFNGLBINDVERTEXARRAYPROC_M2S)(GLuint array);

// VBO functions
typedef void (*PFNGLGENBUFFERSPROC_M2S)(GLsizei n, GLuint *buffers);
typedef void (*PFNGLDELETEBUFFERSPROC_M2S)(GLsizei n, const GLuint *buffers);
typedef void (*PFNGLBINDBUFFERPROC_M2S)(GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC_M2S)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC_M2S)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC_M2S)(GLuint index);

// Framebuffer functions
typedef void (*PFNGLGENFRAMEBUFFERSPROC_M2S)(GLsizei n, GLuint *framebuffers);
typedef void (*PFNGLDELETEFRAMEBUFFERSPROC_M2S)(GLsizei n, const GLuint *framebuffers);
typedef void (*PFNGLBINDFRAMEBUFFERPROC_M2S)(GLenum target, GLuint framebuffer);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DPROC_M2S)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (*PFNGLCHECKFRAMEBUFFERSTATUSPROC_M2S)(GLenum target);
typedef void (*PFNGLDRAWBUFFERSPROC_M2S)(GLsizei n, const GLenum *bufs);
typedef void (*PFNGLCLEARBUFFERFVPROC_M2S)(GLenum buffer, GLint drawbuffer, const GLfloat *value);

// Active texture
typedef void (*PFNGLACTIVETEXTUREPROC_M2S)(GLenum texture);

// Generate mipmaps
typedef void (*PFNGLGENERATEMIPMAPPROC_M2S)(GLenum target);

namespace mesh2splat {
namespace gl {

// Function pointers - declared extern, defined in GLLoader.cpp
extern PFNGLCREATESHADERPROC_M2S glCreateShader;
extern PFNGLSHADERSOURCEPROC_M2S glShaderSource;
extern PFNGLCOMPILESHADERPROC_M2S glCompileShader;
extern PFNGLGETSHADERIVPROC_M2S glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC_M2S glGetShaderInfoLog;
extern PFNGLDELETESHADERPROC_M2S glDeleteShader;

extern PFNGLCREATEPROGRAMPROC_M2S glCreateProgram;
extern PFNGLATTACHSHADERPROC_M2S glAttachShader;
extern PFNGLLINKPROGRAMPROC_M2S glLinkProgram;
extern PFNGLGETPROGRAMIVPROC_M2S glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC_M2S glGetProgramInfoLog;
extern PFNGLDELETEPROGRAMPROC_M2S glDeleteProgram;
extern PFNGLUSEPROGRAMPROC_M2S glUseProgram;
extern PFNGLGETUNIFORMLOCATIONPROC_M2S glGetUniformLocation;

extern PFNGLUNIFORM1IPROC_M2S glUniform1i;
extern PFNGLUNIFORM1FPROC_M2S glUniform1f;
extern PFNGLUNIFORM2FPROC_M2S glUniform2f;
extern PFNGLUNIFORM3FPROC_M2S glUniform3f;
extern PFNGLUNIFORM4FPROC_M2S glUniform4f;

extern PFNGLGENVERTEXARRAYSPROC_M2S glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC_M2S glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC_M2S glBindVertexArray;

extern PFNGLGENBUFFERSPROC_M2S glGenBuffers;
extern PFNGLDELETEBUFFERSPROC_M2S glDeleteBuffers;
extern PFNGLBINDBUFFERPROC_M2S glBindBuffer;
extern PFNGLBUFFERDATAPROC_M2S glBufferData;
extern PFNGLVERTEXATTRIBPOINTERPROC_M2S glVertexAttribPointer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC_M2S glEnableVertexAttribArray;

extern PFNGLGENFRAMEBUFFERSPROC_M2S glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC_M2S glDeleteFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC_M2S glBindFramebuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC_M2S glFramebufferTexture2D;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC_M2S glCheckFramebufferStatus;
extern PFNGLDRAWBUFFERSPROC_M2S glDrawBuffers;
extern PFNGLCLEARBUFFERFVPROC_M2S glClearBufferfv;

extern PFNGLACTIVETEXTUREPROC_M2S glActiveTexture;
extern PFNGLGENERATEMIPMAPPROC_M2S glGenerateMipmap;

// Initialize all GL function pointers. Call after making GL context current.
// Returns true if all required functions were loaded successfully.
bool loadGLFunctions();

// Check if GL functions have been loaded
bool areGLFunctionsLoaded();

} // namespace gl
} // namespace mesh2splat

// Pull into global namespace - use macros to avoid conflicts with GL/gl.h
#define glCreateShader mesh2splat::gl::glCreateShader
#define glShaderSource mesh2splat::gl::glShaderSource
#define glCompileShader mesh2splat::gl::glCompileShader
#define glGetShaderiv mesh2splat::gl::glGetShaderiv
#define glGetShaderInfoLog mesh2splat::gl::glGetShaderInfoLog
#define glDeleteShader mesh2splat::gl::glDeleteShader

#define glCreateProgram mesh2splat::gl::glCreateProgram
#define glAttachShader mesh2splat::gl::glAttachShader
#define glLinkProgram mesh2splat::gl::glLinkProgram
#define glGetProgramiv mesh2splat::gl::glGetProgramiv
#define glGetProgramInfoLog mesh2splat::gl::glGetProgramInfoLog
#define glDeleteProgram mesh2splat::gl::glDeleteProgram
#define glUseProgram mesh2splat::gl::glUseProgram
#define glGetUniformLocation mesh2splat::gl::glGetUniformLocation

#define glUniform1i mesh2splat::gl::glUniform1i
#define glUniform1f mesh2splat::gl::glUniform1f
#define glUniform2f mesh2splat::gl::glUniform2f
#define glUniform3f mesh2splat::gl::glUniform3f
#define glUniform4f mesh2splat::gl::glUniform4f

#define glGenVertexArrays mesh2splat::gl::glGenVertexArrays
#define glDeleteVertexArrays mesh2splat::gl::glDeleteVertexArrays
#define glBindVertexArray mesh2splat::gl::glBindVertexArray

#define glGenBuffers mesh2splat::gl::glGenBuffers
#define glDeleteBuffers mesh2splat::gl::glDeleteBuffers
#define glBindBuffer mesh2splat::gl::glBindBuffer
#define glBufferData mesh2splat::gl::glBufferData
#define glVertexAttribPointer mesh2splat::gl::glVertexAttribPointer
#define glEnableVertexAttribArray mesh2splat::gl::glEnableVertexAttribArray

#define glGenFramebuffers mesh2splat::gl::glGenFramebuffers
#define glDeleteFramebuffers mesh2splat::gl::glDeleteFramebuffers
#define glBindFramebuffer mesh2splat::gl::glBindFramebuffer
#define glFramebufferTexture2D mesh2splat::gl::glFramebufferTexture2D
#define glCheckFramebufferStatus mesh2splat::gl::glCheckFramebufferStatus
#define glDrawBuffers mesh2splat::gl::glDrawBuffers
#define glClearBufferfv mesh2splat::gl::glClearBufferfv

#define glActiveTexture mesh2splat::gl::glActiveTexture
#define glGenerateMipmap mesh2splat::gl::glGenerateMipmap

#endif // __linux__
