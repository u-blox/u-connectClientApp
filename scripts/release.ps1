<#
.SYNOPSIS
  Release u-connectClientApp: tag vX.Y.Z.YYMMDD, let CI build Linux and STM32, sign and upload the Windows exe.

.DESCRIPTION
  1. Checks the work tree is clean, on master and in sync with origin.
  2. Builds the Windows Release exe and signs it (the EV certificate is only available locally).
  3. Tags and pushes; .github/workflows/release.yml builds Linux + STM32 and creates a draft release.
  4. Waits for the draft, uploads the signed exe and its SHA-256, optionally publishes.

  X.Y.Z is APP_VERSION_MAJOR/MINOR/PATCH in app/CMakeLists.txt, YYMMDD is today.

.EXAMPLE
  .\scripts\release.ps1 -DryRun     # build and sign only, nothing is tagged or pushed
  .\scripts\release.ps1 -Publish    # full release, publish the draft when everything is uploaded
#>
[CmdletBinding()]
param(
    [string]$Thumbprint = 'EF3FD135F1CD669E0D7F4F2CF14FE1334EECD16E',
    [switch]$DryRun,
    [switch]$Publish,
    [int]$WaitMinutes = 30
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Fail($msg) { Write-Host "ERROR: $msg" -ForegroundColor Red; exit 1 }
function Step($msg) { Write-Host "`n== $msg" -ForegroundColor Cyan }
function Run($exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { Fail "$exe $($arguments -join ' ') failed ($LASTEXITCODE)" }
}

# Version and tag
$cmake = Get-Content (Join-Path $root 'app\CMakeLists.txt') -Raw
$parts = foreach ($n in 'MAJOR', 'MINOR', 'PATCH') {
    if ($cmake -notmatch "set\(APP_VERSION_$n\s+(\d+)\)") { Fail "APP_VERSION_$n not found in app/CMakeLists.txt" }
    $Matches[1]
}
$tag = 'v{0}.{1}.{2}.{3}' -f $parts[0], $parts[1], $parts[2], (Get-Date -Format 'yyMMdd')
Write-Host "Release tag: $tag"

# Preconditions
Step 'Checking repository'
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { Fail 'GitHub CLI (gh) not found' }
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) { Fail 'cmake not found' }
if (-not $DryRun) {
    Run gh @('auth', 'status')
    if (git status --porcelain) { Fail 'Work tree is not clean; commit or stash first' }
    if ((git branch --show-current) -ne 'master') { Fail 'Not on master' }
    Run git @('fetch', 'origin', 'master', '--tags')
    if ((git rev-parse HEAD) -ne (git rev-parse origin/master)) { Fail 'master differs from origin/master; push or pull first' }
    if (git tag -l $tag) { Fail "Tag $tag already exists" }
}

# SignTool
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' } | Sort-Object FullName -Descending | Select-Object -First 1
if (-not $signtool) { Fail 'signtool.exe not found (install the Windows SDK)' }
if (-not (Get-ChildItem Cert:\CurrentUser\My, Cert:\LocalMachine\My -ErrorAction SilentlyContinue | Where-Object Thumbprint -eq $Thumbprint)) {
    Fail "Certificate $Thumbprint not found in the certificate store (is the token plugged in?)"
}

# Build and sign
Step 'Building Windows Release'
Run cmake @('-S', 'app', '-B', 'build/windows', '-A', 'x64')
Run cmake @('--build', 'build/windows', '--config', 'Release', '--target', 'u-connectClientApp', '--clean-first')
$built = Join-Path $root 'app\bin\u-connectClientApp.exe'
if (-not (Test-Path $built)) { Fail "$built not found" }

Step 'Signing'
$dist = Join-Path $root 'dist'
New-Item -ItemType Directory -Force $dist | Out-Null
$exe = Join-Path $dist 'u-connectClientApp-windows-x64.exe'
Copy-Item $built $exe -Force
Run $signtool.FullName @('sign', '/sha1', $Thumbprint, '/tr', 'http://timestamp.digicert.com', '/td', 'sha256', '/fd', 'sha256', $exe)
Run $signtool.FullName @('verify', '/pa', $exe)
$hash = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLower()
"$hash  u-connectClientApp-windows-x64.exe" | Set-Content "$exe.sha256" -Encoding ascii
Write-Host "SHA-256: $hash"

if ($DryRun) { Write-Host "`nDry run done: $exe"; exit 0 }

# Tag and let CI build the rest
Step "Tagging and pushing $tag"
Run git @('tag', '-a', $tag, '-m', "Release u-connectClientApp $tag")
Run git @('push', 'origin', $tag)

Step 'Waiting for CI to create the draft release (Linux + 3 STM32 boards = 10 files)'
$expected = 10
$deadline = (Get-Date).AddMinutes($WaitMinutes)
$ready = $false
while ((Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 20
    $json = gh release view $tag --json isDraft,assets 2>$null
    if ($LASTEXITCODE -eq 0 -and $json) {
        $rel = $json | ConvertFrom-Json
        Write-Host ("  draft={0} assets={1}" -f $rel.isDraft, @($rel.assets).Count)
        if (@($rel.assets).Count -ge $expected) { $ready = $true; break }
    }
}
if (-not $ready) { Fail "CI did not finish in $WaitMinutes min; check the Actions tab, then run: gh release upload $tag `"$exe`" `"$exe.sha256`"" }

Step 'Uploading signed Windows exe'
Run gh @('release', 'upload', $tag, $exe, "$exe.sha256", '--clobber')

if ($Publish) {
    Step 'Publishing'
    Run gh @('release', 'edit', $tag, '--draft=false')
    Write-Host "Published: $(gh release view $tag --json url --jq .url)"
} else {
    Write-Host "`nDraft ready. Review it and publish: gh release edit $tag --draft=false"
}
