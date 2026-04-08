# Lab 03

Lab 03 的畫面內容與 Lab 02 相同，皆為 triangular prism 的顯示與互動操作。  
本次實作的重點不在模型本身，而在於將原本由 OpenGL/GLU 提供的座標變換，改為以程式自行建立數學物件與矩陣運算完成。

## 實作重點總覽

本實作將整個 transformation pipeline 分成三個部分：

- Model transform
- View transform
- Projection transform

其目的如下：

- Model transform：將物體從自身座標系轉換到世界座標系
- View transform：將世界座標系轉換到相機座標系
- Projection transform：將三維空間投影到螢幕平面

在數學表示上，本次實作使用：

- `Matrix4` 表示 4x4 齊次座標矩陣
- `Vec3` 表示三維向量
- `Quaternion` 表示三維旋轉

其中 `Matrix4` 負責描述線性與仿射變換，`Quaternion` 則專門負責旋轉姿態的表示與累積。

## 齊次座標與 4x4 矩陣

在電腦圖學中，平移、旋轉、縮放皆可用齊次座標下的 4x4 矩陣統一表示。  
這樣的好處是，原本不同型態的幾何操作都可以寫成矩陣乘法，並以固定順序組合成單一 transformation matrix。

若以物體模型為例，最終的 model matrix 可寫為：

```text
M_model = T * R * S
```

其幾何意義為：

- 先對模型做縮放 `S`
- 再施加旋轉 `R`
- 最後施加平移 `T`

由於矩陣乘法不具交換性，乘法順序會直接影響幾何結果，因此順序本身就是 transformation 定義的一部分。

在程式中，`Matrix4` 被保留為純矩陣型別，只提供：

- `identity()`
- `operator*()`
- `at(row, col)`
- `data()`

而 model、view、projection 所需的語義性矩陣，則分別放在不同 namespace 中建立。

## Model Transform 的角色

Model transform 的任務，是描述物體在世界中的位置、方向與大小。  
若模型原本定義在自身的 local coordinate system 中，則 model matrix 用來將其轉換到 world coordinate system。

本實作中，model transform 由三個狀態變數描述：

- 位置變數 `position`
- 姿態變數 `orientation`
- 尺度變數 `scale`

在程式中，對應為：

```text
M_model = T(position) * R(orientation) * S(scale)
```

這表示物體先在自身座標中完成縮放與旋轉，再被放置到世界座標中的指定位置。

## View Transform 的角色

View transform 的任務，是將場景從世界座標系轉換到相機座標系。  
它不改變物體本身，而是改變觀察者的位置與方向。

本實作未使用 `gluLookAt()`，而是以 `View::look_at()` 建立 view matrix。  
其核心做法為先建立相機的三個正交基底向量：

```text
forward = normalize(center - eye)
side = normalize(cross(forward, up))
camera_up = cross(side, forward)
```

其中：

- `eye` 為相機位置
- `center` 為相機觀察目標
- `up` 為參考上方向

由這三個方向向量可建立相機座標系，進而寫成 view matrix。

此一定義有明確的數學前提：

- `eye` 不可與 `center` 重合
- `up` 不可與視線方向平行

若不滿足上述條件，則相機基底無法建立，因此程式中以 `assert` 將其視為非法輸入。

除了 `View::look_at(...)` 所建立的基礎 view matrix 之外，程式中的滑鼠拖曳與方向鍵操作，還會再對既有 view frame 施加一個額外的 orbit-style 旋轉。  
因此目前的互動視角語意較接近：

```text
M_view_total = M_view_base * R_orbit
```

而不是由 `yaw` 與 `pitch` 重新計算新的 `eye` 與 `center` 後，再建立一個全新的 `look_at` view matrix。

## Projection Transform 的角色

Projection transform 的任務，是將三維空間映射到二維螢幕。  
本實作中使用兩種投影：

- `Projection::perspective(...)`
- `Projection::orthographic(...)`

Perspective projection 用於主要 3D 場景，表現近大遠小的視覺效果。  
Orthographic projection 用於畫面上的文字 overlay，使其以固定平面座標顯示。

## 為何旋轉使用四元數

若以 Euler angles 表示旋轉，常見做法是依序繞 X、Y、Z 軸旋轉。  
此表示法雖然直觀，但存在兩個問題：

- 旋轉順序不同會得到不同結果
- 在特定姿態下可能產生 gimbal lock

gimbal lock 的本質是旋轉自由度的退化，也就是原本獨立的兩個旋轉軸重合，導致姿態控制失去一個自由度。

為避免此問題，本實作以 quaternion 表示物體姿態。  
相較於 Euler angles，quaternion 更適合用來累積連續旋轉，且不會因中間姿態而出現萬向節鎖。

## 四元數的基本數學

四元數可寫為：

$$
\mathscr{q = w + xi + yj + zk}
$$

也可視為四維係數：

$$
\mathscr{q = (w, x, y, z)}
$$

若一個旋轉表示為「繞單位軸 `u = (ux, uy, uz)` 旋轉角度 `theta`」，則對應的 quaternion 為：

$$
\mathscr{q = (\cos\frac{\theta}{2}, ux \sin\frac{\theta}{2}, uy \sin\frac{\theta}{2}, uz \sin\frac{\theta}{2})}
$$

兩個旋轉若分別表示為 quaternion，則其組合可透過 quaternion multiplication 完成。  
因此，連續旋轉不必拆解為多次 Euler angle 更新，而可直接累積在同一個姿態物件上。

此外，單位四元數可以轉換成 rotation matrix，這使得 quaternion 能夠自然地接入既有的矩陣 transformation pipeline。

## 在程式中如何使用四元數

在本實作中，物體旋轉不再以三個角度參數表示，而是以：

```cpp
Quaternion orientation;
```

表示物體目前的姿態狀態。

當使用者按下 `W/A/S/D/Q/E` 時，系統會根據旋轉軸與旋轉角度建立一個增量 quaternion：

```text
delta = Quaternion::from_axis_angle(axis, theta)
```

接著以乘法更新姿態狀態：

```text
orientation = normalize(orientation * delta)
```

此作法的幾何意義是，將新的局部旋轉累積到目前姿態之上。  
最後在繪圖前，再將 `orientation` 轉為 rotation matrix，納入 model transform。

由於旋轉矩陣的建立假設 `orientation` 為單位四元數，因此程式在每次更新姿態後都會重新正規化。  
同時，軸角表示中的旋轉軸必須為非零向量，否則無法定義有效旋轉，程式中同樣以 `assert` 視為非法輸入。

## 程式結構與數學對應

目前程式中的數學結構分工如下：

- `Matrix4`：純矩陣資料與矩陣乘法
- `Quaternion`：旋轉姿態、軸角建立、共軛、向量旋轉、轉換為矩陣
- `Transform` namespace：model transform 相關矩陣
- `View` namespace：view matrix
- `Projection` namespace：projection matrix

這樣的安排使不同數學物件各自負責明確角色：

- 矩陣負責描述座標變換
- 四元數負責描述旋轉姿態
- namespace 負責區分幾何意義不同的矩陣來源

## 工程對應細節

在實作層面，`Matrix4` 採用 column-major 方式儲存：

```cpp
value[col * 4 + row]
```

此設計可直接與 OpenGL 的 `glMultMatrixf()` 配合。  
因此，雖然 transformation matrix 由程式自行建立，仍可無縫送入 OpenGL 固定管線使用。

此外，若數學物件的定義域不成立，程式會以 `assert` 主動檢查，例如：

- `normalize(v)` 要求 `v` 為非零向量
- `Quaternion::from_axis_angle(axis, ...)` 要求 `axis` 為非零向量
- `Quaternion::normalized()` 要求 quaternion 長度不可為零
- `Projection::perspective(...)` 要求 `aspect > 0`、`near > 0`、`far > near`、`0 < fov < 180`
- `Projection::orthographic(...)` 要求左右、上下、前後裁切面不可重合

## 小結

Lab 03 的主要目標，是以數學物件明確描述整個 transformation pipeline，並取代原先對 OpenGL/GLU 內建變換函式的依賴。  
整體流程可概括為：

- 以 `Transform::trs(...)` 建立 model matrix
- 以 `View::look_at(...)` 建立 view matrix
- 以 `Projection::perspective(...)` 或 `Projection::orthographic(...)` 建立 projection matrix

其中，旋轉部分改以 quaternion 表示，使姿態累積更穩定，也避免 Euler angles 在特定情況下的 gimbal lock 問題。
