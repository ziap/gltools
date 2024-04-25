# OpenGL tools

Some of my personal tools for working with OpenGL (ES). Most of it are for
generating header files at compile time.

## Shader to header compiler

Loads and compiles GLSL ES shaders in an EGL context. It could catches some
basic errors such as syntax error at compile time. The generated header files
contain a single string representing the shader source.

Dependencies:
- EGL
- GLESv2

## In-progress tools

- [ ] Image loading
- [ ] Audio loading
- [ ] Font/Texture atlas generation
