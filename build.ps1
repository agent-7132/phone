$env:IDF_PATH="D:\esp\v6.0.2\esp-idf"
$env:IDF_PYTHON_ENV_PATH="C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env"
$env:IDF_TOOLS_PATH="C:\Espressif"
$env:ESP_IDF_VERSION="6.0.2"
$env:PATH="C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env\Scripts;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin;C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251204\openocd-esp32\bin;C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64;$env:PATH"

cd e:\phone

function Write-Color {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host "[$(Get-Date -Format 'HH:mm:ss')] $Message" -ForegroundColor $Color
}

function Write-Success {
    param([string]$Message)
    Write-Color $Message "Green"
}

function Write-Error {
    param([string]$Message)
    Write-Color $Message "Red"
}

function Write-Warning {
    param([string]$Message)
    Write-Color $Message "Yellow"
}

function Write-Status {
    param([string]$Message)
    Write-Color $Message "Cyan"
}

if ($args[0] -eq "clean") {
    Write-Status "Cleaning build directory..."
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    Remove-Item sdkconfig -ErrorAction SilentlyContinue
    Write-Success "Build cleaned."
    exit 0
}

if ($args[0] -eq "flash") {
    Write-Status "Flashing firmware to COM3..."
    python "$env:IDF_PATH\tools\idf.py" -p COM3 flash
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Flash completed successfully."
        exit 0
    } else {
        Write-Error "Flash failed."
        exit 1
    }
}

if ($args[0] -eq "monitor") {
    Write-Status "Starting monitor on COM3..."
    python "$env:IDF_PATH\tools\idf.py" -p COM3 monitor
    exit 0
}

if ($args[0] -eq "flash_monitor") {
    Write-Status "Flashing and starting monitor..."
    python "$env:IDF_PATH\tools\idf.py" -p COM3 flash monitor
    exit 0
}

$pushFlag = $false
$commitMsg = $null

for ($i = 0; $i -lt $args.Count; $i++) {
    if ($args[$i] -eq "push") {
        $pushFlag = $true
    } elseif ($args[$i] -eq "-m" -or $args[$i] -eq "--message") {
        if ($i + 1 -lt $args.Count) {
            $commitMsg = $args[$i + 1]
            $i++
        }
    }
}

Write-Status "Starting build..."
python "$env:IDF_PATH\tools\idf.py" install-deps 2>&1 | Out-Null

$buildOutput = python "$env:IDF_PATH\tools\idf.py" build 2>&1
if ($LASTEXITCODE -eq 0) {
    Write-Success "Build succeeded!"
    $sizeLine = $buildOutput | Where-Object { $_ -match "binary size" }
    if ($sizeLine) {
        Write-Status "Firmware size: $sizeLine"
    }
    
    if ($pushFlag) {
        Write-Status "Pushing changes to GitHub..."
        if ($commitMsg) {
            & powershell -ExecutionPolicy Bypass -File .\push_to_github.ps1 -CommitMessage $commitMsg -SkipBuild
        } else {
            & powershell -ExecutionPolicy Bypass -File .\push_to_github.ps1 -SkipBuild
        }
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Push completed successfully!"
        } else {
            Write-Error "Push failed."
            exit 1
        }
    }
    
    exit 0
} else {
    Write-Error "Build failed!"
    $buildOutput | Where-Object { $_ -match "error|Error" } | ForEach-Object { Write-Color $_ "Red" }
    exit 1
}