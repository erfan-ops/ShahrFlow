#version 330 core

uniform float halfWidth;
uniform float halfHeight;

// Edge data uniforms
uniform vec2 mousePos;
uniform float edgeWidth;
uniform float barrierRadius;
uniform float fadeArea;
uniform bool reverseMode;

// Vertex attributes for edge rendering
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec2 edgeP1;  // First point of the edge
layout (location = 3) in vec2 edgeP2;  // Second point of the edge

out vec4 vColor;
out vec2 vEdgeP1;
out vec2 vEdgeP2;
out vec2 vMousePos;

void main() {
    gl_Position = vec4(aPos.x / halfWidth - 1.0, aPos.y / halfHeight - 1.0, 0.0, 1.0);
    vColor = aColor;
    vEdgeP1 = edgeP1;
    vEdgeP2 = edgeP2;
    vMousePos = mousePos;
}