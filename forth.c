#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG 
#define DBPRINT printf
#else 
void donothing(void *v, ...) {}
#define DBPRINT donothing 
#endif 

typedef struct forth_call_t forth_call_t;
typedef struct forth_state_t forth_state_t;
struct forth_call_t {
    char *name; 
    unsigned int len; // Length of defintion, or 0 if primitive
    void (*primitive)(forth_state_t *); // Function pointer, if primitive
    forth_call_t **def; // Definition
    char immediate; // Immediacy flag 
    forth_call_t *prev; // Linked list pointer
};
struct forth_state_t {
    char *word; // Word entry buffer
    unsigned int compile; // Compile flag
    forth_call_t cbuff; // Compile buffer
    
    forth_call_t *dict; // Dictionary
    forth_call_t *dict_latest; 
    forth_call_t *dsp; // Dictionary search pointer (null iff not searching)
    
    double *data; // Data stack
    unsigned int ndata;
    
    forth_call_t ** call; // Call stack
    unsigned int ncall; 
};
void forth_search_dict(forth_state_t* s);

void forth_call(forth_state_t *s, forth_call_t *c) {
    if(0 == c->len) { // is primitive
        c->primitive(s);
    } else { // is compound
        s->call = realloc(s->call, (s->ncall+c->len)*sizeof(forth_call_t*));
        memcpy(&s->call[s->ncall], c->def, c->len*sizeof(forth_call_t*));
        s->ncall += c->len;
    }
}
void forth_cpush(forth_state_t *s, forth_call_t *c) { // Push to call stack
    s->call = realloc(s->call, (++s->ncall)*sizeof(forth_call_t*));
    s->call[s->ncall-1] = c;
} 
forth_call_t * forth_cpop(forth_state_t *s) { 
    if(0 == s->ncall) { printf("Call stack underflow\n"); return NULL; }
    forth_call_t *c = s->call[--s->ncall];
    s->call = realloc(s->call, (s->ncall)*sizeof(forth_call_t*));
    return c;
}
void forth_push(forth_state_t *s, double d) {
    s->data = realloc(s->data, (++s->ndata)*sizeof(double));
    s->data[s->ndata-1] = d;
}
double forth_pop(forth_state_t *s) {
    if(0 == s->ndata) { printf("Data stack underflow\n"); return -1; }
    double d = s->data[s->ndata-1];
    s->data = realloc(s->data, (--s->ndata)*sizeof(double));
    return d;
}
void forth_colon(forth_state_t *s) { s->compile = 1; }
void forth_semicolon(forth_state_t *s) { 
    s->compile = 0; 
    // TODO: Add s->cbuff to dictionary
    // TODO: Update s->dict_latest
    // TODO: FREE s.cbuff string memory!!!
    s->cbuff = (forth_call_t) {NULL, 0, NULL, NULL, 0};
}
void forth_toggle_immediate(forth_state_t *s) { s->dict_latest->immediate = !(s->dict_latest->immediate);}
void forth_search_dict(forth_state_t *s) {
    DBPRINT("search_dict\n");
    if(!(s->dsp)) { // Starting search
        s->dsp = s->dict_latest;
        DBPRINT("\tStarted search at entry %s\n", s->dsp->name);
    }
    if(0 == strcmp(s->dsp->name, s->word)) { // This is the right entry
        DBPRINT("\tMatched with name %s\n", s->word);
        forth_cpush(s, s->dsp);
        s->dsp = NULL; // Reset for next search
        s->word = realloc(s->word,1);
        s->word[0] = '\0';
    } else if(s->dsp->prev) { // Keep looking...
        DBPRINT("\tDid not match %s with name %s\n", s->word, s->dsp->name);
        s->dsp = s->dsp->prev; 
        forth_cpush(s, &forth_call_search_dict); 
    } else { // Search failed. Maybe it's a number...
        DBPRINT("\tExhausted dictionary.\n");
        char *endptr;
        double d = strtod(s->word, &endptr);
        if(endptr != s->word) forth_push(s, d);
        else printf("Bad command\n");
        s->dsp = NULL; // Reset for next search
        s->word = realloc(s->word,1);
        s->word[0] = '\0';
    }
}

void forth_next(forth_state_t *s) {     
    DBPRINT("Next; Call stack size: %u (compile %u)\n\t", s->ncall, s->compile);
    if(s->word[0]) { // Handle word
        DBPRINT("Handle word:%s\n", s->word);
        if(s->compile && !(s->cbuff.name)) { // Compile name
            s->cbuff.name = s->word;
            s->word = malloc(0);
            DBPRINT("New dictionary word: %s\n", s->cbuff.name);
        } else if(!(s->dsp)) forth_call(s, &forth_call_search_dict); 
    } else { // Handle stack
        forth_call_t *c = forth_cpop(s);
        DBPRINT("Handle call: %s (length %u; immediate %u)\n", 
            c->name, c->len, c->immediate);
        if(s->compile && !(s->call[s->ncall-1]->immediate)) { // Compile def
            s->cbuff.def = realloc(s->cbuff.def, (++s->cbuff.len)*sizeof(forth_call_t*));
            s->cbuff.def[s->cbuff.len-1] = c;
        } else forth_call(s, c);
    }
}

void forth_branch(forth_state_t *s) { if(!(forth_pop(s) > 0)) forth_cpop(s); }
void forth_print(forth_state_t *s) {
    if(s->ndata <= 0) { printf("Data stack underflow\n"); return; }
    printf("%i: %f\n", s->ndata, s->data[s->ndata-1]);
}
void forth_plus(forth_state_t *s) {
    if(s->ndata <= 1) { printf("Data stack underflow\n"); return; }
    s->data[s->ndata-2] += s->data[s->ndata-1];
    s->data = realloc(s->data, (--s->ndata)*sizeof(double));
}

static forth_call_t forth_initial_dict[] = { 
    {"br"   , 0, &forth_branch   , NULL, 0, NULL}, 
    {":"    , 0, &forth_colon    , NULL, 0, NULL},  
    {";"    , 0, &forth_semicolon, NULL, 1, NULL}, 
    {"print", 0, &forth_print    , NULL, 0, NULL}, 
    {"+"    , 0, &forth_plus     , NULL, 0, NULL}
};
#define N_INITIAL_D sizeof(forth_initial_dict)/sizeof(forth_call_t)

forth_state_t* forth_init(void) {
    forth_state_t *s = malloc(sizeof(forth_state_t)); 
    s->word = (char*){'\0'}; 
    
    s->dict = malloc(sizeof(forth_initial_dict));
    memcpy(s->dict, forth_initial_dict, sizeof(forth_initial_dict)); 
    for(unsigned int idx = 1; idx < N_INITIAL_D; idx++) 
        s->dict[idx].prev = &s->dict[idx-1];
    s->dict_latest = &s->dict[N_INITIAL_D-1];
    s->dsp = NULL;
    
    s->compile = 0;
    s->cbuff = (forth_call_t) {NULL, 0, NULL, NULL, 0};
    
    s->data = NULL;  
    s->ndata = 0; 
    
    s->call = NULL;  
    s->ncall = 0; 
    return s;
}

int main(void) {
    forth_state_t *s = forth_init();
    char c;
    unsigned int nword = 0;
    for(int ii = 0; ii < 50; ii++) {
        c = getchar();
        s->word = realloc(s->word, (++nword)*sizeof(char));
        s->word[nword-1] = c;
        if((nword > 0) && isspace(c)) { // Word is finished
            s->word[nword-1] = '\0';
            do forth_next(s); while(s->ncall > 0);
            s->word = realloc(s->word, 0); 
            nword = 0;
        }
    }
    free(s->dict); 
    free(s->word);
    free(s->data);
    free(s->call);
    free(s);
    return 0;
}
