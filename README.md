# ShahrFlow – Live Desktop Wallpaper

ShahrFlow is a **live, interactive desktop wallpaper** inspired by the modern geometric identity of **Iran’s Shahr Bank (بانک شهر)**.  
It brings your desktop to life with a flowing grid of **hexagonal patterns** in deep red tones, subtle shading, and interactive edges that respond dynamically to your mouse movements.  

Built with **C++**, **OpenGL**, and **GLFW**, it replaces your static wallpaper with a lightweight, GPU-accelerated background that feels both elegant and responsive.

---

## ✨ Features

- 🎨 **Shahr-inspired design**: hexagonal tessellation in shades of red.
- 🖱️ **Mouse interactivity**: edges glow and fade based on your cursor’s proximity.
- ⚡ **Smooth performance**: hardware-accelerated rendering with optional VSync and frame capping.
- 🪟 **Seamless desktop integration**: runs as a background window pinned to your desktop.
- 🖥️ **Multi-monitor support**: adapts to your full virtual screen resolution.
- 🛠️ **Configurable settings**: customize visuals and performance through `settings.json`.
- 🛎️ **Tray menu**: quickly quit the app via a system tray icon.

---

## Installation
1. **Download**: Get the latest version of ShahrFlow from the [Releases](https://github.com/erfan-ops/ShahrFlow/releases) page. 
2. **Launch**: Run `ShahrFlow.exe` to enjoy your new live wallpaper.

---

## 🔨 Build from Source

1. Clone the repository:
   ```bash
   git clone https://github.com/erfan-ops/ShahrFlow.git
   cd ShahrFlow
   ```

2. Install dependencies:
   - [GLFW](https://www.glfw.org/)
   - [GLAD](https://glad.dav1d.de/)
   - [GLM](https://github.com/g-truc/glm)
   - Windows SDK (for system tray + wallpaper utilities)

3. Open the project in **Visual Studio**:
   - Set build type to **Release x64**.
   - Build the project.

4. Run the generated executable.

---

## ⚙️ Settings

Customize behavior in `settings.json`:

```jsonc
{
    "fps": 120,
    "vsync": false,

    "background-color": [1, 1, 1, 1],
  
    "hexagon-size": 50,

    "cube": {
        "top-color": [0.898, 0.243, 0.243, 1.0],
        "left-color": [0.773, 0.188, 0.188, 1.0],
        "right-color": [0.455, 0.165, 0.165, 1.0]
    },

    "edges": {
        "width": 1.5,
        "color": [ 0.996, 0.843, 0.843, 0.6 ]
    },

    "mouse-barrier": {
        "radius": 200,
        "reverse": false,
        "fade-area": 150
    },

    "wave": {
        "speed": 420,
        "width": 370,
        "interval": 6.2,
        "color": [1, 0, 0, 1]
    },

    "MSAA": 2
}
```

### Settings Overview

These options can be customized in `settings.json` to change how ShahrFlow looks and behaves:

- **`fps`** → Maximum frames per second. Ignored if `vsync` is enabled.  
- **`vsync`** → Synchronizes rendering with your monitor’s refresh rate. Reduces tearing, but ignores `fps`.  
- **`background-color`** → The wallpaper’s background color in RGBA format `[R, G, B, A]`.  
- **`hexagon-size`** → Size of each hexagon (and cube face) in pixels. Larger values create bigger hexagons.  

#### 🎨 Cube Colors
- **`cube.top-color`** → The fill color of the cube’s top face.  
- **`cube.left-color`** → The fill color of the cube’s left face.  
- **`cube.right-color`** → The fill color of the cube’s right face.  

#### ✏️ Edges
- **`edges.width`** → Thickness of cube/hexagon outlines.  
- **`edges.color`** → Outline color in RGBA format.  

#### 🖱️ Mouse Barrier
- **`mouse-barrier.radius`** → Distance (in pixels) around the cursor where outlines react.  
- **`mouse-barrier.reverse`** → If `true`, outlines are visible everywhere *except* near the cursor.  
- **`mouse-barrier.fade-area`** → A soft fade region (in pixels) where outlines gradually disappear instead of cutting off sharply.  

#### 🌊 Waves
- **`wave.speed`** → Movement speed of the wave animation.  
- **`wave.width`** → Width (or thickness) of the wave.  
- **`wave.interval`** → Time spacing between consecutive waves.  
- **`wave.color`** → Color of the wave in RGBA format.  

#### 🖼️ Anti-Aliasing
- **`MSAA`** → Level of multi-sample anti-aliasing. Higher values smooth edges but may cost performance.  


---

## 🎮 Controls

- 🖱️ **Hover mouse** → edges near the cursor fade in and glow.  
- 📋 **Tray menu (right-click icon)** → quit the app.  

---

## 🛑 Uninstall / Exit

- Quit the program via the **system tray icon**.  
- On exit, your original wallpaper will be restored automatically.  

---

## 🔧 Tech Stack

- **C++17**
- **OpenGL 3.3 Core**
- **GLFW 3**
- **GLAD**
- **GLM**
- **Win32 API** (tray + wallpaper control)

---

## 📜 License

MIT License. See [LICENSE](LICENSE) for details.  

---

## 💡 Inspiration

This project is inspired by the **visual identity of Bank Shahr**, featuring its bold red palette and geometric hexagonal motif, reimagined as a dynamic, interactive desktop wallpaper.  
