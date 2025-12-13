// NAN-free WebGL context implementation
// This file has NO V8/NAN dependencies.

#include "webgl_context.h"
#include <iostream>
#include <sstream>

// Static members
bool WebGLContextImpl::HAS_DISPLAY = false;
EGLDisplay WebGLContextImpl::DISPLAY;
WebGLContextImpl *WebGLContextImpl::ACTIVE = nullptr;
WebGLContextImpl *WebGLContextImpl::CONTEXT_LIST_HEAD = nullptr;

static std::set<std::string> GetStringSetFromCString(const char *cstr) {
  std::set<std::string> result;
  if (!cstr) return result;
  std::istringstream iss(cstr);
  std::string word;
  while (iss >> word) {
    result.insert(word);
  }
  return result;
}

bool CaseInsensitiveCompare(const std::string &a, const std::string &b) {
  std::string aLower = a;
  std::string bLower = b;
  std::transform(aLower.begin(), aLower.end(), aLower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  std::transform(bLower.begin(), bLower.end(), bLower.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return aLower < bLower;
}

static bool ContextSupportsExtensions(WebGLContextImpl *inst,
                                      const std::vector<std::string> &extensions) {
  for (const std::string &extension : extensions) {
    if (inst->enabledExtensions.count(extension) == 0 &&
        inst->requestableExtensions.count(extension) == 0) {
      return false;
    }
  }
  return true;
}

WebGLContextImpl::WebGLContextImpl(int width, int height, bool alpha, bool depth,
                                   bool stencil, bool antialias, bool premultipliedAlpha,
                                   bool preserveDrawingBuffer,
                                   bool preferLowPowerToHighPerformance,
                                   bool failIfMajorPerformanceCaveat,
                                   bool createWebGL2Context)
    : state(GLCONTEXT_STATE_INIT), width(width), height(height),
      unpack_flip_y(false), unpack_premultiply_alpha(false),
      unpack_colorspace_conversion(0x9244), unpack_alignment(4),
      webGLToANGLEExtensions(&CaseInsensitiveCompare), next(nullptr), prev(nullptr) {

  if (!eglGetProcAddress) {
    if (!eglLibrary.open("libEGL")) {
      errorMessage = "Error opening ANGLE shared library.";
      state = GLCONTEXT_STATE_ERROR;
      return;
    }

    auto getProcAddress = eglLibrary.getFunction<PFNEGLGETPROCADDRESSPROC>("eglGetProcAddress");
    ::LoadEGL(getProcAddress);
  }

  // Get display
  if (!HAS_DISPLAY) {
    DISPLAY = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (DISPLAY == EGL_NO_DISPLAY) {
      errorMessage = "Error retrieving EGL default display.";
      state = GLCONTEXT_STATE_ERROR;
      return;
    }

    // Initialize EGL
    if (!eglInitialize(DISPLAY, NULL, NULL)) {
      errorMessage = "Error initializing EGL.";
      state = GLCONTEXT_STATE_ERROR;
      return;
    }

    // Save display
    HAS_DISPLAY = true;
  }

  // Set up configuration
  EGLint renderableTypeBit = createWebGL2Context ? EGL_OPENGL_ES3_BIT : EGL_OPENGL_ES2_BIT;
  EGLint attrib_list[] = {EGL_SURFACE_TYPE,
                          EGL_PBUFFER_BIT,
                          EGL_RED_SIZE,
                          8,
                          EGL_GREEN_SIZE,
                          8,
                          EGL_BLUE_SIZE,
                          8,
                          EGL_ALPHA_SIZE,
                          8,
                          EGL_DEPTH_SIZE,
                          24,
                          EGL_STENCIL_SIZE,
                          8,
                          EGL_RENDERABLE_TYPE,
                          renderableTypeBit,
                          EGL_NONE};
  EGLint num_config;
  if (!eglChooseConfig(DISPLAY, attrib_list, &config, 1, &num_config) || num_config != 1) {
    errorMessage = "Error choosing EGL config.";
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  // Create context
  EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION,
                             createWebGL2Context ? 3 : 2,
                             EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE,
                             EGL_TRUE,
                             EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE,
                             EGL_FALSE,
                             EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE,
                             EGL_TRUE,
                             EGL_NONE};
  context = eglCreateContext(DISPLAY, config, EGL_NO_CONTEXT, contextAttribs);
  if (context == EGL_NO_CONTEXT) {
    errorMessage = "Error creating EGL context.";
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  EGLint surfaceAttribs[] = {EGL_WIDTH, (EGLint)width, EGL_HEIGHT, (EGLint)height, EGL_NONE};
  surface = eglCreatePbufferSurface(DISPLAY, config, surfaceAttribs);
  if (surface == EGL_NO_SURFACE) {
    errorMessage = "Error creating EGL surface.";
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  // Set active
  if (!eglMakeCurrent(DISPLAY, surface, surface, context)) {
    errorMessage = "Error making context current.";
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  // Success
  state = GLCONTEXT_STATE_OK;
  registerContext();
  ACTIVE = this;

  LoadGLES(eglGetProcAddress);

  // Check extensions
  const char *extensionsString = (const char *)(glGetString(GL_EXTENSIONS));
  enabledExtensions = GetStringSetFromCString(extensionsString);

  const char *requestableExtensionsString =
      (const char *)glGetString(GL_REQUESTABLE_EXTENSIONS_ANGLE);
  requestableExtensions = GetStringSetFromCString(requestableExtensionsString);

  // Request necessary WebGL extensions.
  glRequestExtensionANGLE("GL_EXT_texture_storage");

  // Select best preferred depth
  preferredDepth = GL_DEPTH_COMPONENT16;
  if (extensionsString && strstr(extensionsString, "GL_OES_depth32")) {
    preferredDepth = GL_DEPTH_COMPONENT32_OES;
  } else if (extensionsString && strstr(extensionsString, "GL_OES_depth24")) {
    preferredDepth = GL_DEPTH_COMPONENT24_OES;
  }

  // Each WebGL extension maps to one or more required ANGLE extensions.
  webGLToANGLEExtensions.insert({"STACKGL_destroy_context", {}});
  webGLToANGLEExtensions.insert({"STACKGL_resize_drawingbuffer", {}});
  webGLToANGLEExtensions.insert(
      {"EXT_texture_filter_anisotropic", {"GL_EXT_texture_filter_anisotropic"}});
  webGLToANGLEExtensions.insert({"OES_texture_float_linear", {"GL_OES_texture_float_linear"}});
  if (createWebGL2Context) {
    webGLToANGLEExtensions.insert({"EXT_color_buffer_float", {"GL_EXT_color_buffer_float"}});
  } else {
    webGLToANGLEExtensions.insert({"ANGLE_instanced_arrays", {"GL_ANGLE_instanced_arrays"}});
    webGLToANGLEExtensions.insert({"OES_element_index_uint", {"GL_OES_element_index_uint"}});
    webGLToANGLEExtensions.insert({"EXT_blend_minmax", {"GL_EXT_blend_minmax"}});
    webGLToANGLEExtensions.insert({"OES_standard_derivatives", {"GL_OES_standard_derivatives"}});
    webGLToANGLEExtensions.insert({"OES_texture_float",
                                   {"GL_OES_texture_float", "GL_CHROMIUM_color_buffer_float_rgba",
                                    "GL_CHROMIUM_color_buffer_float_rgb"}});
    webGLToANGLEExtensions.insert({"WEBGL_draw_buffers", {"GL_EXT_draw_buffers"}});
    webGLToANGLEExtensions.insert({"OES_vertex_array_object", {"GL_OES_vertex_array_object"}});
    webGLToANGLEExtensions.insert({"EXT_shader_texture_lod", {"GL_EXT_shader_texture_lod"}});
  }

  for (const auto &iter : webGLToANGLEExtensions) {
    const std::string &webGLExtension = iter.first;
    const std::vector<std::string> &angleExtensions = iter.second;
    if (ContextSupportsExtensions(this, angleExtensions)) {
      supportedWebGLExtensions.insert(webGLExtension);
    }
  }
}

WebGLContextImpl::~WebGLContextImpl() {
  dispose();
}

void WebGLContextImpl::registerContext() {
  if (CONTEXT_LIST_HEAD) {
    CONTEXT_LIST_HEAD->prev = this;
  }
  next = CONTEXT_LIST_HEAD;
  prev = nullptr;
  CONTEXT_LIST_HEAD = this;
}

void WebGLContextImpl::unregisterContext() {
  if (next) {
    next->prev = this->prev;
  }
  if (prev) {
    prev->next = this->next;
  }
  if (CONTEXT_LIST_HEAD == this) {
    CONTEXT_LIST_HEAD = this->next;
  }
  next = prev = nullptr;
}

bool WebGLContextImpl::setActive() {
  if (state != GLCONTEXT_STATE_OK) {
    return false;
  }
  if (this == ACTIVE) {
    return true;
  }
  if (!eglMakeCurrent(DISPLAY, surface, surface, context)) {
    state = GLCONTEXT_STATE_ERROR;
    return false;
  }
  ACTIVE = this;
  return true;
}

void WebGLContextImpl::setError(GLenum error) {
  if (error == GL_NO_ERROR || errorSet.count(error) > 0) {
    return;
  }
  errorSet.insert(error);
}

GLenum WebGLContextImpl::getError() {
  GLenum error = GL_NO_ERROR;
  if (errorSet.empty()) {
    error = glGetError();
  } else {
    error = *errorSet.begin();
    errorSet.erase(errorSet.begin());
  }
  return error;
}

void WebGLContextImpl::dispose() {
  // Unregister context
  unregisterContext();

  if (!setActive()) {
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  // Update state
  state = GLCONTEXT_STATE_DESTROY;

  // Destroy all object references
  for (auto iter = objects.begin(); iter != objects.end(); ++iter) {
    GLuint obj = iter->first.first;

    switch (iter->first.second) {
    case GLOBJECT_TYPE_PROGRAM:
      glDeleteProgram(obj);
      break;
    case GLOBJECT_TYPE_BUFFER:
      glDeleteBuffers(1, &obj);
      break;
    case GLOBJECT_TYPE_FRAMEBUFFER:
      glDeleteFramebuffers(1, &obj);
      break;
    case GLOBJECT_TYPE_RENDERBUFFER:
      glDeleteRenderbuffers(1, &obj);
      break;
    case GLOBJECT_TYPE_SHADER:
      glDeleteShader(obj);
      break;
    case GLOBJECT_TYPE_TEXTURE:
      glDeleteTextures(1, &obj);
      break;
    case GLOBJECT_TYPE_VERTEX_ARRAY:
      glDeleteVertexArraysOES(1, &obj);
      break;
    default:
      break;
    }
  }

  // Deactivate context
  eglMakeCurrent(DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  ACTIVE = nullptr;

  // Destroy surface and context
  eglDestroyContext(DISPLAY, context);
}

void WebGLContextImpl::resize(int newWidth, int newHeight) {
  if (!setActive()) {
    return;
  }

  // Destroy old surface
  eglDestroySurface(DISPLAY, surface);

  // Create new surface with new dimensions
  EGLint surfaceAttribs[] = {EGL_WIDTH, (EGLint)newWidth, EGL_HEIGHT, (EGLint)newHeight, EGL_NONE};
  surface = eglCreatePbufferSurface(DISPLAY, config, surfaceAttribs);
  if (surface == EGL_NO_SURFACE) {
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  // Make current with new surface
  if (!eglMakeCurrent(DISPLAY, surface, surface, context)) {
    state = GLCONTEXT_STATE_ERROR;
    return;
  }

  width = newWidth;
  height = newHeight;
}

std::vector<uint8_t> WebGLContextImpl::unpackPixels(GLenum type, GLenum format, GLint width,
                                                     GLint height, unsigned char *pixels) {
  // Compute row size
  int channels = 4;
  switch (format) {
  case GL_ALPHA:
  case GL_LUMINANCE:
    channels = 1;
    break;
  case GL_LUMINANCE_ALPHA:
    channels = 2;
    break;
  case GL_RGB:
    channels = 3;
    break;
  case GL_RGBA:
    channels = 4;
    break;
  default:
    break;
  }

  int pixelSize = channels;
  if (type == GL_UNSIGNED_SHORT_5_6_5 || type == GL_UNSIGNED_SHORT_4_4_4_4 ||
      type == GL_UNSIGNED_SHORT_5_5_5_1) {
    pixelSize = 2;
  }

  int rowSize = pixelSize * width;
  int alignedRowSize = (rowSize + unpack_alignment - 1) & ~(unpack_alignment - 1);

  std::vector<uint8_t> result(alignedRowSize * height);

  for (int j = 0; j < height; ++j) {
    int srcRow = unpack_flip_y ? (height - 1 - j) : j;
    memcpy(result.data() + j * alignedRowSize, pixels + srcRow * rowSize, rowSize);
  }

  return result;
}
