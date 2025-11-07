#include <stdio.h>

int main(void) {
    char phrase[] = "play elie 3";
    char mot1[20], mot2[20], mot3[20];

    sscanf(phrase, "%s %s %s", mot1, mot2, mot3);

    printf("mot1 = %s\n", mot1);
    printf("mot2 = %s\n", mot2);
    printf("mot3 = %s\n", mot3);

    return 0;
}
static void play_move(Client *sender, const char *buffer){
    char cmd[20], opp[50];
    int pit;
    sscanf(buffer, "%s %s %d", cmd, opp, &pit);

    for(int i=0;i<match_actual;++i){
      Match *m = matches[i];
      int player = 0;
      if(!strcmp(sender->name, m->pair[0]->name) && !strcmp(opp, m->pair[1]->name)){
         player = 1;
         if(m->pair[1]->state == -1){
            write_client(sender->sock, "player not online, play later");
            return;
         }
      } 
      else if(!strcmp(sender->name, m->pair[1]->name) && !strcmp(opp, m->pair[0]->name)) {
         
         player = 2;
         if(m->pair[0]->state == -1){
            write_client(sender->sock, "player not online, play later");
            return;
         }
      }
      else continue;

      
      if(m->game.phase != 0){
          write_client(sender->sock,"Game already finished!\n");
          return;
      }
      if(m->game.turn != player){
          write_client(sender->sock,"Not your turn!\n");
          return;
      }
      if(apply_move(&m->game,player,pit)==-1){
          write_client(sender->sock,"Invalid move.\n");
          return;
      }
      char board_buf[512];
      ascii_board(&m->game, board_buf, sizeof(board_buf));
      write_client(m->pair[0]->sock, board_buf);
      write_client(m->pair[1]->sock, board_buf);
      for(int s=0;s<m->nb_spectators && m->spectators[s]->state != -1;s++)
          write_client(m->spectators[s]->sock, board_buf);
      if(m->game.phase==1){
          int w=winner(&m->game);
          char msg[128];
          if(w==0) snprintf(msg,sizeof(msg),"Game ended in a draw!\n");
          else snprintf(msg,sizeof(msg),"Player %d wins!\n",w);
          write_client(m->pair[0]->sock,msg);
          write_client(m->pair[1]->sock,msg);
          for(int s=0;s<m->nb_spectators;s++)
              write_client(m->spectators[s]->sock,msg);
      }
      return;
    
    write_client(sender->sock,"No active match found.\n");
   }
}