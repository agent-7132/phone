$env:IDF_PATH="D:\esp\v6.0.2\esp-idf"
$env:IDF_PYTHON_ENV_PATH="C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env"
$env:IDF_TOOLS_PATH="C:\Espressif"
$env:ESP_IDF_VERSION="6.0.2"
$env:PATH="C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env\Scripts;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin;C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251204\openocd-esp32\bin;C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64;$env:PATH"

cd e:\phone

if ($args[0] -eq "clean") {
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    Remove-Item sdkconfig -ErrorAction SilentlyContinue
    Write-Output "Build cleaned."
    exit 0
}

if ($args[0] -eq "flash") {
    python "$env:IDF_PATH\tools\idf.py" -p COM3 flash
    exit 0
}

if ($args[0] -eq "monitor") {
    python "$env:IDF_PATH\tools\idf.py" -p COM3 monitor
    exit 0
}

python "$env:IDF_PATH\tools\idf.py" build