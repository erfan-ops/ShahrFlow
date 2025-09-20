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
    "fps": 60,
    "vsync": false,

    "hexagon-size": 30,

    "edges": {
      "width": 2,
      "color": [ 0.996, 0.843, 0.843, 0.6 ]
    },

    "mouse-barrier": {
      "radius": 220
    },

    "MSAA": 2
}
```

- `fps` → target frame rate (ignored if VSync is enabled).  
- `vsync` → toggle vertical sync.  
- `hexagon-size` → size of each hexagon in pixels.  
- `edges.width` → thickness of hexagon outlines.  
- `edges.color` → RGBA of hexagon edges.  
- `mouse-barrier.radius` → interactive radius for mouse hover effects.  
- `MSAA` → anti-aliasing level.  

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
