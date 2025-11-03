#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "server.h"
#include "client.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}
int challenge_actual = 0;
static void app(void)
{
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];
   Client clients_not_loged_in[MAX_CLIENTS-1];
   int actual_not_loged;

   Challenge * challenges[100];

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      for(i = 0; i < actual_not_loged; i++)
      {
         FD_SET(clients_not_loged_in[i].sock, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      ///check if not loged in client sent new pseudo


      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         size_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if(read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }



         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = { csock };
         strncpy(c.name, buffer, BUF_SIZE - 1);

         int exists = 0;
         //check if pseudo is already used
         for(int i=0; i<actual;++i){
            if(!strcmp(buffer,clients[i].name)){
               printf("pseudo recieved already in use\n");
               fflush(stdout);
               char * message = "code700";
               write_client(c.sock, message);
               clients_not_loged_in[actual_not_loged] = c;
               actual_not_loged++;
               exists = 1;
               break;
            }
         }

         if(exists == 0){
            char * message = "code100";
            printf("client inscrit\n");
            write_client(c.sock, message);

            fflush(stdout);
            clients[actual] = c;
            actual++;
         }



      }
      else
      {
         int i = 0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int c = read_client(clients[i].sock, buffer);
               /* client disconnected */
               if(c == 0)
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(clients, client, actual, buffer, 1);
               }
               else
               {
                  buffer[strcspn(buffer, "\n")] = '\0';
                  read_and_handle_message(clients, client, actual, buffer, 0,challenges);
                  //send_message_to_all_clients(clients, client, actual, buffer, 0);
                  fflush(stdout);
               }
               break;
            }
         }
         //see the clients that are not logged in yet
         for(int i = 0; i < actual_not_loged; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients_not_loged_in[i].sock, &rdfs))
            {
               Client client = clients_not_loged_in[i];
               int c = read_client(clients_not_loged_in[i].sock, buffer);
               /* client disconnected */
               if(c == 0)
               {
                  closesocket(clients_not_loged_in[i].sock);
                  remove_client(clients_not_loged_in, i, &actual_not_loged);
                  //strncpy(buffer, client.name, BUF_SIZE - 1);
                  //strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  //send_message_to_all_clients(clients, client, actual, buffer, 1);
               }
               else
               {
                  buffer[strcspn(buffer, "\n")] = '\0';
                  int exists = 0;
                  //check if pseudo is already used
                  for(int i=0; i<actual;++i){
                     if(!strcmp(buffer,clients[i].name)){
                        printf("pseudo recieved already in use\n");
                        fflush(stdout);
                        char * message = "code700";
                        write_client(clients_not_loged_in[i].sock, message);
                        exists = 1;
                        break;
                     }
                  }
                  if(exists == 0){
                     char * message = "code100";
                     printf("client inscrit\n");
                     write_client(clients_not_loged_in[i].sock, message);
                     strncpy(clients_not_loged_in[i].name, buffer, BUF_SIZE - 1);
                     
                     fflush(stdout);
                     clients[actual] = clients_not_loged_in[i];
                     remove_client(clients_not_loged_in, i, &actual_not_loged);
                     ++actual;
                  }
                           
                  
               }
               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}


static void read_and_handle_message(Client *clients, Client sender, int actual, const char *buffer, char from_server,Challenge * challenges){

   /*char* dest;
   const char *space = strchr(buffer, ' ');  // trouve le premier espace
   size_t len = (space) ? (size_t)(space - buffer) : strlen(buffer);
   strncpy(dest, buffer, len);
   dest[len] = '\0';*/

   if(!strcmp(buffer, "1")){
      printf("sending player list\n");
      send_player_list(actual,sender,clients);
   }else if(strstr(buffer, "challenge")){
      printf("user wants to challenge ");

      const char *nom = strchr(buffer, ' ');  // trouve le premier espace
      ++nom;
      printf(nom);printf("\n");
      challenge(sender, clients, actual, nom, challenges);
      
   }else if(strstr(buffer, "response")){
      respond_to_challenge(sender,challenges,buffer);
   }
   else{
      printf("no match\n");

      send_message_to_all_clients(clients, sender, actual, buffer, 0);
   }
}




static void respond_to_challenge(Client sender, Challenge ** challenges, const char * buffer){
   for(int i = 0; i<challenge_actual; ++i){
      Challenge * challenge = challenges[i];
      const char *nom = strchr(buffer, ' ');  
      if (!nom){
         write_client(challenge->pair[1].sock,"commande introuvable" ); return;
      } 
      nom++;  

      const char *reponse = strchr(nom, ' '); 
      if (!reponse){
         write_client(challenge->pair[1].sock,"commande introuvable" ); return;
         
      } 
      reponse++;

      printf("reponse = %s\n", reponse);

      char nom_formated[50];
      strncpy(nom_formated, nom, reponse - nom - 1); 
      nom_formated[reponse - nom - 1] = '\0'; 

      printf("nom = %s\n", nom_formated);

      if(!strcmp(challenge->pair[1].name,sender.name)){
         if(!strcmp(nom_formated, challenge->pair[0].name)){
            if(!strcmp(reponse, "yes")){
               char message[50] = "challenge accepted by ";
               strcat(message, challenge->pair[1].name);
               write_client(challenge->pair[0].sock,message);
               return;
            }
            if(!strcmp(reponse, "no")){
               char message[50] = "challenge not accpred by ";
               strcat(message, challenge->pair[1].name);
               write_client(challenge->pair[0].sock,message );
               return;
            }
            else{
               write_client(challenge->pair[1].sock,"not a valid reponse");
               return;
            }

         }
         write_client(challenge->pair[1].sock,"no one challenged you by that name");
         return;

      }
      
   }
   write_client(sender.sock,"no one challenged you");
}

static void challenge(Client sender, Client *clients, int actual, char* name, Challenge ** challenges){
   if(!strcmp(name, sender.name)){
      write_client(sender.sock, "can't challenge yourself!");
      return;
   }
   for(int i = 0; i<actual; ++i){
      if(!strcmp(name, clients[i].name)){
         write_client(sender.sock, "challenge sent");
         
         write_client(clients[i].sock, sender.name);
         char * message =" is challenging you";
         write_client(clients[i].sock, message);



         Challenge * challenge = malloc(sizeof(Challenge));
         challenge->pair[0] = sender;
         challenge ->pair[1] = clients[i];
         challenges[challenge_actual] = challenge;
         ++challenge_actual;
         return;

      }      
   }
   write_client(sender.sock, "no user found with this pseudo\n");

};

static void send_player_list(int actual,Client sender,Client *clients){
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < actual; i++)
   {
      strncat(message, clients[i].name, sizeof message - strlen(message) - 1);
      if(i!= actual-1){
         strncat(message, "\n", sizeof message - strlen(message) - 1);
      }

   }
   write_client(sender.sock, message);   

}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if(sender.sock != clients[i].sock)
      {
         if(from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(clients[i].sock, message);
      }
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;

   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}




int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
