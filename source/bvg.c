#include "bvg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json.h"
#include "web.h"

static void urlencode(const char* in, char* out, size_t outlen)
{
	static const char hex[] = "0123456789ABCDEF";
	size_t o = 0;
	for (; *in && o + 3 < outlen; in++)
	{
		unsigned char c = (unsigned char)*in;
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
		    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~')
		{
			out[o++] = (char)c;
		}
		else
		{
			out[o++] = '%';
			out[o++] = hex[c >> 4];
			out[o++] = hex[c & 0xF];
		}
	}
	out[o] = '\0';
}

static const JsonNode* jv(const JsonNode* obj, const char* key)
{
	const JsonNode* n = json_object_get(obj, key);
	if (n && n->type == JSON_NULL)
		return NULL;
	return n;
}

static int parse_tod(const char* iso, int* h, int* m)
{
	if (!iso)
		return -1;
	const char* t = strchr(iso, 'T');
	if (!t)
		return -1;
	if (sscanf(t + 1, "%d:%d", h, m) != 2)
		return -1;
	return 0;
}

static void copy_str(char* dst, size_t dstsz, const char* src)
{
	if (!src)
	{
		dst[0] = '\0';
		return;
	}
	snprintf(dst, dstsz, "%s", src);
}

int bvg_locations(const char* query, BvgStop* out, int max, int* count)
{
	*count = 0;
	if (max <= 0)
		return 0;

	char enc[128];
	urlencode(query, enc, sizeof(enc));

	char url[512];
	snprintf(url, sizeof(url),
		 "https://v6.bvg.transport.rest/locations?stops=true&addresses=false"
		 "&poi=false&results=%d&pretty=false&query=%s",
		 max, enc);

	char* body = http_get(url, NULL);
	if (!body)
		return -1;

	JsonNode* root = json_parse(body);
	free(body);
	if (!root)
		return -2;

	const JsonNode* list = root;
	if (root->type == JSON_OBJECT)
		list = json_object_get(root, "stations");

	int n = 0;
	if (list)
	{
		int total = json_array_len(list);
		if (total > max)
			total = max;
		for (int i = 0; i < total && n < max; i++)
		{
			const JsonNode* item = json_array_at(list, i);
			if (!item || item->type != JSON_OBJECT)
				continue;
			const char* ty = json_string(jv(item, "type"));
			if (ty && strcmp(ty, "stop") != 0)
				continue;
			const char* id = json_string(jv(item, "id"));
			const char* name = json_string(jv(item, "name"));
			if (!id || !name)
				continue;
			copy_str(out[n].id, sizeof(out[n].id), id);
			copy_str(out[n].name, sizeof(out[n].name), name);
			n++;
		}
	}

	json_free(root);
	*count = n;
	return 0;
}

int bvg_journeys(const char* fromId, const char* toId, time_t depart,
		 BvgJourney* out, int max, int* count)
{
	*count = 0;
	if (max <= 0)
		return 0;

	struct tm gtm;
	gmtime_r(&depart, &gtm);
	char depstr[32];
	strftime(depstr, sizeof(depstr), "%Y-%m-%dT%H:%M:%SZ", &gtm);

	char url[640];
	snprintf(url, sizeof(url),
		 "https://v6.bvg.transport.rest/journeys?from=%s&to=%s&departure=%s"
		 "&results=%d&pretty=false&remarks=false&stopovers=false"
		 "&tickets=false&polyline=false",
		 fromId, toId, depstr, max);

	char* body = http_get(url, NULL);
	if (!body)
		return -1;

	JsonNode* root = json_parse(body);
	free(body);
	if (!root)
		return -2;

	const JsonNode* journeys = json_object_get(root, "journeys");
	if (!journeys || journeys->type != JSON_ARRAY)
	{
		json_free(root);
		return -2;
	}

	int written = 0;
	int total = json_array_len(journeys);
	if (total > max)
		total = max;

	for (int i = 0; i < total; i++)
	{
		const JsonNode* jn = json_array_at(journeys, i);
		if (!jn || jn->type != JSON_OBJECT)
			continue;

		BvgJourney j;
		memset(&j, 0, sizeof(j));

		const JsonNode* legs = json_object_get(jn, "legs");
		if (!legs || legs->type != JSON_ARRAY)
			continue;

		int legCount = json_array_len(legs);
		if (legCount > BVG_LEG_MAX)
			legCount = BVG_LEG_MAX;

		int transitCount = 0;
		int firstTransit = -1;

		for (int k = 0; k < legCount; k++)
		{
			const JsonNode* leg = json_array_at(legs, k);
			if (!leg || leg->type != JSON_OBJECT)
				continue;

			const JsonNode* line = jv(leg, "line");
			const JsonNode* org = jv(leg, "origin");
			const JsonNode* dst = jv(leg, "destination");
			const JsonNode* depv = jv(leg, "departure");
			const JsonNode* arrv = jv(leg, "arrival");

			BvgLeg* l = &j.legs[k];
			parse_tod(json_string(depv), &l->depH, &l->depM);
			parse_tod(json_string(arrv), &l->arrH, &l->arrM);
			copy_str(l->from, sizeof(l->from), json_string(jv(org, "name")));
			copy_str(l->to, sizeof(l->to), json_string(jv(dst, "name")));

			if (line)
			{
				l->walking = 0;
				transitCount++;
				if (firstTransit < 0)
				{
					copy_str(l->label, sizeof(l->label), json_string(jv(line, "name")));
					j.depH = l->depH;
					j.depM = l->depM;
					firstTransit = k;
				}
			}
			else
			{
				l->walking = 1;
				copy_str(l->label, sizeof(l->label), "walk");
			}
		}

		if (transitCount == 0)
			continue;

		j.legCount = legCount;
		j.arrH = j.legs[legCount - 1].arrH;
		j.arrM = j.legs[legCount - 1].arrM;

		int depT = j.depH * 60 + j.depM;
		int arrT = j.arrH * 60 + j.arrM;
		j.durationMin = (arrT - depT + 1440) % 1440;
		j.transfers = transitCount - 1;

		copy_str(j.line, sizeof(j.line), j.legs[firstTransit].label);

		const JsonNode* firstLeg = json_array_at(legs, firstTransit);
		const char* dirn = json_string(jv(firstLeg, "direction"));
		if (!dirn)
			dirn = json_string(jv(jv(firstLeg, "line"), "direction"));
		if (!dirn)
		{
			const JsonNode* dstn = jv(firstLeg, "destination");
			dirn = json_string(jv(dstn, "name"));
		}
		copy_str(j.direction, sizeof(j.direction), dirn);

		out[written++] = j;
	}

	json_free(root);
	*count = written;
	return 0;
}