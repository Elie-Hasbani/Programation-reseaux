
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>
#include <time.h>

#define DEFAULT_PORT 9999
#define MAX_CLIENTS 64
#define BUF_SIZE 4096
#define LINE_MAX 1024
#define SESSIONS_FILE "awale_sessions.json"

typedef enum { ROLE_NONE=0, ROLE_PLAYER1, ROLE_PLAYER2, ROLE_SPECTATOR } role_t;

typedef struct {
    int fd;
    char addr[64];
    char inbuf[BUF_SIZE];
    int inbuf_len;
    role_t role;
    char name[32];
} client_t;

/* Game state */
typedef struct {
    int board[12];
    int captured[2];
    int turn; // 1 or 2
    int phase; // 0 playing, 1 finished
    // simple history: pairs of player and pit and captured count
    struct {
        int player;
        int pit;
        int captured_amount;
    } history[1024];
    int history_len;
} game_t;

client_t *clients[MAX_CLIENTS];
int listener_fd = -1;
int maxfd = 0;

game_t game;

/* Utility functions */
static void perror_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void send_to_client(client_t *c, const char *msg) {
    if (!c) return;
    ssize_t n = send(c->fd, msg, strlen(msg), 0);
    (void)n;
}

static void broadcast(const char *msg) {
    for (int i=0;i<MAX_CLIENTS;++i) if (clients[i]) send_to_client(clients[i], msg);
}

/* Game logic helpers */
static void game_init(game_t *g) {
    for (int i=0;i<12;++i) g->board[i] = 4;
    g->captured[0] = g->captured[1] = 0;
    g->turn = 1;
    g->phase = 0;
    g->history_len = 0;
}

static int pits_of_player_start(int player) { return player==1 ? 0 : 6; }
static int pits_of_player_end(int player) { return player==1 ? 5 : 11; }
static int is_opponent_pit(int player, int idx) {
    if (player==1) return idx>=6 && idx<=11;
    return idx>=0 && idx<=5;
}

static void ascii_board(game_t *g, char *out, size_t out_sz) {
    char top[256] = "";
    char bottom[256] = "";
    char tmp[64];
    top[0] = '\0'; bottom[0] = '\0';
    for (int i=11;i>=6;--i) { snprintf(tmp, sizeof(tmp), "%2d ", g->board[i]); strncat(top, tmp, sizeof(top)-strlen(top)-1); }
    for (int i=0;i<=5;++i) { snprintf(tmp, sizeof(tmp), "%2d ", g->board[i]); strncat(bottom, tmp, sizeof(bottom)-strlen(bottom)-1); }
    snprintf(out, out_sz,
        "  +--------------------------------------\n"
        "  |  %s |  <- Player 2 (pits 11..6)\n"
        "P2 captures: %2d\n"
        "  |--------------------------------------|\n"
        "  |  %s |  <- Player 1 (pits 0..5)\n"
        "P1 captures: %2d\n"
        "  +--------------------------------------+\n"
        "  Turn: Player %d | Phase: %s\n",
        top, g->captured[1], bottom, g->captured[0], g->turn, g->phase==0?"playing":"finished");
}

/* Simulate a move on a copy of the board. Returns captured total and resulting board in out_board.
 * If illegal (empty pit), returns -1. */
static int simulate_move_board(const int in_board[12], int player, int pit, int out_board[12], int *last_pos) {
    memcpy(out_board, in_board, 12*sizeof(int));
    if (pit<0 || pit>11) return -1;
    if (out_board[pit] == 0) return -1;
    int seeds = out_board[pit];
    out_board[pit] = 0;
    int pos = pit;
    int skip = pit; // skip origin
    while (seeds > 0) {
        pos = (pos + 1) % 12;
        if (pos == skip) continue;
        out_board[pos] += 1;
        seeds -= 1;
    }
    if (last_pos) *last_pos = pos;
    int captured_total = 0;
    if (is_opponent_pit(player, pos) && (out_board[pos]==2 || out_board[pos]==3)) {
        int i = pos;
        while (is_opponent_pit(player, i) && (out_board[i]==2 || out_board[i]==3)) {
            captured_total += out_board[i];
            out_board[i] = 0;
            i = (i - 1 + 12) % 12;
        }
    }
    return captured_total;
}

/* Compute legal moves for a player, considering the feeding rule. Returns count and writes them to moves[] (size 6).
 * If no non-starving moves available, returns all non-empty pits. */
static int legal_moves(game_t *g, int player, int moves[6]) {
    int start = pits_of_player_start(player);
    int end = pits_of_player_end(player);
    int cnt = 0;
    int nonstarving_cnt = 0;
    int temp_board[12];
    for (int p=start;p<=end;++p) {
        if (g->board[p]==0) continue;
        int last_pos;
        int cap = simulate_move_board(g->board, player, p, temp_board, &last_pos);
        if (cap < 0) continue; // empty or error
        int oppsum = 0;
        for (int i=pits_of_player_start(3-player); i<=pits_of_player_end(3-player); ++i) oppsum += temp_board[i];
        if (oppsum > 0) {
            moves[nonstarving_cnt++] = p;
        }
    }
    if (nonstarving_cnt > 0) {
        for (int i=0;i<nonstarving_cnt;++i) moves[i] = moves[i];
        return nonstarving_cnt;
    }
    // no non-starving moves, return any non-empty pit
    for (int p=start;p<=end;++p) if (g->board[p]>0) moves[cnt++] = p;
    return cnt;
}

/* Apply move to game state (validity must be checked before). Returns 0 on success, -1 on error. */
static int apply_move(game_t *g, int player, int pit) {
    if (g->phase != 0) return -1;
    if (g->turn != player) return -1;
    if (pit < pits_of_player_start(player) || pit > pits_of_player_end(player)) return -1;
    if (g->board[pit] == 0) return -1;
    int moves[6];
    int lm = legal_moves(g, player, moves);
    int allowed = 0;
    for (int i=0;i<lm;++i) if (moves[i]==pit) { allowed = 1; break; }
    if (!allowed) return -1; // violates feeding rule

    int out_board[12];
    int last_pos;
    int captured = simulate_move_board(g->board, player, pit, out_board, &last_pos);
    if (captured < 0) return -1;
    // apply
    memcpy(g->board, out_board, 12*sizeof(int));
    g->captured[player-1] += captured;
    // push history
    if (g->history_len < (int)(sizeof(g->history)/sizeof(g->history[0]))) {
        g->history[g->history_len].player = player;
        g->history[g->history_len].pit = pit;
        g->history[g->history_len].captured_amount = captured;
        g->history_len++;
    }
    // check end conditions
    if (g->captured[0] >= 25 || g->captured[1] >= 25) {
        g->phase = 1; return 0;
    }
    int p1sum=0,p2sum=0;
    for (int i=0;i<=5;++i) p1sum += g->board[i];
    for (int i=6;i<=11;++i) p2sum += g->board[i];
    if (p1sum == 0 || p2sum == 0) {
        g->captured[0] += p1sum;
        g->captured[1] += p2sum;
        for (int i=0;i<12;++i) g->board[i]=0;
        g->phase = 1; return 0;
    }
    int total = p1sum + p2sum;
    if (total < 8) {
        g->captured[0] += p1sum;
        g->captured[1] += p2sum;
        for (int i=0;i<12;++i) g->board[i]=0;
        g->phase = 1; return 0;
    }
    // otherwise change turn
    g->turn = 3 - player;
    return 0;
}

static int winner(game_t *g) {
    if (g->phase==0) return -1; // not finished
    if (g->captured[0] > g->captured[1]) return 1;
    if (g->captured[1] > g->captured[0]) return 2;
    return 0; // draw
}

/* Persist session: append to JSON array file (naive append - keep it simple). */
static void save_session(game_t *g) {
    FILE *f = fopen(SESSIONS_FILE, "a");
    if (!f) return;
    time_t t = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%dT%H:%M:%SZ", gmtime(&t));
    fprintf(f, "{\n");
    fprintf(f, "  \"timestamp\": \"%s\",\n", timestr);
    fprintf(f, "  \"captured\": [%d, %d],\n", g->captured[0], g->captured[1]);
    fprintf(f, "  \"history\": [\n");
    for (int i=0;i<g->history_len;++i) {
        fprintf(f, "    { \"player\": %d, \"pit\": %d, \"captured\": %d }%s\n",
            g->history[i].player, g->history[i].pit, g->history[i].captured_amount, (i+1==g->history_len)?"":" ,");
    }
    fprintf(f, "  ],\n");
    fprintf(f, "  \"final_board\": [");
    for (int i=0;i<12;++i) fprintf(f, "%d%s", g->board[i], (i==11)?"":", ");
    fprintf(f, "],\n");
    fprintf(f, "  \"winner\": %d\n", winner(g));
    fprintf(f, "}\n");
    fclose(f);
}

/* Find free client slot */
static int add_client(int fd, struct sockaddr_in *addr) {
    int i;
    for (i=0;i<MAX_CLIENTS;++i) if (!clients[i]) break;
    if (i==MAX_CLIENTS) return -1;
    client_t *c = calloc(1, sizeof(client_t));
    c->fd = fd;
    snprintf(c->addr, sizeof(c->addr), "%s:%d", inet_ntoa(addr->sin_addr), ntohs(addr->sin_port));
    c->inbuf_len = 0;
    c->role = ROLE_NONE;
    strncpy(c->name, "anon", sizeof(c->name)-1);
    clients[i] = c;
    if (fd > maxfd) maxfd = fd;
    return i;
}

static void remove_client(int idx) {
    if (idx<0 || idx>=MAX_CLIENTS) return;
    if (!clients[idx]) return;
    close(clients[idx]->fd);
    free(clients[idx]);
    clients[idx] = NULL;
}

static int find_player_slot(int player) {
    for (int i=0;i<MAX_CLIENTS;++i) if (clients[i] && (clients[i]->role == (player==1?ROLE_PLAYER1:ROLE_PLAYER2))) return i;
    return -1;
}

/* Send state and ascii board to a client */
static void send_state_to_client(int idx) {
    if (!clients[idx]) return;
    char buf[8192];
    // Construct JSON-ish state
    int len = snprintf(buf, sizeof(buf), "STATE { \"board\": [");
    for (int i=0;i<12;++i) len += snprintf(buf+len, sizeof(buf)-len, "%d%s", game.board[i], i==11?"":", ");
    len += snprintf(buf+len, sizeof(buf)-len, "], \"turn\": %d, \"captured\": [%d, %d], \"phase\": \"%s\", \"legal_moves\": [",
        game.turn, game.captured[0], game.captured[1], game.phase==0?"playing":"finished");
    int moves[6]; int mcount = legal_moves(&game, game.turn, moves);
    for (int i=0;i<mcount;++i) len += snprintf(buf+len, sizeof(buf)-len, "%d%s", moves[i], i==mcount-1?"":", ");
    len += snprintf(buf+len, sizeof(buf)-len, "] }\n");
    send_to_client(clients[idx], buf);
    char board_txt[1024]; ascii_board(&game, board_txt, sizeof(board_txt));
    char wrapped[2048]; snprintf(wrapped, sizeof(wrapped), "INFO ASCII_BOARD\n%s\n", board_txt);
    send_to_client(clients[idx], wrapped);
}

/* Broadcast state and ascii to all */
static void broadcast_state_all() {
    char buf[8192];
    int len = snprintf(buf, sizeof(buf), "STATE { \"board\": [");
    for (int i=0;i<12;++i) len += snprintf(buf+len, sizeof(buf)-len, "%d%s", game.board[i], i==11?"":", ");
    len += snprintf(buf+len, sizeof(buf)-len, "], \"turn\": %d, \"captured\": [%d, %d], \"phase\": \"%s\", \"legal_moves\": [",
        game.turn, game.captured[0], game.captured[1], game.phase==0?"playing":"finished");
    int moves[6]; int mcount = legal_moves(&game, game.turn, moves);
    for (int i=0;i<mcount;++i) len += snprintf(buf+len, sizeof(buf)-len, "%d%s", moves[i], i==mcount-1?"":", ");
    len += snprintf(buf+len, sizeof(buf)-len, "] }\n");
    broadcast(buf);
    char board_txt[1024]; ascii_board(&game, board_txt, sizeof(board_txt));
    char wrapped[2048]; snprintf(wrapped, sizeof(wrapped), "INFO ASCII_BOARD\n%s\n", board_txt);
    broadcast(wrapped);
}

/* Handle a complete line from client */
static void handle_line(int idx, const char *line) {
    if (!clients[idx]) return;
    char cmd[LINE_MAX];
    strncpy(cmd, line, sizeof(cmd)-1); cmd[sizeof(cmd)-1]='\0';
    // Trim leading/trailing
    char *s = cmd; while (*s==' '||*s=='\t') s++;
    char *e = s + strlen(s) - 1; while (e> s && (*e=='\r' || *e=='\n' || *e==' '||*e=='\t')) { *e='\0'; e--; }
    if (strlen(s)==0) return;

    if (strncasecmp(s, "JOIN", 4)==0) {
        char *arg = s+4; while (*arg==' ') arg++;
        if (strncasecmp(arg, "player1", 7)==0) {
            if (find_player_slot(1) != -1) {
                send_to_client(clients[idx], "ERROR player1 already taken\n");
                clients[idx]->role = ROLE_SPECTATOR;
                send_to_client(clients[idx], "INFO you are spectator\n");
            } else {
                clients[idx]->role = ROLE_PLAYER1;
                strcpy(clients[idx]->name, "player1");
                send_to_client(clients[idx], "INFO you are player1 (pits 0..5)\n");
                char info[128]; snprintf(info, sizeof(info), "INFO player1 joined from %s\n", clients[idx]->addr); broadcast(info);
            }
        } else if (strncasecmp(arg, "player2", 7)==0) {
            if (find_player_slot(2) != -1) {
                send_to_client(clients[idx], "ERROR player2 already taken\n");
                clients[idx]->role = ROLE_SPECTATOR;
                send_to_client(clients[idx], "INFO you are spectator\n");
            } else {
                clients[idx]->role = ROLE_PLAYER2;
                strcpy(clients[idx]->name, "player2");
                send_to_client(clients[idx], "INFO you are player2 (pits 6..11)\n");
                char info[128]; snprintf(info, sizeof(info), "INFO player2 joined from %s\n", clients[idx]->addr); broadcast(info);
            }
        } else {
            clients[idx]->role = ROLE_SPECTATOR;
            strcpy(clients[idx]->name, "spectator");
            send_to_client(clients[idx], "INFO you are spectator\n");
        }

	int p1 = find_player_slot(1);
	int p2 = find_player_slot(2);
	if (p1 != -1 && p2 != -1 && game.history_len == 0) {
    		srand(time(NULL));
    		game.turn = (rand() % 2) ? 1 : 2;
    		char msg[128];
    		snprintf(msg, sizeof(msg), "INFO Both players joined. Randomly selected Player %d to start.\n", game.turn);
    		broadcast(msg);
    		broadcast_state_all();
	}


        send_state_to_client(idx);
        return;
    
    if (strncasecmp(s, "STATE", 5)==0) { send_state_to_client(idx); return; }
    if (strncasecmp(s, "BOARD", 5)==0) {
        char board_txt[1024]; ascii_board(&game, board_txt, sizeof(board_txt));
        char wrapped[2048]; snprintf(wrapped, sizeof(wrapped), "INFO ASCII_BOARD\n%s\n", board_txt);
        send_to_client(clients[idx], wrapped);
        return;
    }
    if (strncasecmp(s, "MOVE", 4)==0) {
        char *arg = s+4; while (*arg==' ') arg++;
        if (clients[idx]->role != ROLE_PLAYER1 && clients[idx]->role != ROLE_PLAYER2) {
            send_to_client(clients[idx], "ERROR only players can move\n"); return;
        }
        int player = clients[idx]->role==ROLE_PLAYER1?1:2;
        if (game.turn != player) { send_to_client(clients[idx], "ERROR not your turn\n"); return; }
        int pit = atoi(arg);
        // apply move
        int res = apply_move(&game, player, pit);
        if (res != 0) { send_to_client(clients[idx], "ERROR illegal move or violation of rule\n"); return; }
        char info[128]; snprintf(info,sizeof(info),"INFO Player %d moved pit %d\n", player, pit); broadcast(info);
        broadcast_state_all();
        if (game.phase == 1) {
            char fin[128]; snprintf(fin,sizeof(fin),"INFO Game finished. Winner: %d\n", winner(&game)); broadcast(fin);
            save_session(&game);
        }
        return;
    }
    send_to_client(clients[idx], "ERROR unknown command\n");
}

/* Read data from client, split into lines and handle */
static void client_read_loop(int idx) {
    client_t *c = clients[idx];
    if (!c) return;
    char buf[1024];
    ssize_t r = recv(c->fd, buf, sizeof(buf), 0);
    if (r <= 0) {
        // disconnected
        if (r==0) {
            char info[128]; snprintf(info,sizeof(info),"INFO client %s disconnected\n", c->addr); broadcast(info);
        }
        remove_client(idx);
        return;
    }
    // append to inbuf
    if (c->inbuf_len + r >= (int)sizeof(c->inbuf)-1) {
        // overflow, reset
        c->inbuf_len = 0;
        return;
    }
    memcpy(c->inbuf + c->inbuf_len, buf, r);
    c->inbuf_len += r;
    c->inbuf[c->inbuf_len] = '\0';
    // extract lines
    char *start = c->inbuf;
    char *nl;
    while ((nl = strchr(start, '\n')) != NULL) {
        size_t linelen = nl - start;
        if (linelen > 0 && start[linelen-1] == '\r') linelen--;
        char line[LINE_MAX];
        if (linelen >= sizeof(line)) linelen = sizeof(line)-1;
        memcpy(line, start, linelen); line[linelen]='\0';
        handle_line(idx, line);
        start = nl + 1;
    }
    // move remaining bytes to front
    int remaining = c->inbuf + c->inbuf_len - start;
    if (remaining > 0) memmove(c->inbuf, start, remaining);
    c->inbuf_len = remaining;
}

int main(int argc, char **argv) {
    int port = DEFAULT_PORT;
    if (argc >= 2) port = atoi(argv[1]);

    // init
    for (int i=0;i<MAX_CLIENTS;++i) clients[i]=NULL;
    game_init(&game);

    listener_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listener_fd < 0) perror_exit("socket");
    int opt=1; setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    struct sockaddr_in srv;
    memset(&srv,0,sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_addr.s_addr = INADDR_ANY;
    srv.sin_port = htons(port);
    if (bind(listener_fd, (struct sockaddr*)&srv, sizeof(srv)) < 0) perror_exit("bind");
    if (listen(listener_fd, 16) < 0) perror_exit("listen");
    set_nonblocking(listener_fd);
    maxfd = listener_fd;
    printf("Awale server listening on 0.0.0.0:%d\n", port);

    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listener_fd, &readfds);
        int local_max = listener_fd;
        for (int i=0;i<MAX_CLIENTS;++i) if (clients[i]) {
            FD_SET(clients[i]->fd, &readfds);
            if (clients[i]->fd > local_max) local_max = clients[i]->fd;
        }
        int ready = select(local_max+1, &readfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }
        if (FD_ISSET(listener_fd, &readfds)) {
            struct sockaddr_in cliaddr; socklen_t cli_len = sizeof(cliaddr);
            int fd = accept(listener_fd, (struct sockaddr*)&cliaddr, &cli_len);
            if (fd >= 0) {
                set_nonblocking(fd);
                int slot = add_client(fd, &cliaddr);
                if (slot < 0) {
                    close(fd);
                } else {
                    char welcome[256];
                    snprintf(welcome, sizeof(welcome), "INFO Welcome to Awale server. Send: JOIN player1|player2|spectator\n");
                    send_to_client(clients[slot], welcome);
                }
            }
        }
        for (int i=0;i<MAX_CLIENTS;++i) if (clients[i]) {
            if (FD_ISSET(clients[i]->fd, &readfds)) {
                client_read_loop(i);
            }
        }
    }

    // cleanup
    for (int i=0;i<MAX_CLIENTS;++i) if (clients[i]) remove_client(i);
    close(listener_fd);
    return 0;
}
