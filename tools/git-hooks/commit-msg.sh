#!/usr/bin/env bash

commit_msg=$(cat "$1")
[[ "${commit_msg}" =~ ^\[.+\].*$ ]] || {
    echo -e "\e[31;1mCommit message check failed:\e[0m"
    echo -e "\e[31;1mMessage \e[33;1m${commit_msg}\e[0m \e[31;1mdoes not match the commit message scheme:\e[0m"
    echo -e "\e[36;1m [*] where * is one of Nova's components\e[0m"
    echo -e "\e[32;1mSee \e[35;1mdocs/git.md \e[32;1mfor more info\e[0m"
    exit 1
}

echo -e "\e[30;1mCommit message check passed.\e[0m"
exit 0
