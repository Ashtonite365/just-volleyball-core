
[CmdletBinding()]
param(
    [string]$Path = (Join-Path $env:APPDATA 'Godot\app_userdata\volleyball\jv_mods\status.json'),
    [int]$IntervalMs = 100,
    [switch]$Raw
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

function Read-Shared([string]$p) {
    $fs = $null
    try {
        $share = [System.IO.FileShare]::ReadWrite -bor [System.IO.FileShare]::Delete
        $fs = New-Object System.IO.FileStream($p, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, $share)
        $sr = New-Object System.IO.StreamReader($fs, [System.Text.Encoding]::UTF8)
        return $sr.ReadToEnd()
    } catch {
        return $null
    } finally {
        if ($fs -ne $null) { $fs.Dispose() }
    }
}

function Show-Summary([string]$text) {
    try { $s = $text | ConvertFrom-Json } catch { Write-Host '(status.json not valid JSON yet)'; return }
    $libs = @()
    foreach ($n in @('core', 'textures', 'shots', 'goofy')) {
        $e = $s.libraries.$n
        if ($e -ne $null) { $libs += ('{0}={1}' -f $n, $e.state) }
    }
    $pin = 'MISMATCH'
    if ($s.version_pin.ok) { $pin = 'OK' }
    Write-Host ('mode={0}  full={1}  cosmetic={2}  sync={3}  pin={4}' -f $s.mode, $s.gate.full.active, $s.gate.cosmetic.active, $s.texture_sync.network, $pin) -ForegroundColor Cyan
    Write-Host ('  ' + ($libs -join '  ')) -ForegroundColor Cyan
    Write-Host ('  full: ' + $s.gate.full.reason)
    Write-Host ('  cosmetic: ' + $s.gate.cosmetic.reason)
}

Write-Host ('Watching ' + $Path + '  (Ctrl+C to stop)')
$last = $null
$missingShown = $false
while ($true) {
    if (Test-Path -LiteralPath $Path) {
        $missingShown = $false
        $text = Read-Shared $Path
        if ($text -ne $null -and $text -ne $last) {
            $last = $text
            Write-Host ''
            Write-Host ('==== ' + (Get-Date -Format 'yyyy-MM-dd HH:mm:ss.fff') + ' ====') -ForegroundColor Yellow
            if (-not $Raw) { Show-Summary $text }
            Write-Host $text
        }
    } elseif (-not $missingShown) {
        Write-Host ('(waiting: ' + $Path + ' does not exist yet - start the game with jv-mods installed)')
        $missingShown = $true
    }
    Start-Sleep -Milliseconds $IntervalMs
}
