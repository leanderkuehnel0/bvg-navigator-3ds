#include <stdio.h>
#include <string.h>

#define main app_main
#include "../source/main.c"
#undef main

#include "3ds.h"
#undef printf

static int g_checks = 0;

#define CHECK(cond)                                                        \
	do                                                                   \
	{                                                                    \
		g_checks++;                                                  \
		if (!(cond))                                                   \
		{                                                            \
			fprintf(stderr, "FAIL: %s (line %d)\n", #cond, __LINE__); \
			return 1;                                              \
		}                                                            \
	} while (0)

int main(void)
{
	u32 keys[] = {
		KEY_A, /* main loop: enter run_search     */
		KEY_R, /* route 1/3 -> 2/3               */
		KEY_R, /* route 1/3 -> 3/3               */
		KEY_L, /* route wraps 3/3 -> 2/3         */
		KEY_R, /* 2/3 -> 3/3                     */
		KEY_A, /* view_routes: back to menu      */
		KEY_B  /* main loop: exit                */
	};
	const char* answers[] = { "U Mehringdamm", "S+U Alexanderplatz" };

	console_reset_capture();
	hid_feed(keys, 7);
	swkbd_feed(answers, 2);

	app_main();

	/*
	 * Top screen: menu, header boxes with start/dest, journey counter,
	 * duration, footer tooltips.
	 */
	CHECK(console_capture_contains(GFX_TOP, "BVG Navigator"));
	CHECK(console_capture_contains(GFX_TOP, "MAIN MENU"));
	CHECK(console_capture_contains(GFX_TOP, "U Mehringdamm"));
	CHECK(console_capture_contains(GFX_TOP, "S+U Alexanderplatz"));
	CHECK(console_capture_contains(GFX_TOP, "3/3"));
	CHECK(console_capture_contains(GFX_TOP, "2/3"));
	CHECK(console_capture_contains(GFX_TOP, "44 min"));
	CHECK(console_capture_contains(GFX_TOP, "Next journey"));
	CHECK(console_capture_contains(GFX_TOP, "New route"));

	/*
	 * Bottom screen: route overview with per-leg rows, walking leg,
	 * departure line and travel time.
	 */
	CHECK(console_capture_contains(GFX_BOTTOM, "Route 3/3"));
	CHECK(console_capture_contains(GFX_BOTTOM, "12:35"));
	CHECK(console_capture_contains(GFX_BOTTOM, "U7"));
	CHECK(console_capture_contains(GFX_BOTTOM, "walk"));
	CHECK(console_capture_contains(GFX_BOTTOM, "M8"));
	CHECK(console_capture_contains(GFX_BOTTOM, "S+U Alexanderplatz"));

	printf("PASS: %d checks\n", g_checks);
	return 0;
}