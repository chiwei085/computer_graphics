import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Annotated

import typer

app = typer.Typer(
    help="Helpers for building and running computer graphics labs."
)

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SRC_ROOT = PROJECT_ROOT / "src"
DEFAULT_BUILD_DIR = PROJECT_ROOT / "build"

NO_EXECUTABLE_ERROR = (
    "No runnable executable found in build/src/<lab>. "
    "Run `cg build <lab>` first."
)
EXE_OPTION_HELP = (
    "Executable name/path under build/src/<lab> when multiple exist."
)


def is_windows() -> bool:
    return sys.platform.startswith("win")


def run_cmd(cmd: list[str], cwd: Path = PROJECT_ROOT) -> None:
    rc = subprocess.run(cmd, cwd=cwd).returncode
    if rc != 0:
        raise typer.Exit(code=rc)


def natural_lab_key(name: str) -> tuple[int, str]:
    m = re.search(r"(\d+)", name)
    return (int(m.group(1)), name) if m else (10**9, name)


def discover_labs() -> list[str]:
    if not SRC_ROOT.exists():
        return []

    labs = [
        p.name
        for p in SRC_ROOT.iterdir()
        if p.is_dir()
        and p.name.startswith("lab")
        and (p / "CMakeLists.txt").exists()
    ]
    return sorted(labs, key=natural_lab_key)


def default_vcpkg_root() -> Path | None:
    env = os.environ.get("VCPKG_ROOT")
    if env and Path(env).exists():
        return Path(env)

    local = PROJECT_ROOT / "vcpkg"
    return local if local.exists() else None


def platform_cmake_args() -> list[str]:
    if not is_windows():
        return []

    vcpkg_root = default_vcpkg_root()
    if vcpkg_root is None:
        raise typer.BadParameter(
            "Windows build requires vcpkg. "
            "Set VCPKG_ROOT or clone vcpkg into ./vcpkg."
        )

    toolchain = vcpkg_root / "scripts" / "buildsystems" / "vcpkg.cmake"
    if not toolchain.exists():
        raise typer.BadParameter(
            f"vcpkg toolchain not found: {toolchain}. "
            "Run bootstrap-vcpkg first."
        )

    triplet = os.environ.get("VCPKG_TARGET_TRIPLET", "x64-windows")
    return [
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DVCPKG_TARGET_TRIPLET={triplet}",
    ]


def parse_lab_input(user_input: str, labs: list[str]) -> str:
    if not labs:
        raise typer.BadParameter(
            "No labs found under src/labxx with sub CMakeLists.txt"
        )

    value = user_input.strip()
    if not value:
        raise typer.BadParameter("Please enter a lab name or number")

    if value in labs:
        return value

    if value.isdigit():
        idx = int(value)
        if 1 <= idx <= len(labs):
            return labs[idx - 1]

    lowered = value.lower()
    matches = [lab for lab in labs if lowered in lab.lower()]
    if len(matches) == 1:
        return matches[0]

    msg = f"Unknown lab '{value}'. Use one of: {', '.join(labs)}"
    if len(matches) > 1:
        msg += f". Ambiguous matches: {', '.join(matches)}"
    raise typer.BadParameter(msg)


def pick_lab(user_input: str | None, labs: list[str]) -> str:
    if user_input and user_input.strip():
        return parse_lab_input(user_input, labs)

    typer.echo("Available labs:")
    for i, lab in enumerate(labs, start=1):
        typer.echo(f"  {i:>2}. {lab}")

    while True:
        answer = typer.prompt("Select lab (number or name)", default="1")
        try:
            return parse_lab_input(answer, labs)
        except typer.BadParameter as err:
            typer.secho(str(err), fg=typer.colors.RED)


def cmake_configure(build_dir: Path, build_type: str) -> None:
    build_dir.mkdir(parents=True, exist_ok=True)
    run_cmd(
        [
            "cmake",
            "-S",
            str(PROJECT_ROOT),
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            f"-DCMAKE_BUILD_TYPE={build_type}",
            *platform_cmake_args(),
        ]
    )


def build_lab(lab: str, build_dir: Path, build_type: str) -> None:
    cmake_configure(build_dir, build_type)
    run_cmd(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--target",
            f"src/{lab}/all",
        ]
    )


def is_runnable_binary(path: Path) -> bool:
    if (
        path.is_dir()
        or "CMakeFiles" in path.parts
        or path.name.endswith(".dbg")
    ):
        return False

    if is_windows():
        return path.suffix.lower() in {".exe", ".bat", ".cmd"}

    if path.suffix.lower() in {".ninja", ".cmake", ".txt", ".json", ".pdb"}:
        return False

    return os.access(path, os.X_OK)


def find_executables_for_lab(
    lab: str, build_dir: Path
) -> tuple[Path, list[Path]]:
    lab_dir = build_dir / "src" / lab
    if not lab_dir.exists():
        return lab_dir, []

    exes = [p for p in lab_dir.rglob("*") if is_runnable_binary(p)]
    exes = sorted(set(exes), key=lambda p: p.relative_to(lab_dir).as_posix())
    return lab_dir, exes


def _exe_keys(exe: Path, lab_dir: Path) -> set[str]:
    rel = exe.relative_to(lab_dir).as_posix()
    return {exe.name, rel}


def _find_exe(exes: list[Path], lab_dir: Path, query: str) -> Path | None:
    query_norm = query.lower()
    return next(
        (
            exe
            for exe in exes
            if query_norm in {key.lower() for key in _exe_keys(exe, lab_dir)}
        ),
        None,
    )


def _prompt_select_exe(exes: list[Path], lab_dir: Path) -> Path:
    typer.echo("Multiple executables found:")
    for i, exe in enumerate(exes, start=1):
        rel = exe.relative_to(lab_dir).as_posix()
        typer.echo(f"  {i:>2}. {rel}")

    while True:
        ans = typer.prompt(
            "Select executable (number or name/path)", default="1"
        ).strip()
        if ans.isdigit():
            idx = int(ans)
            if 1 <= idx <= len(exes):
                return exes[idx - 1]

        hit = _find_exe(exes, lab_dir, ans)
        if hit:
            return hit

        typer.secho("Invalid selection", fg=typer.colors.RED)


def pick_executable(
    exes: list[Path], lab_dir: Path, requested: str | None
) -> Path:
    if not exes:
        raise typer.BadParameter(NO_EXECUTABLE_ERROR)

    if requested:
        hit = _find_exe(exes, lab_dir, requested)
        if hit:
            return hit
        known = ", ".join(exe.relative_to(lab_dir).as_posix() for exe in exes)
        raise typer.BadParameter(
            f"Unknown executable '{requested}'. Available: {known}"
        )

    if len(exes) == 1:
        return exes[0]

    return _prompt_select_exe(exes, lab_dir)


def print_check(ok: bool, name: str, detail: str) -> None:
    status = "OK" if ok else "FAIL"
    color = typer.colors.GREEN if ok else typer.colors.RED
    typer.secho(f"[{status}] {name}: {detail}", fg=color)


def check_command_exists(command: str) -> tuple[bool, str]:
    loc = shutil.which(command)
    return (loc is not None, loc or "not found in PATH")


def check_linux_freeglut_header() -> tuple[bool, str]:
    for header in (
        Path("/usr/include/GL/freeglut.h"),
        Path("/usr/local/include/GL/freeglut.h"),
    ):
        if header.exists():
            return True, str(header)
    return False, "freeglut.h not found (install freeglut3-dev)"


def check_windows_vcpkg() -> list[tuple[bool, str, str]]:
    root = default_vcpkg_root()
    if root is None:
        return [
            (
                False,
                "vcpkg root",
                "set VCPKG_ROOT or clone vcpkg into ./vcpkg",
            )
        ]

    toolchain = root / "scripts" / "buildsystems" / "vcpkg.cmake"
    toolchain_exists = toolchain.exists()
    triplet = os.environ.get("VCPKG_TARGET_TRIPLET", "x64-windows")

    return [
        (True, "vcpkg root", str(root)),
        (
            toolchain_exists,
            "vcpkg toolchain",
            str(toolchain) if toolchain_exists else "missing vcpkg.cmake",
        ),
        (True, "vcpkg triplet", triplet),
    ]


def check_platform_dependencies() -> list[tuple[bool, str, str]]:
    if is_windows():
        return check_windows_vcpkg()

    ok, detail = check_linux_freeglut_header()
    return [(ok, "freeglut header", detail)]


@app.command("labs")
def list_labs() -> None:
    """List available lab folders under src/."""
    labs = discover_labs()
    if not labs:
        typer.echo("No labs found")
        raise typer.Exit(code=1)

    for lab in labs:
        typer.echo(lab)


@app.command("build")
def build(
    lab: Annotated[
        str | None,
        typer.Argument(help="Lab name, e.g. lab01. Leave empty for prompt."),
    ] = None,
    build_type: Annotated[str, typer.Option("--build-type", "-t")] = "Debug",
    build_dir: Annotated[
        Path, typer.Option("--build-dir", "-B")
    ] = DEFAULT_BUILD_DIR,
) -> None:
    """Build a specific lab using CMake + Ninja."""
    labs = discover_labs()
    selected = pick_lab(lab, labs)
    typer.echo(f"Building {selected} ({build_type})...")
    build_lab(selected, build_dir, build_type)


@app.command(
    "run",
    context_settings={"allow_extra_args": True, "ignore_unknown_options": True},
)
def run(
    ctx: typer.Context,
    lab: Annotated[
        str | None,
        typer.Argument(help="Lab name, e.g. lab01. Leave empty for prompt."),
    ] = None,
    build_dir: Annotated[
        Path, typer.Option("--build-dir", "-B")
    ] = DEFAULT_BUILD_DIR,
    executable: Annotated[
        str | None,
        typer.Option("--exe", "-e", help=EXE_OPTION_HELP),
    ] = None,
) -> None:
    """Run a previously built lab executable. Pass extra args after command."""
    labs = discover_labs()
    selected = pick_lab(lab, labs)

    lab_dir, exes = find_executables_for_lab(selected, build_dir)
    exe = pick_executable(exes, lab_dir, executable)

    cmd = [str(exe), *ctx.args]
    typer.echo(f"Running: {' '.join(cmd)}")
    run_cmd(cmd, cwd=PROJECT_ROOT)


@app.command("doctor")
def doctor() -> None:
    """Check local build prerequisites for this project."""
    checks: list[tuple[bool, str, str]] = [
        (True, "platform", "windows" if is_windows() else "linux/unix")
    ]

    for name in ("cmake", "ninja"):
        ok, detail = check_command_exists(name)
        checks.append((ok, name, detail))

    checks.extend(check_platform_dependencies())

    all_ok = True
    for ok, name, detail in checks:
        print_check(ok, name, detail)
        all_ok = all_ok and ok

    if not all_ok:
        raise typer.Exit(code=1)


def main() -> None:
    app()


if __name__ == "__main__":
    main()
