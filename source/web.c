#include "web.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BODY (1u << 20)
#define MAX_REDIRECTS 4
#define MAX_RETRIES 3

static const char* kBase = "https://v6.bvg.transport.rest";

enum
{
	R_BODY = 0,
	R_REDIRECT,
	R_HTTPERR,
	R_NET,
	R_LOCATION
};

static int do_request(const char* url, u32* status, u32* err,
		      char** body_out, char* loc, size_t locsz)
{
	httpcContext ctx;
	*body_out = NULL;
	loc[0] = '\0';

	Result ret = httpcOpenContext(&ctx, HTTPC_METHOD_GET, url, 1);
	if (ret != 0)
	{
		*err = (u32)ret;
		return R_NET;
	}

	httpcSetSSLOpt(&ctx, SSLCOPT_DisableVerify);
	httpcAddRequestHeaderField(&ctx, "User-Agent", "bvg-navigator-3ds/0.1");
	httpcAddRequestHeaderField(&ctx, "Accept", "application/json");
	httpcAddRequestHeaderField(&ctx, "Accept-Encoding", "identity");

	ret = httpcBeginRequest(&ctx);
	if (ret != 0)
	{
		httpcCloseContext(&ctx);
		*err = (u32)ret;
		return R_NET;
	}

	ret = httpcGetResponseStatusCode(&ctx, status);
	if (ret != 0)
	{
		httpcCloseContext(&ctx);
		*err = (u32)ret;
		return R_NET;
	}

	if (*status >= 300 && *status < 400 && *status != 304)
	{
		ret = httpcGetResponseHeader(&ctx, "Location", loc, locsz);
		httpcCloseContext(&ctx);
		if (ret != 0 || !loc[0])
		{
			*err = (u32)ret;
			return R_LOCATION;
		}
		return R_REDIRECT;
	}

	if (*status != 200)
	{
		httpcCloseContext(&ctx);
		return R_HTTPERR;
	}

	size_t cap = 8192, used = 0;
	char* body = (char*)malloc(cap);
	if (!body)
	{
		httpcCloseContext(&ctx);
		*err = 0xFFFFFFFFu;
		return R_NET;
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
		*err = (u32)ret;
		break;
	}
	httpcCloseContext(&ctx);

	if (!ok)
	{
		free(body);
		return R_NET;
	}
	body[used] = '\0';
	*body_out = body;
	return R_BODY;
}

char* http_get(const char* url, u32* status_out, u32* err_out)
{
	if (status_out)
		*status_out = 0;
	if (err_out)
		*err_out = 0;

	const char* cur = url;
	char loc[384];

	for (int redirects = 0;;)
	{
		char* body = NULL;
		u32 status = 0, err = 0;
		int r = R_NET;

		for (int attempt = 0; attempt < MAX_RETRIES; attempt++)
		{
			r = do_request(cur, &status, &err, &body, loc, sizeof(loc));
			if (r != R_HTTPERR || status < 500)
				break;
			svcSleepThread((s64)1000000LL * 1000 * (attempt + 1));
		}

		switch (r)
		{
		case R_BODY:
			if (status_out)
				*status_out = 200;
			return body;

		case R_REDIRECT:
			if (redirects++ >= MAX_REDIRECTS)
			{
				if (status_out)
					*status_out = status;
				if (err_out)
					*err_out = err;
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

		case R_LOCATION:
		default:
			if (status_out)
				*status_out = status;
			if (err_out)
				*err_out = err;
			return NULL;
		}
	}
}