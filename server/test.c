#include <stdio.h>
#include <string.h>

int main() {
    const char *buffer = "response elie yes";

    const char *nom = strchr(buffer, ' ');  
    if (!nom) return 1;
    nom++;  

    const char *reponse = strchr(nom, ' '); 
    if (!reponse) return 1;
    reponse++;

    printf("reponse = %s\n", reponse);

    char nom_formated[50];
    strncpy(nom_formated, nom, reponse - nom - 1); 
    nom_formated[reponse - nom - 1] = '\0'; 

    printf("nom = %s\n", nom_formated);  
    return 0;
}

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
