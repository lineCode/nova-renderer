#!/usr/bin/env bash

set -e

hooks_diff="$(git diff HEAD -- tools/git-hooks)"
hooks_revision_diff="$(git diff HEAD -- tools/git-hooks/hooks-revision)"
if [[ -n "${hooks_diff}" ]] && [[ "${hooks_revision_diff}" != "${hooks_diff}" ]] && [[ ! -f ".git/hooks/.changed" ]]
then
    echo -e "\e[32;1mDetected change in tools/hooks, marking changed and installing new hooks\e[0m"
    rm .git/hooks/*
    touch .git/hooks/.changed
    cp tools/git-hooks/* .git/hooks
    echo -e "\e[32;1mSourcing new pre-commit hook\e[0m"
    # shellcheck disable=SC2034
    # Used in the new pre-commit hook
    within_pre_commit_hook=1
    source ".git/hooks/pre-commit"
    exit 0
fi

if [[ ! -f ".git/hooks/.changed" ]]; then
    installed_revision="$(cat .git/hooks/hooks-revision)"
    source_revision="$(cat tools/git-hooks/hooks-revision)"
    if [[ "${installed_revision}" != "${source_revision}" ]]; then
        echo -e "\e[32;1mHooks revisions differ, installing new hooks\e[0m"
        echo -e "\e[32;1m\tInstalled revision: \e[33;1m${installed_revision}\e[0m"
        echo -e "\e[32;1m\tNew revision: \e[33;1m${source_revision}\e[0m"
        rm .git/hooks/*
        cp tools/git-hooks/* .git/hooks
    fi
fi
