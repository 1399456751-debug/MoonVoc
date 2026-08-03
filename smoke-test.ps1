$exe = Join-Path $PSScriptRoot "build\MoonVoc_artefacts\Release\Standalone\MoonVoc.exe"
$p = Start-Process -FilePath $exe -PassThru
Start-Sleep -Seconds 4
if ($p.HasExited) {
    Write-Output "CRASHED exit=$($p.ExitCode)"
} else {
    Write-Output "RUNNING OK"
    Stop-Process -Id $p.Id -Force
}
