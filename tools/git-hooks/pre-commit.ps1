#!/usr/bin/env pwsh

$ErrorActionPreference='Stop'

$HooksDiff = "$(git diff HEAD -- tools/git-hooks)"
$HooksRevisionDiff = "$(git diff HEAD -- tools/git-hooks/hooks-revision)"

$Hooks
$HooksChangedFileExists = $(Test-Path ".git/hooks/.changed")

if ($HooksDiff -and $($HooksRevisionDiff -ne $HooksDiff) -and $(-Not $HooksChangedFileExists)) {
    Write-Host "\e[32;1mDetected change in tools/hooks, marking changed and installing new hooks\e[0m"
    Remove-Item .git/hooks/*
    touch .git/hooks/.changed
    Copy-Item tools/git-hooks/* .git/hooks
    Write-Host "\e[32;1mSourcing new pre-commit hook\e[0m"
    # shellcheck disable=SC2034
    # Used in the new pre-commit hook
    $within_pre_commit_hook = 1
    source ".git/hooks/pre-commit"
    exit 0
fi

if (-Not $(Test-File ".git/hooks/.changed")) {
    $InstalledRevision = Get-Content .git/hooks/hooks-revision
    $SourceRevision = Get-Content tools/git-hooks/hooks-revision
    if ($InstalledRevision -ne $SourceRevision) {
        Write-Host "\e[32;1mHooks revisions differ, installing new hooks\e[0m"
        Write-Host "\e[32;1m\tInstalled revision: \e[33;1m$InstalledRevision\e[0m"
        Write-Host "\e[32;1m\tNew revision: \e[33;1m$SourceRevision\e[0m"
        Remove-Item .git/hooks/*
        Copy-Item tools/git-hooks/* .git/hooks
    }
}
