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

#include "settings.h"
#include "desktopUtils.h"
#include "trayUtils.h"
#include "utils.h"



// main window
GLFWwindow* window;

// handles tray events
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (msg == WM_TRAYICON) {
		if (lParam == WM_RBUTTONUP) {
			// Create a popup menu
			HMENU menu = CreatePopupMenu();
			AppendMenu(menu, MF_STRING, 1, L"Quit");

			// Get the cursor position
			POINT cursorPos;
			GetCursorPos(&cursorPos);

			// Show the menu
			SetForegroundWindow(hwnd);
			// Example with TPM_NONOTIFY to avoid blocking
			int selection = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, cursorPos.x - 120, cursorPos.y - 22, 0, hwnd, NULL);
			DestroyMenu(menu);

			// Handle the menu selection
			if (selection == 1) {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}
	}

	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Vertex structure (2 values for x and y, 4 values for color)
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


int main() {
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
    
    // only use gameTick when VSync is false
    if (settings.vsync) {
        glfwSwapInterval(1);
        tickFunc = [](const float, const float, float&) {};
    } else {
        glfwSwapInterval(0);

        // sleeps so that each frame took `stepInterval` seconds to complete
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

    // enabling alpha channel
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // variables to store the mouse position
    double mouseX, mouseY;

    std::vector<Vertex> vertices;

    // Helper to add a triangle (fill only)
    auto addTriangle = [&](glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color color) {
        vertices.emplace_back(p1.x, p1.y, color[0], color[1], color[2], color[3]);
        vertices.emplace_back(p2.x, p2.y, color[0], color[1], color[2], color[3]);
        vertices.emplace_back(p3.x, p3.y, color[0], color[1], color[2], color[3]);
    };

    auto pointToSegmentDist = [](glm::vec2 p, glm::vec2 a, glm::vec2 b) -> float {
        glm::vec2 ab = b - a;
        glm::vec2 ap = p - a;
        float t = glm::dot(ap, ab) / glm::dot(ab, ab);
        t = glm::clamp(t, 0.0f, 1.0f);
        glm::vec2 closest = a + t * ab;
        return glm::length(p - closest);
    };

    // Helper to add a thick line as two triangles (outline, with alpha fade by mouse)
    auto addEdge = [&](glm::vec2 p1, glm::vec2 p2, Color color, float width) {
        glm::vec2 edge = glm::normalize(p2 - p1);
        glm::vec2 normal(-edge.y, edge.x);
        glm::vec2 offset = normal * (width * 0.5f);

        glm::vec2 q1 = p1 + offset;
        glm::vec2 q2 = p2 + offset;
        glm::vec2 q3 = p2 - offset;
        glm::vec2 q4 = p1 - offset;

        // Calculate alpha based on mouse distance
        glm::vec2 mouse(mouseX, Height - mouseY);
        float dist = pointToSegmentDist(mouse, p1, p2);
        float alpha = 0.0f;
        if (dist < settings.barrier.radius) {
            alpha = (1.0f - dist / settings.barrier.radius) * color[3];
        }

        // First triangle
        vertices.emplace_back(q1.x, q1.y, color[0], color[1], color[2], alpha);
        vertices.emplace_back(q2.x, q2.y, color[0], color[1], color[2], alpha);
        vertices.emplace_back(q3.x, q3.y, color[0], color[1], color[2], alpha);

        // Second triangle
        vertices.emplace_back(q1.x, q1.y, color[0], color[1], color[2], alpha);
        vertices.emplace_back(q3.x, q3.y, color[0], color[1], color[2], alpha);
        vertices.emplace_back(q4.x, q4.y, color[0], color[1], color[2], alpha);
    };

    // Helper to add a filled triangle + outline edges
    auto addTriangleWithOutline = [&](glm::vec2 p1, glm::vec2 p2, glm::vec2 p3, Color fillColor, Color edgeColor, float edgeWidth) {
        // Add filled triangle
        addTriangle(p1, p2, p3, fillColor);

        // Add edges
        addEdge(p1, p2, edgeColor, edgeWidth);
        addEdge(p2, p3, edgeColor, edgeWidth);
        addEdge(p3, p1, edgeColor, edgeWidth);
    };

    float hexagonWidth = 1.7320508075688772f * settings.hexagonSize;
    float hexagonSliceWidth = 0.8660254037844386f * settings.hexagonSize;
    float hexagonHeight = 2.0f * settings.hexagonSize;
    float hexagonYDis = 1.5f * settings.hexagonSize;
    float hexagonHalfSize = 0.5f * settings.hexagonSize;
    int hexagonsInWidth = static_cast<int>(Width / hexagonWidth) + 2;
    int hexagonsInHeight = static_cast<int>(Height / hexagonYDis) + 1;

    vertices.reserve(hexagonsInHeight * hexagonsInWidth * 90);

    auto insertHexagons = [&]() -> void {
        for (int iy = 0; iy <= hexagonsInHeight; iy++) {
            float y = iy * hexagonYDis;
            for (float x = iy % 2 ? 0.0f : hexagonSliceWidth; x <= Width + hexagonWidth; x += hexagonWidth) {
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

                // 6 triangles making a hexagon, each with outline
                addTriangleWithOutline(c, top, leftTop, fill1, settings.edges.color, settings.edges.width);
                addTriangleWithOutline(c, top, rightTop, fill1, settings.edges.color, settings.edges.width);
                addTriangleWithOutline(c, leftTop, leftBottom, fill2, settings.edges.color, settings.edges.width);
                addTriangleWithOutline(c, rightTop, rightBottom, fill3, settings.edges.color, settings.edges.width);
                addTriangleWithOutline(c, bottom, leftBottom, fill2, settings.edges.color, settings.edges.width);
                addTriangleWithOutline(c, bottom, rightBottom, fill3, settings.edges.color, settings.edges.width);
            }
        }
    };

    // initiating VAO and VBO
    GLuint VAO, VBO;

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    // VBO (dynamic since positions change)
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    // vec2 for the position
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);

    // vec4 for the color
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, r));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);


    // compiling shaders
    GLuint shaderProgram = shaderUtils::compileShaders(
        "shaders/vertex.glsl",
        "shaders/fragment.glsl"
    );
    if (shaderProgram == 0) {
        std::cerr << "Failed to compile shaders!" << std::endl;
        return -1;
    }

    GLint halfWidthLocation  = glGetUniformLocation(shaderProgram, "halfWidth");
    GLint halfHeightLocation = glGetUniformLocation(shaderProgram, "halfHeight");

    // interval between frames (useless if vsync is on)
    const float stepInterval = 1.0f / settings.targetFPS;

    float dt{0};
	float fractionalTime{0};

    // app icon
    HICON hIcon = LoadIconFromResource();

	AddTrayIcon(hwnd, hIcon, L"Just a Simple Icon");

    // current wallpaper's path used to set the original wallpaper back when the application is closed
    std::unique_ptr<wchar_t[]> originalWallpaper(GetCurrentWallpaper());

    // setting the window as the desktop background
    SetAsDesktop(hwnd);

    glfwShowWindow(window);

    // timestamps to keep track of delta-time
    auto newF = std::chrono::high_resolution_clock::now();
	auto oldF = std::chrono::high_resolution_clock::now();

    // main loop
    while (!glfwWindowShouldClose(window)) {
        // calculating delta-time
        oldF = newF;
		newF = std::chrono::high_resolution_clock::now();
		dt = std::chrono::duration<float>(newF - oldF).count();

        // getting the mouse position
        glfwGetCursorPos(window, &mouseX, &mouseY);

        // clearing the screen
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);

        insertHexagons();

        // copying the data into the VBO
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(
            GL_ARRAY_BUFFER,
            vertices.size() * sizeof(Vertex),
            vertices.data(),
            GL_DYNAMIC_DRAW
        );
        
        glUseProgram(shaderProgram);

        glUniform1f(halfWidthLocation, HalfWidth);
        glUniform1f(halfHeightLocation, HalfHeight);
        
        // actual drawing
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size()));
        glBindVertexArray(0);

        glUseProgram(0);

        // swapping buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

        tickFunc(dt, stepInterval, fractionalTime);
    }

    // reset to the original wallpaper
    SetParent(hwnd, nullptr);
	SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, static_cast<PVOID>(originalWallpaper.get()), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    // remove tray icon from the system tray menu
    RemoveTrayIcon(hwnd);
    DestroyIcon(hIcon);

    glDeleteProgram(shaderProgram);
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
