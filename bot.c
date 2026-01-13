#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#define NUM_HOLES 16
#define MAX_MOVES 400


#define MAX_DEPTH 6

typedef struct {
    int r[NUM_HOLES];
    int b[NUM_HOLES];
    int t[NUM_HOLES];
    int capA;
    int capB;
    int moveCount;
} GameState;

typedef enum { C_RED=0, C_BLUE=1, C_T=2 } SeedColor;

typedef struct {
    int hole;           // 0..15
    int useT;           // 0 => R/B, 1 => TR/TB
    SeedColor treated;  // C_RED or C_BLUE
} Move;

// --- helpers trous ---
static inline int nextHole(int h) { return (h == 15) ? 0 : h + 1; }
static inline int prevHole(int h) { return (h == 0) ? 15 : h - 1; }

// Joueur A: trous 1,3,5,... => indices 0,2,4,...
static inline int isOwnedByA(int idx) { return (idx % 2) == 0; }
static inline int isOwnedByB(int idx) { return (idx % 2) == 1; }

static int totalSeedsHole(const GameState* s, int idx) {
    return s->r[idx] + s->b[idx] + s->t[idx];
}

static int totalSeedsBoard(const GameState* s) {
    int sum = 0;
    for(int i=0;i<NUM_HOLES;i++) sum += totalSeedsHole(s,i);
    return sum;
}

static int totalSeedsInOpponentHoles(const GameState* s, char player) {
    int sum = 0;
    for(int i=0;i<NUM_HOLES;i++){
        int opp = (player=='A') ? isOwnedByB(i) : isOwnedByA(i);
        if(opp) sum += totalSeedsHole(s,i);
    }
    return sum;
}

// --- init game (pour protocole "dernier coup") ---
static void initGame(GameState* s){
    s->capA = s->capB = 0;
    s->moveCount = 0;
    for(int i=0;i<NUM_HOLES;i++){
        s->r[i]=2; s->b[i]=2; s->t[i]=2;
    }
}

// --- parsing STATE (optionnel, au cas où) ---
static int parseStateLine(const char* line, GameState* s, char* playerToMove) {
    if (strncmp(line, "STATE ", 6) != 0) return 0;

    char buf[4096];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* tok = strtok(buf, " \t\r\n");
    if (!tok || strcmp(tok, "STATE") != 0) return 0;

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;
    *playerToMove = tok[0];

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;
    s->moveCount = atoi(tok);

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;
    s->capA = atoi(tok);

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;
    s->capB = atoi(tok);

    tok = strtok(NULL, " \t\r\n");
    if (!tok) return 0;

    for (int i = 0; i < NUM_HOLES; i++) {
        s->r[i] = s->b[i] = s->t[i] = 0;
    }

    char* seg = strtok(tok, ";");
    int parsed = 0;

    while (seg) {
        int idx, rr, bb, tt;
        if (sscanf(seg, "%d:%d,%d,%d", &idx, &rr, &bb, &tt) == 4) {
            if (idx >= 1 && idx <= 16) {
                s->r[idx - 1] = rr;
                s->b[idx - 1] = bb;
                s->t[idx - 1] = tt;
                parsed++;
            }
        }
        seg = strtok(NULL, ";");
    }

    return (parsed == 16);
}

// --- parse coup entrant: "3R", "12B", "7TR", "16TB" ---
static int parseMoveStr(const char* str, Move* m){
    char s[64];
    int n=0;
    while(str[n] && n<63){
        s[n] = (char)toupper((unsigned char)str[n]);
        n++;
    }
    s[n]='\0';

    // numéro
    int i=0;
    while(s[i] && isdigit((unsigned char)s[i])) i++;
    if(i==0) return 0;

    int hole = atoi(s);
    if(hole < 1 || hole > 16) return 0;

    const char* suf = s+i;
    if(strcmp(suf,"R")==0){ *m = (Move){hole-1,0,C_RED}; return 1; }
    if(strcmp(suf,"B")==0){ *m = (Move){hole-1,0,C_BLUE}; return 1; }
    if(strcmp(suf,"TR")==0){ *m = (Move){hole-1,1,C_RED}; return 1; }
    if(strcmp(suf,"TB")==0){ *m = (Move){hole-1,1,C_BLUE}; return 1; }

    return 0;
}

// --- règles: validité move ---
static int isValidMove(const GameState* s, char player, const Move* m) {
    if (m->hole < 0 || m->hole >= NUM_HOLES) return 0;

    if (player=='A' && !isOwnedByA(m->hole)) return 0;
    if (player=='B' && !isOwnedByB(m->hole)) return 0;

    if (!m->useT) {
        if (m->treated==C_RED)  return s->r[m->hole] > 0;
        if (m->treated==C_BLUE) return s->b[m->hole] > 0;
        return 0;
    } else {
        return s->t[m->hole] > 0;
    }
}

// Semis selon pattern:
// - patternAs RED: déposer dans tous les trous sauf source
// - patternAs BLUE: déposer uniquement chez l'adversaire (du joueur qui joue)
static int sow(GameState* s, char player, int source, int count, SeedColor tokenColor, SeedColor patternAs) {
    if (count <= 0) return source;
    int h = nextHole(source);
    int last = source;

    for(int k=0;k<count;k++){
        while(1){
            if(h == source){ h = nextHole(h); continue; }
            if(patternAs == C_RED) break;
            int opp = (player=='A') ? isOwnedByB(h) : isOwnedByA(h);
            if(opp) break;
            h = nextHole(h);
        }

        if(tokenColor == C_RED) s->r[h] += 1;
        else if(tokenColor == C_BLUE) s->b[h] += 1;
        else s->t[h] += 1;

        last = h;
        h = nextHole(h);
    }
    return last;
}

static int captureFrom(GameState* s, int lastHole) {
    int captured = 0;
    int h = lastHole;
    while(1){
        int tot = totalSeedsHole(s,h);
        if(tot==2 || tot==3){
            captured += tot;
            s->r[h]=s->b[h]=s->t[h]=0;
            h = prevHole(h);
        } else break;
    }
    return captured;
}

// Applique move, met à jour captures, famine, etc.
static int applyMove(GameState* s, char player, const Move* m) {
    s->moveCount++;

    int src = m->hole;
    int last = src;

    if(m->useT){
        int tCount = s->t[src];
        s->t[src] = 0;
        last = sow(s, player, src, tCount, C_T, m->treated);

        if(m->treated == C_RED){
            int c = s->r[src]; s->r[src]=0;
            if(c>0) last = sow(s, player, src, c, C_RED, C_RED);
        } else {
            int c = s->b[src]; s->b[src]=0;
            if(c>0) last = sow(s, player, src, c, C_BLUE, C_BLUE);
        }
    } else {
        if(m->treated == C_RED){
            int c = s->r[src]; s->r[src]=0;
            last = sow(s, player, src, c, C_RED, C_RED);
        } else {
            int c = s->b[src]; s->b[src]=0;
            last = sow(s, player, src, c, C_BLUE, C_BLUE);
        }
    }

    int cap = captureFrom(s, last);
    if(player=='A') s->capA += cap;
    else s->capB += cap;

    // famine
    if(totalSeedsInOpponentHoles(s, player) == 0){
        int rest = totalSeedsBoard(s);
        for(int i=0;i<NUM_HOLES;i++) s->r[i]=s->b[i]=s->t[i]=0;
        if(player=='A') s->capA += rest;
        else s->capB += rest;
    }

    // fin (pas utilisé par l’arbitre “dumb”, mais utile pour minimax)
    if(s->capA >= 49 || s->capB >= 49) return 1;
    if(totalSeedsBoard(s) < 10) return 1;
    if(s->moveCount >= MAX_MOVES) return 1;
    return 0;
}

// --- génération coups ---
static int generateMoves(const GameState* s, char player, Move* moves, int maxMoves) {
    int n = 0;
    for(int i=0;i<NUM_HOLES;i++){
        int owned = (player=='A') ? isOwnedByA(i) : isOwnedByB(i);
        if(!owned) continue;

        if(s->r[i] > 0 && n < maxMoves){
            moves[n++] = (Move){ i, 0, C_RED };
        }
        if(s->b[i] > 0 && n < maxMoves){
            moves[n++] = (Move){ i, 0, C_BLUE };
        }
        if(s->t[i] > 0 && n+1 < maxMoves){
            moves[n++] = (Move){ i, 1, C_RED };   // TR
            moves[n++] = (Move){ i, 1, C_BLUE };  // TB
        }
    }
    return n;
}

// --- évaluation ---
static int evaluate(const GameState* s, char me) {
    int myCap  = (me=='A') ? s->capA : s->capB;
    int opCap  = (me=='A') ? s->capB : s->capA;

    int score = (myCap - opCap) * 20;

    int mySeeds = 0, opSeeds = 0;
    for(int i=0;i<NUM_HOLES;i++){
        int ownedByMe = (me=='A') ? isOwnedByA(i) : isOwnedByB(i);
        int tot = totalSeedsHole(s,i);
        if(ownedByMe) mySeeds += tot;
        else opSeeds += tot;
    }
    score += (mySeeds - opSeeds);

    // pression famine (plus opSeeds est faible, mieux c'est)
    score += (20 - opSeeds);

    // éviter/chercher des trous à 2/3
    int opVul = 0, meVul = 0;
    for(int i=0;i<NUM_HOLES;i++){
        int tot = totalSeedsHole(s,i);
        int ownedByMe = (me=='A') ? isOwnedByA(i) : isOwnedByB(i);
        if(tot==2 || tot==3){
            if(!ownedByMe) opVul += 6;
            else meVul += 6;
        }
    }
    score += (opVul - meVul);

    return score;
}

// --- minimax alpha-beta ---
static int minimax(GameState* s, int depth, int alpha, int beta, int maximizing, char me, char current) {
    if(depth == 0) return evaluate(s, me);

    if(s->capA >= 49 || s->capB >= 49 || totalSeedsBoard(s) < 10 || s->moveCount >= MAX_MOVES){
        return evaluate(s, me);
    }

    Move moves[128];
    int n = generateMoves(s, current, moves, 128);
    if(n == 0) return evaluate(s, me);

    char next = (current=='A') ? 'B' : 'A';

    if(maximizing){
        int best = INT_MIN;
        for(int i=0;i<n;i++){
            if(!isValidMove(s, current, &moves[i])) continue;
            GameState ns = *s;
            applyMove(&ns, current, &moves[i]);
            int val = minimax(&ns, depth-1, alpha, beta, 0, me, next);
            if(val > best) best = val;
            if(best > alpha) alpha = best;
            if(beta <= alpha) break;
        }
        return best;
    } else {
        int best = INT_MAX;
        for(int i=0;i<n;i++){
            if(!isValidMove(s, current, &moves[i])) continue;
            GameState ns = *s;
            applyMove(&ns, current, &moves[i]);
            int val = minimax(&ns, depth-1, alpha, beta, 1, me, next);
            if(val < best) best = val;
            if(best < beta) beta = best;
            if(beta <= alpha) break;
        }
        return best;
    }
}

static Move bestMove(GameState* s, char me) {
    Move moves[128];
    int n = generateMoves(s, me, moves, 128);

    // fallback (au cas où)
    Move best = moves[0];
    int bestVal = INT_MIN;

    for(int i=0;i<n;i++){
        if(!isValidMove(s, me, &moves[i])) continue;
        GameState ns = *s;
        applyMove(&ns, me, &moves[i]);
        int val = minimax(&ns, MAX_DEPTH-1, INT_MIN, INT_MAX, 0, me, (me=='A')?'B':'A');
        if(val > bestVal){
            bestVal = val;
            best = moves[i];
        }
    }
    return best;
}

static void printMove(const Move* m) {
    int hole1 = m->hole + 1;
    if(!m->useT){
        if(m->treated==C_RED)  printf("%dR\n", hole1);
        else printf("%dB\n", hole1);
    } else {
        if(m->treated==C_RED)  printf("%dTR\n", hole1);
        else printf("%dTB\n", hole1);
    }
    fflush(stdout);
}

int main(void) {
    static GameState g;
    static int initialized = 0;
    static char me = '?'; // 'A' ou 'B' déterminé au 1er message

    char line[256];

    while (fgets(line, sizeof(line), stdin)) {
        // strip \r\n
        char in[256];
        int k = 0;
        for (int i = 0; line[i] && k < 255; i++) {
            if (line[i] != '\r' && line[i] != '\n') in[k++] = line[i];
        }
        in[k] = '\0';

        if (strcmp(in, "END") == 0) break;

        // Optionnel: si certains arbitres envoient STATE, on peut répondre aussi
        if (strncmp(in, "STATE ", 6) == 0) {
            GameState s;
            char p='A';
            if (parseStateLine(in, &s, &p)) {
                Move mv = bestMove(&s, p);
                printMove(&mv);
            } else {
                printf("1R\n"); fflush(stdout);
            }
            continue;
        }

        // Init au 1er message
        if (!initialized) {
            initGame(&g);
            initialized = 1;
            if (strcmp(in, "START") == 0) me = 'A';
            else me = 'B';
        }

        // START => je joue
        if (strcmp(in, "START") == 0) {
            Move mv = bestMove(&g, me);
            applyMove(&g, me, &mv);
            printMove(&mv);
            continue;
        }

        // Sinon => coup adverse
        Move opp;
        if (parseMoveStr(in, &opp)) {
            char oppP = (me == 'A') ? 'B' : 'A';
            applyMove(&g, oppP, &opp);
        } else {
            // entrée inconnue : on ignore (pas de print sur stdout)
            // fprintf(stderr, "Unknown input: %s\n", in);
        }

        // je réponds
        Move mv = bestMove(&g, me);
        applyMove(&g, me, &mv);
        printMove(&mv);
    }

    return 0;
}
