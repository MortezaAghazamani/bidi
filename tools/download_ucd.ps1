# Downloads the Unicode Character Database files needed by this project.
#
# Usage:
#   .\tools\download_ucd.ps1

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot

$downloads = @(
    @{
        Url  = "https://www.unicode.org/Public/UCD/latest/ucd/UnicodeData.txt"
        Path = Join-Path $repoRoot "UnicodeData.txt"
    },
    @{
        Url  = "https://www.unicode.org/Public/UCD/latest/ucd/BidiBrackets.txt"
        Path = Join-Path $repoRoot "BidiBrackets.txt"
    },
    @{
        Url  = "https://www.unicode.org/Public/UCD/latest/ucd/BidiMirroring.txt"
        Path = Join-Path $repoRoot "BidiMirroring.txt"
    },
    @{
        Url  = "https://www.unicode.org/Public/UCD/latest/ucd/ArabicShaping.txt"
        Path = Join-Path $repoRoot "ArabicShaping.txt"
    },
    @{
        Url  = "https://www.unicode.org/Public/UCD/latest/ucd/BidiCharacterTest.txt"
        Path = Join-Path $repoRoot "tests\data\BidiCharacterTest.txt"
    }
)

foreach ($item in $downloads) {
    if (Test-Path $item.Path) {
        Write-Host "Already exists: $($item.Path)"
        continue
    }

    $dir = Split-Path -Parent $item.Path
    if (-not (Test-Path $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }

    Write-Host "Downloading $($item.Url)"
    Invoke-WebRequest -Uri $item.Url -OutFile $item.Path
}

Write-Host "Done."
