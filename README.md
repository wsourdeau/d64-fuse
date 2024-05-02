# D64 Fuse

A fuse module to mount Commodore 64 D64 image files.

## Credit(s)

Written by Wolfgang Sourdeau  (<wolfgang@contre.com>) and distributed under the GNU General Public License V3.
Partly based on [DiskImagery64](https://github.com/ProbablyNotArtyom/DiskImagery64)  by Christian Vogelgsang (<chris@vogelgsang.org>) and Carson Herrington (<notartyomowo@gmail.com>). Thanks to them!

## Feature Overview

1. read-only access to volume contents (read-write operations are currently unavailable)
1. access rights and timestamps are based on the permissions associated with the image file
1. metadata support via xattr associated with the mount point and the individual files

## Usage

`d64-fuse --image=[D64 image] [mount point]`

### Example Command Sequence

```sh
wolfgang@wolfgang:/tmp$ d64-fuse --image=/home/wolfgang/Downloads/cbm-c64-demo.d64 /tmp/mount
wolfgang@wolfgang:/tmp xattr -l mount
d64fuse.image_filename: /home/wolfgang/Downloads/cbm-c64-demo.d64
d64fuse.disk_label: DEMO PROGRAMS
user.mime_type: application/x-c64-dir
wolfgang@wolfgang:/tmp$ cd /tmp/mount
wolfgang@wolfgang:/tmp/mount$ ls -l BALLOON
-rw-r--r-- 1 wolfgang wolfgang 913  4 feb  2019 BALLOON
wolfgang@wolfgang:/tmp/mount$ file BALLOON
BALLOON: Commodore C64 BASIC program, offset 0x081d, line 10, token (0x99) PRINT "\223":\201I\2620\24463:\227832\252I,0:\202, offset 0x0828, line 20, token (0x8d) GOSUB 60000
wolfgang@wolfgang:/tmp/mount$ xattr -l BALLOON
d64fuse.file_type: PRG
user.mime_type: application/x-c64-prg-file
d64fuse.is_splat: false
d64fuse.is_locked: false
```
