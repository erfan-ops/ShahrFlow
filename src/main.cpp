// LEGITIMATE USE: This application provides visual effects
// for desktop enhancement. No malicious intent.


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

// Vertex structure for static geometry (triangles)
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

// Vertex structure for dynamic edge geometry
struct EdgeVertex {
    float x;
    float y;
    float r;
    float g;
    float b;
    float a;
    float edgeP1_x;
    float edgeP1_y;
    float edgeP2_x;
    float edgeP2_y;

    EdgeVertex(float x, float y, Color color, const glm::vec2& p1, const glm::vec2& p2) 
        : x(x), y(y), r(color[0]), g(color[1]), b(color[2]), a(color[3]),
          edgeP1_x(p1.x), edgeP1_y(p1.y), edgeP2_x(p2.x), edgeP2_y(p2.y) {}
};

// Note: Edge data now stored as vertex attributes in EdgeVertex struct

// Note: point-to-segment distance calculation moved to GPU shaders

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

    window = glfwCreateWindow(iWidth, iHeight, "ShahrFlow", nullptr, nullptr);
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
    std::vector<EdgeVertex> edgeVertices; // static: edge geometry with edge data (generated once)

    // ---------- helpers to add geometry ----------
    auto addTriangleStatic = [&](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, Color color) {
        triangleVertices.emplace_back(p1.x, p1.y, color[0], color[1], color[2], color[3]);
        triangleVertices.emplace_back(p2.x, p2.y, color[0], color[1], color[2], color[3]);
        triangleVertices.emplace_back(p3.x, p3.y, color[0], color[1], color[2], color[3]);
    };

    // Helper to generate edge geometry with edge data stored as vertex attributes
    auto addEdgeGeometry = [&](const glm::vec2& p1, const glm::vec2& p2, Color color, float width) {
        glm::vec2 edge = glm::normalize(p2 - p1);
        glm::vec2 normal(-edge.y, edge.x);
        glm::vec2 offset = normal * (width * 0.5f);

        glm::vec2 q1 = p1 + offset;
        glm::vec2 q2 = p2 + offset;
        glm::vec2 q3 = p2 - offset;
        glm::vec2 q4 = p1 - offset;

        // First triangle
        edgeVertices.emplace_back(q1.x, q1.y, color, p1, p2);
        edgeVertices.emplace_back(q2.x, q2.y, color, p1, p2);
        edgeVertices.emplace_back(q3.x, q3.y, color, p1, p2);

        // Second triangle
        edgeVertices.emplace_back(q1.x, q1.y, color, p1, p2);
        edgeVertices.emplace_back(q3.x, q3.y, color, p1, p2);
        edgeVertices.emplace_back(q4.x, q4.y, color, p1, p2);
    };

    // Note: Wave effects will be handled in shaders as well

    // This builds the triangles (with randomization applied once) and stores the edges for outlines.
    auto insertHexagonsInit = [&]() -> void {
        const float hexagonWidth = 1.7320508075688772f * settings.hexagonSize;
        const float hexagonSliceWidth = 0.8660254037844386f * settings.hexagonSize;
        const float hexagonHeight = 2.0f * settings.hexagonSize;
        const float hexagonYDis = 1.5f * settings.hexagonSize;
        const float hexagonHalfSize = 0.5f * settings.hexagonSize;
        const int hexagonsInWidth = static_cast<int>(Width / hexagonWidth) + 2;
        const int hexagonsInHeight = static_cast<int>(Height / hexagonYDis) + 1;

        triangleVertices.reserve(hexagonsInHeight * hexagonsInWidth * 18); // 6 triangles per hexagon, 3 verts each
        edgeVertices.reserve(hexagonsInHeight * hexagonsInWidth * 108); // 3 edges per triangle, 6 verts per edge

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

                // For each of the 6 triangles: compute random once and add static triangle + push edges
                auto addTriWithOneTimeRandom = [&](const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, Color baseFill) {
                    float triangleY = (p1.y + p2.y + p3.y) / 3.0f;
                    float normalizedY = triangleY / Height;
                    // float probability = normalizedY;
                    float probability = pow(normalizedY, 2.0f);
                    // float probability = 1.0f / (1.0f + exp(-10.0f * (normalizedY - 0.5f)));
                    Color fill = baseFill;
                    if (randomUniformGlobal(0.0f, 1.0f) < probability) {
                        fill = {0.0f, 0.0f, 0.0f, 0.0f};
                    }
                    addTriangleStatic(p1, p2, p3, fill);

                    // generate edge geometry once (with edge data as vertex attributes)
                    addEdgeGeometry(p1, p2, settings.edges.color, settings.edges.width);
                    addEdgeGeometry(p2, p3, settings.edges.color, settings.edges.width);
                    addEdgeGeometry(p3, p1, settings.edges.color, settings.edges.width);
                };

                addTriWithOneTimeRandom(c, top, leftTop, settings.cube.topColor);
                addTriWithOneTimeRandom(c, top, rightTop, settings.cube.topColor);
                addTriWithOneTimeRandom(c, leftTop, leftBottom, settings.cube.leftColor);
                addTriWithOneTimeRandom(c, rightTop, rightBottom, settings.cube.rightColor);
                addTriWithOneTimeRandom(c, bottom, leftBottom, settings.cube.leftColor);
                addTriWithOneTimeRandom(c, bottom, rightBottom, settings.cube.rightColor);
            }
        }
    };

    // ---------- Create VAOs / VBOs ----------
    GLuint staticVAO = 0, staticVBO = 0;
    GLuint edgeVAO = 0, edgeVBO = 0;

    glGenVertexArrays(1, &staticVAO);
    glGenBuffers(1, &staticVBO);

    glGenVertexArrays(1, &edgeVAO);
    glGenBuffers(1, &edgeVBO);

    // ---------- Build static geometry (triangles and edges) once ----------
    insertHexagonsInit(); // fills triangleVertices and edgeVertices (one-time)

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

    // Setup edge VAO (for outlines with edge data)
    glBindVertexArray(edgeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, edgeVBO);
    if (!edgeVertices.empty()) {
        glBufferData(GL_ARRAY_BUFFER,
                     edgeVertices.size() * sizeof(EdgeVertex),
                     edgeVertices.data(),
                     GL_STATIC_DRAW);
    } else {
        glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_STATIC_DRAW);
    }

    // Position (location 0) vec2
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void*)0);
    glEnableVertexAttribArray(0);

    // Color (location 1) vec4
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void*)offsetof(EdgeVertex, r));
    glEnableVertexAttribArray(1);

    // Edge point 1 (location 2) vec2
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void*)offsetof(EdgeVertex, edgeP1_x));
    glEnableVertexAttribArray(2);

    // Edge point 2 (location 3) vec2
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(EdgeVertex), (void*)offsetof(EdgeVertex, edgeP2_x));
    glEnableVertexAttribArray(3);

    glBindVertexArray(0);

    // ---------- compile shaders ----------
    GLuint staticShaderProgram = shaderUtils::compileShaders("shaders/static_vertex.glsl", "shaders/static_fragment.glsl");
    if (staticShaderProgram == 0) {
        std::cerr << "Failed to compile static shaders!" << std::endl;
        return -1;
    }
    
    GLuint edgeShaderProgram = shaderUtils::compileShaders("shaders/edge_vertex.glsl", "shaders/edge_fragment.glsl");
    if (edgeShaderProgram == 0) {
        std::cerr << "Failed to compile edge shaders!" << std::endl;
        return -1;
    }

    // Static shader uniforms
    GLint staticHalfWidthLocation  = glGetUniformLocation(staticShaderProgram, "halfWidth");
    GLint staticHalfHeightLocation = glGetUniformLocation(staticShaderProgram, "halfHeight");
    
    // Edge shader uniforms
    GLint edgeHalfWidthLocation  = glGetUniformLocation(edgeShaderProgram, "halfWidth");
    GLint edgeHalfHeightLocation = glGetUniformLocation(edgeShaderProgram, "halfHeight");
    GLint mousePosLocation = glGetUniformLocation(edgeShaderProgram, "mousePos");
    GLint barrierRadiusLocation = glGetUniformLocation(edgeShaderProgram, "barrierRadius");
    GLint fadeAreaLocation = glGetUniformLocation(edgeShaderProgram, "fadeArea");
    GLint reverseModeLocation = glGetUniformLocation(edgeShaderProgram, "reverseMode");
    
    // Wave effect uniforms
    GLint waveProgressLocation = glGetUniformLocation(edgeShaderProgram, "waveProgress");
    GLint waveXLocation = glGetUniformLocation(edgeShaderProgram, "waveX");
    GLint waveWidthLocation = glGetUniformLocation(edgeShaderProgram, "waveWidth");
    GLint waveColorLocation = glGetUniformLocation(edgeShaderProgram, "waveColor");

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

        glClearColor(settings.backgroundColor[0], settings.backgroundColor[1], settings.backgroundColor[2], settings.backgroundColor[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        // draw static triangles (fills) with static shader
        glUseProgram(staticShaderProgram);
        glUniform1f(staticHalfWidthLocation, HalfWidth);
        glUniform1f(staticHalfHeightLocation, HalfHeight);
        
        glBindVertexArray(staticVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangleVertices.size()));
        glBindVertexArray(0);

        // draw edge outlines with edge shader
        glUseProgram(edgeShaderProgram);
        glUniform1f(edgeHalfWidthLocation, HalfWidth);
        glUniform1f(edgeHalfHeightLocation, HalfHeight);
        
        // Set mouse position and barrier settings for edge rendering
        glm::vec2 mousePos(static_cast<float>(mouseX), Height - static_cast<float>(mouseY));
        glUniform2f(mousePosLocation, mousePos.x, mousePos.y);
        glUniform1f(barrierRadiusLocation, settings.barrier.radius);
        glUniform1f(fadeAreaLocation, settings.barrier.fadeArea);
        glUniform1i(reverseModeLocation, settings.barrier.reverse ? 1 : 0);
        
        // Set wave effect uniforms
        if (waveActive) {
            float waveProgress = (glfwTime - waveStartTime) / waveDuration;
            float waveX = -settings.wave.width * 0.5f + waveProgress * (Width + settings.wave.width);
            
            glUniform1f(waveProgressLocation, waveProgress);
            glUniform1f(waveXLocation, waveX);
            glUniform1f(waveWidthLocation, settings.wave.width);
            glUniform4f(waveColorLocation, settings.wave.color[0], settings.wave.color[1], 
                       settings.wave.color[2], settings.wave.color[3]);
        } else {
            // Disable wave effect
            glUniform1f(waveProgressLocation, -1.0f);
            glUniform1f(waveXLocation, -999999.0f);
            glUniform1f(waveWidthLocation, 0.0f);
            glUniform4f(waveColorLocation, 0.0f, 0.0f, 0.0f, 0.0f);
        }
        
        glBindVertexArray(edgeVAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(edgeVertices.size()));
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

    glDeleteProgram(staticShaderProgram);
    glDeleteProgram(edgeShaderProgram);

    glDeleteVertexArrays(1, &staticVAO);
    glDeleteBuffers(1, &staticVBO);
    glDeleteVertexArrays(1, &edgeVAO);
    glDeleteBuffers(1, &edgeVBO);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
