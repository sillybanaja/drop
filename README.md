![dropshowcase](https://github.com/sillybanaja/drop/assets/132526605/ec6c36c4-08eb-48a3-ac4a-af749faa6093)

---
### Install
To install or remove drop use the provided Makefile. Simply run a `make install` which
will install to **/usr/local/bin** (by default). To uninstall run `make uninstall` which
removes drop from **/usr/local/bin**, you also need to make sure you have [Xlib](https://en.wikipedia.org/wiki/Xlib) downloaded.
```sh
$ git clone https://github.com/sillybanaja/drop.git
$ cd drop
$ make install
```
### context
I wanted a simple way to drag files from the terminal into windows that force
drag-and-drop without having to open a file browser or interact with a GUI.
Existing solutions like [Dragon](https://github.com/mwh/dragon) by mwh are "bloated" (written in GTK) for what I
need, plus Dragon required me to physically drag the files to the target application with
a gui which is not ideal.

I wrote drop in C using [Xlib](https://en.wikipedia.org/wiki/Xlib), which is lightweight and widely available on
Unix-like systems. It takes a list of filenames as arguments and allows you to
drop them onto any application that supports XDND, without requiring you to physically
drag the files.

### usage
The basic usage is `drop filename ...`. Here's a example:
```sh
$ drop ~/Downloads/*.jpg file.txt /usr/local/bin/*
```
As you can see you can use globbing patterns like "**~/Downloads/*.jpg**" for example when
you pass in files.

After executing the command, you can navigate to the destination window and left click where you want to drop. After
you have dropped the files drop will exit. You can also press escape at any time to close the program.

### note
This implementation is a little crude, but it should work with most
applications that support drag-and-drop (XDND).

### contributions
Pull requests and bug reports are welcome. Please keep in mind that this project
is intended to remain small and focused on its core functionality. 

---

If you found this tool helpful give it a star so others can find it.
