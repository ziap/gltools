#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __FILE_NAME__
  #define ASSERT_FILE __FILE_NAME__
#else
  #define ASSERT_FILE __FILE__
#endif

#define ASSERT_STR_EXPAND(x) ASSERT_STR(x)
#define ASSERT_STR(x) #x
#define ASSERT_LINE ASSERT_STR_EXPAND(__LINE__)

#define ASSERT_LOC ASSERT_FILE ":" ASSERT_LINE ": "

#define ASSERT_LOG(str) fwrite(str, sizeof(str) - 1, 1, stderr)
#define ASSERT_TRAP() __asm__ volatile("int3")

#define ASSERT(x)                                                              \
  do {                                                                         \
    if ((x)) {                                                                 \
    } else {                                                                   \
      ASSERT_LOG(ASSERT_LOC "Assertion `" #x "` failed\n");                    \
      ASSERT_TRAP();                                                           \
    }                                                                          \
  } while (0)

#define ARENA_CAP (1 << 16)
typedef struct ArenaNode ArenaNode;
struct ArenaNode {
  size_t len;
  ArenaNode *prev;
  char data[];
};

typedef struct {
  ArenaNode *free_arenas;
  ArenaNode *current;
  ArenaNode *last;
} Arena;

Arena Arena_create(void) {
  ArenaNode *node = malloc(sizeof(ArenaNode) + ARENA_CAP);
  node->len = 0;
  node->prev = NULL;
  return (Arena) {
    .free_arenas = NULL,
    .current = node,
    .last    = node
  };
}

void Arena_deinit(Arena arena) {
  arena.last->prev = arena.free_arenas;
  ArenaNode *node = arena.current;
  while (node) {
    ArenaNode *prev = node->prev;
    free(node);
    node = prev;
  }
}

void Arena_reset(Arena *arena) {
  if (arena->current != arena->last) {
    arena->last->prev = arena->free_arenas;
    arena->free_arenas = arena->current->prev;
    arena->current->prev = NULL;
    arena->last = arena->current;
  }
  arena->current->len = 0;
}

void Arena_expand(Arena *arena) {
  ArenaNode *node;
  if (!arena->free_arenas) {
    node = malloc(sizeof(ArenaNode) + ARENA_CAP);
  } else {
    node = arena->free_arenas;
    arena->free_arenas = arena->free_arenas->prev;
  }

  node->len = 0;
  node->prev = arena->current;
  arena->current = node;
}

char *Arena_alloc_str(Arena *arena, size_t size) {
  if (size > ARENA_CAP) {
    ArenaNode *node = malloc(sizeof(ArenaNode) + size);
    node->len = size;
    node->prev = arena->current;
    arena->current = node;
    return node->data;
  }
  if (arena->current->len + size > ARENA_CAP) Arena_expand(arena);
  char *data = arena->current->data + arena->current->len;
  arena->current->len += size;
  return data;
}

// Do this to make it easier to add more types, e.g compute or geometry
// But currently it's not possible because this is GLES2
typedef enum {
  VERTEX,
  FRAGMENT,
  SHADER_TYPE_LEN
} ShaderType;

const char *shader_type_name[SHADER_TYPE_LEN] = {
  [VERTEX] = "vertex",
  [FRAGMENT] = "fragment"
};

// Change to vs and fs if you want, I'm sticking with vert and frag
#define SHADER_EXT_LEN 4
const char shader_type_ext[SHADER_TYPE_LEN][SHADER_EXT_LEN + 1] = {
  [VERTEX] = "vert",
  [FRAGMENT] = "frag"
};

GLenum shader_type_ogl[SHADER_TYPE_LEN] = {
  [VERTEX] = GL_VERTEX_SHADER,
  [FRAGMENT] = GL_FRAGMENT_SHADER
};

ShaderType get_type_from_ext(const char *path, size_t len) {
  if (len < (SHADER_EXT_LEN + 1)) return -1;
  const char *ext = path + len - SHADER_EXT_LEN - 1;

  if (*ext != '.') return -1;
  ++ext;

  for (ShaderType i = 0; i < SHADER_TYPE_LEN; ++i) {
    if (!memcmp(ext, shader_type_ext[i], SHADER_EXT_LEN)) return i;
  }

  return -1;
}

char* read_file(const char *file_path, Arena *arena) {
  char *buf = NULL;
  FILE *f = fopen(file_path, "rb");

  if (!f) goto fail;

  if (fseek(f, 0, SEEK_END) < 0) goto fail;

  int len = ftell(f);
  if (len < 0) goto fail;

  if (fseek(f, 0, SEEK_SET) < 0) goto fail;

  buf = Arena_alloc_str(arena, len + 1);

  fread(buf, 1, len, f);
  if (ferror(f)) goto fail;

  buf[len] = 0;
  fclose(f);

  return buf;
fail:
  if (errno) {
    fprintf(stderr, "ERROR: Failed to read file `%s`: %s\n", file_path,
            strerror(errno));
  }

  if (f) fclose(f);
  return NULL;
}

int process_shader(const char *name, Arena *arena) {
  size_t len = strlen(name);

  ShaderType shader_type = get_type_from_ext(name, len);
  if (shader_type == (ShaderType)-1) {
    fprintf(stderr, "ERROR: File `%s` not supported\n", name);
    fprintf(stderr, "Supported file types:\n");
    for (ShaderType i = 0; i < SHADER_TYPE_LEN; ++i) {
      fprintf(stderr, "  `.%s` for %s shader\n", shader_type_ext[i],
              shader_type_name[i]);
    }
    return 1;
  }

  char *shader_str = read_file(name, arena);
  if (!shader_str) return 1;

  fprintf(stderr, "INFO: Loaded %s shader `%s`\n",
          shader_type_name[shader_type], name);

  int failed = 0;

  GLuint shader = glCreateShader(shader_type_ogl[shader_type]);
  ASSERT(shader);

  glShaderSource(shader, 1, (const char**)&shader_str, NULL);
  glCompileShader(shader);

  GLint compiled;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLint max_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &max_length);

    char *error_msg = Arena_alloc_str(arena, max_length);
    glGetShaderInfoLog(shader, max_length, &max_length, error_msg);

    // Insert the file name to every line of the error message and print them
    const char* last_newline = error_msg;
    for (const char *c = error_msg; *c; ++c) {
      if (*c == '\n') {
        fwrite(name, len, 1, stderr);
        fwrite(last_newline + 1, c - last_newline, 1, stderr);
        last_newline = c + 1;
      }
    }
    failed = 1;
    goto cleanup;
  }

  const char ext[] = ".h";
  char *out_file = Arena_alloc_str(arena, len + sizeof(ext));
  memcpy(out_file, name, len);
  memcpy(out_file + len, ext, sizeof(ext));

  FILE *out = fopen(out_file, "w");
  if (!out) {
    fprintf(stderr, "ERROR: Failed to open file `%s`: %s\n", out_file,
            strerror(errno));
    failed = 1;
    goto cleanup;
  }

  char *var_name = Arena_alloc_str(arena, len + 1);
  char *header_name = Arena_alloc_str(arena, len + 1);
  for (size_t i = 0; i < len; ++i) {
    char ch = name[i];
    if (ch >= '0' && ch <= '9') {
      if (i == 0) {
        fprintf(stderr, "WARN: The first character of `%s` is a number, "
                "it is replaced with `_` in the output header", name);
        var_name[i] = '_';
        header_name[i] = '_';
      } else {
        var_name[i] = ch;
        header_name[i] = ch;
      }
    } else if (ch >= 'a' && ch <= 'z') {
      var_name[i] = ch;
      header_name[i] = ch + 'A' - 'a';
    } else if (ch >= 'A' && ch <= 'Z') {
      var_name[i] = ch + 'a' - 'A';
      header_name[i] = ch;
    } else {
      header_name[i] = '_';
      var_name[i] = '_';
    }
  }
  var_name[len] = '\0';
  header_name[len] = '\0';

  fprintf(out, "#ifndef %s_H\n", header_name);
  fprintf(out, "#define %s_H\n", header_name);
  fprintf(out, "\n");

  fprintf(out, "const char %s[] = {\n  ", var_name);

  for (char *c = shader_str; *c; ++c) {
    fprintf(out, "%#02x, ", *c);
  }
  fprintf(out, "0x0\n"); // null-terminator

  fprintf(out, "};\n");
  fprintf(out, "\n");

  fprintf(out, "#endif\n");

  fprintf(stderr, "INFO: Shader written to file `%s`\n", out_file);
  fclose(out);

cleanup:
  glDeleteShader(shader);
  return failed;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(stderr, "ERROR: No input file\n");
    return 1;
  }

  // Initialize EGL
  EGLDisplay eglDpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  EGLint major, minor;
  ASSERT(eglInitialize(eglDpy, &major, &minor));
  ASSERT(eglDpy != EGL_NO_DISPLAY);

  // Bind the API
  ASSERT(eglBindAPI(EGL_OPENGL_API));

  // Create a context and make it current
  EGLContext eglCtx = eglCreateContext(eglDpy, NULL, EGL_NO_CONTEXT, NULL);
  ASSERT(eglCtx != EGL_NO_CONTEXT);
  ASSERT(eglMakeCurrent(eglDpy, EGL_NO_SURFACE, EGL_NO_SURFACE, eglCtx));

  int failed = 0;

  Arena arena = Arena_create();

  for (int i = 1; i < argc; ++i) {
    if (process_shader(argv[i], &arena)) {
      failed = 1;
      break;
    }

    Arena_reset(&arena);
  }

  Arena_deinit(arena);

  ASSERT(eglDestroyContext(eglDpy, eglCtx));
  ASSERT(eglTerminate(eglDpy));
  return failed;
}
