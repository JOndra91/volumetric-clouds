#version 430

in vec2 position;

out vec2 textureCoords;

void main() {
    gl_Position = vec4(position, 0.0, 1.0);
    textureCoords = (position + 1.0) / 2.0;
}
