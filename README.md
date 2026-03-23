![dropshowcase](https://github.com/sillybanaja/drop/assets/132526605/ec6c36c4-08eb-48a3-ac4a-af749faa6093)

## context
drop is an alternative to [Dragon](https://github.com/mwh/dragon).. with Dragon you physically drag from a GTK window, whereas with drop, you run it, move your cursor to the desired target and.. click. written in C with x11 libraries only.

## install
*dependencies: libx11, libxi*
##### build from source
```sh
git clone https://github.com/sillybanaja/drop.git
cd drop
sudo make install
```
##### arch linux AUR
```sh
paru -S drop-xdnd
```

## usage
```sh
drop file.txt ~/Downloads/*.jpg    # multiple files
ls *.png | drop                    # pipe support
```
after running, move your cursor to the target window and left click to drop.. press **\<escape\>** to cancel without dropping.

drop exits automatically once the files are accepted.

## notes
- does not support xdg-desktop-portal (flatpak) or xwayland
- pull requests and bug reports welcome, keep changes minimal

