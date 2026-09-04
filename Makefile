CC = cc
CFLAGS = -O2 -Wall -Wextra -std=gnu89 $(shell pkg-config --cflags libcurl 2>/dev/null)
SQLITE_LIB = $(shell if pkg-config --exists sqlite3 2>/dev/null; then pkg-config --libs sqlite3; else echo -Wl,-l:libsqlite3.so.0; fi)
CURL_LIB = $(shell pkg-config --libs libcurl 2>/dev/null || echo -lcurl)
CRYPTO_LIB = $(shell pkg-config --libs libcrypto 2>/dev/null || echo -lcrypto)

all: music

music: music.c
	$(CC) $(CFLAGS) -o music music.c $(SQLITE_LIB) $(CURL_LIB) $(CRYPTO_LIB)

clean:
	rm -f music
