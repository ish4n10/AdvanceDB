# PowerShell build script for AdvanceDB

param(
    [switch]$Run,
    [switch]$Clean
)

if ($Clean) {
    if (Test-Path build) {
        Remove-Item -Recurse -Force build
        Write-Host "Cleaned build directory" -ForegroundColor Green
    }
    exit
}

# Create build directory if it doesn't exist
if (!(Test-Path build)) {
    New-Item -ItemType Directory -Path build | Out-Null
}

# Configure and build
Push-Location build
try {
    if (!(Test-Path CMakeCache.txt)) {
        Write-Host "Configuring CMake..." -ForegroundColor Cyan
        cmake ..
    }
    
    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build .
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Build successful!" -ForegroundColor Green
        
        if ($Run) {
            Write-Host "Running validation test..." -ForegroundColor Cyan
            .\bin\test_validation.exe
        }
    } else {
        Write-Host "Build failed!" -ForegroundColor Red
    }
} finally {
    Pop-Location
}

