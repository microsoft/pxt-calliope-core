#!/bin/sh

set -x
set -e
yotta version patch
git push --tags
git push
