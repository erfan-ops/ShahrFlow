#version 330 core

uniform float halfWidth;
uniform float halfHeight;

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;

out vec4 vColor;

void main() {
    gl_Position = vec4(aPos.x / halfWidth - 1.0, aPos.y / halfHeight - 1.0, 0.0, 1.0);
    vColor = aColor;
}