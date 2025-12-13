// N-API bindings for WebGL using real ANGLE/EGL implementation
// This replaces the stub implementation with actual GL calls.

#include <node_api.h>
#include "webgl_context.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace {

// Helper to unwrap the WebGLContextImpl from 'this'
WebGLContextImpl* unwrap_ctx(napi_env env, napi_callback_info info, napi_value* out_this = nullptr) {
  napi_value this_arg;
  napi_get_cb_info(env, info, nullptr, nullptr, &this_arg, nullptr);
  if (out_this) {
    *out_this = this_arg;
  }

  void* data = nullptr;
  napi_unwrap(env, this_arg, &data);
  return static_cast<WebGLContextImpl*>(data);
}

// Macro for common boilerplate: unwrap context and ensure it's active
#define GL_NAPI_BOILERPLATE(ctx_var)                                    \
  WebGLContextImpl* ctx_var = unwrap_ctx(env, info);                    \
  if (!ctx_var || !ctx_var->setActive()) {                              \
    napi_throw_error(env, nullptr, "Invalid GL context");               \
    return get_undefined(env);                                          \
  }

// Helper functions for creating JS values
napi_value make_int32(napi_env env, int32_t v) {
  napi_value out;
  napi_create_int32(env, v, &out);
  return out;
}

napi_value make_uint32(napi_env env, uint32_t v) {
  napi_value out;
  napi_create_uint32(env, v, &out);
  return out;
}

napi_value make_double(napi_env env, double v) {
  napi_value out;
  napi_create_double(env, v, &out);
  return out;
}

napi_value make_bool(napi_env env, bool v) {
  napi_value out;
  napi_get_boolean(env, v, &out);
  return out;
}

napi_value make_string(napi_env env, const char* s) {
  napi_value out;
  napi_create_string_utf8(env, s ? s : "", NAPI_AUTO_LENGTH, &out);
  return out;
}

napi_value get_undefined(napi_env env) {
  napi_value out;
  napi_get_undefined(env, &out);
  return out;
}

napi_value get_null(napi_env env) {
  napi_value out;
  napi_get_null(env, &out);
  return out;
}

// Helper to get arguments
int32_t get_int32_arg(napi_env env, napi_value arg, int32_t default_val = 0) {
  int32_t val = default_val;
  napi_get_value_int32(env, arg, &val);
  return val;
}

uint32_t get_uint32_arg(napi_env env, napi_value arg, uint32_t default_val = 0) {
  uint32_t val = default_val;
  napi_get_value_uint32(env, arg, &val);
  return val;
}

double get_double_arg(napi_env env, napi_value arg, double default_val = 0.0) {
  double val = default_val;
  napi_get_value_double(env, arg, &val);
  return val;
}

bool get_bool_arg(napi_env env, napi_value arg, bool default_val = false) {
  bool val = default_val;
  napi_get_value_bool(env, arg, &val);
  return val;
}

std::string get_string_arg(napi_env env, napi_value arg) {
  size_t len = 0;
  napi_get_value_string_utf8(env, arg, nullptr, 0, &len);
  std::string str(len, '\0');
  napi_get_value_string_utf8(env, arg, &str[0], len + 1, &len);
  return str;
}

// Constructor
napi_value ctor(napi_env env, napi_callback_info info) {
  size_t argc = 11;
  napi_value argv[11];
  napi_value this_arg;
  napi_get_cb_info(env, info, &argc, argv, &this_arg, nullptr);

  int width = argc > 0 ? get_int32_arg(env, argv[0], 256) : 256;
  int height = argc > 1 ? get_int32_arg(env, argv[1], 256) : 256;
  bool alpha = argc > 2 ? get_bool_arg(env, argv[2], true) : true;
  bool depth = argc > 3 ? get_bool_arg(env, argv[3], true) : true;
  bool stencil = argc > 4 ? get_bool_arg(env, argv[4], false) : false;
  bool antialias = argc > 5 ? get_bool_arg(env, argv[5], true) : true;
  bool premultipliedAlpha = argc > 6 ? get_bool_arg(env, argv[6], true) : true;
  bool preserveDrawingBuffer = argc > 7 ? get_bool_arg(env, argv[7], false) : false;
  bool preferLowPower = argc > 8 ? get_bool_arg(env, argv[8], false) : false;
  bool failIfCaveat = argc > 9 ? get_bool_arg(env, argv[9], false) : false;
  bool webgl2 = argc > 10 ? get_bool_arg(env, argv[10], false) : false;

  auto* ctx = new WebGLContextImpl(width, height, alpha, depth, stencil, antialias,
                                    premultipliedAlpha, preserveDrawingBuffer,
                                    preferLowPower, failIfCaveat, webgl2);

  if (ctx->state != GLCONTEXT_STATE_OK) {
    std::string error = "Error creating WebGLContext";
    if (!ctx->errorMessage.empty()) {
      error += ": " + ctx->errorMessage;
    }
    delete ctx;
    napi_throw_error(env, nullptr, error.c_str());
    return this_arg;
  }

  napi_wrap(
      env,
      this_arg,
      ctx,
      [](napi_env /*env*/, void* data, void* /*hint*/) {
        delete static_cast<WebGLContextImpl*>(data);
      },
      nullptr,
      nullptr);

  return this_arg;
}

// Destroy
napi_value destroy(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  ctx->dispose();
  return get_undefined(env);
}

// ============== Core rendering methods ==============

napi_value clearColor(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLfloat r = (GLfloat)get_double_arg(env, argv[0]);
  GLfloat g = (GLfloat)get_double_arg(env, argv[1]);
  GLfloat b = (GLfloat)get_double_arg(env, argv[2]);
  GLfloat a = (GLfloat)get_double_arg(env, argv[3]);

  glClearColor(r, g, b, a);
  return get_undefined(env);
}

napi_value clearDepth(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLfloat depth = (GLfloat)get_double_arg(env, argv[0], 1.0);
  glClearDepthf(depth);
  return get_undefined(env);
}

napi_value clearStencil(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint s = get_int32_arg(env, argv[0]);
  glClearStencil(s);
  return get_undefined(env);
}

napi_value clear(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLbitfield mask = get_uint32_arg(env, argv[0]);
  glClear(mask);
  return get_undefined(env);
}

napi_value readPixels(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 7;
  napi_value argv[7];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint x = get_int32_arg(env, argv[0]);
  GLint y = get_int32_arg(env, argv[1]);
  GLsizei width = get_int32_arg(env, argv[2]);
  GLsizei height = get_int32_arg(env, argv[3]);
  GLenum format = get_uint32_arg(env, argv[4]);
  GLenum type = get_uint32_arg(env, argv[5]);

  // argv[6] is the destination buffer (TypedArray)
  void* data = nullptr;
  size_t byte_length = 0;
  napi_typedarray_type arr_type;
  size_t arr_length;
  napi_value arr_buffer;
  size_t byte_offset;

  napi_get_typedarray_info(env, argv[6], &arr_type, &arr_length, &data, &arr_buffer, &byte_offset);

  if (data) {
    glReadPixels(x, y, width, height, format, type, data);
  }

  return get_undefined(env);
}

napi_value viewport(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint x = get_int32_arg(env, argv[0]);
  GLint y = get_int32_arg(env, argv[1]);
  GLsizei w = get_int32_arg(env, argv[2]);
  GLsizei h = get_int32_arg(env, argv[3]);

  glViewport(x, y, w, h);
  return get_undefined(env);
}

napi_value scissor(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint x = get_int32_arg(env, argv[0]);
  GLint y = get_int32_arg(env, argv[1]);
  GLsizei w = get_int32_arg(env, argv[2]);
  GLsizei h = get_int32_arg(env, argv[3]);

  glScissor(x, y, w, h);
  return get_undefined(env);
}

napi_value colorMask(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLboolean r = get_bool_arg(env, argv[0]) ? GL_TRUE : GL_FALSE;
  GLboolean g = get_bool_arg(env, argv[1]) ? GL_TRUE : GL_FALSE;
  GLboolean b = get_bool_arg(env, argv[2]) ? GL_TRUE : GL_FALSE;
  GLboolean a = get_bool_arg(env, argv[3]) ? GL_TRUE : GL_FALSE;

  glColorMask(r, g, b, a);
  return get_undefined(env);
}

// ============== State methods ==============

napi_value enable(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum cap = get_uint32_arg(env, argv[0]);
  glEnable(cap);
  return get_undefined(env);
}

napi_value disable(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum cap = get_uint32_arg(env, argv[0]);
  glDisable(cap);
  return get_undefined(env);
}

napi_value depthFunc(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum func = get_uint32_arg(env, argv[0]);
  glDepthFunc(func);
  return get_undefined(env);
}

napi_value depthMask(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLboolean flag = get_bool_arg(env, argv[0]) ? GL_TRUE : GL_FALSE;
  glDepthMask(flag);
  return get_undefined(env);
}

napi_value frontFace(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  glFrontFace(mode);
  return get_undefined(env);
}

napi_value cullFace(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  glCullFace(mode);
  return get_undefined(env);
}

napi_value activeTexture(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum texture = get_uint32_arg(env, argv[0]);
  glActiveTexture(texture);
  return get_undefined(env);
}

napi_value pixelStorei(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum pname = get_uint32_arg(env, argv[0]);
  GLint param = get_int32_arg(env, argv[1]);

  // Handle WebGL-specific pixel storage modes
  switch (pname) {
    case 0x9240: // UNPACK_FLIP_Y_WEBGL
      ctx->unpack_flip_y = (param != 0);
      break;
    case 0x9241: // UNPACK_PREMULTIPLY_ALPHA_WEBGL
      ctx->unpack_premultiply_alpha = (param != 0);
      break;
    case 0x9243: // UNPACK_COLORSPACE_CONVERSION_WEBGL
      ctx->unpack_colorspace_conversion = param;
      break;
    default:
      glPixelStorei(pname, param);
      break;
  }

  return get_undefined(env);
}

// ============== Buffer methods ==============

napi_value createBuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  GLuint buffer;
  glGenBuffers(1, &buffer);
  ctx->registerGLObj(GLOBJECT_TYPE_BUFFER, buffer);

  return make_uint32(env, buffer);
}

napi_value deleteBuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint buffer = get_uint32_arg(env, argv[0]);
  if (buffer) {
    glDeleteBuffers(1, &buffer);
    ctx->unregisterGLObj(GLOBJECT_TYPE_BUFFER, buffer);
  }

  return get_undefined(env);
}

napi_value bindBuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLuint buffer = get_uint32_arg(env, argv[1]);

  glBindBuffer(target, buffer);
  return get_undefined(env);
}

napi_value bufferData(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum usage = get_uint32_arg(env, argv[2]);

  // Check if argv[1] is a number (size) or typed array (data)
  napi_valuetype type;
  napi_typeof(env, argv[1], &type);

  if (type == napi_number) {
    GLsizeiptr size = (GLsizeiptr)get_int32_arg(env, argv[1]);
    glBufferData(target, size, nullptr, usage);
  } else {
    // Assume it's a typed array
    void* data = nullptr;
    size_t byte_length = 0;
    napi_typedarray_type arr_type;
    size_t arr_length;
    napi_value arr_buffer;
    size_t byte_offset;

    napi_status status = napi_get_typedarray_info(env, argv[1], &arr_type, &arr_length, &data, &arr_buffer, &byte_offset);
    if (status == napi_ok && data) {
      napi_get_arraybuffer_info(env, arr_buffer, nullptr, &byte_length);
      glBufferData(target, byte_length, data, usage);
    }
  }

  return get_undefined(env);
}

napi_value bufferSubData(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLintptr offset = (GLintptr)get_int32_arg(env, argv[1]);

  void* data = nullptr;
  size_t byte_length = 0;
  napi_typedarray_type arr_type;
  size_t arr_length;
  napi_value arr_buffer;
  size_t byte_offset;

  napi_status status = napi_get_typedarray_info(env, argv[2], &arr_type, &arr_length, &data, &arr_buffer, &byte_offset);
  if (status == napi_ok && data) {
    napi_get_arraybuffer_info(env, arr_buffer, nullptr, &byte_length);
    glBufferSubData(target, offset, byte_length, data);
  }

  return get_undefined(env);
}

// ============== Texture methods ==============

napi_value createTexture(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  GLuint texture;
  glGenTextures(1, &texture);
  ctx->registerGLObj(GLOBJECT_TYPE_TEXTURE, texture);

  return make_uint32(env, texture);
}

napi_value deleteTexture(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint texture = get_uint32_arg(env, argv[0]);
  if (texture) {
    glDeleteTextures(1, &texture);
    ctx->unregisterGLObj(GLOBJECT_TYPE_TEXTURE, texture);
  }

  return get_undefined(env);
}

napi_value bindTexture(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLuint texture = get_uint32_arg(env, argv[1]);

  glBindTexture(target, texture);
  return get_undefined(env);
}

napi_value texParameteri(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum pname = get_uint32_arg(env, argv[1]);
  GLint param = get_int32_arg(env, argv[2]);

  glTexParameteri(target, pname, param);
  return get_undefined(env);
}

napi_value texImage2D(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 9;
  napi_value argv[9];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLint level = get_int32_arg(env, argv[1]);
  GLint internalformat = get_int32_arg(env, argv[2]);
  GLsizei width = get_int32_arg(env, argv[3]);
  GLsizei height = get_int32_arg(env, argv[4]);
  GLint border = get_int32_arg(env, argv[5]);
  GLenum format = get_uint32_arg(env, argv[6]);
  GLenum type = get_uint32_arg(env, argv[7]);

  // argv[8] is the pixel data (can be null or typed array)
  napi_valuetype val_type;
  napi_typeof(env, argv[8], &val_type);

  if (val_type == napi_null || val_type == napi_undefined) {
    glTexImage2D(target, level, internalformat, width, height, border, format, type, nullptr);
  } else {
    void* data = nullptr;
    napi_typedarray_type arr_type;
    size_t arr_length;
    napi_value arr_buffer;
    size_t byte_offset;

    napi_get_typedarray_info(env, argv[8], &arr_type, &arr_length, &data, &arr_buffer, &byte_offset);
    glTexImage2D(target, level, internalformat, width, height, border, format, type, data);
  }

  return get_undefined(env);
}

napi_value texImage3D(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 10;
  napi_value argv[10];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLint level = get_int32_arg(env, argv[1]);
  GLint internalformat = get_int32_arg(env, argv[2]);
  GLsizei width = get_int32_arg(env, argv[3]);
  GLsizei height = get_int32_arg(env, argv[4]);
  GLsizei depth = get_int32_arg(env, argv[5]);
  GLint border = get_int32_arg(env, argv[6]);
  GLenum format = get_uint32_arg(env, argv[7]);
  GLenum type = get_uint32_arg(env, argv[8]);

  napi_valuetype val_type;
  napi_typeof(env, argv[9], &val_type);

  if (val_type == napi_null || val_type == napi_undefined) {
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, nullptr);
  } else {
    void* data = nullptr;
    napi_typedarray_type arr_type;
    size_t arr_length;
    napi_value arr_buffer;
    size_t byte_offset;

    napi_get_typedarray_info(env, argv[9], &arr_type, &arr_length, &data, &arr_buffer, &byte_offset);
    glTexImage3D(target, level, internalformat, width, height, depth, border, format, type, data);
  }

  return get_undefined(env);
}

// ============== Framebuffer methods ==============

napi_value createFramebuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  GLuint fbo;
  glGenFramebuffers(1, &fbo);
  ctx->registerGLObj(GLOBJECT_TYPE_FRAMEBUFFER, fbo);

  return make_uint32(env, fbo);
}

napi_value deleteFramebuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint fbo = get_uint32_arg(env, argv[0]);
  if (fbo) {
    glDeleteFramebuffers(1, &fbo);
    ctx->unregisterGLObj(GLOBJECT_TYPE_FRAMEBUFFER, fbo);
  }

  return get_undefined(env);
}

napi_value bindFramebuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLuint fbo = get_uint32_arg(env, argv[1]);

  glBindFramebuffer(target, fbo);
  return get_undefined(env);
}

napi_value framebufferTexture2D(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 5;
  napi_value argv[5];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum attachment = get_uint32_arg(env, argv[1]);
  GLenum textarget = get_uint32_arg(env, argv[2]);
  GLuint texture = get_uint32_arg(env, argv[3]);
  GLint level = get_int32_arg(env, argv[4]);

  glFramebufferTexture2D(target, attachment, textarget, texture, level);
  return get_undefined(env);
}

// ============== Renderbuffer methods ==============

napi_value createRenderbuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  GLuint rbo;
  glGenRenderbuffers(1, &rbo);
  ctx->registerGLObj(GLOBJECT_TYPE_RENDERBUFFER, rbo);

  return make_uint32(env, rbo);
}

napi_value deleteRenderbuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint rbo = get_uint32_arg(env, argv[0]);
  if (rbo) {
    glDeleteRenderbuffers(1, &rbo);
    ctx->unregisterGLObj(GLOBJECT_TYPE_RENDERBUFFER, rbo);
  }

  return get_undefined(env);
}

napi_value bindRenderbuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLuint rbo = get_uint32_arg(env, argv[1]);

  glBindRenderbuffer(target, rbo);
  return get_undefined(env);
}

napi_value renderbufferStorage(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum internalformat = get_uint32_arg(env, argv[1]);
  GLsizei width = get_int32_arg(env, argv[2]);
  GLsizei height = get_int32_arg(env, argv[3]);

  glRenderbufferStorage(target, internalformat, width, height);
  return get_undefined(env);
}

napi_value framebufferRenderbuffer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum attachment = get_uint32_arg(env, argv[1]);
  GLenum rbtarget = get_uint32_arg(env, argv[2]);
  GLuint rbo = get_uint32_arg(env, argv[3]);

  glFramebufferRenderbuffer(target, attachment, rbtarget, rbo);
  return get_undefined(env);
}

// ============== Shader methods ==============

napi_value createShader(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum type = get_uint32_arg(env, argv[0]);
  GLuint shader = glCreateShader(type);
  ctx->registerGLObj(GLOBJECT_TYPE_SHADER, shader);

  return make_uint32(env, shader);
}

napi_value deleteShader(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint shader = get_uint32_arg(env, argv[0]);
  if (shader) {
    glDeleteShader(shader);
    ctx->unregisterGLObj(GLOBJECT_TYPE_SHADER, shader);
  }

  return get_undefined(env);
}

napi_value shaderSource(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint shader = get_uint32_arg(env, argv[0]);
  std::string source = get_string_arg(env, argv[1]);

  const char* src = source.c_str();
  GLint len = (GLint)source.length();
  glShaderSource(shader, 1, &src, &len);

  return get_undefined(env);
}

napi_value compileShader(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint shader = get_uint32_arg(env, argv[0]);
  glCompileShader(shader);

  return get_undefined(env);
}

napi_value getShaderParameter(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint shader = get_uint32_arg(env, argv[0]);
  GLenum pname = get_uint32_arg(env, argv[1]);

  GLint value;
  glGetShaderiv(shader, pname, &value);

  // COMPILE_STATUS returns bool
  if (pname == GL_COMPILE_STATUS || pname == GL_DELETE_STATUS) {
    return make_bool(env, value != 0);
  }

  return make_int32(env, value);
}

napi_value getShaderInfoLog(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint shader = get_uint32_arg(env, argv[0]);

  GLint len = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);

  if (len > 0) {
    std::vector<char> log(len);
    glGetShaderInfoLog(shader, len, nullptr, log.data());
    return make_string(env, log.data());
  }

  return make_string(env, "");
}

// ============== Program methods ==============

napi_value createProgram(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  GLuint program = glCreateProgram();
  ctx->registerGLObj(GLOBJECT_TYPE_PROGRAM, program);

  return make_uint32(env, program);
}

napi_value deleteProgram(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  if (program) {
    glDeleteProgram(program);
    ctx->unregisterGLObj(GLOBJECT_TYPE_PROGRAM, program);
  }

  return get_undefined(env);
}

napi_value attachShader(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  GLuint shader = get_uint32_arg(env, argv[1]);

  glAttachShader(program, shader);
  return get_undefined(env);
}

napi_value linkProgram(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  glLinkProgram(program);

  return get_undefined(env);
}

napi_value useProgram(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  glUseProgram(program);

  return get_undefined(env);
}

napi_value getProgramParameter(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  GLenum pname = get_uint32_arg(env, argv[1]);

  GLint value;
  glGetProgramiv(program, pname, &value);

  // LINK_STATUS, DELETE_STATUS, VALIDATE_STATUS return bool
  if (pname == GL_LINK_STATUS || pname == GL_DELETE_STATUS || pname == GL_VALIDATE_STATUS) {
    return make_bool(env, value != 0);
  }

  return make_int32(env, value);
}

napi_value getProgramInfoLog(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);

  GLint len = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);

  if (len > 0) {
    std::vector<char> log(len);
    glGetProgramInfoLog(program, len, nullptr, log.data());
    return make_string(env, log.data());
  }

  return make_string(env, "");
}

napi_value getUniformLocation(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  std::string name = get_string_arg(env, argv[1]);

  GLint location = glGetUniformLocation(program, name.c_str());

  return make_int32(env, location);
}

napi_value getAttribLocation(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  std::string name = get_string_arg(env, argv[1]);

  GLint location = glGetAttribLocation(program, name.c_str());

  return make_int32(env, location);
}

napi_value getActiveAttrib(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  GLuint index = get_uint32_arg(env, argv[1]);

  GLint maxLength;
  glGetProgramiv(program, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &maxLength);

  std::vector<char> name(maxLength);
  GLsizei length = 0;
  GLenum type;
  GLint size;
  glGetActiveAttrib(program, index, maxLength, &length, &size, &type, name.data());

  if (length > 0) {
    napi_value result;
    napi_create_object(env, &result);
    napi_set_named_property(env, result, "size", make_int32(env, size));
    napi_set_named_property(env, result, "type", make_uint32(env, type));
    napi_set_named_property(env, result, "name", make_string(env, name.data()));
    return result;
  }

  return get_null(env);
}

napi_value getActiveUniform(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  GLuint index = get_uint32_arg(env, argv[1]);

  GLint maxLength;
  glGetProgramiv(program, GL_ACTIVE_UNIFORM_MAX_LENGTH, &maxLength);

  std::vector<char> name(maxLength);
  GLsizei length = 0;
  GLenum type;
  GLint size;
  glGetActiveUniform(program, index, maxLength, &length, &size, &type, name.data());

  if (length > 0) {
    napi_value result;
    napi_create_object(env, &result);
    napi_set_named_property(env, result, "size", make_int32(env, size));
    napi_set_named_property(env, result, "type", make_uint32(env, type));
    napi_set_named_property(env, result, "name", make_string(env, name.data()));
    return result;
  }

  return get_null(env);
}

// ============== Vertex attribute methods ==============

napi_value enableVertexAttribArray(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint index = get_uint32_arg(env, argv[0]);
  glEnableVertexAttribArray(index);

  return get_undefined(env);
}

napi_value disableVertexAttribArray(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint index = get_uint32_arg(env, argv[0]);
  glDisableVertexAttribArray(index);

  return get_undefined(env);
}

napi_value vertexAttribPointer(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 6;
  napi_value argv[6];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint index = get_uint32_arg(env, argv[0]);
  GLint size = get_int32_arg(env, argv[1]);
  GLenum type = get_uint32_arg(env, argv[2]);
  GLboolean normalized = get_bool_arg(env, argv[3]) ? GL_TRUE : GL_FALSE;
  GLsizei stride = get_int32_arg(env, argv[4]);
  GLintptr offset = (GLintptr)get_int32_arg(env, argv[5]);

  glVertexAttribPointer(index, size, type, normalized, stride, (const void*)offset);

  return get_undefined(env);
}

// ============== Draw methods ==============

napi_value drawArrays(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  GLint first = get_int32_arg(env, argv[1]);
  GLsizei count = get_int32_arg(env, argv[2]);

  glDrawArrays(mode, first, count);

  return get_undefined(env);
}

napi_value drawElements(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  GLsizei count = get_int32_arg(env, argv[1]);
  GLenum type = get_uint32_arg(env, argv[2]);
  GLintptr offset = (GLintptr)get_int32_arg(env, argv[3]);

  glDrawElements(mode, count, type, (const void*)offset);

  return get_undefined(env);
}

napi_value drawArraysInstancedANGLE(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  GLint first = get_int32_arg(env, argv[1]);
  GLsizei count = get_int32_arg(env, argv[2]);
  GLsizei instanceCount = get_int32_arg(env, argv[3]);

  glDrawArraysInstancedANGLE(mode, first, count, instanceCount);

  return get_undefined(env);
}

napi_value drawElementsInstancedANGLE(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 5;
  napi_value argv[5];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  GLsizei count = get_int32_arg(env, argv[1]);
  GLenum type = get_uint32_arg(env, argv[2]);
  GLintptr offset = (GLintptr)get_int32_arg(env, argv[3]);
  GLsizei instanceCount = get_int32_arg(env, argv[4]);

  glDrawElementsInstancedANGLE(mode, count, type, (const void*)offset, instanceCount);

  return get_undefined(env);
}

napi_value vertexAttribDivisorANGLE(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint index = get_uint32_arg(env, argv[0]);
  GLuint divisor = get_uint32_arg(env, argv[1]);

  glVertexAttribDivisorANGLE(index, divisor);

  return get_undefined(env);
}

// ============== Uniform methods ==============

napi_value uniform1f(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLfloat x = (GLfloat)get_double_arg(env, argv[1]);
  glUniform1f(loc, x);
  return get_undefined(env);
}

napi_value uniform2f(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLfloat x = (GLfloat)get_double_arg(env, argv[1]);
  GLfloat y = (GLfloat)get_double_arg(env, argv[2]);
  glUniform2f(loc, x, y);
  return get_undefined(env);
}

napi_value uniform3f(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLfloat x = (GLfloat)get_double_arg(env, argv[1]);
  GLfloat y = (GLfloat)get_double_arg(env, argv[2]);
  GLfloat z = (GLfloat)get_double_arg(env, argv[3]);
  glUniform3f(loc, x, y, z);
  return get_undefined(env);
}

napi_value uniform4f(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 5;
  napi_value argv[5];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLfloat x = (GLfloat)get_double_arg(env, argv[1]);
  GLfloat y = (GLfloat)get_double_arg(env, argv[2]);
  GLfloat z = (GLfloat)get_double_arg(env, argv[3]);
  GLfloat w = (GLfloat)get_double_arg(env, argv[4]);
  glUniform4f(loc, x, y, z, w);
  return get_undefined(env);
}

napi_value uniform1i(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLint x = get_int32_arg(env, argv[1]);
  glUniform1i(loc, x);
  return get_undefined(env);
}

napi_value uniform2i(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLint x = get_int32_arg(env, argv[1]);
  GLint y = get_int32_arg(env, argv[2]);
  glUniform2i(loc, x, y);
  return get_undefined(env);
}

napi_value uniform3i(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLint x = get_int32_arg(env, argv[1]);
  GLint y = get_int32_arg(env, argv[2]);
  GLint z = get_int32_arg(env, argv[3]);
  glUniform3i(loc, x, y, z);
  return get_undefined(env);
}

napi_value uniform4i(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 5;
  napi_value argv[5];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);
  GLint loc = get_int32_arg(env, argv[0]);
  GLint x = get_int32_arg(env, argv[1]);
  GLint y = get_int32_arg(env, argv[2]);
  GLint z = get_int32_arg(env, argv[3]);
  GLint w = get_int32_arg(env, argv[4]);
  glUniform4i(loc, x, y, z, w);
  return get_undefined(env);
}

// Helper to get float array from TypedArray or array
bool get_float_array(napi_env env, napi_value arg, std::vector<GLfloat>& out) {
  // Check if it's a TypedArray (Float32Array)
  bool is_typedarray = false;
  napi_is_typedarray(env, arg, &is_typedarray);

  if (is_typedarray) {
    napi_typedarray_type type;
    size_t length;
    void* data;
    napi_get_typedarray_info(env, arg, &type, &length, &data, nullptr, nullptr);

    if (type == napi_float32_array) {
      GLfloat* float_data = static_cast<GLfloat*>(data);
      out.assign(float_data, float_data + length);
      return true;
    }
  }

  // Check if it's a regular array
  bool is_array = false;
  napi_is_array(env, arg, &is_array);

  if (is_array) {
    uint32_t length;
    napi_get_array_length(env, arg, &length);
    out.resize(length);
    for (uint32_t i = 0; i < length; i++) {
      napi_value elem;
      napi_get_element(env, arg, i, &elem);
      double val;
      napi_get_value_double(env, elem, &val);
      out[i] = (GLfloat)val;
    }
    return true;
  }

  return false;
}

napi_value uniformMatrix2fv(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint loc = get_int32_arg(env, argv[0]);
  GLboolean transpose = get_bool_arg(env, argv[1]);

  std::vector<GLfloat> data;
  if (get_float_array(env, argv[2], data) && data.size() >= 4) {
    glUniformMatrix2fv(loc, data.size() / 4, transpose, data.data());
  }

  return get_undefined(env);
}

napi_value uniformMatrix3fv(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint loc = get_int32_arg(env, argv[0]);
  GLboolean transpose = get_bool_arg(env, argv[1]);

  std::vector<GLfloat> data;
  if (get_float_array(env, argv[2], data) && data.size() >= 9) {
    glUniformMatrix3fv(loc, data.size() / 9, transpose, data.data());
  }

  return get_undefined(env);
}

napi_value uniformMatrix4fv(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLint loc = get_int32_arg(env, argv[0]);
  GLboolean transpose = get_bool_arg(env, argv[1]);

  std::vector<GLfloat> data;
  if (get_float_array(env, argv[2], data) && data.size() >= 16) {
    glUniformMatrix4fv(loc, data.size() / 16, transpose, data.data());
  }

  return get_undefined(env);
}

napi_value bindAttribLocation(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint program = get_uint32_arg(env, argv[0]);
  GLuint index = get_uint32_arg(env, argv[1]);
  std::string name = get_string_arg(env, argv[2]);

  glBindAttribLocation(program, index, name.c_str());

  return get_undefined(env);
}

napi_value blendFunc(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum sfactor = get_uint32_arg(env, argv[0]);
  GLenum dfactor = get_uint32_arg(env, argv[1]);

  glBlendFunc(sfactor, dfactor);

  return get_undefined(env);
}

napi_value blendFuncSeparate(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 4;
  napi_value argv[4];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum srcRGB = get_uint32_arg(env, argv[0]);
  GLenum dstRGB = get_uint32_arg(env, argv[1]);
  GLenum srcAlpha = get_uint32_arg(env, argv[2]);
  GLenum dstAlpha = get_uint32_arg(env, argv[3]);

  glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);

  return get_undefined(env);
}

napi_value blendEquation(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum mode = get_uint32_arg(env, argv[0]);
  glBlendEquation(mode);

  return get_undefined(env);
}

napi_value blendEquationSeparate(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum modeRGB = get_uint32_arg(env, argv[0]);
  GLenum modeAlpha = get_uint32_arg(env, argv[1]);

  glBlendEquationSeparate(modeRGB, modeAlpha);

  return get_undefined(env);
}

napi_value generateMipmap(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  glGenerateMipmap(target);

  return get_undefined(env);
}

napi_value checkFramebufferStatus(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum status = glCheckFramebufferStatus(target);

  return make_uint32(env, status);
}

napi_value depthRange(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLfloat zNear = (GLfloat)get_double_arg(env, argv[0]);
  GLfloat zFar = (GLfloat)get_double_arg(env, argv[1]);

  glDepthRangef(zNear, zFar);

  return get_undefined(env);
}

napi_value lineWidth(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLfloat width = (GLfloat)get_double_arg(env, argv[0]);
  glLineWidth(width);

  return get_undefined(env);
}

napi_value polygonOffset(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLfloat factor = (GLfloat)get_double_arg(env, argv[0]);
  GLfloat units = (GLfloat)get_double_arg(env, argv[1]);

  glPolygonOffset(factor, units);

  return get_undefined(env);
}

napi_value stencilFunc(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum func = get_uint32_arg(env, argv[0]);
  GLint ref = get_int32_arg(env, argv[1]);
  GLuint mask = get_uint32_arg(env, argv[2]);

  glStencilFunc(func, ref, mask);

  return get_undefined(env);
}

napi_value stencilMask(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint mask = get_uint32_arg(env, argv[0]);
  glStencilMask(mask);

  return get_undefined(env);
}

napi_value stencilOp(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 3;
  napi_value argv[3];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum fail = get_uint32_arg(env, argv[0]);
  GLenum zfail = get_uint32_arg(env, argv[1]);
  GLenum zpass = get_uint32_arg(env, argv[2]);

  glStencilOp(fail, zfail, zpass);

  return get_undefined(env);
}

napi_value hint(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLenum mode = get_uint32_arg(env, argv[1]);

  glHint(target, mode);

  return get_undefined(env);
}

napi_value isEnabled(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum cap = get_uint32_arg(env, argv[0]);
  GLboolean result = glIsEnabled(cap);

  return make_bool(env, result != GL_FALSE);
}

napi_value texSubImage2D(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  size_t argc = 9;
  napi_value argv[9];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum target = get_uint32_arg(env, argv[0]);
  GLint level = get_int32_arg(env, argv[1]);
  GLint xoffset = get_int32_arg(env, argv[2]);
  GLint yoffset = get_int32_arg(env, argv[3]);
  GLsizei width = get_int32_arg(env, argv[4]);
  GLsizei height = get_int32_arg(env, argv[5]);
  GLenum format = get_uint32_arg(env, argv[6]);
  GLenum type = get_uint32_arg(env, argv[7]);

  // Get pixel data
  bool is_typedarray = false;
  napi_is_typedarray(env, argv[8], &is_typedarray);

  if (is_typedarray) {
    void* data = nullptr;
    size_t length;
    napi_get_typedarray_info(env, argv[8], nullptr, &length, &data, nullptr, nullptr);

    std::vector<uint8_t> pixels = ctx->unpackPixels(type, format, width, height, (unsigned char*)data);
    glTexSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels.data());
  }

  return get_undefined(env);
}

// ============== Query methods ==============

napi_value getParameter(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum pname = get_uint32_arg(env, argv[0]);

  // Handle string parameters
  switch (pname) {
    case GL_VENDOR:
    case GL_RENDERER:
    case GL_VERSION:
    case GL_SHADING_LANGUAGE_VERSION:
      return make_string(env, (const char*)glGetString(pname));
  }

  // Handle integer parameters
  GLint value;
  glGetIntegerv(pname, &value);
  return make_int32(env, value);
}

napi_value getError(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  return make_int32(env, ctx->getError());
}

napi_value getSupportedExtensions(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  std::string extensions;
  for (const auto& ext : ctx->supportedWebGLExtensions) {
    if (!extensions.empty()) extensions += " ";
    extensions += ext;
  }

  return make_string(env, extensions.c_str());
}

napi_value getExtension(napi_env env, napi_callback_info info) {
  // TODO: Implement extension objects
  return get_null(env);
}

napi_value getShaderPrecisionFormat(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 2;
  napi_value argv[2];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum shaderType = get_uint32_arg(env, argv[0]);
  GLenum precisionType = get_uint32_arg(env, argv[1]);

  GLint range[2] = {0, 0};
  GLint precision = 0;
  glGetShaderPrecisionFormat(shaderType, precisionType, range, &precision);

  napi_value obj;
  napi_create_object(env, &obj);

  napi_value range_min, range_max, prec;
  napi_create_int32(env, range[0], &range_min);
  napi_create_int32(env, range[1], &range_max);
  napi_create_int32(env, precision, &prec);

  napi_set_named_property(env, obj, "rangeMin", range_min);
  napi_set_named_property(env, obj, "rangeMax", range_max);
  napi_set_named_property(env, obj, "precision", prec);

  return obj;
}

napi_value flush(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  glFlush();
  return get_undefined(env);
}

napi_value finish(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);
  glFinish();
  return get_undefined(env);
}

// ============== Vertex Array Object methods ==============

napi_value createVertexArray(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  GLuint vao;
  glGenVertexArraysOES(1, &vao);
  ctx->registerGLObj(GLOBJECT_TYPE_VERTEX_ARRAY, vao);

  return make_uint32(env, vao);
}

napi_value deleteVertexArray(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint vao = get_uint32_arg(env, argv[0]);
  if (vao) {
    glDeleteVertexArraysOES(1, &vao);
    ctx->unregisterGLObj(GLOBJECT_TYPE_VERTEX_ARRAY, vao);
  }

  return get_undefined(env);
}

napi_value bindVertexArray(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint vao = get_uint32_arg(env, argv[0]);
  glBindVertexArrayOES(vao);

  return get_undefined(env);
}

napi_value isVertexArray(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLuint vao = get_uint32_arg(env, argv[0]);
  GLboolean result = glIsVertexArrayOES(vao);

  return make_bool(env, result != GL_FALSE);
}

// ============== Extension: WEBGL_draw_buffers ==============

napi_value extWEBGL_draw_buffers(napi_env env, napi_callback_info /*info*/) {
  napi_value result;
  napi_create_object(env, &result);

  // COLOR_ATTACHMENT constants
  napi_set_named_property(env, result, "COLOR_ATTACHMENT0_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT0_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT1_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT1_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT2_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT2_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT3_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT3_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT4_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT4_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT5_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT5_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT6_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT6_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT7_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT7_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT8_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT8_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT9_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT9_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT10_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT10_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT11_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT11_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT12_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT12_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT13_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT13_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT14_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT14_EXT));
  napi_set_named_property(env, result, "COLOR_ATTACHMENT15_WEBGL", make_int32(env, GL_COLOR_ATTACHMENT15_EXT));

  // DRAW_BUFFER constants
  napi_set_named_property(env, result, "DRAW_BUFFER0_WEBGL", make_int32(env, GL_DRAW_BUFFER0_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER1_WEBGL", make_int32(env, GL_DRAW_BUFFER1_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER2_WEBGL", make_int32(env, GL_DRAW_BUFFER2_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER3_WEBGL", make_int32(env, GL_DRAW_BUFFER3_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER4_WEBGL", make_int32(env, GL_DRAW_BUFFER4_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER5_WEBGL", make_int32(env, GL_DRAW_BUFFER5_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER6_WEBGL", make_int32(env, GL_DRAW_BUFFER6_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER7_WEBGL", make_int32(env, GL_DRAW_BUFFER7_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER8_WEBGL", make_int32(env, GL_DRAW_BUFFER8_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER9_WEBGL", make_int32(env, GL_DRAW_BUFFER9_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER10_WEBGL", make_int32(env, GL_DRAW_BUFFER10_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER11_WEBGL", make_int32(env, GL_DRAW_BUFFER11_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER12_WEBGL", make_int32(env, GL_DRAW_BUFFER12_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER13_WEBGL", make_int32(env, GL_DRAW_BUFFER13_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER14_WEBGL", make_int32(env, GL_DRAW_BUFFER14_EXT));
  napi_set_named_property(env, result, "DRAW_BUFFER15_WEBGL", make_int32(env, GL_DRAW_BUFFER15_EXT));

  // Max constants
  napi_set_named_property(env, result, "MAX_COLOR_ATTACHMENTS_WEBGL", make_int32(env, GL_MAX_COLOR_ATTACHMENTS_EXT));
  napi_set_named_property(env, result, "MAX_DRAW_BUFFERS_WEBGL", make_int32(env, GL_MAX_DRAW_BUFFERS_EXT));

  return result;
}

napi_value drawBuffersWEBGL(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  // argv[0] should be an array of GLenum values
  bool is_array = false;
  napi_is_array(env, argv[0], &is_array);
  if (!is_array) {
    return get_undefined(env);
  }

  uint32_t length = 0;
  napi_get_array_length(env, argv[0], &length);

  std::vector<GLenum> buffers(length);
  for (uint32_t i = 0; i < length; i++) {
    napi_value elem;
    napi_get_element(env, argv[0], i, &elem);
    buffers[i] = get_uint32_arg(env, elem);
  }

  glDrawBuffersEXT(length, buffers.data());

  return get_undefined(env);
}

// ============== Cleanup ==============

napi_value cleanup(napi_env env, napi_callback_info /*info*/) {
  while (WebGLContextImpl::CONTEXT_LIST_HEAD) {
    WebGLContextImpl::CONTEXT_LIST_HEAD->dispose();
  }

  if (WebGLContextImpl::HAS_DISPLAY) {
    eglTerminate(WebGLContextImpl::DISPLAY);
    WebGLContextImpl::HAS_DISPLAY = false;
  }

  return get_undefined(env);
}

napi_value setError(napi_env env, napi_callback_info info) {
  GL_NAPI_BOILERPLATE(ctx);

  size_t argc = 1;
  napi_value argv[1];
  napi_get_cb_info(env, info, &argc, argv, nullptr, nullptr);

  GLenum error = get_uint32_arg(env, argv[0]);
  ctx->setError(error);

  return get_undefined(env);
}

// ============== Constants helper ==============

void set_const(napi_env env, napi_value obj, const char* name, int32_t value) {
  napi_value v;
  napi_create_int32(env, value, &v);
  napi_set_named_property(env, obj, name, v);
}

// ============== Module initialization ==============

napi_value init(napi_env env, napi_value exports) {
  napi_property_descriptor methods[] = {
      // Cleanup / error
      {"cleanup", nullptr, cleanup, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"setError", nullptr, setError, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"destroy", nullptr, destroy, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Core rendering
      {"clearColor", nullptr, clearColor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"clearDepth", nullptr, clearDepth, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"clearStencil", nullptr, clearStencil, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"clear", nullptr, clear, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"readPixels", nullptr, readPixels, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"viewport", nullptr, viewport, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"scissor", nullptr, scissor, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"colorMask", nullptr, colorMask, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"flush", nullptr, flush, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"finish", nullptr, finish, nullptr, nullptr, nullptr, napi_default, nullptr},

      // State
      {"enable", nullptr, enable, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"disable", nullptr, disable, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"depthFunc", nullptr, depthFunc, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"depthMask", nullptr, depthMask, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"frontFace", nullptr, frontFace, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"cullFace", nullptr, cullFace, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"activeTexture", nullptr, activeTexture, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"pixelStorei", nullptr, pixelStorei, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Buffers
      {"createBuffer", nullptr, createBuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteBuffer", nullptr, deleteBuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bindBuffer", nullptr, bindBuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bufferData", nullptr, bufferData, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bufferSubData", nullptr, bufferSubData, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Textures
      {"createTexture", nullptr, createTexture, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteTexture", nullptr, deleteTexture, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bindTexture", nullptr, bindTexture, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"texParameteri", nullptr, texParameteri, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"texImage2D", nullptr, texImage2D, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"texImage3D", nullptr, texImage3D, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Framebuffers
      {"createFramebuffer", nullptr, createFramebuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteFramebuffer", nullptr, deleteFramebuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bindFramebuffer", nullptr, bindFramebuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"framebufferTexture2D", nullptr, framebufferTexture2D, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Renderbuffers
      {"createRenderbuffer", nullptr, createRenderbuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteRenderbuffer", nullptr, deleteRenderbuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bindRenderbuffer", nullptr, bindRenderbuffer, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"renderbufferStorage", nullptr, renderbufferStorage, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"framebufferRenderbuffer", nullptr, framebufferRenderbuffer, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Shaders
      {"createShader", nullptr, createShader, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteShader", nullptr, deleteShader, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"shaderSource", nullptr, shaderSource, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"compileShader", nullptr, compileShader, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getShaderParameter", nullptr, getShaderParameter, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getShaderInfoLog", nullptr, getShaderInfoLog, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Programs
      {"createProgram", nullptr, createProgram, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteProgram", nullptr, deleteProgram, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"attachShader", nullptr, attachShader, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"linkProgram", nullptr, linkProgram, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"useProgram", nullptr, useProgram, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getProgramParameter", nullptr, getProgramParameter, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getProgramInfoLog", nullptr, getProgramInfoLog, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getUniformLocation", nullptr, getUniformLocation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getAttribLocation", nullptr, getAttribLocation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getActiveAttrib", nullptr, getActiveAttrib, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getActiveUniform", nullptr, getActiveUniform, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Vertex attributes
      {"enableVertexAttribArray", nullptr, enableVertexAttribArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"disableVertexAttribArray", nullptr, disableVertexAttribArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"vertexAttribPointer", nullptr, vertexAttribPointer, nullptr, nullptr, nullptr, napi_default, nullptr},

      // VAOs (WebGL 2 names)
      {"createVertexArray", nullptr, createVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteVertexArray", nullptr, deleteVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bindVertexArray", nullptr, bindVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      // VAOs (OES extension names - same functions)
      {"createVertexArrayOES", nullptr, createVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"deleteVertexArrayOES", nullptr, deleteVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"bindVertexArrayOES", nullptr, bindVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"isVertexArrayOES", nullptr, isVertexArray, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Draw calls
      {"drawArrays", nullptr, drawArrays, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"drawElements", nullptr, drawElements, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"_drawArraysInstancedANGLE", nullptr, drawArraysInstancedANGLE, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"_drawElementsInstancedANGLE", nullptr, drawElementsInstancedANGLE, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"_vertexAttribDivisorANGLE", nullptr, vertexAttribDivisorANGLE, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Uniforms
      {"uniform1f", nullptr, uniform1f, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform2f", nullptr, uniform2f, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform3f", nullptr, uniform3f, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform4f", nullptr, uniform4f, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform1i", nullptr, uniform1i, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform2i", nullptr, uniform2i, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform3i", nullptr, uniform3i, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniform4i", nullptr, uniform4i, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniformMatrix2fv", nullptr, uniformMatrix2fv, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniformMatrix3fv", nullptr, uniformMatrix3fv, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"uniformMatrix4fv", nullptr, uniformMatrix4fv, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Blending
      {"blendFunc", nullptr, blendFunc, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"blendFuncSeparate", nullptr, blendFuncSeparate, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"blendEquation", nullptr, blendEquation, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"blendEquationSeparate", nullptr, blendEquationSeparate, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Programs extra
      {"bindAttribLocation", nullptr, bindAttribLocation, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Texture extra
      {"generateMipmap", nullptr, generateMipmap, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"texSubImage2D", nullptr, texSubImage2D, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Framebuffer extra
      {"checkFramebufferStatus", nullptr, checkFramebufferStatus, nullptr, nullptr, nullptr, napi_default, nullptr},

      // State extras
      {"depthRange", nullptr, depthRange, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"lineWidth", nullptr, lineWidth, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"polygonOffset", nullptr, polygonOffset, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stencilFunc", nullptr, stencilFunc, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stencilMask", nullptr, stencilMask, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"stencilOp", nullptr, stencilOp, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"hint", nullptr, hint, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"isEnabled", nullptr, isEnabled, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Queries
      {"getParameter", nullptr, getParameter, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getError", nullptr, getError, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getSupportedExtensions", nullptr, getSupportedExtensions, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getExtension", nullptr, getExtension, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"getShaderPrecisionFormat", nullptr, getShaderPrecisionFormat, nullptr, nullptr, nullptr, napi_default, nullptr},

      // Extensions
      {"extWEBGL_draw_buffers", nullptr, extWEBGL_draw_buffers, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"drawBuffersWEBGL", nullptr, drawBuffersWEBGL, nullptr, nullptr, nullptr, napi_default, nullptr},
  };

  napi_value webgl_class;
  napi_define_class(
      env,
      "WebGLRenderingContext",
      NAPI_AUTO_LENGTH,
      ctor,
      nullptr,
      sizeof(methods) / sizeof(methods[0]),
      methods,
      &webgl_class);

  napi_set_named_property(env, exports, "WebGLRenderingContext", webgl_class);

  // Populate constants on the prototype
  napi_value proto;
  napi_get_named_property(env, webgl_class, "prototype", &proto);

  // Error constants
  set_const(env, proto, "NO_ERROR", GL_NO_ERROR);
  set_const(env, proto, "INVALID_ENUM", GL_INVALID_ENUM);
  set_const(env, proto, "INVALID_VALUE", GL_INVALID_VALUE);
  set_const(env, proto, "INVALID_OPERATION", GL_INVALID_OPERATION);
  set_const(env, proto, "OUT_OF_MEMORY", GL_OUT_OF_MEMORY);

  // Buffer targets
  set_const(env, proto, "ARRAY_BUFFER", GL_ARRAY_BUFFER);
  set_const(env, proto, "ELEMENT_ARRAY_BUFFER", GL_ELEMENT_ARRAY_BUFFER);

  // Framebuffer/Renderbuffer
  set_const(env, proto, "FRAMEBUFFER", GL_FRAMEBUFFER);
  set_const(env, proto, "RENDERBUFFER", GL_RENDERBUFFER);
  set_const(env, proto, "DRAW_FRAMEBUFFER", 0x8CA9);

  // Texture targets
  set_const(env, proto, "TEXTURE_2D", GL_TEXTURE_2D);
  set_const(env, proto, "TEXTURE_CUBE_MAP", GL_TEXTURE_CUBE_MAP);
  set_const(env, proto, "TEXTURE_CUBE_MAP_POSITIVE_X", GL_TEXTURE_CUBE_MAP_POSITIVE_X);
  set_const(env, proto, "TEXTURE_CUBE_MAP_NEGATIVE_X", GL_TEXTURE_CUBE_MAP_NEGATIVE_X);
  set_const(env, proto, "TEXTURE_CUBE_MAP_POSITIVE_Y", GL_TEXTURE_CUBE_MAP_POSITIVE_Y);
  set_const(env, proto, "TEXTURE_CUBE_MAP_NEGATIVE_Y", GL_TEXTURE_CUBE_MAP_NEGATIVE_Y);
  set_const(env, proto, "TEXTURE_CUBE_MAP_POSITIVE_Z", GL_TEXTURE_CUBE_MAP_POSITIVE_Z);
  set_const(env, proto, "TEXTURE_CUBE_MAP_NEGATIVE_Z", GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
  set_const(env, proto, "TEXTURE_3D", 0x806F);
  set_const(env, proto, "TEXTURE_2D_ARRAY", 0x8C1A);

  // Attachments
  set_const(env, proto, "COLOR_ATTACHMENT0", GL_COLOR_ATTACHMENT0);
  set_const(env, proto, "DEPTH_ATTACHMENT", GL_DEPTH_ATTACHMENT);
  set_const(env, proto, "STENCIL_ATTACHMENT", GL_STENCIL_ATTACHMENT);
  set_const(env, proto, "DEPTH_STENCIL_ATTACHMENT", 0x821A);

  // Texture units
  set_const(env, proto, "TEXTURE0", GL_TEXTURE0);
  set_const(env, proto, "TEXTURE_MIN_FILTER", GL_TEXTURE_MIN_FILTER);
  set_const(env, proto, "TEXTURE_MAG_FILTER", GL_TEXTURE_MAG_FILTER);
  set_const(env, proto, "NEAREST", GL_NEAREST);
  set_const(env, proto, "LINEAR", GL_LINEAR);

  // Pixel formats
  set_const(env, proto, "RGBA", GL_RGBA);
  set_const(env, proto, "RGB", GL_RGB);
  set_const(env, proto, "ALPHA", GL_ALPHA);
  set_const(env, proto, "LUMINANCE", GL_LUMINANCE);
  set_const(env, proto, "LUMINANCE_ALPHA", GL_LUMINANCE_ALPHA);

  // Clear bits
  set_const(env, proto, "COLOR_BUFFER_BIT", GL_COLOR_BUFFER_BIT);
  set_const(env, proto, "DEPTH_BUFFER_BIT", GL_DEPTH_BUFFER_BIT);
  set_const(env, proto, "STENCIL_BUFFER_BIT", GL_STENCIL_BUFFER_BIT);

  // Enable caps
  set_const(env, proto, "DEPTH_TEST", GL_DEPTH_TEST);
  set_const(env, proto, "CULL_FACE", GL_CULL_FACE);
  set_const(env, proto, "BLEND", GL_BLEND);
  set_const(env, proto, "SCISSOR_TEST", GL_SCISSOR_TEST);
  set_const(env, proto, "STENCIL_TEST", GL_STENCIL_TEST);

  // getParameter pnames
  set_const(env, proto, "MAX_COMBINED_TEXTURE_IMAGE_UNITS", GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS);
  set_const(env, proto, "MAX_TEXTURE_SIZE", GL_MAX_TEXTURE_SIZE);
  set_const(env, proto, "MAX_CUBE_MAP_TEXTURE_SIZE", GL_MAX_CUBE_MAP_TEXTURE_SIZE);
  set_const(env, proto, "MAX_VERTEX_ATTRIBS", GL_MAX_VERTEX_ATTRIBS);
  set_const(env, proto, "MAX_TEXTURE_IMAGE_UNITS", GL_MAX_TEXTURE_IMAGE_UNITS);
  set_const(env, proto, "MAX_VERTEX_TEXTURE_IMAGE_UNITS", GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
  set_const(env, proto, "MAX_RENDERBUFFER_SIZE", GL_MAX_RENDERBUFFER_SIZE);
  set_const(env, proto, "MAX_VIEWPORT_DIMS", GL_MAX_VIEWPORT_DIMS);
  set_const(env, proto, "MAX_VARYING_VECTORS", GL_MAX_VARYING_VECTORS);
  set_const(env, proto, "MAX_VERTEX_UNIFORM_VECTORS", GL_MAX_VERTEX_UNIFORM_VECTORS);
  set_const(env, proto, "MAX_FRAGMENT_UNIFORM_VECTORS", GL_MAX_FRAGMENT_UNIFORM_VECTORS);
  set_const(env, proto, "SCISSOR_BOX", GL_SCISSOR_BOX);
  set_const(env, proto, "VIEWPORT", GL_VIEWPORT);
  set_const(env, proto, "VENDOR", GL_VENDOR);
  set_const(env, proto, "RENDERER", GL_RENDERER);
  set_const(env, proto, "VERSION", GL_VERSION);
  set_const(env, proto, "SHADING_LANGUAGE_VERSION", GL_SHADING_LANGUAGE_VERSION);
  set_const(env, proto, "COMPRESSED_TEXTURE_FORMATS", GL_COMPRESSED_TEXTURE_FORMATS);
  set_const(env, proto, "CURRENT_PROGRAM", GL_CURRENT_PROGRAM);
  set_const(env, proto, "ARRAY_BUFFER_BINDING", GL_ARRAY_BUFFER_BINDING);
  set_const(env, proto, "ELEMENT_ARRAY_BUFFER_BINDING", GL_ELEMENT_ARRAY_BUFFER_BINDING);
  set_const(env, proto, "FRAMEBUFFER_BINDING", GL_FRAMEBUFFER_BINDING);
  set_const(env, proto, "RENDERBUFFER_BINDING", GL_RENDERBUFFER_BINDING);
  set_const(env, proto, "TEXTURE_BINDING_2D", GL_TEXTURE_BINDING_2D);
  set_const(env, proto, "TEXTURE_BINDING_CUBE_MAP", GL_TEXTURE_BINDING_CUBE_MAP);

  // Shader types
  set_const(env, proto, "VERTEX_SHADER", GL_VERTEX_SHADER);
  set_const(env, proto, "FRAGMENT_SHADER", GL_FRAGMENT_SHADER);
  set_const(env, proto, "COMPILE_STATUS", GL_COMPILE_STATUS);
  set_const(env, proto, "LINK_STATUS", GL_LINK_STATUS);
  set_const(env, proto, "DELETE_STATUS", GL_DELETE_STATUS);
  set_const(env, proto, "VALIDATE_STATUS", GL_VALIDATE_STATUS);
  set_const(env, proto, "ACTIVE_ATTRIBUTES", GL_ACTIVE_ATTRIBUTES);
  set_const(env, proto, "ACTIVE_UNIFORMS", GL_ACTIVE_UNIFORMS);

  // Precision types
  set_const(env, proto, "LOW_FLOAT", GL_LOW_FLOAT);
  set_const(env, proto, "MEDIUM_FLOAT", GL_MEDIUM_FLOAT);
  set_const(env, proto, "HIGH_FLOAT", GL_HIGH_FLOAT);
  set_const(env, proto, "LOW_INT", GL_LOW_INT);
  set_const(env, proto, "MEDIUM_INT", GL_MEDIUM_INT);
  set_const(env, proto, "HIGH_INT", GL_HIGH_INT);

  // Primitive types
  set_const(env, proto, "POINTS", GL_POINTS);
  set_const(env, proto, "LINES", GL_LINES);
  set_const(env, proto, "LINE_LOOP", GL_LINE_LOOP);
  set_const(env, proto, "LINE_STRIP", GL_LINE_STRIP);
  set_const(env, proto, "TRIANGLES", GL_TRIANGLES);
  set_const(env, proto, "TRIANGLE_STRIP", GL_TRIANGLE_STRIP);
  set_const(env, proto, "TRIANGLE_FAN", GL_TRIANGLE_FAN);

  // Data types
  set_const(env, proto, "BYTE", GL_BYTE);
  set_const(env, proto, "UNSIGNED_BYTE", GL_UNSIGNED_BYTE);
  set_const(env, proto, "SHORT", GL_SHORT);
  set_const(env, proto, "UNSIGNED_SHORT", GL_UNSIGNED_SHORT);
  set_const(env, proto, "INT", GL_INT);
  set_const(env, proto, "UNSIGNED_INT", GL_UNSIGNED_INT);
  set_const(env, proto, "FLOAT", GL_FLOAT);

  // Uniform types
  set_const(env, proto, "BOOL", GL_BOOL);
  set_const(env, proto, "BOOL_VEC2", GL_BOOL_VEC2);
  set_const(env, proto, "BOOL_VEC3", GL_BOOL_VEC3);
  set_const(env, proto, "BOOL_VEC4", GL_BOOL_VEC4);
  set_const(env, proto, "INT_VEC2", GL_INT_VEC2);
  set_const(env, proto, "INT_VEC3", GL_INT_VEC3);
  set_const(env, proto, "INT_VEC4", GL_INT_VEC4);
  set_const(env, proto, "FLOAT_VEC2", GL_FLOAT_VEC2);
  set_const(env, proto, "FLOAT_VEC3", GL_FLOAT_VEC3);
  set_const(env, proto, "FLOAT_VEC4", GL_FLOAT_VEC4);
  set_const(env, proto, "SAMPLER_2D", GL_SAMPLER_2D);
  set_const(env, proto, "SAMPLER_CUBE", GL_SAMPLER_CUBE);

  // Buffer usage
  set_const(env, proto, "STATIC_DRAW", GL_STATIC_DRAW);
  set_const(env, proto, "DYNAMIC_DRAW", GL_DYNAMIC_DRAW);
  set_const(env, proto, "STREAM_DRAW", GL_STREAM_DRAW);

  // Module-level helpers
  napi_value cleanup_fn;
  napi_create_function(env, "cleanup", NAPI_AUTO_LENGTH, cleanup, nullptr, &cleanup_fn);
  napi_set_named_property(env, exports, "cleanup", cleanup_fn);

  napi_value set_error_fn;
  napi_create_function(env, "setError", NAPI_AUTO_LENGTH, setError, nullptr, &set_error_fn);
  napi_set_named_property(env, exports, "setError", set_error_fn);

  return exports;
}

} // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, init)
