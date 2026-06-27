# Lightweight Renderer for 3D GS

<img src=".\material\renders.png" style="zoom:50%;" />

## Chinese

这是一个基于OpenGL的轻型3D高斯泼溅渲染器。它使用了矩形图元对3D高斯进行平面上的简化表示，同时在排序时使用每个高斯的position进行简化。可以通过EQADWS进行上下左右前后的移动，鼠标左键移动视角。

模型默认加载目录：

```bash
./model/point_cloud.ply
```

运行渲染程序：

```bash
build/Debug/Renderer
```

重新编译：

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE= path to vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```



## English

This is a lightweight 3D Gaussian Splatting renderer based on OpenGL. It uses rectangular primitives to provide a simplified planar representation of 3D Gaussians, and simplifies the sorting process by using the position of each Gaussian. Camera movement can be controlled with **E, Q, A, D, W** and **S** for up, down, left, right, forward, and backward movement, respectively. The viewing direction can be adjusted by dragging with the **left mouse button**.

Default model loading path:

```bash
./model/point_cloud.ply
```

Run the renderer:

```bash
build/Debug/Renderer
```

Recompile:

```bash
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE= path to vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build
```

