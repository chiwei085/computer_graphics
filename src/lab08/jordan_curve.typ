= Polygon Fill: Jordan Curve Tests

== 問題定義

Lab07 的 half-space tests 對 *凸* 多邊形非常直接：只要每條有向邊都把內部夾在同一側即可。但一旦多邊形是凹的，單純的「全部在左側」就不再能描述內部；若進一步允許自交或重複繞行的路徑，還必須明確指定 fill rule 才能定義哪些區域要被填色。

因此，Lab08 改用更一般的觀點：先把多邊形邊界視為一條封閉曲線，再問一個像素中心點 $p = (x, y)$ 與這條曲線的拓樸關係。若能判斷 $p$ 在曲線內部或外部，就能決定是否著色。

== Jordan Curve 觀點

Jordan curve theorem 的核心直覺是：一條*不自交*的封閉曲線會把平面切成兩塊，分別是內部與外部。對 simple polygon 而言，這提供了「邊界包住一個內部區域」的理論基礎。

在實作上，對於 simple polygon，我們可以從測試點 $p$ 向某個固定方向射出一條 ray，觀察它與邊界交會的次數與方向，進而判斷內外。

本實驗介紹兩種常見規則：

- *Ray casting / crossing number*：只看交點數量的奇偶性。
- *Non-zero winding rule*：累積曲線繞過測試點的方向，檢查 winding number 是否非零。

若輸入不是 simple polygon，而是允許自交或重複繞行的 path，Jordan curve theorem 本身已不足以直接定義唯一的內部；這時需要額外指定 even-odd 或 non-zero 等 fill rule。兩種規則都能沿用同樣的 ray-based 計算框架，但它們對這類路徑的詮釋不同。

== Ray Casting（Crossing Number）

=== 基本想法

從測試點 $p = (x, y)$ 向右射出半直線

$
R(t) = (x + t, y), quad t > 0.
$

若這條 ray 與多邊形邊界相交的次數為 $"cross"$，則：

- $"cross"$ 為奇數時，$p$ 在內部。
- $"cross"$ 為偶數時，$p$ 在外部。

原因是每穿越一次邊界，ray 就會在「外部 / 內部」之間切換一次。

=== 半開區間規則

若 ray 恰好穿過頂點，可能同時碰到兩條邊，導致交點被重複計算。標準修正方式是把每條邊視為對 $y$ 的半開區間：

$
min(y_i, y_j) <= y < max(y_i, y_j).
$

也就是：

- 水平邊不計入 crossing。
- 只有當掃描高度 $y$ 落在邊的垂直範圍內，且包含下端點、不含上端點時，才考慮此邊。

這樣每個頂點只會由其中一條相鄰邊負責，不會 double count。

=== 交點測試

對邊 $v_i = (x_i, y_i)$ 到 $v_j = (x_j, y_j)$，若已知 $y$ 落在其有效範圍內，則交點的 $x$ 座標為

$
x_"int" = x_i + (y - y_i) frac (x_j - x_i, y_j - y_i).
$

若

$
x_"int" > x,
$

表示交點在測試點右方，ray 確實穿越此邊，令 $"cross" arrow.l "cross" + 1$。

== Non-Zero Winding Rule

=== 基本想法

crossing number 只看「有沒有穿過」，不在意曲線是往上還是往下繞。non-zero rule 更進一步，追蹤邊界繞著測試點的總方向。

定義 winding number $w$：

- 邊自下往上跨越掃描線，且測試點位於該邊左側，則 $w arrow.l w + 1$。
- 邊自上往下跨越掃描線，且測試點位於該邊右側，則 $w arrow.l w - 1$。

最後判斷：

- 若 $w != 0$，則 $p$ 在內部。
- 若 $w = 0$，則 $p$ 在外部。

這表示：只要邊界對點 $p$ 有淨繞行，就算在內部；若正反方向完全抵消，才算外部。

=== 方向判斷的外積形式

為了判斷點在有向邊哪一側，定義

$
"isLeft"(v_i, v_j, p) = (x_j - x_i)(y - y_i) - (x - x_i)(y_j - y_i).
$

這其實就是二維外積：

- $"isLeft" > 0$：$p$ 在有向邊左側。
- $"isLeft" = 0$：$p$ 在邊所在直線上。
- $"isLeft" < 0$：$p$ 在右側。

這裡的 upward / downward crossing，不是只看邊有沒有碰到 ray，而是看*有向邊*是否跨越測試點所在的水平線；接著再用 $"isLeft"$ 判斷交會是否位於測試點右方，等價於 ray 是否真的與該邊相交。

因此 winding rule 可寫成：

- 若 $y_i <= y < y_j$（ upward crossing ），且 $"isLeft" > 0$，則 $w arrow.l w + 1$。
- 若 $y_j <= y < y_i$（ downward crossing ），且 $"isLeft" < 0$，則 $w arrow.l w - 1$。

== 兩種規則的差異

對於一般簡單多邊形（simple polygon），兩種規則得到的內外分類相同，因為曲線不會自交，也不會對某個區域做多次方向繞行。

但若路徑允許自交或重複繞圈，差異就會出現：

- crossing number 只在意交點奇偶，所以「繞兩圈」可能又回到偶數，判成外部。
- non-zero rule 保留方向資訊，只要總繞行不為零，就仍判成內部。

因此在向量圖學中：

- *even-odd rule* 對應 crossing number。
- *non-zero rule* 對應 winding number。

== 邊界點處理

若測試點剛好落在某條邊上，直接套用 crossing 或 winding 規則容易受浮點誤差與半開區間細節影響。實作上通常先做一次 boundary test：

1. 檢查 $p$ 是否與邊共線。
2. 檢查 $p$ 是否落在該線段的 bounding box 內。

若兩者皆成立，可依需求回傳 $"OnBoundary"$；若任務只是決定 pixel 要不要填色，通常也可直接視為 inside，不再進入後續 crossing / winding 累積。

== Algorithm

=== Crossing Number

1. 若 $"pointOnBoundary"(p, v)$ 成立，依需求回傳 $"OnBoundary"$ 或 inside。
2. 設 $"cross" arrow.l 0$。
3. 對每條邊 $v_i -> v_j$（其中 $j = (i + 1) mod n$）：
   若 $(v_i.y <= p.y < v_j.y) or (v_j.y <= p.y < v_i.y)$，則計算

   $
   x_"int" arrow.l v_i.x + (p.y - v_i.y) frac (v_j.x - v_i.x, v_j.y - v_i.y).
   $

   若 $x_"int" > p.x$，則令 $"cross" arrow.l "cross" + 1$。
4. 回傳 $"cross" mod 2 = 1$。

=== Non-Zero Winding Rule

1. 若 $"pointOnBoundary"(p, v)$ 成立，依需求回傳 $"OnBoundary"$ 或 inside。
2. 設 $w arrow.l 0$。
3. 對每條邊 $v_i -> v_j$（其中 $j = (i + 1) mod n$）：

   $
   L arrow.l (v_j.x - v_i.x)(p.y - v_i.y) - (p.x - v_i.x)(v_j.y - v_i.y).
   $

   若 $v_i.y <= p.y < v_j.y$ 且 $L > 0$，則 $w arrow.l w + 1$。

   若 $v_j.y <= p.y < v_i.y$ 且 $L < 0$，則 $w arrow.l w - 1$。
4. 回傳 $w != 0$。

== 具體範例

考慮凹多邊形

$
v_0 = (0, 0), quad
v_1 = (4, 0), quad
v_2 = (4, 4), quad
v_3 = (2, 2), quad
v_4 = (0, 4).
$

取測試點

$
p = (3, 2).
$

=== Crossing Number

從 $p$ 向右射出 ray：$y = 2$。

逐邊檢查：

- $v_0 -> v_1$：水平邊，不計。
- $v_1 -> v_2$：$0 <= 2 < 4$ 成立，交點 $x_"int" = 4 > 3$，計一次 crossing。
- $v_2 -> v_3$：$2 <= 2 < 4$ 成立，但交點 $x_"int" = 2 < 3$，不計。
- $v_3 -> v_4$：$2 <= 2 < 4$ 成立，但交點 $x_"int" = 2 < 3$，不計。
- $v_4 -> v_0$：$0 <= 2 < 4$ 成立，但交點 $x_"int" = 0 < 3$，不計。

最後右方有效交點共有 $1$ 次，因此 $"cross" = 1$，$p$ 在內部。

=== Non-Zero Rule

對同一點計算 winding：

#table(
  columns: (auto, auto, auto, auto, auto),
  inset: 0.45em,
  stroke: 0.5pt,
  [*邊*], [* crossing 類型 *], [*$L = "isLeft"$*], [*更新*], [*累積 $w$*],
  [$v_0 -> v_1$], [水平邊], [$0$], [略過], [$0$],
  [$v_1 -> v_2$], [upward], [$4 > 0$], [$w arrow.l w + 1$], [$1$],
  [$v_2 -> v_3$], [downward], [$2 > 0$], [非右側，不減], [$1$],
  [$v_3 -> v_4$], [upward], [$-2 < 0$], [非左側，不加], [$1$],
  [$v_4 -> v_0$], [downward], [$12 > 0$], [非右側，不減], [$1$],
)

最後 $w = 1 != 0$，因此同樣判定 $p$ 在內部。

== Fill Polygon 的做法

若目標不是單點測試，而是將整個多邊形填色，作法與 Lab07 類似：仍然先算 bounding box，再對盒內每個像素中心做 point-in-polygon test。

設包圍盒為

$
x_"min" <= x <= x_"max", quad y_"min" <= y <= y_"max".
$

對每個 pixel center $(x + 0.5, y + 0.5)$：

- 若使用 crossing number，則執行一次 odd-even test。
- 若使用 non-zero rule，則執行一次 winding test。

通過者著色。

== 演算法性質

設多邊形有 $n$ 條邊，bounding box 寬高分別為

$
W = x_"max" - x_"min" + 1, quad H = y_"max" - y_"min" + 1.
$

=== 時間複雜度

一次 point-in-polygon test 需掃描全部 $n$ 條邊，因此

$
T_"query" = O(n).
$

若對整個 bounding box 做填色，共測試 $W dot H$ 個像素，則

$
T_"fill" = O(n dot W dot H).
$

這與 Lab07 的 half-space fill 在量級上相同，但常數因子較大，因為每個像素都要重新走訪所有邊，而且 crossing number 還包含除法或交點比較。

=== 空間複雜度

若把輸入頂點陣列一併計入儲存，總空間為

$
S = O(n).
$

但若只計算演算法額外使用的 auxiliary space，則兩種方法都只需要常數個計數器與暫存變數，因此

$
S_"aux" = O(1).
$

=== 與 Half-Space Tests 的比較

- Half-space tests 僅適用於凸多邊形，但每條邊的內外條件固定，且可用增量更新消除乘法。
- Jordan curve tests 適用於一般多邊形，包括凹多邊形。
- Crossing number 與 non-zero rule 都更通用，但 per-pixel 成本較高，實作也更需要注意頂點與邊界的特例。

== 小結

Jordan curve 觀點把「填多邊形」轉成「判斷點與封閉曲線的拓樸關係」：

- crossing number 用奇偶性定義內外，對應 even-odd fill rule。
- non-zero winding rule 用淨繞行次數定義內外，對應 non-zero fill rule。

對 simple polygon 而言，Jordan curve theorem 保證內外分離，而 crossing number 與 non-zero rule 會得到相同分類；對自交或重複繞行路徑，則必須額外指定 fill rule，且兩者可能給出不同答案。這也是向量圖形系統必須明確指定 fill rule 的原因。
