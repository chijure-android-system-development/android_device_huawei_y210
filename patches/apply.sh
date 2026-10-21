#!/bin/bash
# Apply the Y210 patches to the CM10.1 platform projects.
#
# Layout: patches/<project path, '/' -> '_'>/NNNN-*.patch (git format-patch).
# Run from anywhere after `repo sync`:
#   device/huawei/y210/patches/apply.sh
# Already-applied patches are skipped, so it is safe to run more than once.

set -e

PATCHES="$(cd "$(dirname "$0")" && pwd)"
TOP="$(cd "$PATCHES/../../../.." && pwd)"

for dir in "$PATCHES"/*/; do
    project="$(basename "$dir" | tr '_' '/')"
    repo="$TOP/$project"

    if [ ! -d "$repo/.git" ] && [ ! -f "$repo/.git" ]; then
        echo "E: $project is not a git project" >&2
        exit 1
    fi

    for patch in "$dir"*.patch; do
        [ -e "$patch" ] || continue
        # mailinfo unfolds long Subject: headers and strips the [PATCH] tag.
        subject="$(git mailinfo /dev/null /dev/null < "$patch" | sed -n 's/^Subject: //p')"

        if git -C "$repo" log --format=%s -n 200 | grep -qxF "$subject"; then
            echo "skip   $project: $subject"
            continue
        fi

        echo "apply  $project: $subject"
        if ! git -C "$repo" am -3 --keep-cr "$patch"; then
            git -C "$repo" am --abort || true
            echo "E: failed to apply $(basename "$patch") in $project" >&2
            exit 1
        fi
    done
done
