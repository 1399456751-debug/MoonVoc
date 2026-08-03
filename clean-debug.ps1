$p = 'E:\VST Effects Plugin Collection\moonvoc\Source\dsp\VoiceEq.cpp'
$lines = Get-Content -Encoding UTF8 $p
$out = @()
foreach ($l in $lines) {
    if ($l -match 'Dbg' -or $l -match 'TRACE' -or $l -match 'cstdio' -or $l -match 'double e0' -or $l -match 'e0 \+=' -or $l -match 'sqrt\(e0') { continue }
    $out += $l
}
Set-Content -Encoding UTF8 -Path $p -Value ($out -join "`n")
Write-Output "cleaned"
