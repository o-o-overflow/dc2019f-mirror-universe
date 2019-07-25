// x11.c --- X11 routines used by the TV and KBD interfaces

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <err.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>
#include <X11/keysym.h>

#include "usim.h"
#include "utrace.h"
#include "ucode.h"
#include "tv.h"
#include "kbd.h"
#include "mouse.h"

typedef struct DisplayState {
	unsigned char *data;
	int linesize;
	int depth;
	int width;
	int height;
} DisplayState;

static Display *display;
static Window window;
static int bitmap_order;
static int color_depth;
static Visual *visual = NULL;
static GC gc;
static XImage *ximage;

#define USIM_EVENT_MASK ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask

static unsigned long Black;
static unsigned long White;

static int old_run_state;

static XComposeStatus status;

// Store modifier bitmasks: Shift, Caps Lock, and Control are all
// constant, so they don't need to be stored.
static unsigned int alt_mask = Mod1Mask;
static unsigned int meta_mask = Mod2Mask;
static unsigned int super_mask = Mod4Mask;

// Takes E, converts it into a LM (hardware) keycode and sends it to
// the IOB KBD.
static void
process_key(XEvent *e, int keydown)
{
	KeySym keysym;
	unsigned char buffer[5];
	int extra;
	int lmcode;
	int lmshift;

	extra = 0;

	if (e->xkey.state & meta_mask) // Meta key
		extra |= KNIGHT_META;

	if (e->xkey.state & super_mask) // Top key
		extra |= KNIGHT_TOP;

	if (e->xkey.state & ShiftMask) // Shift key
		extra |= KNIGHT_SHIFT;

	if (e->xkey.state & LockMask) // Shift Lock
		extra ^= KNIGHT_SHIFT;

	if (e->xkey.state & ControlMask) // Control
	{
		extra |= KNIGHT_CONTROL;
		extra |= KNIGHT_SHIFT;
	}

	if (keydown) {
		XLookupString(&e->xkey, (char *) buffer, 5, &keysym, &status);

		if (keysym == XK_Shift_L ||
		    keysym == XK_Shift_R ||
		    keysym == XK_Control_L ||
		    keysym == XK_Control_R ||
		    keysym == XK_Alt_L ||
		    keysym == XK_Alt_R ||
		    keysym == XK_Meta_L ||
		    keysym == XK_Meta_R)
			return;

		switch (keysym) {
		case XK_Escape:		lmcode = KNIGHT_escape;		break;
		case XK_F1:		lmcode = KNIGHT_escape;		break;
		case XK_F2:		lmcode = KNIGHT_system;		break;
		case XK_F3:		lmcode = KNIGHT_network;	break;
		case XK_F4:		lmcode = KNIGHT_abort;		break;
		case XK_F5:		lmcode = KNIGHT_clear;		break;
		case XK_F6:		lmcode = KNIGHT_help;		break;
		case XK_F7:		lmcode = KNIGHT_call;		break;
		case XK_F11:		lmcode = KNIGHT_end;		break;
		case XK_F12:		lmcode = KNIGHT_break;		break;
		case XK_Break:		lmcode = KNIGHT_break;		break;

		// Modify mapping to match present-day US keyboard layout.
		case XK_grave:		lmcode = KNIGHT_grave;		break;
		case XK_asciitilde:	lmcode = KNIGHT_asciitilde;	break;
		case XK_exclam:		lmcode = KNIGHT_exclam;		break;
		case XK_at:		lmcode = KNIGHT_at;		break;
		case XK_numbersign:	lmcode = KNIGHT_numbersign;	break;
		case XK_dollar:		lmcode = KNIGHT_dollar;		break;
		case XK_percent:	lmcode = KNIGHT_percent;	break;
		case XK_asciicircum:	lmcode = KNIGHT_asciicircum;	break;
		case XK_ampersand:	lmcode = KNIGHT_ampersand;	break;
		case XK_asterisk:	lmcode = KNIGHT_asterisk;	break;
		case XK_parenleft:	lmcode = KNIGHT_parenleft;	break;
		case XK_parenright:	lmcode = KNIGHT_parenright;	break;
		case XK_minus:		lmcode = KNIGHT_minus;		break;
		case XK_underscore:	lmcode = KNIGHT_underscore;	break;
		case XK_equal:		lmcode = KNIGHT_equal;		break;
		case XK_plus:		lmcode = KNIGHT_plus;		break;
		case XK_BackSpace:	lmcode = KNIGHT_rubout;		break;

		case XK_Tab:		lmcode = KNIGHT_tab;		break;
		case XK_Q:		lmcode = KNIGHT_Q;		break;
		case XK_W:		lmcode = KNIGHT_W;		break;
		case XK_E:		lmcode = KNIGHT_E;		break;
		case XK_R:		lmcode = KNIGHT_R;		break;
		case XK_T:		lmcode = KNIGHT_T;		break;
		case XK_Y:		lmcode = KNIGHT_Y;		break;
		case XK_U:		lmcode = KNIGHT_U;		break;
		case XK_I:		lmcode = KNIGHT_I;		break;
		case XK_O:		lmcode = KNIGHT_O;		break;
		case XK_P:		lmcode = KNIGHT_P;		break;
		case XK_braceleft:	lmcode = KNIGHT_braceleft;	break;
		case XK_braceright:	lmcode = KNIGHT_braceright;	break;

		case XK_A:		lmcode = KNIGHT_A;		break;
		case XK_S:		lmcode = KNIGHT_S;		break;
		case XK_D:		lmcode = KNIGHT_D;		break;
		case XK_F:		lmcode = KNIGHT_F;		break;
		case XK_G:		lmcode = KNIGHT_G;		break;
		case XK_H:		lmcode = KNIGHT_H;		break;
		case XK_J:		lmcode = KNIGHT_J;		break;
		case XK_K:		lmcode = KNIGHT_K;		break;
		case XK_L:		lmcode = KNIGHT_L;		break;
		case XK_semicolon:	lmcode = KNIGHT_semicolon;	break;
		case XK_colon:		lmcode = KNIGHT_colon;		break;
		case XK_quotedbl:	lmcode = KNIGHT_quotedbl;	break;
		case XK_apostrophe:	lmcode = KNIGHT_apostrophe;	break;
		case XK_backslash:	lmcode = KNIGHT_backslash;	break;
		case XK_bar:		lmcode = KNIGHT_bar;		break;

		case XK_Return:		lmcode = KNIGHT_cr;		break;

		case XK_Z:		lmcode = KNIGHT_Z;		break;
		case XK_X:		lmcode = KNIGHT_X;		break;
		case XK_C:		lmcode = KNIGHT_C;		break;
		case XK_V:		lmcode = KNIGHT_V;		break;
		case XK_B:		lmcode = KNIGHT_B;		break;
		case XK_N:		lmcode = KNIGHT_N;		break;
		case XK_M:		lmcode = KNIGHT_M;		break;
		case XK_comma:		lmcode = KNIGHT_comma;		break;
		case XK_less:		lmcode = KNIGHT_less;		break;
		case XK_period:		lmcode = KNIGHT_period;		break;
		case XK_greater:	lmcode = KNIGHT_greater;	break;
		case XK_slash:		lmcode = KNIGHT_slash;		break;
		case XK_question:	lmcode = KNIGHT_question;	break;

		default:
			if (keysym > 255) {
				WARNING(TRACE_MISC, "unknown keycode: %lu", keysym);
				return;
			}

			lmshift = KNIGHT_VANILLA; // Vanilla.
			if (extra & KNIGHT_SHIFT)
				lmshift = 1;  // Shift.
			else if (extra & KNIGHT_TOP)
				lmshift = 2; // Top.

			lmcode = knight_translate_table[lmshift][keysym];
			break;
		}

		// Keep Control and Meta bits, Shift is in the scancode table.
		lmcode |= extra & ~KNIGHT_SHIFT;
		// ... but if Control or Meta, add in Shift.
		if (extra & (17 << 10))
			lmcode |= extra;

		lmcode |= 0xffff0000;

		kbd_key_event(lmcode, keydown);
	}
}

static int u_minh = 0x7fffffff;
static int u_maxh;
static int u_minv = 0x7fffffff;
static int u_maxv;

void
accumulate_update(int h, int v, int hs, int vs)
{
	if (h < u_minh)
		u_minh = h;
	if (h + hs > u_maxh)
		u_maxh = h + hs;
	if (v < u_minv)
		u_minv = v;
	if (v + vs > u_maxv)
		u_maxv = v + vs;
}

void
send_accumulated_updates(void)
{
	int hs;
	int vs;

	hs = u_maxh - u_minh;
	vs = u_maxv - u_minv;
	if (u_minh != 0x7fffffff && u_minv != 0x7fffffff && u_maxh && u_maxv) {
		XPutImage(display, window, gc, ximage, u_minh, u_minv, u_minh, u_minv, hs, vs);
		XFlush(display);
	}

	u_minh = 0x7fffffff;
	u_maxh = 0;
	u_minv = 0x7fffffff;
	u_maxv = 0;
}

void
x11_event(void)
{
	XEvent e;

	send_accumulated_updates();
	kbd_dequeue_key_event();

	while (XCheckWindowEvent(display, window, USIM_EVENT_MASK, &e)) {
		switch (e.type) {
		case Expose:
			XPutImage(display, window, gc, ximage, 0, 0, 0, 0, tv_width, tv_height);
			XFlush(display);
			break;
		case KeyPress:
			process_key(&e, 1);
			break;
		case KeyRelease:
			process_key(&e, 0);
			break;
		case MotionNotify:
		case ButtonPress:
		case ButtonRelease:
			mouse_event(e.xbutton.x, e.xbutton.y, e.xbutton.button);
			break;
		default:
			break;
		}
	}

	if (old_run_state != run_ucode_flag)
		old_run_state = run_ucode_flag;
}

static void
init_mod_map(void)
{
	XModifierKeymap *map;
	int max_mod;

	map = XGetModifierMapping(display);
	max_mod = map->max_keypermod;

	NOTICE(TRACE_MISC, "Looking up X11 Modifier mappings...\n");

	// The modifiers at indices 0-2 have predefined meanings, but
	// those at indices 3-7 (Mod1 through Mod5) have their meaning
	// defined by the keys assigned to them, so loop through to
	// find Alt and Meta.
	for (int mod = 3; mod < 8; mod++) {
		bool is_alt;
		bool is_meta;
		bool is_super;

		is_alt = false;
		is_meta = false;
		is_super = false;

		// Get the keysyms matching this modifier.
		for (int i = 0; i < max_mod; i++) {
			int keysyms_per_code;
			KeyCode code;
			KeySym *syms;

			keysyms_per_code = 0;
			code = map->modifiermap[mod * max_mod + i];

			// Don't try to look up mappings for NoSymbol.
			if (code == NoSymbol)
				continue;

			syms = XGetKeyboardMapping(display, code, max_mod, &keysyms_per_code);
			if (keysyms_per_code == 0)
				WARNING(TRACE_MISC, "No keysyms for code %xu\n", code);

			for (int j = 0; j < keysyms_per_code; j++){
				switch(syms[j]){
				case XK_Alt_L: case XK_Alt_R:
					is_alt = true;
					break;
				case XK_Meta_L: case XK_Meta_R:
					is_meta = true;
					break;
				case XK_Super_L: case XK_Super_R:
					is_super = true;
					break;
				case NoSymbol:
					break;
				default:
					DEBUG(TRACE_MISC, "Sym %lx\n", syms[j]);
					break;
				}
			}
		}

		// Assign the modifer masks corresponding to this
		// modifier.
		if (is_alt)
			alt_mask = 1 << mod;
		if (is_meta)
			meta_mask = 1 << mod;
		if (is_super)
			super_mask = 1 << mod;
	}

	NOTICE(TRACE_MISC, "Modifier mapping is alt_mask = %d, meta_mask = %d, super_mask = %d\n",
	       alt_mask, meta_mask, super_mask);
}

void
x11_init(void)
{
	char *displayname;
	unsigned long bg_pixel = 0L;
	int xscreen;
	Window root;
	XEvent e;
	XGCValues gcvalues;
	XSetWindowAttributes attr;
	XSizeHints *size_hints;
	XTextProperty windowName;
	XTextProperty *pWindowName = &windowName;
	XTextProperty iconName;
	XTextProperty *pIconName = &iconName;
	XWMHints *wm_hints;
	char *window_name = (char *) "CADR";
	char *icon_name = (char *) "CADR";

	displayname = getenv("DISPLAY");
	display = XOpenDisplay(displayname);
	if (display == NULL)
		errx(1, "failed to open display");

	bitmap_order = BitmapBitOrder(display);
	xscreen = DefaultScreen(display);
	color_depth = DisplayPlanes(display, xscreen);

	Black = BlackPixel(display, xscreen);
	White = WhitePixel(display, xscreen);

	root = RootWindow(display, xscreen);
	attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
	window = XCreateWindow(display, root, 0, 0, tv_width, tv_height, 0, color_depth, InputOutput, visual, CWBorderPixel | CWEventMask, &attr);
	if (window == None)
		errx(1, "failed to open window");

	if (!XStringListToTextProperty(&window_name, 1, pWindowName))
		pWindowName = NULL;

	if (!XStringListToTextProperty(&icon_name, 1, pIconName))
		pIconName = NULL;

	size_hints = XAllocSizeHints();
	if (size_hints != NULL) {
		// The window will not be resizable.
		size_hints->flags = PMinSize | PMaxSize;
		size_hints->min_width = size_hints->max_width = tv_width;
		size_hints->min_height = size_hints->max_height = tv_height;
	}

	wm_hints = XAllocWMHints();
	if (wm_hints != NULL) {
		wm_hints->initial_state = NormalState;
		wm_hints->input = True;
		wm_hints->flags = StateHint | InputHint;
	}

	XSetWMProperties(display, window, pWindowName, pIconName, NULL, 0, size_hints, wm_hints, NULL);
	XMapWindow(display, window);

	gc = XCreateGC(display, window, 0, &gcvalues);

	// Fill window with the specified background color.
	bg_pixel = 0;
	XSetForeground(display, gc, bg_pixel);
	XFillRectangle(display, window, gc, 0, 0, tv_width, tv_height);

	// Wait for first Expose event to do any drawing, then flush.
	do
		XNextEvent(display, &e);
	while (e.type != Expose || e.xexpose.count);

	XFlush(display);
	ximage = XCreateImage(display, visual, (unsigned) color_depth, ZPixmap, 0, (char *) tv_bitmap, tv_width, tv_height, 32, 0);
	ximage->byte_order = LSBFirst;

	init_mod_map();
}
