#ifndef BVG_BVG_H
#define BVG_BVG_H

#include <time.h>

#define BVG_MAX_STOPS 5
#define BVG_MAX_JOURNEYS 4
#define BVG_LEG_MAX 8

typedef struct
{
	char id[24];
	char name[48];
} BvgStop;

typedef struct
{
	int depH, depM;
	int arrH, arrM;
	int walking;
	char label[12];
	char from[24];
	char to[24];
} BvgLeg;

typedef struct
{
	int depH, depM;
	int arrH, arrM;
	int durationMin;
	int transfers;
	char line[12];
	char direction[48];
	int legCount;
	BvgLeg legs[BVG_LEG_MAX];
} BvgJourney;

/* Returns 0 on success and fills up to 'max' matches. -1 network, -2 http, -3 parse. */
int bvg_locations(const char* query, BvgStop* out, int max, int* count);

/* Routes from stop id to stop id, departing at 'depart' (unix seconds).
   Returns 0 on success and fills up to 'max' journeys. Error codes as above. */
int bvg_journeys(const char* fromId, const char* toId, time_t depart,
		 BvgJourney* out, int max, int* count);

#endif