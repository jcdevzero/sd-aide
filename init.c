//
// init.c
//
// Code for the INIT resource for SD Aide.
// Applies a patch to delay N seconds after a SCSI drive
// has been powered on to allow SCSI emulator devices
// enough time to properly boot up and become ready.
//

#include <A4Stuff.h>
#include <SetUpA4.h>
#include <OSUtils.h>
#include <Traps.h>
#include <Gestalt.h>
#include <Resources.h>
#include <Folders.h>
#include <TextUtils.h>
#include "prefs.h"

#define INIT_RSRC_ID		-4064
#define GOOD_ICON			-4064
#define BAD_ICON			-4063
#define STR_LIST_ID			-4064
#define PREF_NAME_STR_ID	1

long gPrevProc = 0;
long gDelayTicks = 180; // if not configured, default to 3 seconds (180 ticks)

pascal void PatchedPowerManagerOp(void);
pascal void ShowINIT(short iconID, Boolean advance);
static Boolean CanRun(void);
static void ReadPreferences(void);

void main()
{
	Handle initH;
	OSErr err;
	long oldA4;
	
	Boolean installed = false;
	
	// INIT resources don't have access to globals as there is
	// no A5 world. Most toolchains (like CodeWarrior which is
	// being used for this right now) provide an A4 based mechanism.
	// Use this A4 mechanism.
	
	oldA4 = SetCurrentA4();
	RememberA4();
	
	// Check whether this system is >= System 7 and supports the Power Manager
	// which is what we're going to patch.
	
	if (!CanRun()) {
		goto showIcon;
	}
	
	// The system loads INITs temporarily and then releases the memory
	// after they execute. This means that any code we want to keep
	// (for example if you patched a system call) will get unloaded.
	// To prevent this, we need to detach this code resource (INIT).
	// We also lock it and move it high in the heap zone to help avoid
	// heap fragmentation.
	
	initH = Get1Resource('INIT', INIT_RSRC_ID);
	if (!initH) {
		goto showIcon;
	}
	
	DetachResource(initH);
	MoveHHi(initH);
	HLock(initH);
	
	// Check to see if there are any preferences and if so,
	// load the delay we need from the preferences file.
	
	ReadPreferences();
	
	// Patch the trap now with our patched version.
	// This is the meat of what we wanted to do. Everything up to this
	// point was just preparing to make this patch.
	
	gPrevProc = (long)GetOSTrapAddress(_PMgrOp);
	SetOSTrapAddress((UniversalProcPtr)PatchedPowerManagerOp, _PMgrOp);
	
	installed = true;

showIcon:
	ShowINIT(installed ? GOOD_ICON : BAD_ICON, true);
	
	SetA4(oldA4);
}

// Check if we should actually install this patch
// or just skip it.

static Boolean CanRun()
{
	OSErr err;
	long gestaltResponse;
	long unimplProc;
	long stockProc;
	
	// Require System 7.
	
	err = Gestalt(gestaltSystemVersion, &gestaltResponse);
	if (err || gestaltResponse < 0x700) {
		return false;
	}
	
	// Require FindFolder call is available.
	
	err = Gestalt(gestaltFindFolderAttr, &gestaltResponse);
	if (err || (gestaltResponse & (1 << gestaltFindFolderPresent)) == 0) {
		return false;
	}
	
	// Require Power Manager is implemented.
	
	err = Gestalt(gestaltPowerMgrAttr, &gestaltResponse);
	if (err || (gestaltResponse & (1 << gestaltPMgrExists)) == 0) {
		return false;
	}
	
	// Require 68K. Error is ok because if gestaltSysArchitecture
	// isn't a valid check, it's definitely 68K.
	
	err = Gestalt(gestaltSysArchitecture, &gestaltResponse);
	if (!err && gestaltResponse != gestalt68k) {
		return false;
	}
	
	// Even though the Power Manager is present, lets double check
	// and ensure the trap we want to patch is actually implemented.
	// Docs say we should do this by checking the address against
	// the unimplemented trap address.
	
	unimplProc = (long)GetToolboxTrapAddress(_Unimplemented);
	stockProc = (long)GetOSTrapAddress(_PMgrOp);
	
	if (!unimplProc || !stockProc || stockProc == unimplProc) {
		return false;
	}
	
	return true;
}

// Attempt to read the delay from the preferences file
// if it exists. If it does not, the default value declared
// earlier in this code will be used.

static void ReadPreferences()
{
	HParamBlockRec hpb;
	ParamBlockRec pb;
	PrefsRecord prefs;
	Str255 prefName;
	OSErr err;
	long dirID;
	short vRefNum;
	
	GetIndString(prefName, STR_LIST_ID, PREF_NAME_STR_ID);
	
	err = FindFolder(kOnSystemDisk, kPreferencesFolderType, false, &vRefNum, &dirID);
	if (!err) {
		hpb.fileParam.ioNamePtr = prefName;
		hpb.fileParam.ioVRefNum = vRefNum;
		hpb.ioParam.ioPermssn = fsRdPerm;
		hpb.ioParam.ioMisc = 0;
		hpb.fileParam.ioDirID = dirID;
		
		err = PBHOpen(&hpb, false);
		if (!err) {
			pb.ioParam.ioRefNum = hpb.ioParam.ioRefNum;
			pb.ioParam.ioBuffer = (void *)&prefs;
			pb.ioParam.ioReqCount = sizeof(PrefsRecord);
			pb.ioParam.ioPosMode = fsFromStart;
			pb.ioParam.ioPosOffset = 0;
			
			err = PBRead(&pb, false);
			if (!err && pb.ioParam.ioActCount == sizeof(PrefsRecord)) {
				gDelayTicks = prefs.delayInTicks;
			}
			
			pb.ioParam.ioRefNum = hpb.ioParam.ioRefNum;
			
			PBClose(&pb, false);
		}
	}
}

// For the _PMgrOp trap, it is expecting a power manager parameter block in a0.
// The structure of this paramter block looks like this:
//
// pmCommandRec
//		pmCommand	WORD
//		pmLength	WORD
//		pmSBuffer	LONG
//		pmRBuffer	LONG
//		pmData		LONG
//
// Offsets to the fields we want to look at:
//
//		  (a0)	pmCommand
//		 2(a0)	pmLength
//		 4(a0)	pmSBuffer
//		 8(a0)	pmRBuffer
//		12(a0)	pmData
//
// We are specifically looking for the case where:
//
//		pmCommand = 0x0010 (which is powerCntl)
//		pmData = 0x84 (which is hdOn; just want to look at the first byte of pmData)
//
// This function messes with registers a0, a1, d0, and d1.
//
//		a0 - Need to preserve on stack before we modify it.
//		a1 - Should be fine to trash as the original trap also trashes it but save anyway.
//		d0 - Should be fine to trash in the beginning as the original trap will
//		     also write to it for the return result, but after we call the
//		     original trap we need to preserve it.
//		d1 - Just need to ensure it's preserved properly.

pascal asm void PatchedPowerManagerOp()
{
	move.l		a1, -(sp);			// save a1
	move.l		a0, -(sp);			// save a0
	move.l		d1, -(sp);			// save d1
	jsr			SetUpA4;			// setup a4 world and get old a4 into d0
	move.l		gDelayTicks, -(sp);	// save delay in ticks
	move.l		gPrevProc, -(sp);	// save original trap address
	move.l		d0, a4;				// restore the original a4 (aka RestoreA4)
	move.l		(sp)+, a1;			// load original trap address (gPrevProc) into a1
	move.l		(sp)+, d0;			// load delay (gDelayTicks) into d0
	move.l		(sp)+, d1;			// restore d1
	move.l		(sp)+, a0;			// restore a0
	move.l		d0, -(sp);			// save delay (gDelayTicks) for later
	move.l		%0x0, -(sp);		// save shouldDelay (0 = no) for later

	move.w		(a0), d0;			// copy pmCommand (in a0) into d0
	cmpi.w		%0x0010, d0;		// check if pmCommand is 0x0010 (powerCntl)
	bne.s		@doDefault;			// skip if it's not
	
	move.b		12(a0), d0;			// copy pmData (first byte) into d0
	cmpi.b		%0x84, d0;			// check if pmData is 0x84 (hdOn)
	bne.s		@doDefault;			// skip if it's not
	
	move.l		(sp)+, d0;			// pop off the default shouldDelay (0 = no)
	move.l		%0x1, -(sp);		// save shouldDelay (1 = yes) for later

@doDefault:
	jsr			(a1);				// jump to the original trap

@shouldDelayBeforeReturning:
	exg.l		d0, a1;				// save d0 (original trap result) into a1
	move.l		(sp)+, d0;			// load shouldDelay into d0
	cmpi.l		%0x1, d0;			// check if we should delay or not
	exg.l		d0, a1;				// load d0 (original trap result)
	move.l		(sp)+, a1;			// load delay (gDelayTicks) into a1
	bne.s		@done;				// if we do not need to delay, go to the end

@checkDelayIsZero:
	exg.l		d0, a1;				// save d0 into a1, and put delay (gDelayTicks) into d0
	cmpi.l		%0x0, d0;			// check if the delay (gDelayTicks) is 0
	exg.l		d0, a1;				// restore d0
	beq.s		@done;				// if delay (gDelayTicks) is zero, go to the end

@doDelay:
	move.l		a0, -(sp);			// save a0
	move.l		d0, -(sp);			// save d0
	move.l		a1, a0;				// put delay (gDelayTicks) into a0
	_Delay;							// call the delay
	move.l		(sp)+, d0;			// restore d0
	move.l		(sp)+, a0;			// restore a0

@done:
	move.l		(sp)+, a1;			// restore a1
	rts;
}