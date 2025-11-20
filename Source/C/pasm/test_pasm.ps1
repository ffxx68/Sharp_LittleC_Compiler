param(
    [string]$asmfile
)

if (-not $asmfile) {
    Write-Host "Usage: .\test_pasm.ps1 <file.asm>"
    exit 1
}

$base = [System.IO.Path]::GetFileNameWithoutExtension($asmfile)
$bin_c = $base + "_c.bin"
$bin_pascal = $base + "_pascal.bin"

# Compila con PASM C locale
.\pasm.exe $asmfile $bin_c

# Compila con PASM Pascal originale
..\..\..\pasm.exe $asmfile $bin_pascal

# Confronta i due binari
fc.exe /b $bin_c $bin_pascal
