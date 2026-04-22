///////////////////////////////////////////////////////////////////////////////
//         Mesh2Splat: Python bindings - OpenGL Function Loader Impl        //
//        Copyright (c) 2025 Electronic Arts Inc. All rights reserved.       //
///////////////////////////////////////////////////////////////////////////////

// Don't include GLLoader.hpp here - it has macros that conflict with definitions
// Instead, define everything directly

#if defined(__linux__)

#include <EGL/egl.h>
#define GL_GLEXT_PROTOTYPES 0
#include <GL/gl.h>

// Function pointer type definitions (must match GLLoader.hpp)
typedef GLuint (*PFNGLCREATESHADERPROC_M2S)(GLenum type);
typedef void (*PFNGLSHADERSOURCEPROC_M2S)(GLuint shader, GLsizei count, const GLchar *const* string, const GLint *length);
typedef void (*PFNGLCOMPILESHADERPROC_M2S)(GLuint shader);
typedef void (*PFNGLGETSHADERIVPROC_M2S)(GLuint shader, GLenum pname, GLint *params);
typedef void (*PFNGLGETSHADERINFOLOGPROC_M2S)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLDELETESHADERPROC_M2S)(GLuint shader);

typedef GLuint (*PFNGLCREATEPROGRAMPROC_M2S)(void);
typedef void (*PFNGLATTACHSHADERPROC_M2S)(GLuint program, GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC_M2S)(GLuint program);
typedef void (*PFNGLGETPROGRAMIVPROC_M2S)(GLuint program, GLenum pname, GLint *params);
typedef void (*PFNGLGETPROGRAMINFOLOGPROC_M2S)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (*PFNGLDELETEPROGRAMPROC_M2S)(GLuint program);
typedef void (*PFNGLUSEPROGRAMPROC_M2S)(GLuint program);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC_M2S)(GLuint program, const GLchar *name);

typedef void (*PFNGLUNIFORM1IPROC_M2S)(GLint location, GLint v0);
typedef void (*PFNGLUNIFORM1FPROC_M2S)(GLint location, GLfloat v0);
typedef void (*PFNGLUNIFORM2FPROC_M2S)(GLint location, GLfloat v0, GLfloat v1);
typedef void (*PFNGLUNIFORM3FPROC_M2S)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (*PFNGLUNIFORM4FPROC_M2S)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);

typedef void (*PFNGLGENVERTEXARRAYSPROC_M2S)(GLsizei n, GLuint *arrays);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC_M2S)(GLsizei n, const GLuint *arrays);
typedef void (*PFNGLBINDVERTEXARRAYPROC_M2S)(GLuint array);

typedef void (*PFNGLGENBUFFERSPROC_M2S)(GLsizei n, GLuint *buffers);
typedef void (*PFNGLDELETEBUFFERSPROC_M2S)(GLsizei n, const GLuint *buffers);
typedef void (*PFNGLBINDBUFFERPROC_M2S)(GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC_M2S)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC_M2S)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC_M2S)(GLuint index);

typedef void (*PFNGLGENFRAMEBUFFERSPROC_M2S)(GLsizei n, GLuint *framebuffers);
typedef void (*PFNGLDELETEFRAMEBUFFERSPROC_M2S)(GLsizei n, const GLuint *framebuffers);
typedef void (*PFNGLBINDFRAMEBUFFERPROC_M2S)(GLenum target, GLuint framebuffer);
typedef void (*PFNGLFRAMEBUFFERTEXTURE2DPROC_M2S)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (*PFNGLCHECKFRAMEBUFFERSTATUSPROC_M2S)(GLenum target);
typedef void (*PFNGLDRAWBUFFERSPROC_M2S)(GLsizei n, const GLenum *bufs);
typedef void (*PFNGLCLEARBUFFERFVPROC_M2S)(GLenum buffer, GLint drawbuffer, const GLfloat *value);

typedef void (*PFNGLACTIVETEXTUREPROC_M2S)(GLenum texture);
typedef void (*PFNGLGENERATEMIPMAPPROC_M2S)(GLenum target);

namespace mesh2splat {
namespace gl {

// Function pointer definitions
PFNGLCREATESHADERPROC_M2S glCreateShader = nullptr;
PFNGLSHADERSOURCEPROC_M2S glShaderSource = nullptr;
PFNGLCOMPILESHADERPROC_M2S glCompileShader = nullptr;
PFNGLGETSHADERIVPROC_M2S glGetShaderiv = nullptr;
PFNGLGETSHADERINFOLOGPROC_M2S glGetShaderInfoLog = nullptr;
PFNGLDELETESHADERPROC_M2S glDeleteShader = nullptr;

PFNGLCREATEPROGRAMPROC_M2S glCreateProgram = nullptr;
PFNGLATTACHSHADERPROC_M2S glAttachShader = nullptr;
PFNGLLINKPROGRAMPROC_M2S glLinkProgram = nullptr;
PFNGLGETPROGRAMIVPROC_M2S glGetProgramiv = nullptr;
PFNGLGETPROGRAMINFOLOGPROC_M2S glGetProgramInfoLog = nullptr;
PFNGLDELETEPROGRAMPROC_M2S glDeleteProgram = nullptr;
PFNGLUSEPROGRAMPROC_M2S glUseProgram = nullptr;
PFNGLGETUNIFORMLOCATIONPROC_M2S glGetUniformLocation = nullptr;

PFNGLUNIFORM1IPROC_M2S glUniform1i = nullptr;
PFNGLUNIFORM1FPROC_M2S glUniform1f = nullptr;
PFNGLUNIFORM2FPROC_M2S glUniform2f = nullptr;
PFNGLUNIFORM3FPROC_M2S glUniform3f = nullptr;
PFNGLUNIFORM4FPROC_M2S glUniform4f = nullptr;

PFNGLGENVERTEXARRAYSPROC_M2S glGenVertexArrays = nullptr;
PFNGLDELETEVERTEXARRAYSPROC_M2S glDeleteVertexArrays = nullptr;
PFNGLBINDVERTEXARRAYPROC_M2S glBindVertexArray = nullptr;

PFNGLGENBUFFERSPROC_M2S glGenBuffers = nullptr;
PFNGLDELETEBUFFERSPROC_M2S glDeleteBuffers = nullptr;
PFNGLBINDBUFFERPROC_M2S glBindBuffer = nullptr;
PFNGLBUFFERDATAPROC_M2S glBufferData = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC_M2S glVertexAttribPointer = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC_M2S glEnableVertexAttribArray = nullptr;

PFNGLGENFRAMEBUFFERSPROC_M2S glGenFramebuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC_M2S glDeleteFramebuffers = nullptr;
PFNGLBINDFRAMEBUFFERPROC_M2S glBindFramebuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC_M2S glFramebufferTexture2D = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC_M2S glCheckFramebufferStatus = nullptr;
PFNGLDRAWBUFFERSPROC_M2S glDrawBuffers = nullptr;
PFNGLCLEARBUFFERFVPROC_M2S glClearBufferfv = nullptr;

PFNGLACTIVETEXTUREPROC_M2S glActiveTexture = nullptr;
PFNGLGENERATEMIPMAPPROC_M2S glGenerateMipmap = nullptr;

static bool s_loaded = false;

bool loadGLFunctions() {
    if (s_loaded) return true;
    
    #define LOAD_GL(name) \
        name = (decltype(name))eglGetProcAddress(#name); \
        if (!name) return false
    
    LOAD_GL(glCreateShader);
    LOAD_GL(glShaderSource);
    LOAD_GL(glCompileShader);
    LOAD_GL(glGetShaderiv);
    LOAD_GL(glGetShaderInfoLog);
    LOAD_GL(glDeleteShader);
    
    LOAD_GL(glCreateProgram);
    LOAD_GL(glAttachShader);
    LOAD_GL(glLinkProgram);
    LOAD_GL(glGetProgramiv);
    LOAD_GL(glGetProgramInfoLog);
    LOAD_GL(glDeleteProgram);
    LOAD_GL(glUseProgram);
    LOAD_GL(glGetUniformLocation);
    
    LOAD_GL(glUniform1i);
    LOAD_GL(glUniform1f);
    LOAD_GL(glUniform2f);
    LOAD_GL(glUniform3f);
    LOAD_GL(glUniform4f);
    
    LOAD_GL(glGenVertexArrays);
    LOAD_GL(glDeleteVertexArrays);
    LOAD_GL(glBindVertexArray);
    
    LOAD_GL(glGenBuffers);
    LOAD_GL(glDeleteBuffers);
    LOAD_GL(glBindBuffer);
    LOAD_GL(glBufferData);
    LOAD_GL(glVertexAttribPointer);
    LOAD_GL(glEnableVertexAttribArray);
    
    LOAD_GL(glGenFramebuffers);
    LOAD_GL(glDeleteFramebuffers);
    LOAD_GL(glBindFramebuffer);
    LOAD_GL(glFramebufferTexture2D);
    LOAD_GL(glCheckFramebufferStatus);
    LOAD_GL(glDrawBuffers);
    LOAD_GL(glClearBufferfv);
    
    LOAD_GL(glActiveTexture);
    LOAD_GL(glGenerateMipmap);
    
    #undef LOAD_GL
    
    s_loaded = true;
    return true;
}

bool areGLFunctionsLoaded() {
    return s_loaded;
}

} // namespace gl
} // namespace mesh2splat

#endif // __linux__
