#define NOMINMAX
#include <windows.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>

#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <functional>
#include <random>
#include <vector>
#include <memory>

#include "settings.h"
#include "desktopUtils.h"
#include "trayUtils.h"
#include "utils.h"


// --- Random engine (single global engine, seeded once) ---
static std::random_device rd_global;
static std::mt19937 gen_global(rd_global());

static float randomUniformGlobal(float start, float end) {
    std::uniform_real_distribution<> dist(start, end);
    return static_cast<float>(dist(gen_global));
}

// main window
GLFWwindow* window;

// handles tray events (unchanged)
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_TRAYICON) {
        if (lParam == WM_RBUTTONUP) {
            HMENU menu = CreatePopupMenu();
            AppendMenu(menu, MF_STRING, 1, L"Quit");
            POINT cursorPos;
            GetCursorPos(&cursorPos);
            SetForegroundWindow(hwnd);
            int selection = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, cursorPos.x - 120, cursorPos.y - 22, 0, hwnd, NULL);
            DestroyMenu(menu);

            if (selection == 1) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
        }
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Vertex structure (same as your original)
struct Vertex {
    float x;
    float y;

    float r;
    float g;
    float b;
    float a;

    Vertex(float x, float y) : x(x), y(y), r(0.0f), g(0.0f), b(0.0f), a(0.0f) {}
    Vertex(float x, float y, float r, float g, float b, float a) : x(x), y(y), r(r), g(g), b(b), a(a) {}
    Vertex(float x, float y, Color color) : x(x), y(y), r(color[0]), g(color[1]), b(color[2]), a(color[3]) {}
};

// Edge storage for dynamic outlines
struct EdgeData {
    glm::vec2 p1;
    glm::vec2 p2;
    Color color;
    float width;
};

// Helper: point-to-segment distance (kept as lambda originally)
static float pointToSegmentDist(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    glm::vec2 ab = b - a;
    glm::vec2 ap = p - a;
    float denom = glm::dot(ab, ab);
    if (denom == 0.0f) return glm::length(p - a);
    float t = glm::dot(ap, ab) / denom;
    t = glm::clamp(t, 0.0f, 1.0f);
    glm::vec2 closest = a + t * ab;
    return glm::length(p - closest);
}

int main() {
    // ---------- GLFW / GL init ----------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Request OpenGL 3.3 Core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    const int iWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int iHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const float Width = static_cast<float>(iWidth);
    const float Height = static_cast<float>(iHeight);
    const float HalfWidth = Width * 0.5f;
    const float HalfHeight = Height * 0.5f;

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

    Settings settings = loadSettings("settings.json");

    // using multi-sample anti-aliasing
    glfwWindowHint(GLFW_SAMPLES, settings.MSAA);

    window = glfwCreateWindow(iWidth, iHeight, "Delaunay Flow", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }

    HWND hwnd = glfwGetWin32Window(window);
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WindowProc);

    using GameTickFunc = void(*)(const float, const float, float&);
    GameTickFunc tickFunc;

    glfwMakeContextCurrent(window);

    if (settings.vsync) {
        glfwSwapInterval(1);
        tickFunc = [](const float, const float, float&) {};
    } else {
        glfwSwapInterval(0);
        tickFunc = [](const float frameTime, const float stepInterval, float& fractionalTime) {
            if (frameTime < stepInterval) {
                float totalSleepTime = (stepInterval - frameTime) + fractionalTime;
                int sleepMilliseconds = static_cast<int>(totalSleepTime * 1e+3f);
                fractionalTime = (totalSleepTime - sleepMilliseconds * 1e-3f);
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepMilliseconds));
            }
        };
    }

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    bool waveActive = false;
    float waveStartTime = 0.0f;
    float waveTravelDistance = Width + settings.wave.width;
    float waveDuration = waveTravelDistance / settings.wave.speed;
    float waveInterval = settings.wave.interval;

    // ---------- mouse ----------
    double mouseX = 0.0, mouseY = 0.0;

    // ---------- geometry storage ----------
    std::vector<Vertex> triangleVertices; // static: generated once at startup (fills)
    std::vector<EdgeData> edges;          // list of edges for outlines (positions/color/width)
    std::vector<Vertex> outlineVertices;  // dynamic: regenerated each frame (outlines)

    // ---------- helpers to add geometry ----------
    auto addTriangleStatic = [&](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, Color color) {
        triangleVertices.emplace_back(p1.x, p1.y, color[0], color[1], color[2], color[3]);
        triangleVertices.emplace_back(p2.x, p2.y, color[0], color[1], color[2], color[3]);
        triangleVertices.emplace_back(p3.x, p3.y, color[0], color[1], color[2], color[3]);
    };

    auto pushEdgeForDynamic = [&](const glm::vec2& p1, const glm::vec2& p2, Color color, float width) {
        EdgeData e;
        e.p1 = p1; e.p2 = p2; e.width = width;
        e.color[0] = color[0]; e.color[1] = color[1]; e.color[2] = color[2]; e.color[3] = color[3];
        edges.push_back(e);
    };

    // dynamic outline creation for a single edge (uses current mouse)
    auto addEdgeDynamicToOutlineVertices = [&](const glm::vec2& p1, const glm::vec2& p2, Color color, float width, double curMouseX, double curMouseY, float glfwTime) {
        glm::vec2 edge = glm::normalize(p2 - p1);
        glm::vec2 normal(-edge.y, edge.x);
        glm::vec2 offset = normal * (width * 0.5f);

        glm::vec2 q1 = p1 + offset;
        glm::vec2 q2 = p2 + offset;
        glm::vec2 q3 = p2 - offset;
        glm::vec2 q4 = p1 - offset;

        glm::vec2 mouse(static_cast<float>(curMouseX), Height - static_cast<float>(curMouseY));
        float dist = pointToSegmentDist(mouse, p1, p2);
        float alpha = 0.0f;
        if (settings.barrier.reverse) {
            if (dist > settings.barrier.radius + settings.barrier.fadeArea) {
                alpha = color[3];
            } else if (dist > settings.barrier.radius) {
                alpha = ((dist - settings.barrier.radius) / settings.barrier.fadeArea) * color[3];
            }
        }
        else {
            if (dist < settings.barrier.radius) {
                alpha = (1.0f - dist / settings.barrier.radius) * color[3];
            }
        }
        

        glm::vec2 midpoint = (p1 + p2) * 0.5f;

        // Wave effect
        if (waveActive) {
            float waveProgress = (glfwTime - waveStartTime) / waveDuration;
            float waveX = -settings.wave.width * 0.5f + waveProgress * (Width + settings.wave.width);

            float distToWave = fabs(midpoint.x - waveX);
            float waveThickness = settings.wave.width * 0.5f;

            if (distToWave < waveThickness) {
                float factor = 1.0f - (distToWave / waveThickness);
                factor = glm::clamp(factor, 0.0f, 1.0f);

                float wAlpha = settings.wave.color[3] * factor; // wave alpha
                float bAlpha = alpha;                           // barrier alpha
                alpha = 1 - (1 - bAlpha) * (1 - wAlpha);
                color[0] = (color[0] * bAlpha / alpha) + (settings.wave.color[0] * wAlpha * (1 - bAlpha) / alpha);
                color[1] = (color[1] * bAlpha / alpha) + (settings.wave.color[1] * wAlpha * (1 - bAlpha) / alpha);
                color[2] = (color[2] * bAlpha / alpha) + (settings.wave.color[2] * wAlpha * (1 - bAlpha) / alpha);
            }
        }

        // First triangle
        outlineVertices.emplace_back(q1.x, q1.y, color[0], color[1], color[2], alpha);
        outlineVertices.emplace_back(q2.x, q2.y, color[0], color[1], color[2], alpha);
        outlineVertices.emplace_back(q3.x, q3.y, color[0], color[1], color[2], alpha);

        // Second triangle
        outlineVertices.emplace_back(q1.x, q1.y, color[0], color[1], color[2], alpha);
        outlineVertices.emplace_back(q3.x, q3.y, color[0], color[1], color[2], alpha);
        outlineVertices.emplace_back(q4.x, q4.y, color[0], color[1], color[2], alpha);
    };

    // This builds the triangles (with randomization applied once) and stores the edges for outlines.
    auto insertHexagonsInit = [&]() -> void {
        float hexagonWidth = 1.7320508075688772f * settings.hexagonSize;
        float hexagonSliceWidth = 0.8660254037844386f * settings.hexagonSize;
        float hexagonHeight = 2.0f * settings.hexagonSize;
        float hexagonYDis = 1.5f * settings.hexagonSize;
        float hexagonHalfSize = 0.5f * settings.hexagonSize;
        int hexagonsInWidth = static_cast<int>(Width / hexagonWidth) + 2;
        int hexagonsInHeight = static_cast<int>(Height / hexagonYDis) + 1;

        triangleVertices.reserve(hexagonsInHeight * hexagonsInWidth * 6 * 3); // 6 triangles per hexagon, 3 verts each
        edges.reserve(hexagonsInHeight * hexagonsInWidth * 6 * 3); // 3 edges per triangle -> safe reserve

        for (int iy = 0; iy <= hexagonsInHeight; iy++) {
            float y = iy * hexagonYDis;
            for (float x = (iy % 2 ? 0.0f : hexagonSliceWidth); x <= Width + hexagonWidth; x += hexagonWidth) {
                glm::vec2 c(x, y);
                glm::vec2 top(x, y + settings.hexagonSize);
                glm::vec2 bottom(x, y - settings.hexagonSize);
                glm::vec2 leftTop(x - hexagonSliceWidth, y + hexagonHalfSize);
                glm::vec2 rightTop(x + hexagonSliceWidth, y + hexagonHalfSize);
                glm::vec2 leftBottom(x - hexagonSliceWidth, y - hexagonHalfSize);
                glm::vec2 rightBottom(x + hexagonSliceWidth, y - hexagonHalfSize);

                Color fill1 = {0.898f, 0.243f, 0.243f, 1.0f};
                Color fill2 = {0.773f, 0.188f, 0.188f, 1.0f};
                Color fill3 = {0.455f, 0.165f, 0.165f, 1.0f};

                // For each of the 6 triangles: compute random once and add static triangle + push edges
                auto addTriWithOneTimeRandom = [&](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, Color baseFill) {
                    float triangleY = (p1.y + p2.y + p3.y) / 3.0f;
                    float normalizedY = triangleY / Height;
                    // float probability = normalizedY;
                    float probability = pow(normalizedY, 2.0f);
                    // float probability = 1.0f / (1.0f + exp(-10.0f * (normalizedY - 0.5f)));
                    Color fill = baseFill;
                    if (randomUniformGlobal(0.0f, 1.0f) < probability) {
                        fill = {1.0f, 1.0f, 1.0f, 1.0f};
                    }
                    addTriangleStatic(p1, p2, p3, fill);

                    // store edges so outlines can be generated each frame
                    pushEdgeForDynamic(p1, p2, settings.edges.color, settings.edges.width);
                    pushEdgeForDynamic(p2, p3, settings.edges.color, settings.edges.width);
                    pushEdgeForDynamic(p3, p1, settings.edges.color, settings.edges.width);
                };

                addTriWithOneTimeRandom(c, top, leftTop, fill1);
                addTriWithOneTimeRandom(c, top, rightTop, fill1);
                addTriWithOneTimeRandom(c, leftTop, leftBottom, fill2);
                addTriWithOneTimeRandom(c, rightTop, rightBottom, fill3);
                addTriWithOneTimeRandom(c, bottom, leftBottom, fill2);
                addTriWithOneTimeRandom(c, bottom, rightBottom, fill3);
            }
        }
    };

    // ---------- Create VAOs / VBOs ----------
    GLuint staticVAO = 0, staticVBO = 0;
    GLuint dynamicVAO = 0, dynamicVBO = 0;

    glGenVertexArrays(1, &staticVAO);
    glGenBuffers(1, &staticVBO);

    glGenVertexArrays(1, &dynamicVAO);
    glGenBuffers(1, &dynamicVBO);

    // ---------- Build static geometry (triangles) once ----------
    insertHexagonsInit(); // fills triangleVertices and edges (one-time)

    // Bind static VAO & VBO and upload triangle data
    glBindVertexArray(staticVAO);
    glBindBuffer(GL_ARRAY_BUFFER, staticVBO);
    if (!triangleVertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER,
                     triangleVertices.size() * sizeof(Vertex),
                     triangleVertices.data(),
                     GL_STATIC_DRAW);
    } else {
        // ensure there's at least an empty buffer
        glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STATIC_DRAW);
    }

    // layout: position (location 0) vec2
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // layout: color (location 1) vec4
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // Setup dynamic VAO (for outlines). We'll upload data each frame to dynamicVBO.
    glBindVertexArray(dynamicVAO);
    glBindBuffer(GL_ARRAY_BUFFER, dynamicVBO);
    // allocate small initial size; we'll reallocate with glBufferData each frame
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);

    // ---------- compile shaders ----------
    GLuint shaderProgram = shaderUtils::compileShaders("shaders/vertex.glsl", "shaders/fragment.glsl");
    if (shaderProgram == 0) {
        std::cerr << "Failed to compile shaders!" << std::endl;
        return -1;
    }

    GLint halfWidthLocation  = glGetUniformLocation(shaderProgram, "halfWidth");
    GLint halfHeightLocation = glGetUniformLocation(shaderProgram, "halfHeight");

    // frame timing
    const float stepInterval = 1.0f / settings.targetFPS;
    float dt{0};
    float fractionalTime{0};

    // app icon, tray, wallpaper setup (same as original)
    HICON hIcon = LoadIconFromResource();
    AddTrayIcon(hwnd, hIcon, L"Just a Simple Icon");
    std::unique_ptr<wchar_t[]> originalWallpaper(GetCurrentWallpaper());
    SetAsDesktop(hwnd);
    glfwShowWindow(window);

    auto newF = std::chrono::high_resolution_clock::now();
    auto oldF = std::chrono::high_resolution_clock::now();

    // ---------- Main loop ----------
    while (!glfwWindowShouldClose(window)) {
        oldF = newF;
        newF = std::chrono::high_resolution_clock::now();
        dt = std::chrono::duration<float>(newF - oldF).count();

        float glfwTime = static_cast<float>(glfwGetTime()); // or your timer system

        // Start a new wave every interval
        if (!waveActive && fmod(glfwTime, waveInterval) < dt) {
            waveActive = true;
            waveStartTime = glfwTime;
        }

        // Check if current wave finished
        if (waveActive && (glfwTime - waveStartTime) > waveDuration) {
            waveActive = false;
        }

        glfwGetCursorPos(window, &mouseX, &mouseY);

        glClearColor(1.f, 1.f, 1.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        // --- Build outlines based on edges and current mouse (only this happens per-frame) ---
        outlineVertices.clear();
        outlineVertices.reserve(edges.size() * 6); // each edge -> 6 vertices (two tris)

        for (const EdgeData& e : edges) {
            addEdgeDynamicToOutlineVertices(e.p1, e.p2, e.color, e.width, mouseX, mouseY, glfwTime);
        }

        // upload outlineVertices to dynamic VBO
        glBindBuffer(GL_ARRAY_BUFFER, dynamicVBO);
        if (!outlineVertices.empty()) {
            glBufferData(GL_ARRAY_BUFFER,
                         outlineVertices.size() * sizeof(Vertex),
                         outlineVertices.data(),
                         GL_DYNAMIC_DRAW);
        } else {
            // keep at least a tiny buffer to avoid undefined behaviour
            glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);
        }

        // use shader and set uniforms
        glUseProgram(shaderProgram);
        glUniform1f(halfWidthLocation, HalfWidth);
        glUniform1f(halfHeightLocation, HalfHeight);

        // draw static triangles (fills)
        glBindVertexArray(staticVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangleVertices.size()));
        glBindVertexArray(0);

        // draw outlines (dynamic)
        glBindVertexArray(dynamicVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(outlineVertices.size()));
        glBindVertexArray(0);

        glUseProgram(0);

        glfwSwapBuffers(window);
        glfwPollEvents();

        tickFunc(dt, stepInterval, fractionalTime);
    }

    // ---------- cleanup & restore wallpaper ----------
    SetParent(hwnd, nullptr);
    SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, static_cast<PVOID>(originalWallpaper.get()), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    RemoveTrayIcon(hwnd);
    DestroyIcon(hIcon);

    glDeleteProgram(shaderProgram);

    glDeleteVertexArrays(1, &staticVAO);
    glDeleteBuffers(1, &staticVBO);
    glDeleteVertexArrays(1, &dynamicVAO);
    glDeleteBuffers(1, &dynamicVBO);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
