#!/usr/bin/env pwsh

$ErrorActionPreference='Stop'

if (Test-Path ".git/hooks/.changed") {
    $GitRevision = $(git rev-parse HEAD)
    Remove-Item ".git/hooks/.changed"
    Set-Content -Path ".git/hooks/hooks-revision" -Value "$GitRevision"
    Set-Content -Path "tools/git-hooks/hooks-revision" -Value "$GitRevision"
    git add tools/git-hooks/hooks-revision | Out-Null
    git commit --amend --no-edit --no-verify | Out-Null
    Write-Host "\e[32;1mUpdated git hooks revision to \e[33;1m$GitRevision\e[0m"
}
