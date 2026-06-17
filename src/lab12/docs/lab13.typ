#set document(title: "Lab 13", author: "葉騏緯")
#set page(paper: "a4", margin: (x: 2.4cm, y: 2.8cm))
#set text(font: "New Computer Modern", size: 11pt, lang: "zh")
#set heading(numbering: "1.1.")
#set par(justify: true, leading: 0.75em)
#show raw: it => text(font: "Noto Sans Mono", it)
#show heading.where(level: 1): it => {
  v(1.2em)
  it
  v(0.4em)
}

#align(center)[
  #text(size: 18pt, weight: "bold")[Lab 13：關節機器人與平面陰影]
  #v(0.3em)
  #text(size: 10pt, fill: luma(120))[資工碩一 114598085 葉騏緯]
]

#v(1.5em)

= 專案架構與執行流程

== 整體模組劃分

專案分成四個職責明確的層次：

#table(
  columns: (auto, auto, 1fr),
  inset: 8pt,
  stroke: 0.5pt + luma(200),
  align: (left, left, left),
  [*檔案*], [*類別*], [*職責*],
  [`main.cpp`], [—], [GLUT 初始化、視窗建立、字型路徑],
  [`app.cpp/hpp`], [`App`], [事件分派器：鍵盤 → 狀態 → 重繪],
  [`app.cpp`], [`SceneRenderer`], [3D 場景：機器人、地面、光源、陰影],
  [`app.cpp`], [`OverlayRenderer`], [2D UI：文字 keycap、說明面板],
  [`gl_resources.hpp`], [`FontAtlas` / `TextureHandle`], [FreeType 字型貼圖資源管理],
  [`vertex.hpp`], [`Quads` / `Face` / `FlatBox`], [幾何發射的 fluent API],
  [`math3d.hpp`], [純函式], [向量、矩陣、平面方程、陰影矩陣],
)

== GLUT 事件迴圈

```
glutMainLoop()
  ├── glutDisplayFunc  → App::display()
  ├── glutReshapeFunc  → App::reshape()        (更新 Projection 矩陣)
  ├── glutKeyboardFunc → App::on_key()         (一般鍵)
  └── glutSpecialFunc  → App::on_special_key() (方向鍵)
```

每次 `display()` 的執行順序：

+ `glClear(COLOR | DEPTH)` — 清除畫面
+ `SceneRenderer::draw_scene()` — 繪製 3D 場景
+ `OverlayRenderer::draw()` — 在最上層疊加 2D HUD
+ `glutSwapBuffers()` — 雙緩衝交換

`App` 物件存放在 GLUT 的 window data（`glutSetWindowData / glutGetWindowData`），所以所有 callback 都能透過靜態輔助函式 `current()` 取得同一個實例，避免全域變數。

= 狀態機（State Machine）

== RenderState 是唯一真相來源

整個應用程式的互動邏輯完全由一個不可分割的結構體驅動：

```cpp
struct RenderState {
    float robot_yaw_deg    = 0;   // 機器人水平旋轉
    float head_yaw_deg     = 0;   // 頭部左右轉
    float arm_pose_deg     = 0;   // 手臂抬起角度
    float leg_pose_deg     = 0;   // 腿部抬起角度
    float camera_pitch_deg = 0;   // 鏡頭俯仰
    PlaneState plane_rotation{};  // 地面傾斜 (pitch, roll)
    int  active_light = 0;        // 目前光源編號 (0–2)
    bool overlay_expanded = false; // 說明面板展開狀態
};
```

*沒有任何 render 函式擁有自己的內部狀態。* `SceneRenderer` 和 `OverlayRenderer` 都是無狀態的，每幀完全根據傳入的 `RenderState` 決定畫什麼。

== 狀態轉移圖

```
        ┌─────────────── 方向鍵 ←/→ ────────────────┐
        │                                            │
        ↓                                            │
 [robot_yaw] ←── wrap_deg() ──────────────────────→ [robot_yaw]

        A/D ──→ arm_pose ←── clamp_pose(−70°, 70°)
        W/S ──→ leg_pose ←── clamp_pose(−70°, 70°)
        Q/E ──→ head_yaw ←── wrap_deg()
      Up/Dn ──→ camera_pitch ←── clamp_camera(−24°, 24°)
      I/K/J/L ──→ plane_rotation.pitch / roll ←── wrap_deg()
        1/2/3 ──→ active_light (0, 1, 2)
          H ──→ overlay_expanded toggle
          R ──→ state = {} (保留 overlay_expanded)
         ESC ──→ exit(0)
```

`wrap_deg()` 將角度取模到 [0°, 360°)，確保連續旋轉不會數值爆炸。`clamp_pose/camera` 則限制關節角防止穿模。

= OpenGL Fixed-Function Pipeline 原理

== ModelView 矩陣堆疊

OpenGL 維護一個矩陣堆疊（matrix stack）。每次呼叫 `glTranslatef` / `glRotatef` / `glScalef` 都是在當前頂部矩陣右乘一個變換：

$ M_"new" = M_"current" times T $

`glPushMatrix()` 複製頂部矩陣壓入堆疊；`glPopMatrix()` 彈出，恢復原本的狀態。這正是「階層式變換」的實作基礎。

本專案用 RAII 封裝這組操作：

```cpp
struct MatrixScope {
    MatrixScope()  { glPushMatrix(); }
    ~MatrixScope() { glPopMatrix(); }
};
```

只要在任何需要局部變換的函式中宣告 `MatrixScope scope;`，離開作用域時就自動還原矩陣，不會有「忘記 pop」的問題。

== 攝影機設定

```cpp
void SceneRenderer::configure_frame() const {
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0.0f, -20.0f, -430.0f);  // 場景推離鏡頭
}

void SceneRenderer::apply_camera(const RenderState& state) const {
    glRotatef(state.camera_pitch_deg, 1.0f, 0.0f, 0.0f);  // 俯仰
}
```

OpenGL 的預設鏡頭在原點朝 −Z 方向看。`glTranslatef(0, -20, -430)` 將整個場景往 −Z 推 430 單位，等效於把鏡頭放在場景前方 430 單位處。

== 透視投影

```cpp
gluPerspective(60.0,          // 垂直視角 (FOV)
               (double)w / h, // 畫面長寬比
               20.0,          // 近裁剪面
               900.0);        // 遠裁剪面
```

`gluPerspective` 設定 `GL_PROJECTION` 矩陣，將視錐體（frustum）中的點映射到 NDC（Normalized Device Coordinates）。近裁剪面 20 確保距離鏡頭太近的物件不被顯示（避免 z-fighting 和穿透鏡頭）。

== Phong 光照模型

OpenGL fixed-function 的光照計算發生在每個*頂點*（per-vertex），稱為 Gouraud shading，再由光柵化階段插值到像素。

每個頂點的顏色由以下公式決定（簡化版）：

$ C = C_"ambient" + C_"diffuse" dot max(bold(N) dot bold(L), 0) + C_"specular" dot max(bold(N) dot bold(H), 0)^s $

其中：
- $bold(N)$ — 頂點法向量（`glNormal3f`）
- $bold(L)$ — 頂點到光源的單位向量
- $bold(H)$ — 半向量 $= (bold(L) + bold(V)) / |bold(L) + bold(V)|$
- $s$ — 光澤度（shininess = 96）

本程式透過 `GL_COLOR_MATERIAL` 讓 `glColor3f` 同時設定材質的 ambient + diffuse，簡化每個 box 的材質設定。

光源設定（每幀在 modelview 有效時呼叫）：

```cpp
glLightfv(GL_LIGHT0, GL_AMBIENT,  kAmbientLight);   // 環境光 (0.3, 0.3, 0.3)
glLightfv(GL_LIGHT0, GL_DIFFUSE,  light.diffuse);   // 各光源自定義
glLightfv(GL_LIGHT0, GL_SPECULAR, light.specular);
glLightfv(GL_LIGHT0, GL_POSITION, light.position);  // 在目前 modelview 下變換
```

*重要：* `GL_POSITION` 在呼叫 `glLightfv` 時立即被當前 ModelView 矩陣變換並儲存在眼睛空間（eye space）。因此光源位置定義在 model 空間，程式在設定完攝影機後、繪製物件前設定光源。

= 機器人關節動畫

== 幾何層次結構

機器人的各部位形成一棵樹狀結構，父節點的變換會累積到所有子節點：

```
世界原點
└── 機器人底座  glTranslatef(0, kGroundY − kRobotFootRelY, 0)
    │            glRotatef(robot_yaw_deg, Y)
    ├── 軀幹    draw_box({0, kTorsoCenterY, 0}, ...)
    ├── 頭部    glTranslatef(0, kTorsoTopY+4, 0)
    │   │        glRotatef(head_yaw_deg, Y)
    │   │        glTranslatef(0, head.h/2, 0)
    │   ├── 頭  draw_box(...)
    │   └── 眼  draw_box(left), draw_box(right)
    ├── 左臂    glTranslatef(−28, kTorsoTopY−8, 0)
    │   │        glRotatef(side×7, Z)          ← 自然垂手角
    │   │        glRotatef(pose × upper_ratio, X)
    │   ├── 上臂 draw_segment(arm.upper)
    │   │        glTranslatef(0, −upper.h, 0)
    │   │        glRotatef(pose × lower_ratio − 18, X)  ← 手肘預彎
    │   ├── 前臂 draw_segment(arm.lower)
    │   └── 手掌 draw_segment(arm.end)
    ├── 右臂    (對稱，side = +1)
    ├── 左腿    glTranslatef(−11, hip_y, 0)
    │   │        glRotatef(pose × upper_ratio, X)
    │   ├── 大腿 draw_segment(leg.upper)
    │   ├── 小腿 draw_segment(leg.lower)
    │   └── 腳掌 draw_segment(leg.end)
    └── 右腿    (對稱)
```

== draw_segment 的原點慣例

所有 `draw_segment` 都以「關節點在 Y=0，往下延伸」為慣例：

```cpp
void SceneRenderer::draw_segment(const BoxConfig& box, bool shadow) const {
    MatrixScope scope;
    glTranslatef(0.0f, -box.size[1] * 0.5f, 0.0f);  // 中心下移半個高度
    glScalef(box.size[0], box.size[1], box.size[2]);
    set_color(box.color, shadow);
    quads() | flat_box(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
}
```

這樣每次 `glTranslatef(0, −length, 0)` 移到下個關節點後，直接呼叫 `draw_segment` 就能正確繪製，不需要額外計算偏移。

== 關節角度的比例係數

```cpp
// 手臂
glRotatef(pose_deg * arm.upper_ratio,  1, 0, 0);  // upper_ratio = 1.0
glRotatef(pose_deg * arm.lower_ratio - 18.0f, 1, 0, 0); // lower_ratio = -0.72
```

- `upper_ratio = 1.0`：肩膀完整反映 `arm_pose_deg`
- `lower_ratio = −0.72`：手肘反方向彎曲 72%，形成自然的肘部曲線（抬手時肘彎向後）
- `−18.0f`：手肘的靜止預彎角，讓機器人在 pose=0 時手臂自然微彎

= 平面陰影（Planar Shadow）

== 數學原理

設光源位置為 $bold(L) = (L_x, L_y, L_z)$，物體上一點為 $bold(P) = (P_x, P_y, P_z)$，地面平面方程為：

$ a x + b y + c z + d = 0 $

從 $bold(L)$ 通過 $bold(P)$ 的射線為：

$ bold(R)(t) = bold(L) + t (bold(P) - bold(L)) $

令 $bold(R)(t)$ 滿足平面方程，解出 $t$：

$ t = -frac(a L_x + b L_y + c L_z + d, a(P_x - L_x) + b(P_y - L_y) + c(P_z - L_z)) $

代回得陰影點 $bold(S) = bold(R)(t)$。這個映射對所有點 $bold(P)$ 都是線性的，可以表示為 $4 times 4$ 矩陣（齊次坐標）：

$ bold(S)_"hom" = M_"shadow" dot bold(P)_"hom" $

其中（令 $d_i = -L_i$）：

$
  M_"shadow" = mat(
    b d_y + c d_z, -a d_y, -a d_z, 0;
    -b d_x, a d_x + c d_z, -b d_z, 0;
    -c d_x, -c d_y, a d_x + b d_y, 0;
    -d d_x, -d d_y, -d d_z, a d_x + b d_y + c d_z
  )
$

== 實作：`planar_shadow_matrix`

```cpp
Mat4 planar_shadow_matrix(const Vec4& plane, const Vec3& light_position) {
    const float a = plane[0], b = plane[1], c = plane[2], d = plane[3];
    const float dx = -light_position[0];
    const float dy = -light_position[1];
    const float dz = -light_position[2];
    // 以 column-major 儲存（OpenGL 慣例）
    return {b*dy+c*dz, -b*dx,    -c*dx,    -d*dx,
            -a*dy,   a*dx+c*dz, -c*dy,    -d*dy,
            -a*dz,   -b*dz,    a*dx+b*dy, -d*dz,
             0,       0,        0,     a*dx+b*dy+c*dz};
}
```

使用時直接 `glMultMatrixf(shadow_matrix.data())`，讓 OpenGL 將機器人的整個幾何體重新投影到地面。

== 動態平面方程

地面可以被 IJKL 鍵傾斜（`PlaneState::pitch_deg` 和 `roll_deg`）。陰影必須對應更新：

```cpp
Vec4 current_plane_equation(const PlaneState& plane) {
    // 取平面上三個點，套用旋轉後計算法向量
    const Vec3 p1 = transform_plane_point({-30, 0, -20}, plane, kShadowLift);
    const Vec3 p2 = transform_plane_point({-30, 0,  20}, plane, kShadowLift);
    const Vec3 p3 = transform_plane_point({ 40, 0,  20}, plane, kShadowLift);
    return plane_equation(p1, p2, p3);
}
```

`kShadowLift = 0.20` 將平面方程稍微上抬，避免陰影多邊形與地面完全共面造成 z-fighting（閃爍）。

== 模板緩衝（Stencil Buffer）消除重疊暗化

若直接將黑色半透明幾何體疊在地面上，機器人多個部位的陰影在重疊區域會被多次疊加（越疊越黑，不自然）。

解法：使用 *stencil buffer* 確保每個像素只被著色一次。

```
Pass 1：僅寫 stencil
  glColorMask(FALSE, FALSE, FALSE, FALSE)  // 不寫顏色
  glDepthMask(FALSE)                        // 不寫深度
  glStencilOp(REPLACE, REPLACE, REPLACE)   // 凡觸及 → stencil = 1
  → 繪製投影後的機器人（純幾何，無色彩）

Pass 2：僅在 stencil=1 的像素著色
  glColorMask(TRUE, TRUE, TRUE, TRUE)
  glStencilFunc(EQUAL, 1, 0xFF)            // 只通過 stencil=1
  glStencilOp(KEEP, KEEP, KEEP)           // 不再改 stencil
  → 繪製半透明黑色地面覆蓋（α = 0.36）
```

這樣不論機器人幾何有多少層重疊，每個地面像素最多只被一次暗化。

= 幾何發射 API

== Fluent 風格的頂點發射

`vertex.hpp` 設計了一套 operator overloading 的 fluent API，讓繪製程式碼讀起來接近宣告式：

```cpp
// 繪製地面（手動指定法向量與四個頂點）
quads() | face(0.0f, 1.0f, 0.0f)
              .v( kW, 0, -kD)
              .v(-kW, 0, -kD)
              .v(-kW, 0,  kD)
              .v( kW, 0,  kD);

// 繪製 box（六個面、共 24 個頂點）
quads() | flat_box(-0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f);
```

`Quads` 的建構子呼叫 `glBegin(GL_QUADS)`，解構子呼叫 `glEnd()`。即使中途拋出例外也能正確關閉 GL 狀態。

`flat_box` 展開成六個面（+Z, −Z, +Y, −Y, +X, −X），每個面根據 `kBoxFaces` 查表計算法向量與角落坐標。

= 字型渲染系統

== FreeType → 單字元紋理 → 四邊形

```
字型檔 (.ttf)
  ↓ FT_New_Face + FT_Set_Pixel_Sizes
字元點陣圖 (alpha-only bitmap)
  ↓ glTexImage2D (GL_ALPHA 格式, 22px)
逐字元 GL_TEXTURE_2D 四邊形
  ↓ GL_BLEND (SRC_ALPHA, ONE_MINUS_SRC_ALPHA)
螢幕上的抗鋸齒文字
```

每個 ASCII 字元（32–126）在初始化時各自生成一個 `GL_ALPHA` 格式的紋理貼圖（僅儲存不透明度，無 RGB）。繪製時：

1. `glColor3f(r, g, b)` 決定字色
2. `GL_ALPHA` 紋理與 alpha blend 搭配，讓字元邊緣平滑

`FontAtlas::glyph(ch)` 回傳 `Glyph`，包含：
- `texture` — 該字元的貼圖 handle
- `bearing_x/y` — 字元相對於基線的偏移（FreeType 字形量度）
- `advance` — 移到下個字元的水平距離（以 1/64 像素為單位，需 `>> 6`）

== 2D 疊加渲染的座標系

```cpp
gluOrtho2D(0, width, 0, height);  // (0,0) = 左下角
```

所有 UI 元素以左下為原點的像素坐標繪製。`kOverlayTopY = 48` 表示面板頂端距視窗頂端 48px，換算：

```cpp
float sy = (float)height - kOverlayTopY - y;
```

其中 `y` 是面板內部的相對坐標（向下為正）。

== 面板尺寸

#table(
  columns: (auto, auto, auto),
  inset: 8pt,
  stroke: 0.5pt + luma(200),
  align: (left, center, center),
  [*常數*], [*值*], [*說明*],
  [`kOverlayWidth`], [820 px], [展開說明面板寬度],
  [`kOverlayHeight`], [254 px], [展開說明面板高度],
  [`kMiniOverlayWidth`], [308 px], [收起狀態的迷你面板寬度],
  [`kMiniOverlayHeight`], [54 px], [迷你面板高度],
  [`kOverlayFontSizePx`], [22 px], [字型大小],
)

= 光源系統

== 三組預設光源

#table(
  columns: (auto, auto, auto, auto),
  inset: 8pt,
  stroke: 0.5pt + luma(200),
  align: (left, left, left, left),
  [*編號*], [*名稱*], [*位置 (x, y, z)*], [*色調*],
  [1], [left-front warm], [(−70, 180, 80)], [暖黃 (1.0, 0.78, 0.45)],
  [2], [right-side cool], [(135, 125, 25)], [冷藍 (0.42, 0.66, 1.0)],
  [3], [rear red], [(10, 135, −145)], [深紅 (1.0, 0.22, 0.16)],
)

光源 1 的 Y=180 確保陰影落在地面可見範圍內（$Z_"shadow" approx -114$，遠小於地面邊界 $plus.minus 250$）。

== 光源標記球

每幀以 `glutSolidSphere` 在光源位置畫一顆直徑 12 單位的實心球，顏色使用該光源的 diffuse 色，讓使用者直觀看到光從哪個方向打來。

= 使用說明

== 操作一覽

#table(
  columns: (auto, auto),
  inset: 8pt,
  stroke: 0.5pt + luma(200),
  align: (left, left),
  [*按鍵*], [*效果*],
  [`H`], [展開 / 收起說明面板],
  [`1` / `2` / `3`], [切換光源（同時切換陰影計算所用的光源）],
  [`←` / `→`], [機器人整體水平旋轉（yaw）],
  [`↑` / `↓`], [鏡頭俯仰（pitch）],
  [`A` / `D`], [手臂姿勢（±70° 範圍）],
  [`W` / `S`], [腿部姿勢（±70° 範圍）],
  [`Q` / `E`], [頭部左右轉（無限旋轉）],
  [`I` / `K`], [地面前後傾（pitch），影響陰影投影面],
  [`J` / `L`], [地面左右傾（roll）],
  [`R`], [重置所有狀態（保留面板展開狀態）],
  [`ESC`], [離開程式],
)

== 說明面板狀態

*收起（預設）：* 左上角顯示迷你面板，包含 `[H]` keycap 提示及當前光源指示器 `[1][2][3]`（active 者反白）。不遮蔽場景。

*展開（按 H）：* 顯示完整 2×4 控制表，分左右兩欄，各行對應一組動作鍵。底部顯示當前光源名稱。
