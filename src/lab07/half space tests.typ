#import "@preview/algo:0.3.4": algo, comment, d, i

= Polygon Fill: Half-Space Tests

== 問題定義

在 Lab06 中，我們把多邊形的邊界 rasterize 成一串整數格點。Lab07 更進一步：給定一個閉合多邊形的頂點序列

$
V = (v_0, v_1, dots, v_{n-1}),
$

目標是找出所有位於多邊形內部的整數格點 $(x, y)$，並將它們著色，使螢幕上呈現一個填滿的實心多邊形。

和 line rasterization 一樣，問題的本質是在連續幾何與離散格點之間架橋：一個多邊形的「內部」在數學上是連續的面積，但螢幕只有整數位置的 pixel。

== 隱式直線方程

要判斷一個點落在有向線段的哪一側，最自然的工具是隱式直線方程。

給定有向邊從 $v_0 = (x_0, y_0)$ 到 $v_1 = (x_1, y_1)$，令

$
"dx" = x_1 - x_0, quad "dy" = y_1 - y_0.
$

定義邊函數

$
E(x, y) = "dy" (x - x_0) - "dx" (y - y_0).
$

展開後為

$
E(x, y) = "dy" dot x - "dx" dot y + ("dx" dot y_0 - "dy" dot x_0).
$

這個式子等於二維外積

$
E(x, y) = -["dx" (y - y_0) - "dy" (x - x_0)],
$

也就是邊向量 $("dx", "dy")$ 與點向量 $(x - x_0, y - y_0)$ 的外積取負號。因此：

- $E(x, y) < 0$ 表示點 $(x, y)$ 在有向邊 $v_0 -> v_1$ 的左側（在 $y$ 向上的數學座標中，CCW 多邊形的內部方向）。
- $E(x, y) = 0$ 表示點在直線上。
- $E(x, y) > 0$ 表示點在右側（外部）。

=== 係數的整數形式

把 $E(x, y) = a x + b y + c$ 寫成係數形式：

$
a = "dy", quad b = -"dx", quad c = -(a x_0 + b y_0).
$

這樣做有兩個好處：

+ 求值只需要乘法與加法。
+ 增量更新時，只需要加法，完全不需要乘法（見下節）。

== 半空間測試

對於一個 $n$ 邊的凸多邊形，內部恰好是所有 $n$ 個有向邊「左側」的交集。也就是說，像素 $(x, y)$ 在多邊形內部，若且唯若對每條邊 $k$ 都有

$
E_k(x, y) < 0.
$

這個條件成立的前提是多邊形的頂點以 CCW 順序（逆時針，在 $y$ 向上座標中面積為正）排列。若輸入為 CW 順序，只需將頂點序列反轉即可。

判斷 CCW 的方法是 shoelace 公式，計算有號面積的兩倍：

$
2 A = sum_{i=0}^{n-1} (x_i y_{i+1} - x_{i+1} y_i) quad ("索引對" n "取模").
$

$2A > 0$ 時為 CCW，$2A < 0$ 時為 CW。

== Bounding Box 方法

直接對整個畫面的每個 pixel 做測試效率極差。觀察到所有內部點都落在頂點的軸對齊包圍盒內，因此只需掃描

$
x_"min" <= x <= x_"max", quad y_"min" <= y <= y_"max",
$

其中

$
x_"min" = min_i x_i, quad x_"max" = max_i x_i, quad y_"min" = min_i y_i, quad y_"max" = max_i y_i.
$

包圍盒外的點必然在多邊形外部，不需要測試。對包圍盒內的每個候選像素做半空間測試，通過者著色。

== 增量更新

對每個像素從頭計算 $E_k(x, y) = a_k x + b_k y + c_k$ 需要乘法。注意到

$
E_k(x + 1, y) = E_k(x, y) + a_k,
$

$
E_k(x, y + 1) = E_k(x, y) + b_k.
$

因此，若已知 $(x, y)$ 的邊函數值，向右或向上移動一格只需一次加法。這讓 loop body 的核心操作可以完全消除乘法。

=== 掃描結構

整個掃描流程如下：

+ 在包圍盒左下角 $(x_"min", y_"min")$ 計算所有邊的初始值，總共需要 $n$ 次乘法，只做一次。
+ 對每一列 $y$（從 $y_"min"$ 到 $y_"max"$）：
  + 對每一欄 $x$（從 $x_"min"$ 到 $x_"max"$）：
    + 做半空間測試，若通過則著色。
    + 向右步進：對每條邊 $e_k arrow.l e_k + a_k$。
  + 一列結束後，$e_k$ 已超過 $x_"max"$ 一步，需要「rewind 並上移一行」：對每條邊

    $
    e_k arrow.l e_k - x_"span" dot a_k + b_k,
    $

    其中 $x_"span" = x_"max" - x_"min" + 1$ 是每列的掃描寬度。

這個更新式等價於：先退回 $x_"span"$ 步（rewind 至 $x_"min"$），再向上移一格。

== Top-Left Fill Rule

當兩個相鄰的 CCW 多邊形共享一條邊時，這條邊在兩個多邊形中的有向方向相反，因此它們的邊函數互為反號。對於剛好落在共享邊上的像素，兩個多邊形都會計算出 $E = 0$，必須指定規則決定哪一個多邊形認領它，否則會雙重著色或漏填。

Top-left fill rule 的規定：

- 頂邊（top edge）與左邊（left edge）上的邊界像素（$E = 0$）算作內部。
- 其他邊上的邊界像素算作外部。

對於 $y$ 向上的 CCW 多邊形：

- *左邊*：有向邊朝下，即 $"dy" < 0$（$a < 0$）。
- *頂邊*：水平邊，$"dy" = 0$（$a = 0$），且朝左，即 $"dx" < 0$（$b = -"dx" > 0$）。

以一個整數偏移量（bias）實現此規則：

$
cases(
  "bias" = -1 & "if top or left edge" quad (a < 0, "or" a = 0 "and" b > 0),
  "bias" = 0  & "otherwise",
)
$

內部判斷條件改為

$
E_k(x, y) + "bias"_k < 0,
$

等價於：
- 頂/左邊：$E_k < 1 arrow.l.double E_k <= 0$（含邊界）。
- 其他邊：$E_k < 0$（不含邊界）。

由於兩個相鄰多邊形的共享邊方向相反，若邊在 A 中是「頂或左」，在 B 中必然是「底或右」，因此 $E = 0$ 的像素恰好被其中一個多邊形納入，沒有重複也沒有遺漏。

== Algorithm

#algo(
  title: "FILL-POLYGON-HALF-SPACE",
  parameters: ($(v_0, dots, v_{n-1})$,),
  comment-prefix: [#sym.triangle.r #h(0.3em)],
  stroke: 0.5pt,
  radius: 3pt,
  indent-guides: 0.5pt + gray,
)[
  *if* $"signed\_area2"(v) < 0$ *then* reverse $v$ #comment[normalize to CCW]\
  *for* $i arrow.l 0$ to $n - 1$ *do*#i\
  $a_i arrow.l v_{i+1}.y - v_i.y$\
  $b_i arrow.l -(v_{i+1}.x - v_i.x)$\
  $c_i arrow.l -(a_i v_i.x + b_i v_i.y)$\
  *if* $a_i < 0$ or ($a_i = 0$ and $b_i > 0$) *then* $"bias"_i arrow.l -1$ *else* $"bias"_i arrow.l 0$#d\
  $x_"min", x_"max", y_"min", y_"max" arrow.l$ bounding box of $v$\
  $x_"span" arrow.l x_"max" - x_"min" + 1$\
  *for* $k arrow.l 0$ to $n - 1$ *do*#i\
  $e_k arrow.l a_k x_"min" + b_k y_"min" + c_k$ #comment[seed at bounding-box origin]#d\
  *for* $y arrow.l y_"min"$ to $y_"max"$ *do*#i\
  *for* $x arrow.l x_"min"$ to $x_"max"$ *do*#i\
  *if* $forall k: e_k + "bias"_k < 0$ *then* setPixel$(x, y)$\
  *for* $k arrow.l 0$ to $n - 1$ *do* $e_k arrow.l e_k + a_k$ #comment[step right]#d\
  *for* $k arrow.l 0$ to $n - 1$ *do* $e_k arrow.l e_k - x_"span" dot a_k + b_k$ #comment[rewind + step up]#d
]

== 具體範例

考慮三角形

$
v_0 = (0, 0), quad v_1 = (4, 0), quad v_2 = (2, 3).
$

=== 驗證 CCW

$
2A = (0 dot 0 - 4 dot 0) + (4 dot 3 - 2 dot 0) + (2 dot 0 - 0 dot 3) = 0 + 12 + 0 = 12 > 0. quad checkmark
$

=== 邊方程式

#table(
  columns: (auto, auto, auto, auto, auto, auto, auto),
  inset: 0.45em,
  stroke: 0.5pt,
  [*邊*], [*方向*], [*$a$*], [*$b$*], [*$c$*], [*top/left?*], [*bias*],
  [$e_0$: $v_0 -> v_1$], [$(4,0)$],  [$0$],  [$-4$], [$0$],   [否（底邊）], [$0$],
  [$e_1$: $v_1 -> v_2$], [$(-2,3)$], [$3$],  [$2$],  [$-12$], [否（右邊）], [$0$],
  [$e_2$: $v_2 -> v_0$], [$(-2,-3)$],[$-3$], [$2$],  [$0$],   [是（左邊）], [$-1$],
)

=== Bounding box

$
x_"min" = 0, quad x_"max" = 4, quad y_"min" = 0, quad y_"max" = 3, quad x_"span" = 5.
$

=== Seed at $(0, 0)$

$
e_0 = 0, quad e_1 = -12, quad e_2 = 0.
$

=== 掃描 $y = 1$ 的完整過程

$y = 1$ 列開始時，先把 $e_0, e_1, e_2$ 重置為 $E_k(x_"min" = 0,\ y = 1)$，計算得

$
e_0 = -4, quad e_1 = -10, quad e_2 = 2.
$

#table(
  columns: (auto, auto, auto, auto, auto, auto, auto, auto),
  inset: 0.45em,
  stroke: 0.5pt,
  [*$x$*], [*$e_0$*], [*$e_1$*], [*$e_2$*],
  [*$e_0 + 0$*], [*$e_1 + 0$*], [*$e_2 + (-1)$*], [*填色？*],
  [$0$], [$-4$],  [$-10$], [$2$],  [$-4$],  [$-10$], [$1 >= 0$],  [否],
  [$1$], [$-4$],  [$-7$],  [$-1$], [$-4$],  [$-7$],  [$-2 < 0$],  [*是*],
  [$2$], [$-4$],  [$-4$],  [$-4$], [$-4$],  [$-4$],  [$-5 < 0$],  [*是*],
  [$3$], [$-4$],  [$-1$],  [$-7$], [$-4$],  [$-1$],  [$-8 < 0$],  [*是*],
  [$4$], [$-4$],  [$2$],   [$-10$],[$-4$],  [$2 >= 0$], [$-11$], [否],
)

$x = 0$：$e_2 = 2$，$2 + (-1) = 1 >= 0$，左邊界外，排除。

$x = 1, 2, 3$：三條邊函數值加 bias 均 $< 0$，著色。

$x = 4$：$e_1 = 2$，$2 + 0 = 2 >= 0$，右邊界外，排除。

=== 完整填色結果

對 $y = 0$ 到 $y = 3$ 逐列掃描後，所有通過測試的像素為：

$
(1, 1), quad (2, 1), quad (3, 1), quad (2, 2).
$

$y = 0$：$e_0 = 0$，底邊 bias $= 0$，$0 + 0 = 0 >= 0$，整列排除（底邊為非 top-left，不含邊界）。

$y = 3$：$e_1(x=2) = 0$，右邊 bias $= 0$，$0 + 0 = 0 >= 0$，頂點 $(2, 3)$ 亦排除。

== 演算法性質

設包圍盒的寬度為 $W = x_"max" - x_"min" + 1$，高度為 $H = y_"max" - y_"min" + 1$。

=== 時間複雜度

迴圈共掃描 $W dot H$ 個候選像素，每個像素對 $n$ 條邊各做一次加法與比較，因此

$
T = O(n dot W dot H).
$

初始化（seed 與 bounding box）只需 $O(n)$，可以忽略。

主迴圈不含任何乘法或除法，所有更新只用整數加減法，這是 Half-Space Tests 相較於 naive 逐點計算（每次做 $n$ 次乘法）的關鍵優勢。

=== 空間複雜度

除了頂點與邊係數，掃描時只需一個大小為 $n$ 的整數陣列 $e[0..n-1]$。因此

$
S = O(n).
$

=== 與 Scanline 的比較

另一個常見的多邊形填充方法是 Scanline，其做法是對每一列 $y$ 求所有邊與該列的交點，排序後兩兩配對填色。

- Scanline 對一般（含凹）多邊形同樣有效，但需要維護 Active Edge Table（AET）與排序。
- Half-Space Tests 在概念上更簡單，對凸多邊形有效，且只需整數加法，沒有浮點交點計算。
- 兩者的時間複雜度都是 $O(n dot W dot H)$（對凸多邊形而言 Scanline 可以提早終止，但量級相同）。
