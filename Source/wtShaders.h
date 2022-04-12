#pragma once
#include "deviantPro.h"

static const char* waveTableVertexShader = R"(
#version 330 core
layout (location = 0) in vec4 position;
uniform mat4 matrix;
//layout (location = 1) in float next;
//out float nextY;
out vec4 fColour;
void main() {
    fColour = vec4(0.3, 0.9, 0.4, 0.3);
    gl_Position = matrix * position;
})";

static const char* waveTableGeometryShader = R"(
#version 330 core
layout (lines) in;
layout (triangle_strip, max_vertices = 24) out;
uniform mat4 matrix;
uniform vec3 colour;
in float nextY[];
out vec4 fColour;
//float lineWidthDivisor = 16.0;
//float lineHeight = 0.03;

void emit(float x, float y, float z) { gl_Position = matrix * vec4(x, y, z, 1.0); EmitVertex(); }
//void emit(vec3 v) { emit(v.x, v.y, v.z); }
//void emit(float x, float y) { emit(x, y, wtPosition); }
//void emit(vec2 v) { emit(v.x, v.y); }

void drawLine(vec4 left, vec4 right) {
    emit(left.x, left.y, left.z);
//    EmitVertex();
    emit(left.x, nextY[0], left.z);
//    EmitVertex();
    emit(right.x, right.y, right.z);
//    EmitVertex();
    emit(right.x, nextY[1], right.z);
//    EmitVertex();
    EndPrimitive();
}

void main() {
    fColour = vec4(colour);
    drawLine(gl_in[0].gl_Position, gl_in[1].gl_Position);
} )";

static const char* waveTableFragmentShader = R"(
#version 330 core
uniform vec3 colour;
out vec4 color;
void main() {
    color = vec4(colour.x, colour.y, colour.z, 1.0);
})";

static const char* wtActiveMoonVertexShader = R"(
#version 330 core
layout (location = 0) in vec4 position;

void main() {
    gl_Position = position;
})";

static const char* wtActiveMoonGeometryShader = R"(
#version 330 core
#define WAVE_RADIUS 0.026
#define WAVE_GIRTH_RESOLUTION 6
#define PI 3.1415926538
#define GIRTH_ANGLE_OFFSET (2.0 * PI / float (WAVE_GIRTH_RESOLUTION))
layout (triangles) in;
layout (triangle_strip, max_vertices = 128) out;
uniform vec3 colour;
uniform float wtPosition;
uniform mat4 matrix;
out vec4 fColour;
out vec2 texCoord;

void emit(float x, float y, float z) { texCoord.x = x/2.0 + 0.5; gl_Position = matrix * vec4(x, y, z, 1.0); EmitVertex(); }
void emit(vec3 v) { emit(v.x, v.y, v.z); }
void emit(float x, float y) { emit(x, y, wtPosition); }
void emit(vec2 v) { emit(v.x, v.y); }

void extrude (in vec3 sliceOrigin, in int girthIndex, out vec3 extrapolation) {
    float zOffset = WAVE_RADIUS * cos (-float (girthIndex) * GIRTH_ANGLE_OFFSET);
    float yOffset = WAVE_RADIUS * sin (-float (girthIndex) * GIRTH_ANGLE_OFFSET);
    extrapolation = vec3 (sliceOrigin.x, sliceOrigin.y + yOffset, sliceOrigin.z + zOffset);
}

void drawWrapSegment(vec3 left, vec3 right) {
    if (left.y != 0.0 || right.y != 0.0) {
        fColour = vec4(colour, 1.0);
        texCoord.y = 0.0;
        vec3 leftCorner = vec3(left.x, left.y, left.z + WAVE_RADIUS);
        vec3 rightCorner = vec3(right.x, right.y, right.z + WAVE_RADIUS);
        emit(leftCorner);
        for (int i = 1; i <= WAVE_GIRTH_RESOLUTION; ++i) {
            emit(rightCorner);
            texCoord.y = float(i)/float(WAVE_GIRTH_RESOLUTION);
            extrude(left, i, leftCorner);
            extrude(right, i, rightCorner);
            emit(leftCorner);
        }
        texCoord.y = 1.0;
        emit(rightCorner);
        EndPrimitive();
    }
}

void main() {
    vec3 left = vec3(gl_in[0].gl_Position.x, gl_in[0].gl_Position.y, wtPosition);
    vec3 right = vec3(gl_in[2].gl_Position.x, gl_in[2].gl_Position.y, wtPosition);
    drawWrapSegment(left, right);
} )";

static const char* wtActiveMoonFragmentShader = R"(
#version 330 core
in vec4 fColour;
uniform sampler2D demoTexture;
in vec2 texCoord;
out vec4 color;
void main() {
    vec4 t = fColour * texture (demoTexture, texCoord);
    color = vec4(t.rgb, fColour.a);
})";


/////////////////////////////////////////////////////////

static const char* wtActiveWaveVertexShader = R"(
#version 330 core
layout (location = 0) in vec4 position;

void main() {
    gl_Position = position;
})";

static const char* wtActiveWaveGeometryShader = R"(
#version 330 core
#define ALPHA 0.8
layout (triangles) in;
layout (triangle_strip, max_vertices = 24) out;
uniform vec3 colour;
//uniform float wtPosition;
uniform mat4 matrix;
float lineHeight, dispersionHeight = 0.002;
out vec4 fColour;

void emit(float x, float y, float z) { gl_Position = matrix * vec4(x, y, z, 1.0); EmitVertex(); }
void emit(vec3 v) { emit(v.x, v.y, v.z); }
void emit(float x, float y) { emit(x, y, 0.0); }
void emit(vec2 v) { emit(v.x, v.y); }

void drawStrip(vec2 left, vec2 right) {
    if (left.y != 0.0 || right.y != 0.0) {
        fColour = vec4(colour, ALPHA);
        emit(left);
        emit(right);
        fColour = vec4(0.0, 0.0, 0.0, 0.0);
        emit(left.x, 0.0);
        emit(right.x, 0.0);
        EndPrimitive();
    }
}

void drawLine(vec3 left, vec3 right) {
//    fColour = vec4(colour.x, colour.y,colour.z, 1.0);
//    emit(0.0, 0.0, 0.0);
    if (left.y != 0.0 || right.y != 0.0) {
        fColour = vec4(colour, 1.0);
        emit(left.x, left.y + lineHeight, left.z);
        emit(left.x, left.y - lineHeight, left.z);
        emit(right.x, right.y + lineHeight, right.z);
        emit(right.x, right.y - lineHeight, right.z);
        EndPrimitive();
        
        vec4 whiterColour = vec4(min(colour.r + 0.3, 1.0), min(colour.g + 0.3, 1.0), min(colour.b + 0.3, 1.0), 1.0);
        
        fColour = vec4(0.0, 0.0, 0.0, 0.0);
        emit(left.x, left.y + lineHeight + dispersionHeight, left.z);
        emit(right.x, right.y + lineHeight + dispersionHeight, right.z);
        fColour = whiterColour;
        emit(left.x, left.y + lineHeight, left.z);
        emit(right.x, right.y + lineHeight, right.z);
        EndPrimitive();
        
        emit(left.x, left.y - lineHeight, left.z);
        emit(right.x, right.y - lineHeight, right.z);
        fColour = vec4(0.0, 0.0, 0.0, 0.0);
        emit(left.x, (left.y - lineHeight) - dispersionHeight, left.z);
        emit(right.x, (right.y - lineHeight) - dispersionHeight, right.z);
        EndPrimitive();
    }
}

void main() {
    lineHeight = 0.002;
    vec3 left = gl_in[0].gl_Position.xyz;
    vec3 right = gl_in[2].gl_Position.xyz;
    drawLine(left, right);
} )";

static const char* wtActiveWaveFragmentShader = R"(
#version 330 core
in vec4 fColour;
out vec4 color;
void main() {
    color = fColour;
})";
