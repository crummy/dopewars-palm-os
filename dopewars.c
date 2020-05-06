/* 
 * dopewars.c
 *
 * Matt Lee
 * mattlee@mit.edu
 *
 * Some code by Ken Shirriff (ken.shirriff@eng.sun.com)
 * (main event loop taken/adapted from RollingCubes)
 *
 * Copyright 1999 Matthew Lee
 * You may use ths code as you wish, but please give mention to me in derivative
 * works.
 *
 * Thanks goes out to all of the people who made great feature suggestions and
 * pointed out bugs.  This game is great because of your contributions!
 *
 */

/* revision history
 *
 * 1.0 : first release version
 * 1.5 : high score table added
 * 1.51: set focus to input fields
 * 1.52: rewrote number parser to fix bug
 *       removed "1" from number field default
 * 2.00: added auto save/resume feature
 *       added cheat codes
 * 2.01: put a limit on loan shark borrowing
 *       fixed a nasty UI bug dating back to v1.0
 * 2.10: changed loan shark to be more giving (4x cash instead of 2x)
 *       added high score verification for upcoming website
 *       removed $ from "None" entries in price table
 *       made Cancel button hidden on initial Travel screen
 * 2.15: changed loan shark to be more giving (30x cash instead of 4x)
 *       removed silly "Winners Don't Do Drugs" screen (the joke was getting old)
 * 2.2 : added "Max" button to buy/sell dialogs that buys or sells the maximum
 *         allowable amount
 *       added cheat menu for people who are too inept to use the keystroke commands
 *       changed cash/debt/savings displays to display "$x.x M" when amount is
 *         over $100 million
 *       made buy screen say "a lot" if the allowable purchase quantity is 
 *         over 1000
 *       added "Current location:" to Travel screen
 *
 */

#pragma pack(2)

#include "dopewars.h"
#include "verify.h"
#include <Common.h>
#include <System/SysAll.h>
#include <SysEvtMgr.h>
#include <UI/UIAll.h>

#define GAME_MESSAGE_COUNT		17
#define NAMELEN			20
#define SHARK_MULTIPLIER	30

// Define the minimum OS version we support
#define ourMinVersion	sysMakeROMVersion(2,0,0,sysROMStageRelease,0)

#define ABS(x) ((x)>=0)?(x):(-(x))

static Boolean simpleFormEvent (EventPtr event);
static Boolean controlFormEvent (EventPtr event);
static void EventLoop(void);
static DmOpenRef pmDB;

typedef struct MessageType {
	int freq;
	CharPtr msg;
	int drug;
	int plus;
	int minus;
	int add;
} Message;

typedef struct HighScoreType {
	char name [NAMELEN];
	long score;
} HighScore;

static HighScore hscores [5];

static Message gameMessages [GAME_MESSAGE_COUNT] = {
	{13, "The cops just did a big Weed bust!  Prices are sky-high!", 5, 4, 0, 0},
	{20, "The cops just did a big PCP bust!  Prices are sky-high!", 3, 4, 0, 0},
	{25, "The cops just did a big Heroin bust!  Prices are sky-high!", 4, 4, 0, 0},
	{13, "The cops just did a big Ludes bust!  Prices are sky-high!", 2, 4, 0, 0},
	{35, "The cops just did a big Cocaine bust!  Prices are sky-high!", 1, 4, 0, 0},
	{15, "The cops just did a big Speed bust!  Prices are sky-high!", 7, 4, 0, 0},
	{25, "Addicts are buying Heroin at outrageous prices!", 4, 8, 0, 0},
	{20, "Addicts are buying Speed at outrageous prices!", 7, 8, 0, 0},
	{20, "Addicts are buying PCP at outrageous prices!", 3, 8, 0, 0},
	{17, "Addicts are buying Shrooms at outrageous prices!", 6, 8, 0, 0},
	{35, "Addicts are buying Cocaine at outrageous prices!", 1, 8, 0, 0},
	{17, "The market has been flooded with cheap home-made Acid!", 0, 0, 8, 0},
	{10, "A Columbian freighter dusted the Coast Guard!  Weed prices have bottomed out!", 5, 0, 4, 0},
	{11, "A gang raided a local pharmacy and are selling cheap Ludes!", 2, 0, 8, 0},
	{55, "You found some Cocaine on a dead dude in the subway!", 1, 0, 0, 3},
	{45, "You found some Acid on a dead dude in the subway!", 0, 0, 0, 6},
	{35, "You found some PCP on a dead dude in the subway!", 3, 0, 0, 4}};

static int kidForm;
static int updatelocation;

static char s [40];

static int myDrugs [8];
static long myCash;
static long myDebt;
static long myBank;
static int myCoat;
static int myGuns;
static int myTotal;
static int myLocation;
static int timeLeft;
static int copCount;
static char myLocationName [20];
static int cheatsEnabled;
static int cheatKey;

static int drawflag;

static long drugPrices [8];

typedef struct SaveGameStruct
{
  int myDrugs [8];
  long myCash;
  long myDebt;
  long myBank;
  int myCoat;
  int myGuns;
  int myTotal;
  int myLocation;
  int timeLeft;
  int copCount;
  char myLocationName [20];
  long drugPrices [8];
  int currentForm;
  int cheatsEnabled;
  char FillerForFutureExpansion[16];
} SaveGameType;

static void MakeScoreVerificationCode(int nScoreIndex)
{
	int i, j, k;
	char t;

	StrIToA(s, hscores[nScoreIndex].score);
	StrCopy(s + StrLen(s), hscores[nScoreIndex].name);

	for (i = StrLen(s); i < 40; i++)
		s[i] = (char) (i % MAGIC_NUMBER_A);
	s[39] = '\0';

	j = MAGIC_NUMBER_B % 10;
	k = MAGIC_NUMBER_C % 20;
	for (i = 0; i < 40; i++)
	{
		t = s[j];
		s[j] = s[k];
		s[k] = t;
		j = (j + MAGIC_NUMBER_D + i) % 20;
		k = (k + MAGIC_NUMBER_E + i) % 20;
	}

	for (i = 0; i < 40; i += 8)
	{
		s[i] = (s[i] + MAGIC_NUMBER_A) % 256;
		s[i + 1] = (s[i + 1] + MAGIC_NUMBER_B) % 256;
		s[i + 2] = (s[i + 2] + MAGIC_NUMBER_C) % 256;
		s[i + 3] = (s[i + 3] + MAGIC_NUMBER_D) % 256;
		s[i + 4] = (s[i + 4] + MAGIC_NUMBER_E) % 256;
		s[i + 5] = (s[i + 5] + MAGIC_NUMBER_F) % 256;
		s[i + 6] = (s[i + 6] + MAGIC_NUMBER_G) % 256;
		s[i + 7] = (s[i + 7] + MAGIC_NUMBER_H) % 256;
	}

/*
	for (i = 0; i < 8; i++)
		s[i] = s[2 * i];
*/

	for (i = 0; i < 8; i++)
		s[i] = (s[i] & 0x0F) + 65;

/*
	for (i = 0; i < 8; i++)
		s[i] = 65;
*/

	s[8] = '\0';
}

/***********************************************************************
 *
 * FUNCTION:    RomVersionCompatible
 *
 * DESCRIPTION: This routine checks that a ROM version is meet your
 *              minimum requirement.
 *
 * PARAMETERS:  requiredVersion - minimum rom version required
 *                                (see sysFtrNumROMVersion in SystemMgr.h 
 *                                for format)
 *              launchFlags     - flags that indicate if the application 
 *                                UI is initialized.
 *
 * RETURNED:    error code or zero if rom is compatible
 *
 * REVISION HISTORY:
 *
 ***********************************************************************/
static Err RomVersionCompatible(DWord requiredVersion, Word launchFlags)
{
	DWord romVersion;

	// See if we're on in minimum required version of the ROM or later.
	FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
	if (romVersion < requiredVersion)
		{
		if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) ==
			(sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
			{
			FrmAlert (RomIncompatibleAlert);
		
			}
		
		return (sysErrRomIncompatible);
		}

	return (0);
}

static void InitHighScores (void)
{
 	int i;
	for (i = 0; i < 5; i++)
	{
		StrCopy (hscores [i].name, " ");
		hscores [i].score = 0;
	}
}

static void InsertScore (CharPtr playername, long newscore)
{
	int i = 0, j = 0;
	while (i < 5)
	{
		if (newscore >= hscores [i].score)
		{
			for (j = 4; j > i; j--)
			{
				hscores [j].score = hscores [j - 1].score;
				StrCopy (hscores [j].name, hscores [j - 1].name);
			}
			hscores [i].score = newscore;
			StrCopy (hscores [i].name, playername);
			i = 10;
		}
		i++;
	}
}

static void GetHighScoreName ()
{
	FormPtr frm;
	int d;
	CharPtr c;

	int objindex;
	FieldPtr fp;
	long result;
	long a, b;
	int i;

	frm = FrmInitForm (kidHighScore);
	FrmSetFocus (frm, FrmGetObjectIndex (frm, 3000));
	d = FrmDoDialog (frm);

	objindex = FrmGetObjectIndex (frm, 3000);
	fp = (FieldPtr) FrmGetObjectPtr (frm, objindex);
	c = FldGetTextPtr (fp);
	if (c == NULL)
	{
		StrCopy (s, "-- No Name --");
	}
	else
	{
		StrCopy (s, c);
	}
	InsertScore (s, myCash);
	FrmDeleteForm (frm);
}	

/**
 * Database handling routines for the high scores.
 */
static Err OpenDatabase(void)
{
  UInt          index = 0;
  VoidHand      RecHandle;
  VoidPtr       RecPointer;
  Err           err;

  // Create database, if it doesn't exist, and save default game status.
  if (!(pmDB = DmOpenDatabaseByTypeCreator('Data', 'Dope', dmModeReadWrite))) {
    if ((err = DmCreateDatabase(0, "DopeWarsDB", 'Dope', 'Data', false)))
      return err;
    pmDB = DmOpenDatabaseByTypeCreator('Data', 'Dope', dmModeReadWrite);

    InitHighScores ();
    RecHandle = DmNewRecord(pmDB, &index, sizeof(hscores));
    DmWrite(MemHandleLock(RecHandle), 0, &hscores, sizeof(hscores));
    MemHandleUnlock(RecHandle);
    DmReleaseRecord(pmDB, index, true);
  }

  // Load a saved game status.
  RecHandle = DmGetRecord(pmDB, 0);
  RecPointer = MemHandleLock(RecHandle);
  MemMove(&hscores, RecPointer, sizeof(hscores));
  MemHandleUnlock(RecHandle);
  DmReleaseRecord(pmDB, 0, true);

  return 0;
}

/*
 * Save game status information.
 */
static void SaveStatus()
{
  VoidPtr p = MemHandleLock(DmGetRecord(pmDB, 0));
  DmWrite(p, 0, &hscores, sizeof(hscores));
  MemPtrUnlock(p);
  DmReleaseRecord(pmDB, 0, true);
}

static void makeDrugPrices (int leaveout)
{
	int i, j;

	drugPrices [0] = 1000 + SysRandom (0) % 3500;
	drugPrices [1] = 15000 + SysRandom (0) % 15000;
	drugPrices [2] = 10 + SysRandom (0) % 50;
	drugPrices [3] = 1000 + SysRandom (0) % 2500;
	drugPrices [4] = 5000 + SysRandom (0) % 9000;
	drugPrices [5] = 300 + SysRandom (0) % 600;
	drugPrices [6] = 600 + SysRandom (0) % 750;
	drugPrices [7] = 70 + SysRandom (0) % 180;

	for (i = 0; i < leaveout; i++)
	{
		j = SysRandom (0) % 8;
		drugPrices [j] = 0;
	}
}

static CharPtr getDrugName (int drugnum)
{
	FormPtr frm;
	int objindex;
	ControlPtr cp;

	drugnum = drugnum % 8;
	frm = FrmGetFormPtr (kidBuySell);
	if (frm == NULL)
		return (NULL);
	objindex = FrmGetObjectIndex (frm, 2000 + drugnum);
	cp = (ControlPtr) FrmGetObjectPtr (frm, objindex);
	if (cp == NULL)
		return (NULL);
	return (CtlGetLabel (cp));
}

static void setFieldNumeric (int formid, int fieldid)
{
	FormPtr frm;
	int objindex;
	FieldPtr fp;
	CharPtr c;

	frm = FrmGetFormPtr (formid);
	objindex = FrmGetObjectIndex (frm, fieldid);
	fp = (FieldPtr) FrmGetObjectPtr (frm, objindex);
	fp->attr.numeric = 1;
	c = FldGetTextPtr (fp);
	if (c != NULL)
		FldDelete (fp, 0, StrLen (c));	
	//FldInsert (fp, "1", 1);
	FrmSetFocus (frm, FrmGetObjectIndex (frm, fieldid));
}	

static long getFieldValue (FormPtr frm, int fieldid)
{
	int objindex;
	FieldPtr fp;
	CharPtr c;
	long result;
	long a, b;
	int i, len;

	objindex = FrmGetObjectIndex (frm, fieldid);
	fp = (FieldPtr) FrmGetObjectPtr (frm, objindex);
	c = FldGetTextPtr (fp);
	if (c == NULL)
		return 0;
	len = StrLen (c);
	if (len < 5)
		result = (long)StrAToI (c);
	else
	{
		StrNCopy (s, c, len);
		a = 0; b = 1;
		for (i = len - 1; i >= 0; i--, b *= 10)
		{
			a += ((char)s[i] - 48) * b;
		}
		result = a;
	}
	return result;
}	

static void initGame (void)
{
	int i;
	for (i = 0; i < 8; i++)
		myDrugs [i] = 0;

	myCash = 2000;
	myDebt = 5500;
	myBank = 0;
	myCoat = 100;
	myGuns = 0;
	myTotal = 0;
	myLocation = 0;
	timeLeft = 31;
	copCount = 3;
	cheatsEnabled = 0;
	cheatKey = 0;
	StrCopy (myLocationName, "No Name");
	//getLocationName (2000);
	makeDrugPrices (3);
	updatelocation = 0;
}

static void LoadApplicationState()
{
  Word	prefsSize;
  SWord	prefsVersion = noPreferenceFound;
  SaveGameType sg;
  int i;
	
  prefsSize = sizeof (SaveGameType);
  prefsVersion = PrefGetAppPreferences ('Dope', 0, &sg, &prefsSize, true);
	
  if (prefsVersion > 1)
    prefsVersion = noPreferenceFound;
		
  if (prefsVersion == noPreferenceFound)
  {
    initGame();
    kidForm = kidIntro;
  }
  else
  {
    myCash = sg.myCash;
    myDebt = sg.myDebt;
    myBank = sg.myBank;
    myCoat = sg.myCoat;
    myGuns = sg.myGuns;
    myTotal = sg.myTotal;
    myLocation = sg.myLocation;
    timeLeft = sg.timeLeft;
    copCount = sg.copCount;
    kidForm = sg.currentForm;
    cheatsEnabled = sg.cheatsEnabled;
    cheatKey = 0;
    for(i = 0; i < 8; i++)
    {
      myDrugs[i] = sg.myDrugs[i];
      drugPrices[i] = sg.drugPrices[i];
    }
    StrCopy(myLocationName, sg.myLocationName);
  }
}

static void finishGame (void)
{
	int i;

	makeDrugPrices (0);
	myCash += (myBank - myDebt);
	myBank = 0;
	myDebt = 0;
	for (i = 0; i < 8; i++)
	{
		myCash += (myDrugs [i] * drugPrices [i]);
		myDrugs [i] = 0;
	}
}	

static void setCurrentWinTitle (CharPtr s)
{
	FormPtr frm;
	CharPtr wintitle;

	frm = FrmGetActiveForm ();
	//wintitle = FrmGetTitle (frm);
	//StrCopy (wintitle, s);
	FrmSetTitle (frm, s);
}		

static void setFormTitle (int formID, CharPtr s)
{
	FormPtr frm;
	CharPtr wintitle;

	frm = FrmGetFormPtr (formID);
	wintitle = FrmGetTitle (frm);
	StrCopy (wintitle, s);
	FrmSetTitle (frm, wintitle);
}

static void setLabelText (FormPtr frm, int labelid, CharPtr s)
{
	int objindex;
	ControlPtr cp;
	CharPtr c;

	objindex = FrmGetObjectIndex (frm, labelid);
	cp = (ControlPtr) FrmGetObjectPtr (frm, objindex);
	c = CtlGetLabel (cp);
	StrCopy (c, s);
}

static void DrawHighScores (int offset, int drawmode)
{
	int i;
	RectangleType a;

	a.topLeft.x = 5;
	a.topLeft.y = offset;
	a.extent.x = 88;
	a.extent.y = 12;
	FntSetFont (1);
	for (i = 0; i < 5; i++)
	{
		if (hscores [i].score < 1)
			continue;
		WinSetClip (&a);
		WinDrawChars (hscores [i].name, StrLen (hscores [i].name), 5, offset + (12 * i));
		WinResetClip ();
		a.topLeft.y += 12;
		if (drawmode == 0)
		{
			StrIToA (s, hscores [i].score);
			WinDrawChars ("$", 1, 94, offset + (12 * i));
			WinDrawChars (s, StrLen (s), 100, offset + (12 * i));
			FntSetFont (0);
		}
		else
		{
			MakeScoreVerificationCode(i);
			WinDrawChars(s, StrLen(s), 100, offset + (12 * i));
			FntSetFont (0);
		}
	}
}	 		

static void SetCancelButtonVisible(int visible)
{
	FormPtr frm;
	int objIndex;

	frm = FrmGetFormPtr (kidTravel);
	objIndex = FrmGetObjectIndex (frm, kidOk);

	if (visible == 0)
		FrmHideObject(frm, objIndex);
	else
		FrmShowObject(frm, objIndex);
}

static void ProcessForm (int formcode)
{
	int i, j, sum;
	long p, q, r, n;
	CharPtr c;
	RectangleType a, b;

	if (formcode == kidBuySell)
	{
		FntSetFont (0);
		a.topLeft.x = 6;
		a.extent.x = 18;
		a.extent.y = 11;

		b.topLeft.x = 75;
		b.extent.x = 30;
		b.extent.y = 11;
		setCurrentWinTitle (myLocationName);
		for (i = 20, j = 0, sum = 0; i < 124; i += 13, j++)
		{	
			a.topLeft.y = i;
			b.topLeft.y = i;
			WinEraseRectangle (&a, 0);
			WinEraseRectangle (&b, 0);
			StrIToA (s, myDrugs [j]);
			sum += myDrugs [j];
			WinDrawChars (s, StrLen (s), 7, i);
			if (drugPrices [j] > 0)
			{
				StrCopy(s, "$");
				StrIToA (s + StrLen(s), drugPrices [j]);
			}
			else
			{
				StrCopy (s, "   None");
			}
			WinDrawChars (s, StrLen (s), 69, i);
		}
		a.topLeft.x = 115;
		a.extent.x = 40;
		a.topLeft.y = 32;
		WinEraseRectangle (&a, 0);
		if (myCash < 100000000)
		{
			StrIToA (s, myCash);
		}
		else
		{
			p = (myCash / 10000) % 100;
			StrIToA (s, myCash / 1000000);
			if (p < 10)
				StrCopy(s + StrLen(s), ".0");
			else
				StrCopy(s + StrLen(s), ".");
			StrIToA (s + StrLen(s), p); 
			StrCopy(s + StrLen(s), " M");
		}
		WinDrawChars (s, StrLen (s), 115, 32);
		a.topLeft.y = 58;
		WinEraseRectangle (&a, 0);
		if (myDebt < 100000000)
		{
			StrIToA (s, myDebt);
		}
		else
		{
			p = (myDebt / 10000) % 100;
			StrIToA (s, myDebt / 1000000);
			if (p < 10)
				StrCopy(s + StrLen(s), ".0");
			else
				StrCopy(s + StrLen(s), ".");
			StrIToA (s + StrLen(s), p); 
			StrCopy(s + StrLen(s), " M");
		}
		WinDrawChars (s, StrLen (s), 115, 58);
		a.topLeft.y = 84;
		WinEraseRectangle (&a, 0);
		if (myBank < 100000000)
		{
			StrIToA (s, myBank);
		}
		else
		{
			p = (myBank / 10000) % 100;
			StrIToA (s, myBank / 1000000);
			if (p < 10)
				StrCopy(s + StrLen(s), ".0");
			else
				StrCopy(s + StrLen(s), ".");
			StrIToA (s + StrLen(s), p); 
			StrCopy(s + StrLen(s), " M");
		}
		WinDrawChars (s, StrLen (s), 115, 84);
		a.topLeft.y = 110;
		WinEraseRectangle (&a, 0);
		StrIToA (s, sum);
		StrCopy (s + StrLen (s), "/");
		StrIToA (s + StrLen (s), myCoat);
		WinDrawChars (s, StrLen (s), 115, 110);
		a.topLeft.y = 125;
		a.topLeft.x = 6;
		a.extent.x = 150;
		WinEraseRectangle (&a, 0);
		StrCopy (s, "Time remaining: ");
		StrIToA (s + StrLen (s), timeLeft);
		StrCopy (s + StrLen (s), " days");
		WinDrawChars (s, StrLen (s), 7, 126);
		if (myGuns)
		{
			StrCopy (s, "Gun");
			FntSetFont (1);
			WinDrawChars (s, StrLen (s), 123, 126);
			FntSetFont (0);
		}
		myTotal = sum;
	}
	else if (formcode == kidBuy)
	{
		setFieldNumeric (kidBuy, 3000);
	}
	else if (formcode == kidGameOver)
	{
		FntSetFont (1);
		StrIToA (s, myCash);
		WinDrawChars (s, StrLen (s), 64, 20);
		FntSetFont (0);
		if (copCount == -1)
		{
			StrCopy (s, "You're dead.");
			WinDrawChars (s, StrLen (s), 5, 35);
			StrCopy (s, "Congratulations.");
			WinDrawChars (s, StrLen (s), 5, 46);
		}
		else if (myCash < 0)
		{
			StrCopy (s, "The Loan Shark's thugs broke your");
			WinDrawChars (s, StrLen (s), 5, 35);
			StrCopy (s, "legs.");
			WinDrawChars (s, StrLen (s), 5, 46);
		}
		else if (myCash >= 1000000)
		{
			StrCopy (s, "You retired a millionaire in the");
			WinDrawChars (s, StrLen (s), 5, 35);
			StrCopy (s, "Carribbean.");
			WinDrawChars (s, StrLen (s), 5, 46);
		}
		else if (myCash > 2000)
		{
			StrCopy (s, "Congratulations!");
			WinDrawChars (s, StrLen (s), 5, 35);
			StrCopy (s, "You didn't do half bad.");
			WinDrawChars (s, StrLen (s), 5, 46);
		}
		else
		{
			StrCopy (s, "You didn't make any money!");
			WinDrawChars (s, StrLen (s), 5, 35);
			StrCopy (s, "Better luck next time.");
			WinDrawChars (s, StrLen (s), 5, 46);
		}
		DrawHighScores (75, 0);
		FntSetFont (0);
	}
	else if (formcode == kidScoresDlg)
	{
		DrawHighScores (15, drawflag);
		if (hscores [0].score == 0)
		{
			FntSetFont (1);
			StrCopy (s, "There are no high scores.");
			WinDrawChars (s, StrLen (s), 16, 40);
			FntSetFont (0);
		} 
	}
	else if (formcode == kidTravel)
	{
		SetCancelButtonVisible(myLocation != 0);
		if (myLocation != 0)
		{
			FntSetFont(1);
			i = FntCharsWidth(myLocationName, StrLen(myLocationName));
			j = 80 - ((70 + i) / 2);
			FntSetFont(0);
			StrCopy(s, "Current location: ");
			WinDrawChars(s, StrLen(s), j, 123);
			FntSetFont(1);
			WinDrawChars(myLocationName, StrLen(myLocationName), j + 70, 123);
			FntSetFont(0);
		}
	}
}

static void NegotiateBuy (int drugnum)
{
	FormPtr frm;
	int okay = 0;
	long count;
	long maxCount;
	int d;
	CharPtr c;

	frm = FrmInitForm (kidBuy);
	setFieldNumeric (kidBuy, 3000);
	c = getDrugName (drugnum);
	StrCopy (s, "Buy ");
	StrCopy (s + StrLen (s), c);
	setFormTitle (kidBuy, s);
	StrCopy (s, "At $");
	StrIToA (s + StrLen (s), drugPrices [drugnum]);
	StrCopy (s + StrLen (s), " each, you can afford ");
	count = myCash / drugPrices [drugnum];
	maxCount = count;
	if (count < 1000)
	{
		StrIToA (s + StrLen (s), count);
		StrCopy (s + StrLen (s), ".");
	}
	else
	{
		StrCopy (s + StrLen (s), "a lot.");
	}
	setLabelText (frm, 4000, s);
	while (okay == 0)
	{
		d = FrmDoDialog (frm);
		//StrIToA (s, d);
		//WinDrawChars (s, StrLen (s), 121, 10);
		if (d == 3001 || d == 0)
		{
			okay = 1;
			count = 0;
			continue;
		}
		if (d == 3003)
		{
			count = maxCount;
		}
		else
		{
			count = getFieldValue (frm, 3000);
		}
		if ((count * drugPrices [drugnum]) > myCash)
		{
			FrmAlert (kidLackMoney);
			continue;
		}
		if ((count + myTotal) > myCoat)
		{
			count = myCoat - myTotal;
			okay = 1;
			//FrmAlert (kidNoRoom);
			continue;
		}
		okay = 1;
	}
	FrmDeleteForm (frm);
	myCash -= count * drugPrices [drugnum];
	myDrugs [drugnum] += count;
}	

static void NegotiateSell (int drugnum)
{
	FormPtr frm;
	int okay = 0;
	long count;
	long maxCount;
	int d;
	CharPtr c;

	frm = FrmInitForm (kidSell);
	setFieldNumeric (kidSell, 3000);
	c = getDrugName (drugnum);
	StrCopy (s, "Sell ");
	StrCopy (s + StrLen (s), c);
	setFormTitle (kidSell, s);
	StrCopy (s, "You can sell up to ");
	StrIToA (s + StrLen (s), myDrugs [drugnum]);
	StrCopy (s + StrLen (s), " at $");
	StrIToA (s + StrLen (s), drugPrices [drugnum]);
	StrCopy (s + StrLen (s), " each.");
	setLabelText (frm, 4000, s);
	maxCount = myDrugs[drugnum];
	while (okay == 0)
	{
		d = FrmDoDialog (frm);
		if (d == 3001 || d == 0)
		{
			okay = 1;
			count = 0;
			continue;
		}
		if (d == 3003)
		{
			count = maxCount;
		}
		else
		{
			count = getFieldValue (frm, 3000);
		}
		if ((myDrugs [drugnum] - count) < 0)
		{
			FrmAlert (kidOverSell);
			continue;
		}
		okay = 1;
	}
	FrmDeleteForm (frm);
	myCash += count * drugPrices [drugnum];
	myDrugs [drugnum] -= count;
}	

static void BuyDrugs (int drugnum)
{
	CharPtr c;

	if (drugPrices [drugnum] == 0)
	{
		FrmAlert (kidNoSellers);
		return;
	}
	if (drugPrices [drugnum] > myCash)
	{
		FrmAlert (kidNoCash);
		return;
	}
	if (myTotal + 1 > myCoat)
	{
		FrmAlert (kidNoRoom);
		return;
	}
	NegotiateBuy (drugnum);
}	

int getLocationName (int evtcode)
{
	FormPtr frm;
	int objindex;
	ControlPtr cp;
	CharPtr label;
	//char s [20];

	if (evtcode == myLocation)
		return 0;
	frm = FrmGetFormPtr (kidTravel);
	objindex = FrmGetObjectIndex (frm, evtcode);
	cp = (ControlPtr) FrmGetObjectPtr (frm, objindex);
	label = CtlGetLabel (cp);
	StrCopy (myLocationName, label);
	myLocation = evtcode;
	//WinDrawChars (myLocationName, StrLen (myLocationName), 10, 100);
	return 1;
}

static void SellDrugs (int drugnum)
{
	if (myDrugs [drugnum] < 1)
	{
		FrmAlert (kidNoDrugs);
		return;
	}
	if (drugPrices [drugnum] == 0)
	{
		FrmAlert (kidNoBuyers);
		return;
	}
	NegotiateSell (drugnum);
}

static void VisitShark (void)
{
	FormPtr frm;
	int result = 3;
	int okay = 0;
	long count;
	long maxloan;
	int d, loantoday = 0;

	while (result != 0)
	{
		result = FrmAlert (kidSharkOptions);
		if (result == 1)
		{
			if (myDebt < 1)
			{
				FrmAlert (kidNoDebt);
				continue;
			}
			okay = 0;
			frm = FrmInitForm (kidAmount);
			setFieldNumeric (kidAmount, 3000);
			setLabelText (frm, 4000, "How much would you like to repay?");
			while (okay == 0)
			{
				d = FrmDoDialog (frm);
				if (d == 3001 || d == 0)
				{
					okay = 1;
					count = 0;
					continue;
				}
				count = getFieldValue (frm, 3000);
				if (count < 0)
					continue;
				if ((myCash - count) < 0)
				{
					FrmAlert (kidLackMoney);
					continue;
				}
				if ((myDebt - count) < 0)
				{
					FrmAlert (kidOverPay);
					continue;
				}
				okay = 1;
			}
			FrmDeleteForm (frm);
			myDebt -= count;
			myCash -= count;
			ProcessForm (kidBuySell);
		}				
		else if (result == 2)
		{
			if (loantoday == 1)
			{
				FrmAlert(kidNoMoreLoans);
				continue;
			}
			maxloan = myCash * SHARK_MULTIPLIER;
			if (maxloan < 5500)
				maxloan = 5500;
			if ((maxloan + myCash) > 999999999)
				maxloan = 999999999 - myCash;
			okay = 0;
			frm = FrmInitForm (kidAmount);
			setFieldNumeric (kidAmount, 3000);
			setLabelText (frm, 4000, "How much would you like to borrow?");
			while (okay == 0)
			{
				d = FrmDoDialog (frm);
				if (d == 3001 || d == 0)
				{
					okay = 2;
					count = 0;
					continue;
				}
				count = getFieldValue (frm, 3000);
				if (count < 0)
					continue;
				if (count > maxloan)
				{
					StrIToA(s, maxloan);
					FrmCustomAlert (kidBorrowLimit, s, " ", " ");
					continue;
				}
				if ((count + myCash) > 99999999)
					continue;
				okay = 1;
			}
			FrmDeleteForm (frm);
			myDebt += count;
			myCash += count;
			if (okay == 1)
				loantoday = 1;
			ProcessForm (kidBuySell);
		}				
	}
}

static void VisitBank (void)
{
	FormPtr frm;
	int result = 3;
	int okay = 0;
	long count;
	int d;

	while (result != 0)
	{
		result = FrmAlert (kidBankOptions);
		if (result == 1)
		{
			okay = 0;
			frm = FrmInitForm (kidAmount);
			setFieldNumeric (kidAmount, 3000);
			setLabelText (frm, 4000, "How much would you like to withdraw?");
			while (okay == 0)
			{
				d = FrmDoDialog (frm);
				if (d == 3001 || d == 0)
				{
					okay = 1;
					count = 0;
					continue;
				}
				count = getFieldValue (frm, 3000);
				if (count < 0)
					continue;
				if ((myBank - count) < 0)
				{
					FrmAlert (kidLackBank);
					continue;
				}
				okay = 1;
			}
			FrmDeleteForm (frm);
			myBank -= count;
			myCash += count;
			ProcessForm (kidBuySell);
		}				
		else if (result == 2)
		{
			okay = 0;
			frm = FrmInitForm (kidAmount);
			setFieldNumeric (kidAmount, 3000);
			setLabelText (frm, 4000, "How much would you like to deposit?");
			while (okay == 0)
			{
				d = FrmDoDialog (frm);
				if (d == 3001 || d == 0)
				{
					okay = 1;
					count = 0;
					continue;
				}
				count = getFieldValue (frm, 3000);
				if (count < 0)
					continue;
				if (count > myCash)
				{
					FrmAlert (kidLackMoney);
					continue;
				}
				okay = 1;
			}
			FrmDeleteForm (frm);
			myBank += count;
			myCash -= count;
			ProcessForm (kidBuySell);
		}				
	}
}

static void VerifyScoreDialog(void)
{
	int result = 3;
	int scoreindex = 0;
	char strbuf[12];

	if (hscores[0].score == 0)
	{
		result = FrmAlert(kidNoHighScores);
		return;
	}
	while (result != 0)
	{
		StrIToA(strbuf, hscores[scoreindex].score);
		MakeScoreVerificationCode(scoreindex);
		result = FrmCustomAlert (kidVerifyDlg, hscores[scoreindex].name, strbuf, s);
		if (result == 1)
		{
			do
			{
				scoreindex--;
				if (scoreindex < 0)
					scoreindex = 4;
			} while (hscores[scoreindex].score == 0);
		}				
		else if (result == 2)
		{
			do
			{
				scoreindex++;
				if (scoreindex > 4)
					scoreindex = 0;
			} while (hscores[scoreindex].score == 0);
		}				
	}
}

static void FuzzEncounter (void)
{
	int i;
	int result = 3;
	int CopForm = kidCops;
	int resolved = 0;
	char isare [5];

	if (copCount < 1)
		return;
	
	while (resolved == 0)
	{
		switch (copCount)
		{
		case 1:
			StrCopy (s, "");
			StrCopy (isare, "is ");
			break;
		case 2:
			StrCopy (s, "and one of his deputies ");
			StrCopy (isare, "are ");
			break;
		case 3:
			StrCopy (s, "and two of his deputies ");
			StrCopy (isare, "are ");
			break;
		default:
			StrCopy (s, "");
			StrCopy (isare, "is ");
			break;
		}
		result = FrmCustomAlert (CopForm, s, isare, "");
		if (result == 0)
		{
			if (!(SysRandom (0) % 3))
			{
				if (!(SysRandom (0) % 5))
				{
					resolved = 2;
					FrmAlert (kidCopsCaught);
					continue;
				}
				resolved = 0;
				CopForm = kidCopsRemain;
				continue;
			}
			else
			{
				resolved = 1;
				FrmAlert (kidCopsRan);
				continue;
			}
		}				
		else if (result == 1)
		{
			if (myGuns == 0)
			{
				FrmAlert (kidNoGun);
				continue;
			}
			if (!(SysRandom (0) % 4))
			{
				FrmAlert (kidCopsHit);
				copCount--;
				if (copCount < 1)
				{
					resolved = 1;
					continue;
				}
				CopForm = kidCopsRemain;				
			}
			else
			{
				FrmAlert (kidCopsMiss);
				CopForm = kidCopsRemain;
			}
			if (!(SysRandom (0) % 5))
			{
				if (!(SysRandom (0) % 5))
				{
					FrmAlert (kidCopsKill);
					timeLeft = 0;
					copCount = -1;
					resolved = 1;
					continue;
				}
				FrmAlert (kidCopsWounded);
				resolved = 2;
				continue;
			}
			else
			{
				FrmAlert (kidCopsMissYou);
			}
		}				
	}
	if (resolved == 2)
	{
		FrmAlert (kidCopsSeize);
		for (i = 0; i < 8; i++)
			myDrugs [i] = 0;
		myCash /= 2;
		ProcessForm (kidBuySell);
	}
}

static void DoRandomStuff (void)
{
	int i;
	int addcount;
	for (i = 0; i < GAME_MESSAGE_COUNT; i++)
	{
		if (!(SysRandom (0) % gameMessages [i].freq))
		{
			if (drugPrices [gameMessages [i].drug] == 0)
				continue;
			FrmCustomAlert (kidMessage, gameMessages [i].msg, " ", " ");
			if (gameMessages [i].plus > 0)
				drugPrices [gameMessages [i].drug] *= gameMessages [i].plus;
			if (gameMessages [i].minus > 0)
				drugPrices [gameMessages [i].drug] /= gameMessages [i].minus;
			if (gameMessages [i].add + myTotal > myCoat)
				addcount = myCoat - myTotal;
			else
				addcount = gameMessages [i].add;
			myDrugs [gameMessages [i].drug] += addcount;
			ProcessForm (kidBuySell);
		}
	}
}

static void DoTravel (void)
{
	int result;

	if (timeLeft < 31)
		myDebt += (myDebt >> 3);
	myBank += (myBank >> 4);
	timeLeft--;
	ProcessForm (kidBuySell);
	if (!(SysRandom (0) % 7) && timeLeft && myTotal)
		FuzzEncounter ();
	if (timeLeft == 0)
	{
		finishGame ();
		updatelocation = 0;
		OpenDatabase ();
		if (myCash >= hscores [4].score && !cheatsEnabled)
			GetHighScoreName ();
		SaveStatus ();
    		DmCloseDatabase(pmDB);
		FrmGotoForm (kidGameOver);
		return;
	}
	if (!(SysRandom (0) % 10))
	{
		result = FrmAlert (kidCoat);
		if (result == 1)
		{
			if (myCash < 200)
				FrmAlert (kidLackMoney);
			else
			{
				myCash -= 200;
				myCoat += 40;
				ProcessForm (kidBuySell);
			}
		}
	}
	if (myGuns == 0 && !(SysRandom (0) % 10))
	{
		result = FrmAlert (kidGun);
		if (result == 1)
		{
			if (myCash < 400)
				FrmAlert (kidLackMoney);
			else
			{
				myCash -= 400;
				myGuns = 1;
				ProcessForm (kidBuySell);
			}
		}
	}
	DoRandomStuff ();
	if (myLocation == 2000)
	{
		result = FrmAlert (kidLoanShark);
		if (result == 1)
			VisitShark ();
		result = FrmAlert (kidBank);
		if (result == 1)
			VisitBank ();
	}
	updatelocation = 0;
}

static void ProcessControl (int formcode, int evtcode)
{
	if (formcode == kidBuySell)
	{
		if (evtcode >= 2010 && evtcode < 2020)
		{
			SellDrugs (evtcode % 10);
			ProcessForm (kidBuySell);
			return;
		}
		else if (evtcode >= 2020 && evtcode < 2030)
		{
			BuyDrugs (evtcode % 10);
			ProcessForm (kidBuySell);
			return;
		}
		else if (evtcode == 2030)
		{
			FrmAlert (kidSellMsg);
			return;
		}
		else if (evtcode == 2032)
		{
			//BuyDrugList ();
			FrmAlert (kidBuyMsg);
			return;
		}
		else if (evtcode == 2031)
		{
			FrmGotoForm (kidTravel);
			return;
		}
	}
	else if (formcode == kidTravel)
	{
		if (evtcode == kidOk)
		{
    			if (timeLeft > 30)
      				return;
    			kidForm++;
			FrmGotoForm (kidForm);
		}
		else if (evtcode >= 2000 && evtcode < 2010)
		{
			if (getLocationName (evtcode))
				updatelocation = 1;
			FrmGotoForm (kidBuySell);
			return;
		}
	}
	else if (formcode == kidScoresDlg)
	{
		if (evtcode == 3001)
		{
			kidForm = kidBuySell;
			FrmReturnToForm (kidForm);
		}
	}
}

DWord  PilotMain (Word cmd, Ptr cmdPBP, Word launchFlags)
{

  Err error;
  error = RomVersionCompatible (ourMinVersion, launchFlags);
  if (error) return (error);


  if (cmd == sysAppLaunchCmdNormalLaunch)
  {
    LoadApplicationState();
    FrmGotoForm(kidForm);

    EventLoop();  // Event loop
  }
  return 0;
}

static void SaveApplicationState(void)
{
  SaveGameType sg;
  int i;
  
  for(i = 0; i < 8; i++)
  {
    sg.myDrugs[i] = myDrugs[i];
    sg.drugPrices[i] = drugPrices[i];
  }

  sg.myCash = myCash;
  sg.myDebt = myDebt;
  sg.myBank = myBank;
  sg.myCoat = myCoat;
  sg.myGuns = myGuns;
  sg.myTotal = myTotal;
  sg.myLocation = myLocation;
  sg.timeLeft = timeLeft;
  sg.copCount = copCount;
  sg.currentForm = kidForm;
  sg.cheatsEnabled = cheatsEnabled;

  StrCopy(sg.myLocationName, myLocationName);

  PrefSetAppPreferences('Dope', 0, 1, &sg, sizeof(SaveGameType), true);
}  

static void EventLoop(void)
{
  short err;
  int formID;
  FormPtr form;
  EventType event;
		
  do
  {

    EvtGetEvent(&event, evtWaitForever);

    if (SysHandleEvent(&event)) continue;
    if (MenuHandleEvent((void *)0, &event, &err)) continue;

    if (event.eType == frmLoadEvent)
    {
      formID = event.data.frmLoad.formID;
      form = FrmInitForm(formID);
      FrmSetActiveForm(form);
      switch (formID) 
      {
      case kidIntro:
	case kidIntro2:
	case kidGameOver:
        FrmSetEventHandler(form, (FormEventHandlerPtr) simpleFormEvent);
        break;
	default:
        FrmSetEventHandler(form, (FormEventHandlerPtr) controlFormEvent);
	  break;	
      }
    }

    FrmDispatchEvent(&event);
  } while(event.eType != appStopEvent);

  SaveApplicationState();
//    DmCloseDatabase(pmDB);
}

static Boolean simpleFormEvent (EventPtr event)
{
  FormPtr   form;
  int       handled = 0;

  switch (event->eType) 
  {
  case frmOpenEvent:				
    form = FrmGetActiveForm();
    FrmDrawForm(form);
    if (updatelocation)
      makeDrugPrices (3);
    kidForm = event->data.frmLoad.formID;
    ProcessForm (event->data.frmLoad.formID);
    if (updatelocation)
      DoTravel ();			
    handled = 1;
    break;
    
  case ctlSelectEvent:
    if (myLocation == 0 && kidForm == kidTravel)
      break;
    kidForm++;
    if (kidForm == kidIntro2)
	kidForm++;
    if (kidForm > 1005)
      initGame ();
    if (kidForm > kidFormLast)
      kidForm = kidTravel;
    if (myLocation != 0)
      kidForm = kidBuySell;
    FrmGotoForm(kidForm);
    handled = 1;

  }
  return handled;
}

static void HighScoreDialog (void)
{
	drawflag = 0;
	FrmPopupForm (kidScoresDlg);
}

/*
static void VerifyScoreDialog (void)
{
	drawflag = 1;	
	FrmPopupForm (kidScoresDlg);
}
*/

static void KeyDownHandler(int key)
{
  if (key == 165)
  {
    StrCopy (s, "Cheat");
    FntSetFont (1);
    WinDrawChars (s, StrLen (s), 120, 136);
    FntSetFont (0);
    cheatKey = 1;
    return;
  }
  else if (cheatKey == 1)
  {
    switch (key)
    {
      case 99:
        myCash += 10000000;
        cheatsEnabled = 1;
        break;
      case 112:
        copCount = 0;
        cheatsEnabled = 1;
        break;
      case 116:
        myCoat = 999;
        cheatsEnabled = 1;
        break;
      case 100:
        myDebt = 0;
        cheatsEnabled = 1;
        break;
      case 103:
        myGuns = 1;
        cheatsEnabled = 1;
        break;
      case 108:
        timeLeft = 365;
        cheatsEnabled = 1;
        break;
      case 115:
        timeLeft = 1;
        break;
      default:
        cheatKey = 0;
        break;
    }
    if (cheatsEnabled && cheatKey)
    {
      cheatKey = 0;
      FrmGotoForm(kidForm);
    }
  }
  else
  {
    cheatKey = 0;
  }
}

static Boolean controlFormEvent (EventPtr event)
{
  FormPtr   form;
  int       handled = 0;

  switch (event->eType) 
  {
  case frmOpenEvent:				
    form = FrmGetActiveForm();
    FrmDrawForm(form);
    if (updatelocation)
      makeDrugPrices (3);
    kidForm = event->data.frmLoad.formID;
    ProcessForm (event->data.frmLoad.formID);
    if (updatelocation)
      DoTravel ();			
    handled = 1;
    break;

  case keyDownEvent:
    KeyDownHandler(event->data.keyDown.chr);
    handled = 1;
    break;
    
  case ctlSelectEvent:
    ProcessControl (kidForm, event->data.ctlSelect.controlID);
    handled = 1;
    break;

  case menuEvent:
    switch (event->data.menu.itemID)
    {
      case 5000:
        if (FrmAlert (kidNewGame) == 1)
        {
          initGame ();
          FrmGotoForm (kidTravel);
        }
        break;
      case 5001:
        if (FrmAlert (kidClearScores) == 1)
        {
	  	OpenDatabase ();
		InitHighScores ();
	  	SaveStatus ();
    	  	DmCloseDatabase(pmDB);
        }
        break;
	case 5002:
	  OpenDatabase ();
    	  DmCloseDatabase(pmDB);
	  HighScoreDialog ();
	  break;
	case 5003:
	  OpenDatabase ();
    	  DmCloseDatabase(pmDB);
	  VerifyScoreDialog ();
	  break;
      case 6000:
	  FrmHelp (kidRulesHelp);
	  break;
      case 6001:
	  FrmHelp (kidBuySellHelp);
	  break;
      case 6002:
	  FrmHelp (kidSharkHelp);
	  break;
      case 6003:
	  FrmHelp (kidBankHelp);
	  break;
      case 6004:
	  FrmHelp (kidCopHelp);
	  break;
      case 6005:
	  FrmHelp (kidTravelHelp);
	  break;
      case 6006:
	  FrmGotoForm (kidIntro);
	  break;
      case 6007:
	  FrmHelp (kidScoreHelp);
	  break;
      case 7000:
        myCash += 10000000;
        cheatsEnabled = 1;
        break;
      case 7001:
        copCount = 0;
        cheatsEnabled = 1;
        break;
      case 7002:
        myCoat = 999;
        cheatsEnabled = 1;
        break;
      case 7003:
        myDebt = 0;
        cheatsEnabled = 1;
        break;
      case 7004:
        myGuns = 1;
        cheatsEnabled = 1;
        break;
      case 7005:
        timeLeft = 365;
        cheatsEnabled = 1;
        break;
    }
    if (event->data.menu.itemID >= 7000 && cheatsEnabled == 1)
    {
      cheatKey = 0;
      FrmGotoForm(kidForm);
    }
    break;

  }
  return handled;
}
