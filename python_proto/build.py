#!/usr/bin/env python3
"""
打包脚本 - 将启动器打包为单文件exe
"""

import subprocess
import sys
import shutil
from pathlib import Path


def main():
    # 清理旧构建
    for dir_name in ['build', 'dist']:
        if Path(dir_name).exists():
            shutil.rmtree(dir_name)
            print(f"Clean {dir_name}/")
    
    # 清理旧的 .spec 文件
    for spec_file in Path('.').glob('*.spec'):
        spec_file.unlink()
        print(f"Clean {spec_file.name}")
    
    # Anaconda 库路径（包含 DLL）
    anaconda_lib = Path(r"C:\ProgramData\anaconda3\Library\bin")
    
    # 构建命令列表
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--onefile",
        "--noconsole",
        "--name", "OpenClawLauncher",
        "--paths", str(anaconda_lib),
    ]
    
    # 添加 DLL 文件
    dlls = [
        "libjpeg.dll", "libpng16.dll", "libtiff.dll", "tiff.dll",
        "libwebp.dll", "libwebpmux.dll", "libwebpdemux.dll",
        "freetype.dll", "lcms2.dll", "openjp2.dll", "zlib.dll", "jpeg8.dll"
    ]
    
    for dll in dlls:
        dll_path = anaconda_lib / dll
        if dll_path.exists():
            cmd.extend(["--add-binary", f"{dll_path};."])
    
    # 添加其他参数
    cmd.extend([
        "--collect-all", "PIL",
        "--collect-all", "customtkinter",
        "--hidden-import", "PIL._imaging",
        "--hidden-import", "PIL._imagingft",
        "launcher.py"
    ])
    
    print("Building...")
    result = subprocess.run(cmd)
    
    if result.returncode == 0:
        print("\n" + "="*50)
        print("Build Success!")
        print("="*50)
        exe_path = Path("dist/OpenClawLauncher.exe").resolve()
        print(f"Output: {exe_path}")
        size_mb = exe_path.stat().st_size / 1024 / 1024
        print(f"Size: {size_mb:.1f} MB")
    else:
        print("\nBuild Failed")
        return 1
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
