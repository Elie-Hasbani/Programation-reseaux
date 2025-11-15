#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"

typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
   char pswd[BUF_SIZE];
   int state;
   int first_connexion; //-1 not responded    0 no         1 yes
   char bio[500];
} Client;

#endif /* guard */
