## Building

You most likely do not need to build this package - it's built automatically
by [pxt](https://pxt.io) when needed.

Otherwise, instructions follow:

- Install Yotta http://docs.yottabuild.org/#installing
- Install [srecord](http://srecord.sourceforge.net/); add it to your path

Run `./run.sh`.

You cannot use local version of this package in PXT. After making changes,
and making sure everything builds, you need to bump version in git
using `./bump.sh` and then update version number in `pxt-microbit/pxtarget.json`.
