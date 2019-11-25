#!/usr/bin/env pwsh

$CommitMessage=$1
if ($CommitMessage -match "^\[.+\].*$") {
    Write-Warning "\e[31;1mCommit message check failed:\e[0m"
    Write-Warning "\e[31;1mMessage \e[33;1m$CommitMessage\e[0m \e[31;1mdoes not match the commit message scheme:\e[0m"
    Write-Warning "\e[36;1m [*] where * is one of Nova's components\e[0m"
    Write-Warning "\e[32;1mSee \e[35;1mdocs/git.md \e[32;1mfor more info\e[0m"
    exit 1
}

Write-Host "\e[30;1mCommit message check passed.\e[0m"
exit 0
