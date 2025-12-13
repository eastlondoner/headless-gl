#ifndef WEBGL_CONTEXT_H_
#define WEBGL_CONTEXT_H_

// NAN-free WebGL context implementation for use with N-API bindings.
// This header has NO V8/NAN dependencies.

#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define EGL_EGL_PROTOTYPES 0
#define GL_GLES_PROTOTYPES 0

#include "SharedLibrary.h"
#include "angle-loader/egl_loader.h"
#include "angle-loader/gles_loader.h"

enum GLObjectType {
  GLOBJECT_TYPE_BUFFER,
  GLOBJECT_TYPE_FRAMEBUFFER,
  GLOBJECT_TYPE_PROGRAM,
  GLOBJECT_TYPE_RENDERBUFFER,
  GLOBJECT_TYPE_SHADER,
  GLOBJECT_TYPE_TEXTURE,
  GLOBJECT_TYPE_VERTEX_ARRAY,
};

enum GLContextState {
  GLCONTEXT_STATE_INIT,
  GLCONTEXT_STATE_OK,
  GLCONTEXT_STATE_DESTROY,
  GLCONTEXT_STATE_ERROR
};

bool CaseInsensitiveCompare(const std::string &a, const std::string &b);

using GLObjectReference = std::pair<GLuint, GLObjectType>;
using WebGLToANGLEExtensionsMap =
    std::map<std::string, std::vector<std::string>, decltype(&CaseInsensitiveCompare)>;

// Pure C++ WebGL context - no V8/NAN dependencies
class WebGLContextImpl {
public:
  // Static display shared across all contexts
  static bool HAS_DISPLAY;
  static EGLDisplay DISPLAY;
  static WebGLContextImpl *ACTIVE;
  static WebGLContextImpl *CONTEXT_LIST_HEAD;

  // Instance state
  SharedLibrary eglLibrary;
  EGLContext context;
  EGLConfig config;
  EGLSurface surface;
  GLContextState state;
  std::string errorMessage;

  // Dimensions
  int width;
  int height;

  // Pixel storage flags
  bool unpack_flip_y;
  bool unpack_premultiply_alpha;
  GLint unpack_colorspace_conversion;
  GLint unpack_alignment;

  // Extensions
  std::set<std::string> requestableExtensions;
  std::set<std::string> enabledExtensions;
  std::set<std::string> supportedWebGLExtensions;
  WebGLToANGLEExtensionsMap webGLToANGLEExtensions;

  // Object tracking
  std::map<std::pair<GLuint, GLObjectType>, bool> objects;
  void registerGLObj(GLObjectType type, GLuint obj) { objects[std::make_pair(obj, type)] = true; }
  void unregisterGLObj(GLObjectType type, GLuint obj) { objects.erase(std::make_pair(obj, type)); }

  // Context list management
  WebGLContextImpl *next, *prev;
  void registerContext();
  void unregisterContext();

  // Preferred depth format
  GLenum preferredDepth;

  // Error handling
  std::set<GLenum> errorSet;
  void setError(GLenum error);
  GLenum getError();

  // Constructor / Destructor
  WebGLContextImpl(int width, int height, bool alpha, bool depth, bool stencil, bool antialias,
                   bool premultipliedAlpha, bool preserveDrawingBuffer,
                   bool preferLowPowerToHighPerformance, bool failIfMajorPerformanceCaveat,
                   bool createWebGL2Context);
  ~WebGLContextImpl();

  // Context management
  bool setActive();
  void dispose();

  // Resize
  void resize(int newWidth, int newHeight);

  // Unpacks a buffer full of pixels into memory
  std::vector<uint8_t> unpackPixels(GLenum type, GLenum format, GLint width, GLint height,
                                    unsigned char *pixels);
};

#endif // WEBGL_CONTEXT_H_
