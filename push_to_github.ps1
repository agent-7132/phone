param(
    [string]$CommitMessage = $null,
    [string]$Branch = "main",
    [switch]$SkipBuild = $false
)

$ErrorActionPreference = "Continue"
$script:SUCCESS = $true

function Write-Status {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host "`n[$(Get-Date -Format 'HH:mm:ss')] $Message" -ForegroundColor $Color
}

function Write-Success {
    param([string]$Message)
    Write-Status $Message "Green"
}

function Write-Error {
    param([string]$Message)
    Write-Status $Message "Red"
    $script:SUCCESS = $false
}

function Write-Warning {
    param([string]$Message)
    Write-Status $Message "Yellow"
}

function Test-GitInstallation {
    Write-Status "Checking Git installation..."
    try {
        $gitVersion = git --version
        Write-Success "Git installed: $gitVersion"
        return $true
    } catch {
        Write-Error "Git not installed, please install Git first"
        return $false
    }
}

function Test-GitRepository {
    Write-Status "Checking Git repository..."
    try {
        $remote = git remote -v
        if (-not $remote) {
            Write-Error "Current directory is not a Git repository"
            return $false
        }
        Write-Success "Git repository configured"
        Write-Status "Remote: $($remote[0])"
        return $true
    } catch {
        Write-Error "Failed to check repository: $_"
        return $false
    }
}

function Get-GitStatus {
    Write-Status "Getting current status..."
    try {
        $status = git status --porcelain
        if (-not $status) {
            Write-Warning "No pending changes"
            return @()
        }
        Write-Success "Found $($status.Count) changed files"
        foreach ($line in $status) {
            Write-Status "  $line" "Cyan"
        }
        return $status
    } catch {
        Write-Error "Failed to get status: $_"
        return $null
    }
}

function Build-Project {
    if ($SkipBuild) {
        Write-Warning "Build verification skipped"
        return $true
    }
    
    Write-Status "Building project for verification..."
    $env:IDF_PATH="D:\esp\v6.0.2\esp-idf"
    $env:IDF_PYTHON_ENV_PATH="C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env"
    $env:IDF_TOOLS_PATH="C:\Espressif"
    $env:PATH="C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env\Scripts;C:\Espressif\tools\ninja\1.12.1;C:\Espressif\tools\cmake\3.30.2\bin;C:\Espressif\tools\riscv32-esp-elf\esp-15.2.0_20251204\riscv32-esp-elf\bin;C:\Espressif\tools\xtensa-esp-elf\esp-15.2.0_20251204\xtensa-esp-elf\bin;C:\Espressif\tools\openocd-esp32\v0.12.0-esp32-20251204\openocd-esp32\bin;C:\Espressif\tools\dfu-util\0.11\dfu-util-0.11-win64;$env:PATH"
    
    $pythonPath = "C:\Users\agent\.espressif\python_env\idf6.0_py3.12_env\Scripts\python.exe"
    $buildOutput = & $pythonPath "$env:IDF_PATH\tools\idf.py" build 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Success "Build succeeded"
        $sizeLine = $buildOutput | Where-Object { $_ -match "binary size" }
        if ($sizeLine) {
            Write-Status "Firmware size: $sizeLine" "Cyan"
        }
        return $true
    } else {
        Write-Error "Build failed, please fix build errors first"
        $buildOutput | Where-Object { $_ -match "error|Error" } | ForEach-Object { Write-Status $_ "Red" }
        return $false
    }
}

function Generate-CommitMessage {
    Write-Status "Generating commit message..."
    try {
        $status = git status --porcelain
        $categories = @{
            "feat" = @()
            "fix" = @()
            "docs" = @()
            "refactor" = @()
            "chore" = @()
        }
        
        foreach ($line in $status) {
            $file = $line.Substring(3)
            if ($file -match "\.(md|txt)$") {
                $categories["docs"] += $file
            } elseif ($file -match "\.(c|h|cpp|hpp)$") {
                if ($file -match "app_|app/") {
                    $categories["feat"] += $file
                } elseif ($file -match "fix|error|bug") {
                    $categories["fix"] += $file
                } else {
                    $categories["refactor"] += $file
                }
            } else {
                $categories["chore"] += $file
            }
        }
        
        $messageParts = @()
        if ($categories["feat"].Count -gt 0) {
            $messageParts += "feat: add new features"
        }
        if ($categories["fix"].Count -gt 0) {
            $messageParts += "fix: fix issues"
        }
        if ($categories["docs"].Count -gt 0) {
            $messageParts += "docs: update documentation"
        }
        if ($categories["refactor"].Count -gt 0) {
            $messageParts += "refactor: code refactoring"
        }
        if ($categories["chore"].Count -gt 0) {
            $messageParts += "chore: misc updates"
        }
        
        $message = $messageParts -join ", "
        if (-not $message) {
            $message = "chore: update project"
        }
        
        Write-Success "Generated commit message: $message"
        return $message
    } catch {
        Write-Error "Failed to generate commit message: $_"
        return "chore: update project"
    }
}

function Commit-Changes {
    param([string]$Message)
    Write-Status "Committing changes..."
    try {
        git add -A
        git commit -m $Message
        Write-Success "Commit succeeded"
        
        $commitHash = git rev-parse --short HEAD
        Write-Status "Commit hash: $commitHash" "Cyan"
        return $true
    } catch {
        Write-Error "Commit failed: $_"
        return $false
    }
}

function Push-Changes {
    param([string]$TargetBranch)
    Write-Status "Pushing changes to remote repository..."
    try {
        $pushOutput = git push origin $TargetBranch 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Success "Push succeeded"
            return $true
        } else {
            Write-Error "Push failed"
            if ($pushOutput -match "rejected|non-fast-forward") {
                Write-Warning "Remote branch has updates, please pull first"
                Write-Status "Suggested: git pull origin $TargetBranch" "Cyan"
            } elseif ($pushOutput -match "443|timeout|network") {
                Write-Warning "Network connection failed, please check network"
            } elseif ($pushOutput -match "authentication|login") {
                Write-Warning "Authentication failed, please check GitHub credentials"
                Write-Status "Suggested: configure SSH key or use GitHub CLI" "Cyan"
            }
            return $false
        }
    } catch {
        Write-Error "Push execution failed: $_"
        return $false
    }
}

function Show-Summary {
    Write-Status "`n========== Operation Complete ==========" "Magenta"
    if ($script:SUCCESS) {
        Write-Success "All operations completed successfully!"
        Write-Status "Repository: https://github.com/agent-7132/phone" "Cyan"
        Write-Status "Branch: $Branch" "Cyan"
    } else {
        Write-Error "Operation failed, please check error messages"
    }
}

Write-Status "========== Push to GitHub ==========" "Magenta"

if (-not (Test-GitInstallation)) {
    Show-Summary
    exit 1
}

if (-not (Test-GitRepository)) {
    Show-Summary
    exit 1
}

$changes = Get-GitStatus
if (-not $changes -or $changes.Count -eq 0) {
    Write-Warning "No changes to commit"
    Show-Summary
    exit 0
}

if (-not (Build-Project)) {
    Show-Summary
    exit 1
}

if (-not $CommitMessage) {
    $CommitMessage = Generate-CommitMessage
} else {
    Write-Status "Using custom commit message: $CommitMessage" "Cyan"
}

if (-not (Commit-Changes $CommitMessage)) {
    Show-Summary
    exit 1
}

if (-not (Push-Changes $Branch)) {
    Show-Summary
    exit 1
}

Show-Summary