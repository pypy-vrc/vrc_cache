#pragma comment(linker, "/OPT:NOWIN98")
#pragma comment(lib, "wininet.lib")
#define _WIN32_WINNT 0x600
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#pragma hdrstop

FILE *log_;

void writelog(const char *fmt, ...)
{
	if (log_) {
		vfprintf(log_, fmt, (char *)&fmt + sizeof(void *));
	}
	vprintf(fmt, (char *)&fmt + sizeof(void *));
}

void dump(const void *buf, size_t len)
{
	static const char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
	unsigned char *begin, *end, c;
	char line[64 + 1];
	size_t i;

	begin = (unsigned char *)buf;
	end = begin + len;

	while (begin < end) {
		line[64] = 0;
		memset(line, ' ', 64);
		i = (size_t)(end - begin);
		if (i > 16)
			i = 16;
		do {
			c = begin[--i];
			line[i * 3] = hex[c >> 4];
			line[i * 3 + 1] = hex[c & 15];
			line[i + 48] = (c > 31 && c < 127) ? c : '.';
		} while (i);
		begin += 16;
		printf("%.*s\n", 64, line);
	}
}

void request(const char *url, size_t url_length)
{
	HINTERNET h1, h2, h3;
	URL_COMPONENTS uc;
	DWORD len, bytes;
	char *begin, *end, buf[32768];

	ZeroMemory(&uc, sizeof(uc));
	uc.dwStructSize = sizeof(uc);
	uc.lpszHostName = buf;
	uc.dwHostNameLength = 128;
	uc.lpszUrlPath = &buf[128];
	uc.dwUrlPathLength = 8192;
	if (InternetCrackUrl(url, url_length, ICU_ESCAPE, &uc) &&
		!memcmp(uc.lpszHostName, "api.vrchat.cloud", sizeof("api.vrchat.cloud")) &&
		!memcmp(uc.lpszUrlPath, "/api/1/file/file_", sizeof("/api/1/file/file_") - 1)) {
		*strrchr(uc.lpszUrlPath, '/') = 0;
		*strrchr(uc.lpszUrlPath, '/') = 0;
		writelog("%.*s\n", url_length, url);
		// writelog("%s://%s%s\n", (uc.nScheme == INTERNET_SCHEME_HTTPS) ? "https" : "http", uc.lpszHostName, uc.lpszUrlPath);
		if (h1 = InternetOpen(NULL, INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0)) {
			if (h2 = InternetConnect(h1, uc.lpszHostName, uc.nPort, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0)) {
				DWORD flags = INTERNET_FLAG_KEEP_CONNECTION |
					INTERNET_FLAG_NO_CACHE_WRITE |
					INTERNET_FLAG_NO_UI |
					INTERNET_FLAG_PRAGMA_NOCACHE |
					INTERNET_FLAG_RELOAD;
				if (uc.nScheme == INTERNET_SCHEME_HTTPS) {
					flags |= INTERNET_FLAG_SECURE;
				}
				if (h3 = HttpOpenRequest(h2, NULL, uc.lpszUrlPath, NULL, NULL, NULL, flags, 0)) {
					if (HttpSendRequest(h3, NULL, 0, NULL, 0)) {
						len = 0;
						while (InternetReadFile(h3, &buf[len], sizeof(buf) - 1 - len, &bytes) &&
							bytes > 0) {
							len += bytes;
						}
						buf[len] = 0;
						if ((begin = strstr(buf, "\"name\":")) &&
							(end = strchr(begin = &begin[8], '"'))) {
							writelog("%.*s\n", end - begin, begin);
						}
						if ((begin = strstr(buf, "\"ownerId\":")) &&
							(end = strchr(begin = &begin[11], '"'))) {
							writelog("%.*s\n", end - begin, begin);
						}
					}
					InternetCloseHandle(h3);
				}
				InternetCloseHandle(h2);
			}
			InternetCloseHandle(h1);
		}
	}
}

int main(int argc, const char **argv)
{
	unsigned i, j, version, num, len;
	BYTE *file = NULL;
	HANDLE h1, h2;
	char path[512];

	SetConsoleOutputCP(CP_UTF8);

	log_ = fopen("vrc_cache.txt", "wc");

	sprintf(path, "%s%s", getenv("LOCALAPPDATA"), "Low\\VRChat\\vrchat\\Library");
	if ((h1 = CreateFile(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE) {
		if (h2 = CreateFileMapping(h1, NULL, PAGE_READONLY, 0, 0, NULL)) {
			file = MapViewOfFile(h2, FILE_MAP_READ, 0, 0, 0);
			CloseHandle(h2);
		}
		CloseHandle(h1);
	}

	if (file) {
		BYTE *ptr = file;
		version = *(unsigned *)ptr;
		ptr += 4;
		if (version > 1) {
			ptr += 8; // NextNameIDX
		}
		num = *(unsigned *)ptr;
		ptr += 4;
		for (i = 0; i < num; ++i) {
			for (len = 0, j = 0; j < 5; ++j) {
				len |= (127 & *ptr) << (7 * j);
				if (!(128 & *ptr++)) {
					break;
				}
			}
			writelog("%u/%u\n", i + 1, num);
			request(ptr, len);
			ptr += len; // Uri
			ptr += 8; // LastAccess (DateTime)
			ptr += 4; // BodyLength
			if (version == 1 || version == 2) {
				if (version == 2) {
					ptr += 8; // MappedNameIDX
				}
				for (len = 0, j = 0; j < 5; ++j) {
					len |= (127 & *ptr) << (7 * j);
					if (!(128 & *ptr++)) {
						break;
					}
				}
				ptr += len; // ETag
				for (len = 0, j = 0; j < 5; ++j) {
					len |= (127 & *ptr) << (7 * j);
					if (!(128 & *ptr++)) {
						break;
					}
				}
				ptr += len; // LastModified
				ptr += 8; // Expires (DateTime)
				ptr += 8; // Age
				ptr += 8; // MaxAge
				ptr += 8; // Date (DateTime)
				ptr += 1; // MustRevalidate
				ptr += 8; // Received (DateTime)
			}
		}
		UnmapViewOfFile(file);
	}

	fflush(log_);
	fclose(log_);
	log_ = NULL;
	system("pause");
	return 0;
}