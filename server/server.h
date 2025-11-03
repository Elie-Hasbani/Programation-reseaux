#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>

#elif defined (linux)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;


#else

#error not defined for this platform

#endif

#define CRLF        "\r\n"
#define PORT         9999
#define MAX_CLIENTS     100

#define BUF_SIZE    1024

#include "client.h"

typedef struct {Client pair[2]} Challenge;


static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static int read_client(SOCKET sock, char *buffer);
static void write_client(SOCKET sock, const char *buffer);
static void send_message_to_all_clients(Client *clients, Client client, int actual, const char *buffer, char from_server);
static void remove_client(Client *clients, int to_remove, int *actual);
static void clear_clients(Client *clients, int actual);

static void read_and_handle_message(Client *clients, Client client, int actual, const char *buffer, char from_server,Challenge * challenges);
static void send_player_list(int actual,Client sender,Client *clients);
static void challenge(Client sender, Client *clients, int actual, char* name,Challenge ** challenges);
static void respond_to_challenge(Client sender, Challenge ** challenges, const char * buffer);
static void login_waiting_clients(Client * clients_not_loged_in, int actual_not_loged_in);
#endif /* guard */
