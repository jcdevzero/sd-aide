//
// showinit.c
//
// Derived from code found online, originally written by Paul Mercer,
// subsequently modified by Bruce Partridge, and then further modified
// in this version (jmacz).
//
// This code works using an undocumented hack that stores information
// inside the CurApName. This information (a horizontal offset) is used
// to let subsequent system extensions know where to render its icon.
// Each system extension updates this info to allow the next system
// extension to show its icon, etc. There was a checksum added to allow
// the first system extension to know that it was the first and to set
// up the location for subsequent extensions.
//
// This version only works post System 7.
// 

#include <Quickdraw.h>
#include <Icons.h>
#include <Gestalt.h>

#define CURAPNAME_LM		0x910
#define SHOW_INIT_TABLE		((unsigned short*)(CURAPNAME_LM + 32 - 4))
#define CHECKSUM			0x1021
#define X_OFFSET			40
#define Y_OFFSET			40
#define X_INITIAL			8
#define ICON_WIDTH			32

#pragma options align=mac68k
typedef struct {
	char privates[76];
	long randSeed;
	BitMap screenBits;
	Cursor arrow;
	Pattern dkGray;
	Pattern ltGray;
	Pattern gray;
	Pattern black;
	Pattern white;
	GrafPtr thePort;
	long	end;
} ShowINITQDGlobals;
#pragma options align=reset

// Compute a simple checksum on the horizontal position to see if
// the value was modified by a ShowINIT aware system extension. If not,
// we assume we are the first system extension to use ShowINIT.

static short CheckSum(unsigned short x)
{
	if (x & 0x8000)	{
		return ((x << 1) ^ CHECKSUM ^ 0x01);
	} else {
		return ((x << 1) ^ CHECKSUM);
	}
}

// Generate rectangle to display the next INIT's icon in.
// Also update the horizontal position.

static void RenderIcon(short iconID, short hOffset, Rect *screenRect)
{
	register short screenWidth;
	Rect iconRect;
	CGrafPort port;
		
	screenWidth = screenRect->right - screenRect->left;
	screenWidth -= screenWidth % X_OFFSET;
	
	iconRect.left = (hOffset % screenWidth);
	iconRect.top = screenRect->bottom -  Y_OFFSET * (1 + (hOffset / screenWidth));
	iconRect.right = iconRect.left + ICON_WIDTH;
	iconRect.bottom = iconRect.top + ICON_WIDTH;
	
	OpenCPort(&port);
	PlotIconID(&iconRect, kAlignNone, kTransformNone, iconID);
	CloseCPort(&port);
}

// ShowINIT
//
// iconID is the resource ID of the icon that should be rendered.
// It can be any of the various ICN# icon resource types.
//
// advance is used to specify whether the horizontal offset
// should be advanced or not. Normally you would always advance
// but this is useful in case your system extension wants to
// animate its icon (ie. not advance so that you can render another
// icon on top of the current one.) If you do this, keep in mind
// that your icon should use the same mask as there's no erase
// and if your mask changes, you will see the previous icon
// underneath.

pascal void ShowINIT(short iconID, Boolean advance)
{
	ShowINITQDGlobals qd;
	long oldA5;
	long gestaltResponse;
	short hOffset;
	
	// This version only runs with System 7+ because it
	// utilizes the PlotIconID call to render the icon.
	
	Gestalt(gestaltSystemVersion, &gestaltResponse);
	if (gestaltResponse < 0x700) {
		return;
	}
	
	// QuickDraw requires A5 point to a set of its globals but
	// INITs don't automatically get their own set of QuickDraw
	// globals. Use the current system one.
	
	oldA5 = SetA5((long)&qd.end);
	InitGraf(&qd.thePort);
	
	// Read in the expected horizontal offset and see if it's
	// valid value (ie. another system extension using ShowINIT
	// has already run). If not, initialize the value. We don't
	// compute the checksum and store it in this case because
	// if we are advancing, it's done later. If we're not advancing,
	// we are not technically the first system extension as we
	// are not advancing the horizontal offset so leave it alone.
	
	hOffset = SHOW_INIT_TABLE[0];
	
	if (CheckSum(SHOW_INIT_TABLE[0]) != SHOW_INIT_TABLE[1]) {
		hOffset = X_INITIAL;
	}
	
	// Actually draw the icon to the screen.
	
	RenderIcon(iconID, hOffset, &(qd.screenBits.bounds));
	
	// If we are advancing, then update the horizontal offset
	// and also compute the checksum so that subsequent system
	// extensions know.

	if (advance) {
		SHOW_INIT_TABLE[0] += X_OFFSET;
		SHOW_INIT_TABLE[1] = CheckSum(SHOW_INIT_TABLE[0]);
	}
	
	SetA5(oldA5);
}