/*
 * Copyright (C) 2026 sillybanaja
 * See LICENSE file for license details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>

#define XDND_PROTOCOL_VERSION 5

struct {
    Display* dpy;
    Window root, win, target;
    int scr;
    int x_root, y_root;
    int finished, position_ready, last_status;
    Time time;
    struct {
        char* urilist;
        char* string;
        size_t file_paths_size;
        char** file_paths;
    } data;
    struct {
        Atom xdnd_enter;
        Atom xdnd_finished;
        Atom xdnd_selection;
        Atom xdnd_leave;
        Atom xdnd_position;
        Atom xdnd_drop;
        Atom xdnd_status;
        Atom xdnd_aware;
        Atom xdnd_action_copy;
        Atom mime_urilist;
        Atom mime_string;
    } atom;
} source = {
    .scr = 0, .win = None, .root = None, .target = None,
    .finished = 0, .position_ready = 0, .x_root = -1,
    .time = CurrentTime,
};

static int swallow_badwindow(Display*, XErrorEvent*);
static void cleanup(void);
static void fail(const char*, ...);

int
swallow_badwindow(Display* dpy, XErrorEvent* e) {
    return (e->error_code == BadWindow) ? 0 : 0;
}

void
cleanup(void) {
    if(source.dpy) {
        XDestroyWindow(source.dpy, source.win);
        XCloseDisplay(source.dpy);
    }
    free(source.data.string); free(source.data.urilist);
    for(int i=0;i<source.data.file_paths_size;i++) free(source.data.file_paths[i]);
}

void
fail(const char* fmt, ...) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    cleanup();
    exit(EXIT_FAILURE);
}

int
main(int argc, char* argv[]) {
    if(argc <= 1)
        fail("usage: drop filename ...\n");

    // file handling
    source.data.file_paths_size = argc-1;
    source.data.file_paths = argv+1;
    for(int i=0;i<argc-1;i++) {
        const char* current = source.data.file_paths[i];
        source.data.file_paths[i] = realpath(current, NULL);
        if(!source.data.file_paths[i])
            fail("invalid file path: \"%s\"\n", current);
    }

    size_t uri_size = 1, str_size = 1;
    for(int i=0;i<source.data.file_paths_size;i++) {
        size_t len = strlen(source.data.file_paths[i]);
        uri_size += len + 9;        // file://\r\n
        str_size += len + 7;        // file://
        if(i > 0) str_size += 1;    // <space>
    }

    source.data.urilist = malloc(uri_size);
    char* pu = source.data.urilist;
    for(int i=0;i<source.data.file_paths_size;i++)
        pu += snprintf(pu, uri_size - (pu - source.data.urilist),
                "file://%s\r\n", source.data.file_paths[i]);
    *pu = '\0';

    source.data.string = malloc(str_size);
    char* ps = source.data.string;
    for(int i=0;i<source.data.file_paths_size;i++) {
        if(i > 0) *ps++ = ' ';
        ps += snprintf(ps, str_size - (ps - source.data.string),
                "file://%s", source.data.file_paths[i]);
    }
    *ps = '\0';

    // x11
    if(!(source.dpy = XOpenDisplay(NULL)))
        fail("XOpenDisplay failed\n");
    source.scr = DefaultScreen(source.dpy);
    source.root = RootWindow(source.dpy, source.scr);
    if(!(source.win = XCreateSimpleWindow(source.dpy, source.root, 0, 0, 10, 10, 0, 0, 0)))
        fail("XCreateSimpleWindow failed\n");
    XSetErrorHandler(swallow_badwindow);

    source.atom.mime_urilist = XInternAtom(source.dpy, "text/uri-list", False);
    source.atom.mime_string = XInternAtom(source.dpy, "STRING", False);
    source.atom.xdnd_aware = XInternAtom(source.dpy, "XdndAware", False);
    source.atom.xdnd_enter = XInternAtom(source.dpy, "XdndEnter", False);
    source.atom.xdnd_leave = XInternAtom(source.dpy, "XdndLeave", False);
    source.atom.xdnd_position = XInternAtom(source.dpy, "XdndPosition", False);
    source.atom.xdnd_selection = XInternAtom(source.dpy, "XdndSelection", False);
    source.atom.xdnd_status = XInternAtom(source.dpy, "XdndStatus", False);
    source.atom.xdnd_drop = XInternAtom(source.dpy, "XdndDrop", False);
    source.atom.xdnd_finished = XInternAtom(source.dpy, "XdndFinished", False);
    source.atom.xdnd_action_copy = XInternAtom(source.dpy, "XdndActionCopy", False);
    XSetSelectionOwner(source.dpy, source.atom.xdnd_selection, source.win, CurrentTime);
    XChangeProperty(source.dpy, source.win,
            XInternAtom(source.dpy, "XdndActionList", False),
            4/*XA_ATOM*/, 32, PropModeReplace,
            (unsigned char*)(Atom[]){source.atom.xdnd_action_copy}, 1);

    int xi_opcode;
    if(!XQueryExtension(source.dpy, "XInputExtension", &xi_opcode, &(int){0}, &(int){0}))
        fail("XInput extension doesn't exist or is not available\n");
    unsigned char mask_data[XIMaskLen(XI_LASTEVENT)] = {0};
    XISetMask(mask_data, XI_RawMotion);
    XISelectEvents(source.dpy, source.root, &(XIEventMask){
            .deviceid = XIAllMasterDevices,
            .mask_len = sizeof(mask_data),
            .mask = mask_data,
            }, 1);
    XGrabKey(source.dpy, XKeysymToKeycode(source.dpy, XK_Escape), 0,
            source.root, False, GrabModeAsync, GrabModeAsync);
    XGrabButton(source.dpy, Button1, 0, source.root, 
            False, ButtonPressMask, GrabModeAsync, GrabModeAsync, None, None);

    XEvent e;
    do {
        XNextEvent(source.dpy, &e);

        if(e.xcookie.extension == xi_opcode) {
            Window child;
            XQueryPointer(source.dpy, source.root, &(Window){None}, &child,
                    &source.x_root, &source.y_root, &(int){0}, &(int){0}, &(unsigned int){0});

            XGetEventData(source.dpy, &e.xcookie);
            source.time = ((XIRawEvent*)e.xcookie.data)->time;
            XFreeEventData(source.dpy, &e.xcookie);

            if(source.target && child != source.target) {
                XSendEvent(source.dpy, source.target, False, 0, &(XEvent){ // xdndleave
                        .xclient.type = ClientMessage,
                        .xclient.display = source.dpy,
                        .xclient.window = source.target,
                        .xclient.message_type = source.atom.xdnd_leave,
                        .xclient.format = 32,
                        .xclient.data.l[0] = source.win,
                        .xclient.data.l[1] = 0,
                        });
                source.target = None;
                source.position_ready = 0;
            }
            else if(child && source.target == None) {
                source.target = child;

                unsigned char* d;
                if(XGetWindowProperty(source.dpy, source.target, source.atom.xdnd_aware,
                            0, 1, False, AnyPropertyType, &(Atom){None}, &(int){0},
                            &(unsigned long){0}, &(unsigned long){0}, &d) == Success && d) {
                    XFree(d);
                    XSendEvent(source.dpy, source.target, False, 0, &(XEvent){ // xdndenter
                            .xclient.type = ClientMessage,
                            .xclient.display = source.dpy,
                            .xclient.window = source.target,
                            .xclient.message_type = source.atom.xdnd_enter,
                            .xclient.format = 32,
                            .xclient.data.l[0] = source.win,
                            .xclient.data.l[1] = XDND_PROTOCOL_VERSION << 24,
                            .xclient.data.l[2] = source.atom.mime_urilist,
                            .xclient.data.l[3] = source.atom.mime_string,
                            .xclient.data.l[4] = 0,
                            });
                    XSendEvent(source.dpy, source.target, False, 0, &(XEvent){ // xdndposition
                            .xclient.type = ClientMessage,
                            .xclient.display = source.dpy,
                            .xclient.window = source.target,
                            .xclient.message_type = source.atom.xdnd_position,
                            .xclient.format = 32,
                            .xclient.data.l[0] = source.win,
                            .xclient.data.l[1] = 0,
                            .xclient.data.l[2] = (source.x_root << 16) | source.y_root,
                            .xclient.data.l[3] = source.time,
                            .xclient.data.l[4] = source.atom.xdnd_action_copy,
                            });
                    XFlush(source.dpy);
                }
                else
                    source.target = None;
            }  
            else if(source.position_ready) {
                XSendEvent(source.dpy, source.target, False, 0, &(XEvent){ // xdndposition
                        .xclient.type = ClientMessage,
                        .xclient.display = source.dpy,
                        .xclient.window = source.target,
                        .xclient.message_type = source.atom.xdnd_position,
                        .xclient.format = 32,
                        .xclient.data.l[0] = source.win,
                        .xclient.data.l[1] = 0,
                        .xclient.data.l[2] = (source.x_root << 16) | source.y_root,
                        .xclient.data.l[3] = source.time,
                        .xclient.data.l[4] = source.atom.xdnd_action_copy,
                        });
            }
        }
        else if(e.type == SelectionRequest) {
            XSelectionRequestEvent* req = &e.xselectionrequest;
            Atom reply_prop = req->property ? req->property : req->target;

            if(req->target == source.atom.mime_urilist) {
                XChangeProperty(source.dpy, req->requestor, reply_prop,req->target, 8, PropModeReplace,
                        (unsigned char*)source.data.urilist, strlen(source.data.urilist));
            }
            else if(req->target == source.atom.mime_string) {
                XChangeProperty(source.dpy, req->requestor, reply_prop,req->target, 8, PropModeReplace,
                        (unsigned char*)source.data.string, strlen(source.data.string));
            }
            else // unsupported mime
                reply_prop = None;

            XEvent notify = {0};
            notify.type = SelectionNotify;
            notify.xselection.display = source.dpy;
            notify.xselection.requestor = e.xselectionrequest.requestor;
            notify.xselection.selection = e.xselectionrequest.selection;
            notify.xselection.target = e.xselectionrequest.target;
            notify.xselection.time = e.xselectionrequest.time;
            notify.xselection.property = reply_prop;
            XSendEvent(source.dpy, e.xselectionrequest.requestor, False, NoEventMask, &notify);
        }
        else if(e.xclient.message_type == source.atom.xdnd_status) {
            source.last_status = e.xclient.data.l[1];
            source.position_ready = 1;
        }
        else if(e.xclient.message_type == source.atom.xdnd_finished) {
            source.finished = 1;
        }
        else if(e.type == KeyPress) {
            source.finished = 1;
            if(source.target) {
                XSendEvent(source.dpy, source.target, False, 0, &(XEvent){ // xdndleave
                        .xclient.type = ClientMessage,
                        .xclient.display = source.dpy,
                        .xclient.window = source.target,
                        .xclient.message_type = source.atom.xdnd_leave,
                        .xclient.format = 32,
                        .xclient.data.l[0] = source.win,
                        .xclient.data.l[1] = 0,
                        });
            }
        }
        else if(e.type == ButtonPress) {
            if(source.position_ready) {
                XSendEvent(source.dpy, source.target, False, 0, &(XEvent){ // xdnddrop
                        .xclient.type = ClientMessage,
                        .xclient.display = source.dpy,
                        .xclient.window = source.target,
                        .xclient.message_type = source.atom.xdnd_drop,
                        .xclient.format = 32,
                        .xclient.data.l[0] = source.win,
                        .xclient.data.l[2] = source.time,
                        });

                XUngrabKey(source.dpy, XKeysymToKeycode(source.dpy, XK_Escape), AnyModifier, source.root);
                XUngrabButton(source.dpy, Button1, AnyModifier, source.root);
                XISelectEvents(source.dpy, source.root, &(XIEventMask){ .deviceid = XIAllMasterDevices }, 0);
                XFlush(source.dpy);
            }
            else if(source.x_root == -1) fail("XI_RawMotion not triggered before drop\n");
            else if(source.target == None) fail("window not supporting xdnd protocol\n");
            else if(source.last_status == 0) fail("target(0x%08lx) wont accept the drop\n", source.target);
            else fail("target(0x%08lx) not accepting xdndenter or xdndposition events\n", source.target);
        }
    } while(!source.finished);

    cleanup();
    return EXIT_SUCCESS;
}
