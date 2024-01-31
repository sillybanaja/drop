// See LICENSE file for license details.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#define XDND_PROTOCOL_VERSION 5
#define BUFFER_SIZE 1024

struct {
    Display *dpy;
    Window root, scr, win;
    int finished;
    char ** file_paths;
    size_t file_paths_size;
    struct {
        Window target;
        int x_root, y_root;
    } target_info;
    struct {
        Atom delete_window;
        Atom data_type;
        Atom xdnd_enter;
        Atom xdnd_finished;
        Atom xdnd_selection;
        Atom xdnd_leave;
        Atom xdnd_position;
        Atom xdnd_drop;
        Atom xdnd_status;
        Atom xdnd_aware;
        Atom xdnd_action_copy;
    } atom;
} source = {
    .scr = None, .win = None, .root = None,
    .target_info.target = None,
    .finished = 0,
};

static void fail(const char*, ...);
static void get_target_info();
static int is_target_xdnd_aware();
static void send_selection_notify(XSelectionRequestEvent*);

void
fail (const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    exit(1);
}

void // assumes XGrabPointer
get_target_info() {
    int dxy; unsigned int dmask; Window droot;
    XQueryPointer(source.dpy, source.root, &droot, &source.target_info.target,
            &source.target_info.x_root, &source.target_info.y_root, &dxy, &dxy, &dmask);
}

int
is_target_xdnd_aware() {
    Atom actual = None;
    int dformat;
    unsigned long dcount, dremaining;
    unsigned char* data = NULL;
    
    XGetWindowProperty(source.dpy, source.target_info.target, source.atom.xdnd_aware,
            0, BUFFER_SIZE, False, AnyPropertyType, &actual, &dformat, &dcount, &dremaining, &data);

    return (actual != None && data[0] <= XDND_PROTOCOL_VERSION);
}

void
send_selection_notify(XSelectionRequestEvent *xselectionrequest) {
    // 1 is for nullbyte, +9 is for file:// and carrage return
    // file:// is ilegal over file:/hostname/path. But fuck it we ball
    size_t property_data_size = 1;
    for(int i=0;i<source.file_paths_size;property_data_size+=strlen(source.file_paths[i])+9,i++);

    char *property_data = calloc(property_data_size, 1);
    for(int i=0;i<source.file_paths_size;i++) {
        char path[BUFFER_SIZE];
        snprintf(path, BUFFER_SIZE, "file://%s\r\n", source.file_paths[i]);
        strcat(property_data, path);
    }
    strcat(property_data, "\0");

    XChangeProperty(source.dpy, xselectionrequest->requestor, xselectionrequest->property,
            source.atom.data_type, 8, PropModeReplace, (unsigned char *)property_data,
            property_data_size-1);

    free(property_data);

    XSendEvent(source.dpy, source.target_info.target, False, 0, &(XEvent){
            .xselection.type = SelectionNotify,
            .xselection.display = source.dpy,
            .xselection.requestor = xselectionrequest->requestor,
            .xselection.selection = xselectionrequest->selection,
            .xselection.target = xselectionrequest->target,
            .xselection.property = xselectionrequest->property,
            .xselection.time = xselectionrequest->time,
            });
}

int
main(int argc, char* argv[]) {
    if(argc <= 1)
        fail("usage: drop filename ...\n");
    if(!(source.dpy = XOpenDisplay(NULL)))
        fail("could not open display\n");

    source.file_paths_size = argc-1;
    source.file_paths = argv+1;
    for(int i=0;i<argc-1;i++) {
        const char* current = source.file_paths[i];
        source.file_paths[i] = realpath(source.file_paths[i], NULL);
        if(!source.file_paths[i])
            fail("invalid file path: \"%s\"\n", current);
    }
    source.scr = DefaultScreen(source.dpy);
    source.root = RootWindow(source.dpy, source.scr);
    source.win = XCreateSimpleWindow(source.dpy, source.root, 0, 0, 10, 10, 0, 0, 0);
    XSelectInput(source.dpy, source.win, ButtonPressMask|KeyPressMask);

    XGrabButton(source.dpy, Button1, None, source.root, False, ButtonPressMask,
                GrabModeAsync, GrabModeAsync, None, None);
    XGrabKey(source.dpy, XKeysymToKeycode(source.dpy, XK_Escape), None, source.root,
            False, GrabModeAsync, GrabModeAsync);

    source.atom.delete_window = XInternAtom(source.dpy, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(source.dpy, source.win, &source.atom.delete_window, 1);
    source.atom.xdnd_enter = XInternAtom(source.dpy, "XdndEnter", False);
    source.atom.xdnd_drop = XInternAtom(source.dpy, "XdndDrop", False);
    source.atom.xdnd_finished = XInternAtom(source.dpy, "XdndFinished", False);
    source.atom.xdnd_selection = XInternAtom(source.dpy, "XdndSelection", False);
    source.atom.xdnd_leave = XInternAtom(source.dpy, "XdndLeave", False);
    source.atom.xdnd_position = XInternAtom(source.dpy, "XdndPosition", False);
    source.atom.xdnd_status = XInternAtom(source.dpy, "XdndStatus", False);
    source.atom.xdnd_aware = XInternAtom(source.dpy, "XdndAware", False);
    source.atom.xdnd_action_copy = XInternAtom(source.dpy, "XdndActionCopy", False);
    source.atom.data_type = XInternAtom(source.dpy, "text/uri-list", False);

    Cursor cursor = XCreateFontCursor(source.dpy, XC_X_cursor);
    if ((XGrabPointer
        (source.dpy, source.root, False,
         ButtonPressMask, GrabModeAsync,
         GrabModeAsync, None, cursor, CurrentTime) != GrabSuccess))
        fail("couldn't grab pointer\n");

    do {
        XEvent e;
        XNextEvent(source.dpy, &e);

        switch (e.type) {
            case ButtonPress:
                if(XGrabPointer(source.dpy, source.root, False,
                            PointerMotionMask, GrabModeAsync, GrabModeAsync,
                            None, None, CurrentTime) != GrabSuccess)
                    fail("could not grab pointer\n");

                get_target_info();

                if(source.target_info.target == None || !is_target_xdnd_aware()) {
                    XUngrabPointer(source.dpy, CurrentTime);
                    break;
                }

                XSetSelectionOwner(source.dpy, source.atom.xdnd_selection,
                        source.win, CurrentTime);
                // XdndEnter
                XSendEvent(source.dpy, source.target_info.target, False, 0, &(XEvent){
                        .xclient.type = ClientMessage,
                        .xclient.display = source.dpy,
                        .xclient.window = source.target_info.target,
                        .xclient.message_type = source.atom.xdnd_enter,
                        .xclient.format = 32,
                        .xclient.data.l[0] = source.win,
                        .xclient.data.l[1] = XDND_PROTOCOL_VERSION << 24,
                        .xclient.data.l[2] = source.atom.data_type,
                        });
                // XdndPosition
                XSendEvent(source.dpy, source.target_info.target, False, 0, &(XEvent){
                        .xclient.type = ClientMessage,
                        .xclient.display = source.dpy,
                        .xclient.window = source.target_info.target,
                        .xclient.message_type = source.atom.xdnd_position,
                        .xclient.format = 32,
                        .xclient.data.l[0] = source.win,
                        .xclient.data.l[2] = (source.target_info.x_root << 16) | source.target_info.y_root,
                        .xclient.data.l[3] = CurrentTime,
                        .xclient.data.l[4] = source.atom.xdnd_action_copy,
                        });

                XUngrabPointer(source.dpy, CurrentTime);
                break;
            case SelectionRequest:
                send_selection_notify(&e.xselectionrequest);
                break;
            case ClientMessage:
                if(e.xclient.message_type == source.atom.xdnd_status) {
                    if((e.xclient.data.l[1] & 0x1) != 1) {
                        // XdndLeave
                        XSendEvent(source.dpy, source.target_info.target, False, 0, &(XEvent){
                                .xclient.type = ClientMessage,
                                .xclient.display = source.dpy,
                                .xclient.window = source.target_info.target,
                                .xclient.message_type = source.atom.xdnd_leave,
                                .xclient.format = 32,
                                .xclient.data.l[0] = source.win,
                                });
                        fail("target wont accept the drop\n");
                    }
                    // XdndDrop
                    XSendEvent(source.dpy, source.target_info.target, False, 0, &(XEvent){
                            .xclient.type = ClientMessage,
                            .xclient.display = source.dpy,
                            .xclient.window = source.target_info.target,
                            .xclient.message_type = source.atom.xdnd_drop,
                            .xclient.format = 32,
                            .xclient.data.l[0] = source.win,
                            .xclient.data.l[2] = CurrentTime,
                            });
                }
                if(e.xclient.message_type == source.atom.xdnd_finished) {
                    source.finished = 1;
                }
                if(e.xclient.data.l[0] == source.atom.delete_window)
                    source.finished = 1;
                break;
            case KeyPress:
                source.finished = 1;
                break;
        }
    } while (!source.finished);

    XFreeCursor(source.dpy, cursor);
    XDestroyWindow(source.dpy, source.win);
    XCloseDisplay(source.dpy);
    return EXIT_SUCCESS;
}

