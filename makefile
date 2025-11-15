CC = gcc
CFLAGS = -Wall -Wextra -g
TRG = trg

all: $(TRG) $(TRG)/server $(TRG)/client

$(TRG):
	mkdir -p $(TRG)

$(TRG)/server: server/server.c server/server.h server/client.h
	$(CC) $(CFLAGS) server/server.c -o $(TRG)/server

$(TRG)/client: client/client.c client/client.h
	$(CC) $(CFLAGS) client/client.c -o $(TRG)/client

clean:
	rm -rf $(TRG)

.PHONY: all clean
