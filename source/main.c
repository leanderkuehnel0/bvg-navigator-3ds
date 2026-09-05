#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <3ds.h>

#include "bvg.h"

static PrintConsole topConsole, bottomConsole;

static void flip(void)
{
	gfxFlushBuffers();
	gfxSwapBuffers();
	gspWaitForVBlank();
}

static void draw_menu(void)
{
	consoleSelect(&topConsole);
	printf("\x1b[2J");
	printf("\x1b[1;1H\x1b[33mBVG Navigator\x1b[0m");
	printf("\x1b[2;1HBerlin public transport");
	printf("\x1b[4;1H\x1b[36mA\x1b[0m  Search a route");
	printf("\x1b[5;1H\x1b[36mB\x1b[0m  Exit");
	printf("\x1b[7;1HExample:");
	printf("\x1b[8;1H  U Mehringdamm ->");
	printf("\x1b[9;1H  S+U Alexanderplatz");
	consoleSelect(&bottomConsole);
	printf("\x1b[2J");
	printf("\x1b[1;1HPress A to plan a route");
	printf("\x1b[2;1HThe keyboard opens on the");
	printf("\x1b[3;1Hbottom screen.");
}

static int prompt_text(const char* hint, char* out, size_t outsz)
{
	static SwkbdState swkbd;
	swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, -1);
	swkbdSetHintText(&swkbd, hint);
	swkbdSetValidation(&swkbd, SWKBD_NOTEMPTY_NOTBLANK, 0, 0);
	swkbdSetFeatures(&swkbd, SWKBD_DARKEN_TOP_SCREEN);
	SwkbdButton btn = swkbdInputText(&swkbd, out, outsz);
	flip();
	return (btn == SWKBD_BUTTON_CONFIRM) ? 0 : -1;
}

static void status_top(const char* fmt, const char* a)
{
	consoleSelect(&topConsole);
	printf("\x1b[2J");
	printf("\x1b[1;1H%s", fmt);
	if (a)
		printf("%s", a);
	flip();
}

static void wait_key(void)
{
	while (aptMainLoop())
	{
		hidScanInput();
		if (hidKeysDown() & (KEY_A | KEY_B | KEY_X | KEY_START))
			break;
		flip();
	}
}

static void search_error(const char* what, const char* detail)
{
	consoleSelect(&topConsole);
	printf("\x1b[1;1H\x1b[31m%s\x1b[0m\n", what);
	if (detail)
		printf("%s\n", detail);
	printf("Press any key...");
	wait_key();
}

static void fetch_error(const char* stage, u32 status, u32 err)
{
	char buf[96];
	consoleSelect(&topConsole);
	printf("\x1b[1;1H\x1b[31m%s\x1b[0m\n", stage);
	if (status >= 400)
		snprintf(buf, sizeof(buf), "Server responded HTTP %lu", (unsigned long)status);
	else if (err)
		snprintf(buf, sizeof(buf), "Network result 0x%08lX", (unsigned long)err);
	else
		snprintf(buf, sizeof(buf), "No answer from server");
	printf("%s\n", buf);
	printf("Press any key...");
	wait_key();
}

static void draw_results(const BvgStop* start, const BvgStop* dest,
			 const BvgJourney* js, int jc, int sel)
{
	consoleSelect(&topConsole);
	printf("\x1b[2J");
	printf("\x1b[1;1H\x1b[33mBVG Navigator - Route search\x1b[0m");
	printf("\x1b[2;1HFrom: %.*s\x1b[K", 44, start->name);
	printf("\x1b[3;1HTo:   %.*s\x1b[K", 44, dest->name);
	printf("\x1b[4;1HDeparting now - %d result(s)\x1b[K", jc);

	int y = 6;
	for (int i = 0; i < jc; i++)
	{
		const BvgJourney* j = &js[i];
		printf("\x1b[%d;1H%c%d) %02d:%02d %-8.8s %.*s\x1b[K",
		       y, (i == sel) ? '>' : ' ', i + 1,
		       j->depH, j->depM, j->line, 34, j->direction);
		printf("\x1b[%d;1H   arr %02d:%02d  %3d min  %d change(s)\x1b[K",
		       y + 1, j->arrH, j->arrM, j->durationMin, j->transfers);
		y += 2;
	}

	printf("\x1b[29;1H\x1b[32mX\x1b[0m New search   \x1b[32mB\x1b[0m Exit   \x1b[36mUP/DOWN\x1b[0m select\x1b[K");

	consoleSelect(&bottomConsole);
	printf("\x1b[2J");
	printf("\x1b[1;1H\x1b[33mJourney %d/%d\x1b[0m\x1b[K", sel + 1, jc);
	const BvgJourney* j = &js[sel];
	for (int i = 0; i < j->legCount; i++)
	{
		const BvgLeg* l = &j->legs[i];
		printf("\x1b[%d;1H%02d:%02d %-8.8s %.*s -> %.*s\x1b[K",
		       2 + i, l->depH, l->depM, l->label, 18, l->from, 18, l->to);
	}
	printf("\x1b[29;1Htime is local, realtime included\x1b[K");
}

static int show_results(const BvgStop* start, const BvgStop* dest,
			BvgJourney* js, int jc)
{
	int sel = 0;
	int redraw = 1;

	while (aptMainLoop())
	{
		hidScanInput();
		u32 kDown = hidKeysDown();

		if (kDown & KEY_B)
			return -1;
		if (kDown & KEY_X)
			return 1;
		if (kDown & KEY_DUP)
		{
			sel = (sel + jc - 1) % jc;
			redraw = 1;
		}
		if (kDown & KEY_DDOWN)
		{
			sel = (sel + 1) % jc;
			redraw = 1;
		}

		if (redraw)
		{
			draw_results(start, dest, js, jc, sel);
			redraw = 0;
			flip();
		}
		else
		{
			gspWaitForVBlank();
		}
	}
	return -1;
}

static int run_search(void)
{
	char startText[48];
	char destText[48];
	BvgStop start, dest;
	BvgStop list[BVG_MAX_STOPS];
	int n;

	if (prompt_text("Start? e.g. U Mehringdamm", startText, sizeof(startText)) != 0)
		return 1;
	status_top("Looking up start: ", startText);

	n = 0;
	{
		u32 status = 0, err = 0;
		if (bvg_locations(startText, list, BVG_MAX_STOPS, &n, &status, &err) < 0)
		{
			fetch_error("Failed to look up the start station", status, err);
			return 1;
		}
	}
	if (n == 0)
	{
		search_error("Start station not found", startText);
		return 1;
	}
	start = list[0];

	if (prompt_text("Destination? e.g. S+U Alexanderplatz", destText, sizeof(destText)) != 0)
		return 1;
	status_top("Looking up destination: ", destText);

	n = 0;
	{
		u32 status = 0, err = 0;
		if (bvg_locations(destText, list, BVG_MAX_STOPS, &n, &status, &err) < 0)
		{
			fetch_error("Failed to look up the destination station", status, err);
			return 1;
		}
	}
	if (n == 0)
	{
		search_error("Destination station not found", destText);
		return 1;
	}
	dest = list[0];

	consoleSelect(&topConsole);
	printf("\x1b[2J");
	printf("\x1b[1;1H%s -> %s\n", start.name, dest.name);
	printf("Fetching journeys...");
	flip();

	BvgJourney js[BVG_MAX_JOURNEYS];
	int jc = 0;
	u32 status = 0, err = 0;
	int rc = bvg_journeys(&start, &dest, time(NULL), js, BVG_MAX_JOURNEYS, &jc,
			      &status, &err);

	if (rc == -2)
	{
		search_error("Could not parse API response", NULL);
		return 1;
	}
	if (rc < 0)
	{
		fetch_error("Failed to plan the route", status, err);
		return 1;
	}
	if (jc == 0)
	{
		search_error("No journeys found", NULL);
		return 1;
	}

	return show_results(&start, &dest, js, jc);
}

int main(void)
{
	gfxInitDefault();

	u8* socbuf = (u8*)linearAlloc(0x100000);
	if (socbuf)
		socInit((u32*)socbuf, 0x100000);

	consoleInit(GFX_TOP, &topConsole);
	consoleInit(GFX_BOTTOM, &bottomConsole);

	draw_menu();
	flip();

	while (aptMainLoop())
	{
		hidScanInput();
		u32 kDown = hidKeysDown();

		if (kDown & KEY_B)
			break;

		if (kDown & KEY_A)
		{
			int r = run_search();
			if (r < 0)
				break;
			draw_menu();
			flip();
		}
		else
		{
			gspWaitForVBlank();
		}
	}

	socExit();
	gfxExit();
	return 0;
}