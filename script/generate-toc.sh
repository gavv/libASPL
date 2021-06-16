#! /bin/bash
set -euxo pipefail
markdown-toc --maxdepth 2 -i "$(dirname "$0")"/../README.md
