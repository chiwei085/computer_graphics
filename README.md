# Computer Graphics

![Language](https://img.shields.io/badge/C%2B%2B-17-blue)
![Build](https://img.shields.io/badge/Build-CMake-00599C)
![Graphics](https://img.shields.io/badge/OpenGL-Enabled-5586A4)
![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-333333)

1142_電腦圖學_356787

## 前置要求

- Operating system: Linux (including WSL2) with Ubuntu 22.04 or newer, or Windows
- Compiler: GCC, Clang, or MSVC
- Build system: CMake
- Build generator: Ninja
- Graphics libraries: OpenGL and GLUT (FreeGLUT)

### Linux 環境準備 (Ubuntu, Debian, Fedora, Arch)

以下提供常見 Linux 發行版的套件安裝指令, 請依你使用的系統擇一執行

#### Ubuntu, Debian (APT)

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  cmake \
  ninja-build \
  pkg-config \
  mesa-utils \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  freeglut3-dev
```

#### Fedora (DNF)

```bash
sudo dnf install -y \
  gcc-c++ \
  cmake \
  ninja-build \
  pkgconf-pkg-config \
  mesa-dri-drivers \
  mesa-libGL-devel \
  mesa-libGLU-devel \
  freeglut-devel \
  mesa-demos
```

#### Arch Linux (Pacman)

```bash
sudo pacman -S --needed \
  base-devel \
  cmake \
  ninja \
  pkgconf \
  mesa \
  glu \
  freeglut \
  mesa-demos
```

安裝後可做快速檢查

```bash
glxinfo | head
```

若系統找不到 `glxinfo`, 請安裝提供此工具的套件

- Ubuntu or Debian: `mesa-utils`
- Fedora: `mesa-demos`
- Arch: `mesa-demos`

### Windows 環境準備 (PowerShell, CLion workflow)

在 Windows 上, 建議搭配 CLion + Ninja toolchain, 依賴用 vcpkg 管理

#### 1. 安裝必要工具

先安裝以下工具

- Git
- CMake
- Ninja
- LLVM MinGW (Clang)

> [!note]
> 若你本機已有可用編譯器, 可略過編譯器安裝

```powershell
winget source update

winget install -e --id Git.Git
winget install -e --id Kitware.CMake
winget install -e --id Ninja-build.Ninja
winget install -e --id JetBrains.CLion
winget install -e --id LLVM.LLVM
```

安裝完成後, 可以重開 PowerShell 確認版本與環境變數是否已載入

#### 2. 安裝 vcpkg 與 FreeGLUT

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg install freeglut --triplet x64-windows
```

#### 3. 設定 `VCPKG_ROOT`

```powershell
setx VCPKG_ROOT "$PWD\vcpkg"
```

重新開啟終端機後生效, Windows 下 `cg build` 會自動帶入 vcpkg toolchain

#### 4. 設定 CLion

在 CLion 內請設定

- CMake generator: Ninja
- Toolchain: LLVM
- CMake project directory: 專案根目錄

CLion 會使用與 `cg build` 相同的 CMake 設定流程

### 自檢

```bash
uv run cg doctor
```

## 使用方式

你可以用三種方式建置與執行 Lab

### 1. 用 uv 使用 (推薦)

```bash
# 一次同步 or 建立 uv 環境
uv sync

# 安裝完成後，顯示可用 lab
uv run cg labs

# 建置指定 lab (e.g. lab01)
uv run cg build lab01

# 執行 lab
uv run cg run lab01

# 可以直接傳參數給程式
uv run cg run lab01 -- --help
```

### 2. 不用 uv 使用

```bash
# 建立 Python 虛擬環境
python -m venv .venv
source .venv/bin/activate

# 安裝 CLI（可編輯模式）
pip install -e .

# 清單、建置與執行
cg labs
cg build lab01
cg run lab01
```

### 3. 不用 cli 直接用 cmake 指令

```bash
# 產生 build 資料夾
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug

# 編譯所有目標
cmake --build build

# 手動執行對應可執行檔
./build/src/lab01/<binary_name>
```

> [!note]
> Windows 若使用 CLion 介面, 對應流程如下
> 1. `cmake -S . -B build ...` -> 開啟專案後由 CLion 自動 Configure
> 2. `cmake --build build` -> 在右上角選目標後按 Build
> 3. `./build/src/lab01/<binary_name>` -> 建立對應 Run Configuration 後按 Run
> 若已完成前置設定中的 Ninja 與 vcpkg, CLion 會使用相同工具鏈進行建置
