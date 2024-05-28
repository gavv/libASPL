#! /bin/bash
#
# This script builds a nice list of contributors (full name, github login, email).
# If you want to change your appearance in generated file, feel free to send a PR!
#

set -euo pipefail

function find_login() {
    local github_login="$(curl -s --get "https://api.github.com/search/users" \
        --data-urlencode "q=$1" \
        | jq -r '.items[0].login' 2>/dev/null)"

    if [[ "${github_login}" != "" ]] && [[ "${github_login}" != "null" ]]
    then
        echo "${github_login}"
    fi
}

# If the user has full name in their public github profile, use it.
# Otherwise we'll fallback to whatever we have in git.
function find_name() {
    local github_name="$(curl -s "https://api.github.com/users/$1" \
        | jq -r .name 2>/dev/null \
        | sed -r -e 's,^\s*,,' -e 's,\s*$,,')"

    if [[ "${github_name}" != "" ]] && [[ "${github_name}" != "null" ]]
    then
        echo "${github_name}"
    fi
}

# Github preserves git committer email if PR was merged using rebase-merge, but
# spoils it if it was merged using squash-merge, which is annoying! If the
# user originally provided a real email in their public git commits, we'll use
# it, otherwise we'll fall back to whatever we have in git after merging PR.
function find_email() {
    local reflog_email="$(git reflog --pretty=format:"%an <%ae>" | sort -u | \
        grep -vF users.noreply.github.com | grep -F "$1" | sed -re 's,.*<(.*)>,\1,')"

    if [[ "${reflog_email}" != "" ]]
    then
        echo "${reflog_email}"
        return
    fi

    local github_email="$(curl -s "https://api.github.com/users/$1/events/public" \
        | jq -r \
    '((.[].payload.commits | select(. != null))[].author | select(.name == "'$1'")).email' \
    2>/dev/null \
        | sort -u \
        | grep -v users.noreply.github.com \
        | head -1)"

    if [[ "${github_email}" != "" ]] && [[ "${github_email}" != "null" ]]
    then
        echo "${github_email}"
    fi
}

# Add contributor if not already added.
function add_if_new() {
    local file="$1"

    local commit_name="$2"
    local commit_email="$3"

    if grep -qiF "${commit_name}" "${file}" || grep -qiF "${commit_email}" "${file}"
    then
        return
    fi

    local github_login="$(find_login "${commit_email}")"
    if [ -z "${github_login}" ]
    then
        github_login="$(find_login "${commit_name}")"
    fi
    if [[ -z "${github_login}" ]]
    then
        if echo "${commit_email}" | grep -q users.noreply.github.com
        then
            github_login="$(echo "${commit_email}" | sed -re 's,^([0-9]+\+)?([^@]+).*$,\2,')"
        fi
    fi

    if grep -qiF "/${github_login}/" "${file}"
    then
        return
    fi

    local full_name="$(find_name "${github_login}")"
    if [ -z "${full_name}" ]
    then
        full_name="${commit_name}"
    fi
    full_name="$(echo "${full_name}" | sed -re 's/\S+/\u&/g')"

    local address=""
    if echo "${commit_email}" | grep -q users.noreply.github.com
    then
        if [[ ! -z "${github_login}" ]]
        then
            address="$(find_email "${github_login}")"
        fi
    else
        address="${commit_email}"
    fi
    if [[ -z "${address}" && ! -z "${github_login}" ]]
    then
        address="https://github.com/${github_login}"
    fi

    local result="${full_name}"
    if [ ! -z "${github_login}" ]
    then
        result="${result} \`${github_login}\`"
    fi
    if [ ! -z "${address}" ]
    then
        result="${result} (${address})"
    fi

    echo "adding ${result}" 1>&2
    echo "* ${result}"
}

# Find all new contributors and append to the file.
function add_contributors() {
    out_file="$1"
    repo_dir="$(pwd)"

    if [ ! -d "${repo_dir}" ]
    then
        return
    fi

    GIT_DIR="${repo_dir}"/.git \
           git log --encoding=utf-8 --full-history --reverse "--format=format:%at,%an,%ae" \
        | sort -u -t, -k3,3 \
        | sort -t, -k1n \
        | while read line
    do
        name="$(echo "${line}" | cut -d, -f2)"
        email="$(echo "${line}" | cut -d, -f3)"

        add_if_new "${out_file}" "${name}" "${email}" >> "${out_file}"
    done
}

result_file="AUTHORS.md"
temp_file="$(mktemp)"

cat "${result_file}" > "${temp_file}"
add_contributors "${temp_file}"

cat "${temp_file}" > "${result_file}"
rm "${temp_file}"

echo "updated ${result_file}"
