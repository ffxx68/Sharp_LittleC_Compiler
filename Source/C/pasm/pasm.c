// pasm.c
// File generato per la migrazione da Pascal a C
// Strutture dati e costanti iniziali

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include "pasm_constants.h"

#define CODE_SIZE 100001
#define SYM_SIZE 1001
#define LAB_SIZE 1001
#define NLAB_SIZE 1001


// Variabili globali
char tok[256], op[256], param1[256], param2[256], params[256];
unsigned char code[CODE_SIZE];
int codpos = 0;
char sym[SYM_SIZE][256];
char symval[SYM_SIZE][256];
int symcnt = 0;
char lab[LAB_SIZE][256];
int labpos[LAB_SIZE];
int labcnt = 0, labp = 0;
char nlab[NLAB_SIZE][256];
int nlabpos[NLAB_SIZE];
char nlabasm[NLAB_SIZE][256];
int nlabcnt = 0, nlabp = 0;
int startadr = 0, blcnt, opp = 0;
bool mcase = false;
int ccase = 0, casecnt = 0, cline = 0, i = 0;
char s[1024], cf[256];

// Funzione di utilità: trim (rimuove spazi, tab, newline)
void trim(char *str) {
    char *end;
    char *start = str;

    // Trova l'inizio dei caratteri non-whitespace
    while(*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;

    // Se la stringa è tutta whitespace
    if(*start == 0) {
        str[0] = 0;
        return;
    }

    // Sposta i caratteri all'inizio della stringa se necessario
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }

    // Trim spazi finali
    end = str + strlen(str) - 1;
    while(end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    *(end+1) = 0;
}

// Funzione di utilità: replace_text
void replace_text(char *text, const char *such, const char *ers) {
    char buffer[1024];
    char *pos, *start = text;
    size_t such_len = strlen(such);
    size_t ers_len = strlen(ers);
    if (such_len == 0 || strlen(text) == 0) return;
    buffer[0] = '\0';
    while ((pos = strstr(start, such)) != NULL) {
        strncat(buffer, start, pos - start);
        strcat(buffer, ers);
        start = pos + such_len;
    }
    strcat(buffer, start);
    strncpy(text, buffer, 1023);
    text[1023] = '\0';
    trim(text);
}

// Funzione di utilità: abort
void abort_c(const char *t) {
    fprintf(stderr, "Line %d: %s in file %s\n", cline + 1, t, cf);
    exit(1);
}

// Funzione di utilità: findnlabel
bool findnlabel(const char *l) {
    char up[256];
    int i;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (i = 0; up[i]; i++) up[i] = toupper(up[i]);
    nlabp = -1;
    for (i = 0; i < nlabcnt; i++) {
        if (strcmp(nlab[i], up) == 0) {
            nlabp = i;
            return true;
        }
    }
    return false;
}

// Funzione di utilità: addnlabel
void addnlabel(const char *l) {
    char up[256];
    int i;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (i = 0; up[i]; i++) up[i] = toupper(up[i]);
    strcpy(nlab[nlabcnt], up);
    nlabpos[nlabcnt] = codpos;
    snprintf(nlabasm[nlabcnt], 255, "%s %s", op, params);
    if (mcase) {
        char tmp[256];
        snprintf(tmp, 255, "#%s %s", op, params);
        strncpy(nlabasm[nlabcnt], tmp, 255);
    }
    nlabcnt++;
}

// Funzione di utilità: delnlabel
void delnlabel(int l) {
    int i;
    for (i = l; i < nlabcnt - 1; i++) {
        strcpy(nlab[i], nlab[i + 1]);
        nlabpos[i] = nlabpos[i + 1];
        strcpy(nlabasm[i], nlabasm[i + 1]);
    }
    nlabcnt--;
}

// Funzione di utilità: findlabel
bool findlabel(const char *l) {
    char up[256];
    int i;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (i = 0; up[i]; i++) up[i] = toupper(up[i]);
    labp = -1;
    for (i = 0; i < labcnt; i++) {
        if (strcmp(lab[i], up) == 0) {
            labp = i;
            return true;
        }
    }
    return false;
}

// Funzione di utilità: addlabel (parziale, da completare con logica di estrazione e doasm)
void addlabel(const char *l) {
    char up[256];
    int tpos, bup;
    strncpy(up, l, 255);
    up[255] = '\0';
    for (int i = 0; up[i]; i++) up[i] = toupper(up[i]);
    if (findlabel(up)) abort_c("Label già definita!");
    printf("SYMBOL: %s - %d\n", up, codpos + startadr);
    strcpy(lab[labcnt], up);
    labpos[labcnt] = codpos;
    labcnt++;
    // La parte di gestione nlab, estrazione e doasm va completata con le funzioni migrate successive
}

// Funzione di utilità: findsymbol
bool findsymbol(const char *l) {
    int i;
    for (i = 0; i < symcnt; i++) {
        if (strcmp(sym[i], l) == 0) {
            return true;
        }
    }
    return false;
}

// Funzione di utilità: addsymbol
void addsymbol(const char *s1, const char *s2) {
    if (findsymbol(s1)) abort_c("Symbol già definito!");
    strcpy(sym[symcnt], s1);
    strcpy(symval[symcnt], s2);
    printf("SYMBOL: %s - %s\n", s1, s2);
    symcnt++;
}

// Funzione di utilità: findop
bool findop(const char *l) {
    int i;
    opp = -1;
    for (i = 0; i < 256; i++) {
        if (strcmp(OPCODE[i], l) == 0) {
            opp = i;
            return true;
        }
    }
    return false;
}

// Funzione di utilità: addcode
void addcode(unsigned char b) {
    code[codpos] = b;
    codpos++;
    if (codpos + startadr >= 65536) {
        abort_c("Code exceeds maximum memory!");
    }
}

// Funzione di utilità: extractop
void extractop(const char *src) {
    char s[1024];
    int i = 0, j = 0;
    if (src == NULL || src[0] == '\0') return;
    strncpy(s, src, 1023);
    s[1023] = '\0';
    strcat(s, " ");
    // Salta spazi iniziali
    while (s[i] == ' ' || s[i] == '\t') i++;
    // Copia l'operatore
    j = 0;
    while (s[i] != ' ' && s[i] != '\t' && s[i] != '\0') {
        op[j++] = toupper(s[i++]);
    }
    op[j] = '\0';
    // Salta spazi dopo l'operatore
    while (s[i] == ' ' || s[i] == '\t') i++;
    // Copia i parametri
    strncpy(params, s + i, 255);
    params[255] = '\0';
    // Sostituisci la prima virgola con spazio
    char *comma = strchr(params, ',');
    if (comma) *comma = ' ';
    // Estrai param1 e param2
    i = 0;
    while (params[i] == ' ' || params[i] == '\t') i++;
    j = 0;
    while (params[i] != ' ' && params[i] != '\t' && params[i] != '\0') {
        param1[j++] = params[i++];
    }
    param1[j] = '\0';
    while (params[i] == ' ' || params[i] == '\t') i++;
    if (params[i] != '\0') {
        strncpy(param2, params + i, 255);
        param2[255] = '\0';
    } else {
        param2[0] = '\0';
    }
}

// Funzione di utilità: calcadr migliorata per gestire simboli
int calcadr() {
    int val = 0;
    char *endptr;

    // Prima prova a convertire come numero
    val = (int)strtol(param1, &endptr, 0);
    if (*endptr == '\0') {
        return val; // È un numero valido
    }

    // Se non è un numero, cerca tra i simboli
    for (int i = 0; i < symcnt; i++) {
        if (strcmp(sym[i], param1) == 0) {
            return atoi(symval[i]);
        }
    }

    // Per i salti relativi, restituisci l'indirizzo assoluto dell'etichetta
    if (opp == 44 || opp == 45 || (opp >= 40 && opp <= 43) || (opp >= 56 && opp <= 59) || opp == 47) {
        if (findlabel(param1)) {
            return labpos[labp] + startadr;  // Indirizzo assoluto come nel Pascal
        } else {
            addnlabel(param1);
            return 0; // Placeholder
        }
    }

    // Per altri tipi di istruzioni, cerca le etichette
    if (findlabel(param1)) {
        return labpos[labp] + startadr;
    }

    // Se non trovato da nessuna parte, aggiungi come etichetta non risolta
    addnlabel(param1);
    return 0;
}


// Funzione di utilità per verificare se un opcode è in un set
bool in_set(int value, const int *set, int size) {
    for (int i = 0; i < size; i++) {
        if (set[i] == value) return true;
    }
    return false;
}

// Funzione di parsing matematico migliorata per gestire simboli
int mathparse(const char *expr, int base) {
    char *endptr;
    int result = 0;

    if (expr == NULL || *expr == '\0') return 0;

    // Prima cerca tra i simboli definiti
    for (int i = 0; i < symcnt; i++) {
        if (strcmp(sym[i], expr) == 0) {
            return atoi(symval[i]);
        }
    }

    // Gestione esadecimale (0x...)
    if (strncmp(expr, "0x", 2) == 0 || strncmp(expr, "0X", 2) == 0) {
        result = (int)strtol(expr, &endptr, 16);
    }
    // Gestione binario (0b...)
    else if (strncmp(expr, "0b", 2) == 0 || strncmp(expr, "0B", 2) == 0) {
        result = (int)strtol(expr + 2, &endptr, 2);
    }
    // Gestione decimale/ottale
    else {
        result = (int)strtol(expr, &endptr, base);
    }

    return result;
}

// Funzione di utilità: doasm implementazione completa
void doasm(const char *line) {
    char s[1024];
    int adr, reladr;

    // Copia la riga ed elabora
    strncpy(s, line, 1023);
    s[1023] = '\0';
    trim(s);

    // Ignora righe vuote e commenti
    if (s[0] == '\0' || s[0] == ';') return;

    // Gestisce le etichette (righe che contengono ':' come etichetta) PRIMA di extractop
    char *colon = strchr(s, ':');
    if (colon != NULL) {
        // Verifica che non sia all'interno di un commento
        char *comment = strchr(s, ';');
        if (comment == NULL || colon < comment) {
            // Estrai la parte prima dei due punti come etichetta
            *colon = '\0';
            trim(s);
            if (strlen(s) > 0) {
                addlabel(s);
            }
            // Processa eventuale codice dopo i due punti
            char *after_colon = colon + 1;
            trim(after_colon);
            if (strlen(after_colon) > 0 && after_colon[0] != ';') {
                doasm(after_colon); // Ricorsione per processare istruzione dopo etichetta
            }
            return;
        }
    }

    // Gestisce direttive assemblatore
    if (s[0] == '.') {
        // .org directive
        if (strncmp(s, ".org", 4) == 0 || strncmp(s, ".ORG", 4) == 0) {
            char *addr_str = s + 4;
            while (*addr_str == ' ' || *addr_str == '\t') addr_str++;
            startadr = mathparse(addr_str, 10);
            printf("ORG: Start address set to %d\n", startadr);
            return;
        }
        // .equ directive
        if (strncmp(s, ".equ", 4) == 0 || strncmp(s, ".EQU", 4) == 0) {
            char *rest = s + 4;
            while (*rest == ' ' || *rest == '\t') rest++;
            char *space = strchr(rest, ' ');
            if (!space) space = strchr(rest, '\t');
            if (space) {
                *space = '\0';
                char *value = space + 1;
                while (*value == ' ' || *value == '\t') value++;
                char val_str[32];
                snprintf(val_str, 31, "%d", mathparse(value, 10));
                addsymbol(rest, val_str);
                printf("EQU: %s = %s\n", rest, val_str);
            }
            return;
        }
        // .include directive (compatibile con virgolette o senza)
        if (strncasecmp(s, ".include", 8) == 0) {
            char *rest = s + 8;
            while (*rest == ' ' || *rest == '\t') rest++;
            // Rimuovi virgolette se presenti
            char fname[256];
            if (*rest == '"' || *rest == '\'') {
                char quote = *rest;
                rest++;
                int i = 0;
                while (*rest && *rest != quote && i < 255) fname[i++] = *rest++;
                fname[i] = '\0';
            } else {
                int i = 0;
                while (*rest && *rest != ' ' && *rest != '\t' && *rest != ';' && i < 255) fname[i++] = *rest++;
                fname[i] = '\0';
            }
            // Apri e processa il file di include
            FILE *inc = fopen(fname, "r");
            if (!inc) {
                fprintf(stderr, "Line %d: Include file '%s' not found! in file %s\n", cline + 1, fname, cf);
                exit(1);
            }
            char line[1024];
            while (fgets(line, sizeof(line), inc)) {
                trim(line);
                if (line[0] != '\0') doasm(line);
            }
            fclose(inc);
            return;
        }
        // .db directive - definizione dati byte
        if (strncasecmp(s, ".db", 3) == 0) {
            char *rest = s + 3;
            while (*rest == ' ' || *rest == '\t') rest++;

            // Processa i parametri separati da virgola
            char *token = strtok(rest, ",");
            while (token != NULL) {
                // Rimuovi spazi iniziali e finali dal token
                while (*token == ' ' || *token == '\t') token++;
                char *end = token + strlen(token) - 1;
                while (end > token && (*end == ' ' || *end == '\t')) end--;
                *(end+1) = '\0';

                // Converte il token in byte e lo aggiunge al codice
                addcode(mathparse(token, 8));

                token = strtok(NULL, ",");
            }
            return;
        }
        return; // Ignora altre direttive non implementate
    }

    // Estrai operazione e parametri SOLO se non è un'etichetta o direttiva
    extractop(s);

    // Pulisci param1 rimuovendo virgole finali
    if (strlen(param1) > 0 && param1[strlen(param1) - 1] == ',') {
        param1[strlen(param1) - 1] = '\0';
    }
    trim(param1);
    trim(param2);

    // Cerca l'opcode nella tabella
    if (findop(op)) {
        // Gestione salti assoluti
        if (opp == 120 || opp == 121 || opp == 16 || (opp >= 124 && opp <= 127)) {
            adr = calcadr();
            if (adr > 0) {
                addcode(opp);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        // Gestione salti relativi
        else if (in_set(opp, JR, 11)) {
            adr = calcadr();
            addcode(opp);
            if (adr >= 8192) {
                if (in_set(opp, JRPLUS, 5)) {
                    addcode(adr - codpos - startadr);
                } else {
                    addcode(abs(codpos + startadr - adr));
                }
            } else if (adr > 0) {
                // Per tutti i salti relativi, calcola l'offset relativo
                if (in_set(opp, JRPLUS, 5)) {
                    addcode(adr - codpos - startadr);
                } else {
                    addcode(abs(codpos + startadr - adr));
                }
            } else {
                addcode(0); // Placeholder per etichette non risolte
            }
        }
        // Gestione istruzioni normali
        else {
            addcode(opp);
            if (NBARGU[opp] == 2) {
                addcode(mathparse(param1, 8));
            } else if (NBARGU[opp] == 3) {
                if (param2[0] == '\0') {
                    int val = mathparse(param1, 16);
                    addcode((val >> 8) & 0xFF);
                    addcode(val & 0xFF);
                } else {
                    addcode(mathparse(param1, 8));
                    addcode(mathparse(param2, 8));
                }
            }
        }
    }
    // Gestione istruzioni speciali non nella tabella OPCODE
    else {
        if (strcmp(op, "ADD") == 0) {
            if (strcmp(param1, "[P]") == 0 && strcmp(param2, "A") == 0) {
                addcode(0x44);
            } else if (strcmp(param1, "[P]") == 0) {
                addcode(0x70);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "A") == 0) {
                addcode(0x74);
                addcode(mathparse(param2, 8));
            }
        }
        else if (strcmp(op, "MOV") == 0) {
            if (strcmp(param1, "A") == 0 && strcmp(param2, "[+X]") == 0) {
                addcode(36);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "[-X]") == 0) {
                addcode(37);
            } else if (strcmp(param1, "[+Y]") == 0 && strcmp(param2, "A") == 0) {
                addcode(38);
            } else if (strcmp(param1, "[-Y]") == 0 && strcmp(param2, "A") == 0) {
                addcode(39);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "R") == 0) {
                addcode(34);
            } else if (strcmp(param1, "R") == 0 && strcmp(param2, "A") == 0) {
                addcode(50);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "Q") == 0) {
                addcode(33);
            } else if (strcmp(param1, "Q") == 0 && strcmp(param2, "A") == 0) {
                addcode(49);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "P") == 0) {
                addcode(32);
            } else if (strcmp(param1, "P") == 0 && strcmp(param2, "A") == 0) {
                addcode(48);
            } else if (strcmp(param1, "[P]") == 0 && strcmp(param2, "[DP]") == 0) {
                addcode(85);
            } else if (strcmp(param1, "[DP]") == 0 && strcmp(param2, "[P]") == 0) {
                addcode(83);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "[DP]") == 0) {
                addcode(87);
            } else if (strcmp(param1, "[DP]") == 0 && strcmp(param2, "A") == 0) {
                addcode(82);
            } else if (strcmp(param1, "A") == 0 && strcmp(param2, "[P]") == 0) {
                addcode(89);
            } else if (strcmp(param1, "[P]") == 0 && strcmp(param2, "A") == 0) {
                addcode(219);
                addcode(89);
            } else if (strcmp(param1, "A") == 0) {
                addcode(2);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "B") == 0) {
                addcode(3);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "I") == 0) {
                addcode(0);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "J") == 0) {
                addcode(1);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "P") == 0) {
                addcode(18);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "Q") == 0) {
                addcode(19);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "DPL") == 0) {
                addcode(17);
                addcode(mathparse(param2, 8));
            } else if (strcmp(param1, "DP") == 0) {
                addcode(16);
                int val = mathparse(param2, 16);
                addcode(val / 256);
                addcode(val % 256);
            }
        }
        else if (strcmp(op, "NOP") == 0) {
            if (param1[0] == '\0') {
                addcode(77); // NOP constant
            } else {
                addcode(78);
                addcode(mathparse(param1, 8));
            }
        }
        else if (strcmp(op, "LP") == 0) {
            int val = mathparse(param1, 8);
            if (val > 63) abort_c("LP command exceeds range!");
            addcode(128 + val);
        }
        else if (strcmp(op, "RTN") == 0) {
            addcode(55);
        }
        else if (strcmp(op, "CALL") == 0) {
            adr = calcadr();
            if (adr > 0) {
                if (adr < 8192) {
                    addcode(0xE0 + adr / 256);
                    addcode(adr % 256);
                } else {
                    addcode(0x78);
                    addcode(adr / 256);
                    addcode(adr % 256);
                }
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else if (strcmp(op, "JMP") == 0) {
            adr = calcadr();
            if (adr > 0) {
                addcode(121);
                addcode(adr / 256);
                addcode(adr % 256);
            } else {
                addcode(NOP);
                addcode(NOP);
                addcode(NOP);
            }
        }
        else {
            printf("Warning: Unknown instruction '%s' at line %d\n", op, cline);
        }
    }
}

// Funzione di utilità: resolve_labels_and_write_bin implementazione completa
void resolve_labels_and_write_bin(const char *output_filename) {
    FILE *fout;
    int i, j;
    bool resolved;

    printf("Risolvendo etichette non definite...\n");

    // Risolvi le etichette non definite con multiple passate
    for (int pass = 0; pass < 10 && nlabcnt > 0; pass++) {
        resolved = false;

        for (i = 0; i < nlabcnt; i++) {
            if (findlabel(nlab[i])) {
                int saved_codpos = codpos;
                int saved_opp = opp;
                char saved_op[256], saved_params[256], saved_param1[256], saved_param2[256];

                // Salva stato corrente
                strcpy(saved_op, op);
                strcpy(saved_params, params);
                strcpy(saved_param1, param1);
                strcpy(saved_param2, param2);

                // Riposiziona il codice alla posizione dell'etichetta non risolta
                codpos = nlabpos[i];

                // Riesegui l'assemblaggio dell'istruzione con l'etichetta ora risolta
                extractop(nlabasm[i]);

                if (findop(op)) {
                    // Sostituisci il placeholder NOP con l'istruzione vera
                    if (in_set(opp, JR, 11)) {
                        // Salto relativo - usa posizioni relative
                        int dest = labpos[labp];  // Posizione relativa dell'etichetta
                        int current = codpos + 2; // Posizione corrente + dimensione istruzione
                        int offset = dest - current; // Calcolo offset relativo
                        code[codpos] = opp;
                        code[codpos + 1] = offset & 0xFF;
                    } else if (opp == 120 || opp == 121 || opp == 16 || (opp >= 124 && opp <= 127)) {
                        // Salto assoluto
                        int dest = labpos[labp] + startadr;
                        code[codpos] = opp;
                        code[codpos + 1] = dest / 256;
                        code[codpos + 2] = dest % 256;
                    }
                }

                // Ripristina stato
                codpos = saved_codpos;
                opp = saved_opp;
                strcpy(op, saved_op);
                strcpy(params, saved_params);
                strcpy(param1, saved_param1);
                strcpy(param2, saved_param2);

                printf("Risolta etichetta: %s -> %d\n", nlab[i], labpos[labp] + startadr);

                // Rimuovi l'etichetta non risolta dalla lista
                delnlabel(i);
                i--; // Decrementa per compensare la rimozione
                resolved = true;
            }
        }

        if (!resolved) break; // Nessuna etichetta risolta in questo passaggio
    }

    if (nlabcnt > 0) {
        printf("Warning: %d etichette non risolte:\n", nlabcnt);
        for (i = 0; i < nlabcnt; i++) {
            printf("  %s (linea con istruzione: %s)\n", nlab[i], nlabasm[i]);
        }
    }

    // Scrivi il file binario
    fout = fopen(output_filename, "wb");
    if (!fout) {
        perror("Errore creazione file output");
        return;
    }

    // Scrivi tutti i byte del codice generato
    size_t written = fwrite(code, 1, codpos, fout);
    fclose(fout);

    printf("File binario scritto: %s (%zu byte)\n", output_filename, written);
    printf("Indirizzo di partenza: %d (0x%X)\n", startadr, startadr);
    printf("Dimensione codice: %d byte\n", codpos);

    // Stampa riepilogo simboli e etichette
    if (symcnt > 0) {
        printf("\nSimboli definiti:\n");
        for (i = 0; i < symcnt; i++) {
            printf("  %s = %s\n", sym[i], symval[i]);
        }
    }

    if (labcnt > 0) {
        printf("\nEtichette definite:\n");
        for (i = 0; i < labcnt; i++) {
            printf("  %s: %d (0x%X)\n", lab[i], labpos[i] + startadr, labpos[i] + startadr);
        }
    }
}

// Funzione main di esempio (da estendere per lettura file)
int main(int argc, char *argv[]) {
    char line[1024];
    FILE *fin;
    if (argc < 3) {
        printf("Uso: %s <file.asm> <file.bin>\n", argv[0]);
        return 1;
    }
    fin = fopen(argv[1], "r");
    if (!fin) {
        perror("Errore apertura file sorgente");
        return 1;
    }
    cline = 0;
    while (fgets(line, sizeof(line), fin)) {
        cline++;
        // Rimuovi newline
        line[strcspn(line, "\r\n")] = 0;
        doasm(line);
    }
    fclose(fin);
    printf("Assemblaggio completato. Byte generati: %d\n", codpos);
    resolve_labels_and_write_bin(argv[2]);
    return 0;
}
