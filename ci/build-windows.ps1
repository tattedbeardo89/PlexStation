# Requires Docker Desktop's Linux containers with its WSL2 backend enabled.
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
& docker info --format '{{.OSType}}'
if ($LASTEXITCODE -ne 0) { throw 'Start Docker Desktop (WSL2 backend, Linux containers).' }
& docker build --progress plain -f ci/Dockerfile.ps5 -t plexstation-ps5:locked .
if ($LASTEXITCODE -ne 0) { throw 'Arch build-image preparation failed. See Docker output.' }
$SourceRoot = (Get-Location).Path
& docker run --rm --init -e BUILD_JOBS=2 `
    --mount "type=bind,source=$SourceRoot,target=/src" `
    --mount 'type=volume,source=plexstation-ps5-cache,target=/build' `
    plexstation-ps5:locked
if ($LASTEXITCODE -ne 0) { throw 'Native build failed. See artifacts/logs and artifacts/validation.json.' }
Write-Host 'Validated installation ZIP: artifacts/PlexStation-PPSA85301.zip'
Write-Host 'PS5 13.40 / kstuff Lite / ShadowMountPlus device compatibility remains untested.'
