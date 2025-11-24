param(
    [string]$asmfile
)

if (-not $asmfile) {
    Write-Host "Usage: .\test_pasm.ps1 <file.asm>"
    exit 1
}

$basename = [System.IO.Path]::GetFileNameWithoutExtension($asmfile)
$bin_c = "${basename}_c.bin"
$bin_pascal = "${basename}_pascal.bin"

# Compila con PASM C (con debug se richiesto)
Write-Host "Compilazione con PASM C..."
..\pasm.exe -d $asmfile $bin_c

# Compila con PASM Pascal
Write-Host "Compilazione con PASM Pascal..."
..\..\..\..\pasm.exe $asmfile $bin_pascal

# Mostra hexdump dei binari
Write-Host "`nHexdump PASM C:"
hexdump -C $bin_c

Write-Host "`nHexdump PASM Pascal:"
hexdump -C $bin_pascal

# Confronto binario
Write-Host "`nConfronto binario (fc /b):"
fc.exe /b $bin_c $bin_pascal

Write-Host "`nTest completato."
