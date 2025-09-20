#version 330 core

// Barrier settings uniforms
uniform float barrierRadius;
uniform float fadeArea;
uniform bool reverseMode;

// Wave effect uniforms
uniform float waveProgress;
uniform float waveX;
uniform float waveWidth;
uniform vec4 waveColor;

in vec4 vColor;
in vec2 vEdgeP1;
in vec2 vEdgeP2;
in vec2 vMousePos;

out vec4 FragColor;

// Point to segment distance function
float pointToSegmentDistance(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    vec2 ap = p - a;
    float denom = dot(ab, ab);
    if (denom == 0.0) return length(p - a);
    float t = dot(ap, ab) / denom;
    t = clamp(t, 0.0, 1.0);
    vec2 closest = a + t * ab;
    return length(p - closest);
}

void main() {
    // Calculate distance from mouse to edge
    float dist = pointToSegmentDistance(vMousePos, vEdgeP1, vEdgeP2);
    
    // Calculate alpha based on mouse barrier settings
    float alpha = 0.0;
    
    if (reverseMode) {
        if (dist > barrierRadius + fadeArea) {
            alpha = vColor.a;
        } else if (dist > barrierRadius) {
            alpha = ((dist - barrierRadius) / fadeArea) * vColor.a;
        }
    } else {
        if (dist < barrierRadius) {
            alpha = (1.0 - dist / barrierRadius) * vColor.a;
        }
    }
    
    // Wave effect calculation (only if wave is active)
    if (waveProgress >= 0.0) {
        vec2 midpoint = (vEdgeP1 + vEdgeP2) * 0.5;
        float distToWave = abs(midpoint.x - waveX);
        float waveThickness = waveWidth * 0.5;
        
        if (distToWave < waveThickness) {
            float factor = 1.0 - (distToWave / waveThickness);
            factor = clamp(factor, 0.0, 1.0);
            
            float wAlpha = waveColor.a * factor;
            float bAlpha = alpha;
            alpha = 1.0 - (1.0 - bAlpha) * (1.0 - wAlpha);
            
            vec3 finalColor = (vColor.rgb * bAlpha / alpha) + (waveColor.rgb * wAlpha * (1.0 - bAlpha) / alpha);
            FragColor = vec4(finalColor, alpha);
            return;
        }
    }
    
    FragColor = vec4(vColor.rgb, alpha);
}
