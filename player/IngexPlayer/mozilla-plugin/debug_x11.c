#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdio.h>

/*
 * Lookup: lookup a code in a table.
 */
static char _lookup_buffer[100];

typedef struct {
	long code;
	const char *name;
} binding;

static binding _bool[] = {
	{ 0, "No" },
	{ 1, "Yes" },
	{ 0, 0 } };

static binding _event_mask_names[] = {
	{ KeyPressMask, "KeyPress" },
	{ KeyReleaseMask, "KeyRelease" },
	{ ButtonPressMask, "ButtonPress" },
	{ ButtonReleaseMask, "ButtonRelease" },
	{ EnterWindowMask, "EnterWindow" },
	{ LeaveWindowMask, "LeaveWindow" },
	{ PointerMotionMask, "PointerMotion" },
	{ PointerMotionHintMask, "PointerMotionHint" },
	{ Button1MotionMask, "Button1Motion" },
	{ Button2MotionMask, "Button2Motion" },
	{ Button3MotionMask, "Button3Motion" },
	{ Button4MotionMask, "Button4Motion" },
	{ Button5MotionMask, "Button5Motion" },
	{ ButtonMotionMask, "ButtonMotion" },
	{ KeymapStateMask, "KeymapState" },
	{ ExposureMask, "Exposure" },
	{ VisibilityChangeMask, "VisibilityChange" },
	{ StructureNotifyMask, "StructureNotify" },
	{ ResizeRedirectMask, "ResizeRedirect" },
	{ SubstructureNotifyMask, "SubstructureNotify" },
	{ SubstructureRedirectMask, "SubstructureRedirect" },
	{ FocusChangeMask, "FocusChange" },
	{ PropertyChangeMask, "PropertyChange" },
	{ ColormapChangeMask, "ColormapChange" },
	{ OwnerGrabButtonMask, "OwnerGrabButton" },
	{ 0, 0 } };

static const char *
LookupL(long code, binding *table)
{
	const char *name;

	sprintf(_lookup_buffer, "unknown (code = %ld. = 0x%lx)", code, code);
	name = _lookup_buffer;

	while (table->name) {
		if (table->code == code) {
			name = table->name;
			break;
		}
		table++;
	}

	return(name);
}

static const char *
Lookup(int code, binding *table)
{
    return LookupL((long)code, table);
}
static void
Display_Event_Mask(long mask)
{
  size_t bit, bit_mask;

  for (bit=0, bit_mask=1; bit<sizeof(long)*8; bit++, bit_mask <<= 1)
    if (mask & bit_mask)
      printf("      %s\n",
	     LookupL(bit_mask, _event_mask_names));
}

extern void
debug_x11_events(Display *display, Window window)
{
  XWindowAttributes win_attributes;

  if (!XGetWindowAttributes(display, window, &win_attributes))
    printf("Can't get window attributes.\n");

  printf("\n");
  printf("  Someone wants these events:\n");
  Display_Event_Mask(win_attributes.all_event_masks);

  printf("  Do not propagate these events:\n");
  Display_Event_Mask(win_attributes.do_not_propagate_mask);

  printf("  Override redirection?: %s\n",
	 Lookup(win_attributes.override_redirect, _bool));
}
