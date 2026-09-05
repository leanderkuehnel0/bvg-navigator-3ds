#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BODY (1u << 20)

static const char* kBase = "https://v6.bvg.transport.rest";

char* http_get(const char* url, u32* status_out)
{
	const char* cur = url;
	char loc[384];
	int redirects = 0;

	if (status_out)
		*status_out = 0;

	for (;;)
	{
		httpcContext ctx;
		u32 status = 0;

		Result ret = httpcOpenContext(&ctx, HTTPC_METHOD_GET, cur, 1);
		if (ret != 0)
			return NULL;

		httpcSetSSLOpt(&ctx, SSLCOPT_DisableVerify);
		httpcAddRequestHeaderField(&ctx, "User-Agent", "bvg-navigator-3ds/0.1");
		httpcAddRequestHeaderField(&ctx, "Accept", "application/json");
		httpcAddRequestHeaderField(&ctx, "Accept-Encoding", "identity");

		ret = httpcBeginRequest(&ctx);
		if (ret != 0)
		{
			httpcCloseContext(&ctx);
			return NULL;
		}

		ret = httpcGetResponseStatusCode(&ctx, &status);
		if (ret != 0)
		{
			httpcCloseContext(&ctx);
			return NULL;
		}

		if (status >= 300 && status < 400 && status != 304)
		{
			if (redirects++ < 4)
			{
				ret = httpcGetResponseHeader(&ctx, "Location", loc, sizeof(loc));
				httpcCloseContext(&ctx);
				if (ret != 0 || !loc[0])
				{
					if (status_out)
						*status_out = status;
					return NULL;
				}
				if (loc[0] == '/')
				{
					char abs[512];
					snprintf(abs, sizeof(abs), "%s%s", kBase, loc);
					strncpy(loc, abs, sizeof(loc) - 1);
					loc[sizeof(loc) - 1] = '\0';
				}
				cur = loc;
				continue;
			}
			httpcCloseContext(&ctx);
			if (status_out)
				*status_out = status;
			return NULL;
		}

		if (status != 200)
		{
			httpcCloseContext(&ctx);
			if (status_out)
				*status_out = status;
			return NULL;
		}

		size_t cap = 8192, used = 0;
		char* body = (char*)malloc(cap);
		if (!body)
		{
			httpcCloseContext(&ctx);
			return NULL;
		}

		int ok = 1;
		for (;;)
		{
			if (used == cap)
			{
				if (cap >= MAX_BODY)
				{
					ok = 0;
					break;
				}
				cap *= 2;
				if (cap > MAX_BODY)
					cap = MAX_BODY;
				char* nb = (char*)realloc(body, cap);
				if (!nb)
				{
					ok = 0;
					break;
				}
				body = nb;
			}
			u32 nread = 0;
			ret = httpcDownloadData(&ctx, (u8*)body + used, (u32)(cap - used), &nread);
			used += nread;
			if (ret == 0)
				break;
			if (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING)
			{
				if (used >= MAX_BODY)
				{
					ok = 0;
					break;
				}
				continue;
			}
			ok = 0;
			break;
		}
		httpcCloseContext(&ctx);

		if (!ok)
		{
			free(body);
			return NULL;
		}
		body[used] = '\0';

		if (status_out)
			*status_out = 200;
		return body;
	}
}