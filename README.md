# 🧱 DX Ball Visuals (OpenGL Game)

A simple **DX Ball / Brick Breaker** clone built in **C++** using **OpenGL (GLUT)**.  
The project demonstrates fundamental 2D game mechanics such as paddle movement, ball physics, brick collisions, and visual UI screens.

---

## 🎮 Features
- 🎨 Centered brick grid layout  
- 🕹️ Paddle & ball movement with smooth physics  
- 💥 Brick collision and score updates  
- 🧩 Game states: Menu, Instructions, Play, Game Over, and Win screen  
- 🪄 Polished overlay UI with OpenGL blending  
- 🧰 Easy to modify and extend  

---

## ⚙️ Installation & Compilation

### 🧠 Requirements
Make sure you have these installed:
- **g++**
- **OpenGL**
- **GLU**
- **GLUT** or **FreeGLUT**

### 🖥️ Compile Command

#### 🐧 Linux / macOS
```bash
g++ dx_ball_visuals.cpp -o dx_ball_visuals -lGL -lGLU -lglut
