#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define DEBUG
#ifdef DEBUG 
#define DBPRINT printf
#else 
void donothing(void *v, ...) {}
#define DBPRINT donothing 
#endif 

typedef struct forth_call_t forth_call_t;
typedef struct forth_dict_entry_t forth_dict_entry_t;
typedef struct forth_state_t forth_state_t;
typedef void (*forth_primitive_t)(forth_state_t*);
struct forth_dict_entry_t {
    char *name; 
    unsigned int len; // Length of defintion, or 0 if primitive
    forth_call_t *def; // List of calls
    char immediate; // Immediacy flag 
    forth_dict_entry_t *prev; // Linked list pointer
};
enum forth_calltype {
    PRIMITIVE, // forth_primitive_t
    DICT_ENTRY, // forth_dict_entry_t
    NUMBER, // double
    ERROR, // error
};
struct forth_call_t {
    enum forth_calltype type; // Defines pointer type of *def
    void *def;
    char immediate; 
};
// TODO: integrate the above structure in the code below
// TODO: split usage of "forth_call_t" into "forth_dict_type" and "forth_call_t" 

#define FORTH_DICT_ENTRY_EMPTY \
    (forth_dict_entry_t){.name=NULL, .len=0, .def=NULL, .immediate=0, .prev=NULL};
    
struct forth_state_t {
    char *word; // Word entry buffer
    unsigned int compile; // Compile flag
    forth_dict_entry_t *dbuff; // Compile buffer
    
    forth_call_t *call; // Call stack
    unsigned int ncall;
    
    forth_dict_entry_t *dict; // Dictionary
    forth_dict_entry_t *dict_latest; 
    forth_dict_entry_t *dsp; // Dictionary search pointer (null iff not searching)
     
    double *data; // Data stack
    unsigned int ndata;
};

void forth_search_dict(forth_state_t* s);
static forth_call_t forth_call_search_dict = (forth_call_t) {.type = PRIMITIVE, .def = &forth_search_dict};

void forth_cpush(forth_state_t *s, forth_call_t c) { // Push to call stack
    s->call = realloc(s->call, (++s->ncall)*sizeof(forth_call_t*));
    s->call[s->ncall-1] = c;
} 
forth_call_t forth_cpop(forth_state_t *s) { 
    if(0 == s->ncall) { printf("Call stack underflow\n"); return (forth_call_t) {ERROR,NULL}; }
    forth_call_t c = s->call[--s->ncall];
    s->call = realloc(s->call, (s->ncall)*sizeof(forth_call_t));
    return c;
}
void forth_push(forth_state_t *s, double d) { // todo: make it take a forth_call_t 
    s->data = realloc(s->data, (++s->ndata)*sizeof(double));
    s->data[s->ndata-1] = d;
}
double forth_pop(forth_state_t *s) {
    if(0 == s->ndata) { printf("Data stack underflow\n"); return -1; }
    double d = s->data[s->ndata-1];
    s->data = realloc(s->data, (--s->ndata)*sizeof(double));
    return d;
}
void forth_call(forth_state_t *s, forth_call_t c) {
    switch(c.type) {
        case NUMBER: forth_push(s, *((double *) c.def)); break; // push to stack
        case PRIMITIVE: ((forth_primitive_t) (c.def))(s); break; // run primitive
        case DICT_ENTRY: // expand dictionary entry
            s->call = realloc(s->call, (
                s->ncall + ((forth_dict_entry_t*) c.def)->len)*sizeof(forth_call_t*));
            memcpy(&s->call[s->ncall], 
                ((forth_dict_entry_t*) c.def)->def, 
                ((forth_dict_entry_t*) c.def)->len*sizeof(forth_call_t*));
            s->ncall += ((forth_dict_entry_t*) c.def)->len;
            break;
    }
}
void forth_colon(forth_state_t *s) { s->compile = 1; }
void forth_semicolon(forth_state_t *s) { 
    if(s->compile = 1 && s->dbuff->name) { // Add to dictionary
        s->dbuff->prev = s->dict_latest;
        s->dict_latest = s->dbuff;
        *(s->dbuff) = FORTH_DICT_ENTRY_EMPTY;
    }
    s->compile = 0; 
}
void forth_toggle_immediate(forth_state_t *s) { s->dict_latest->immediate = !(s->dict_latest->immediate);}
void forth_search_dict(forth_state_t *s) {
    if(!(s->dsp)) { s->dsp = s->dict_latest; // Starting search
        DBPRINT("SEARCH: Started search at entry %s\n", s->dsp->name);
    }
    if(0 == strcmp(s->dsp->name, s->word)) { // This is the right entry
        DBPRINT("SEARCH: Matched with name %s\n", s->word);
        forth_cpush(s, (forth_call_t){.type=DICT_ENTRY, .def=s->dsp});
        s->dsp = NULL; // Reset for next search
        s->word = realloc(s->word,0);
    } else if(s->dsp->prev) { // Keep looking...
        DBPRINT("SEARCH: Did not match \"%s\" with dict name \"%s\"\n", s->word, s->dsp->name);
        s->dsp = s->dsp->prev; 
        forth_cpush(s, forth_call_search_dict);
    } else { // Search failed. Maybe it's a number...
        DBPRINT("SEARCH: Exhausted dictionary.\n");
        char *endptr;
        double d = strtod(s->word, &endptr);
        if(endptr != s->word) forth_push(s, d);
        else printf("SEARCH: Bad command\n");
        s->dsp = NULL; // Reset for next search
        s->word = realloc(s->word,0);
    }
}

void forth_next(forth_state_t *s) {     
    DBPRINT("NEXT: Call stack size: %u (compile %u, dbuff->name %p)\n", 
        s->ncall, s->compile, s->dbuff->name);
    if(s->word && !(s->dsp)) { // Handle word
        DBPRINT("NEXT: Handle word:%s\n", s->word);
        if(s->compile && !(s->dbuff->name)) { // Compile name
            DBPRINT("NEXT: Compile dictionary word: %s\n", s->word);
            s->dbuff->name = s->word;
            s->word = malloc(0);
            DBPRINT("NEXT: New dictionary word: %s\n", s->dbuff->name);
        } else forth_call(s, forth_call_search_dict); 
    } else { // Handle stack
        forth_call_t c = forth_cpop(s);
        DBPRINT("NEXT: Handle call of type %c\n", 
            c.type);
        if(s->compile && !(s->call[s->ncall-1].immediate)) { // Compile def
            s->dbuff->def = realloc(s->dbuff->def, (++s->dbuff->len)*sizeof(forth_call_t*));
            s->dbuff->def[s->dbuff->len-1] = c;
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

#define FORTH_PRIMITIVE(x) &((forth_call_t){.type=PRIMITIVE, .def=&x});
static forth_dict_entry_t forth_initial_dict[] = { 
    {.name="br"   , .len=1, .def=FORTH_PRIM(forth_branch)   , .immediate=0, .prev=NULL}, 
    {.name=":"    , .len=1, .def=FORTH_PRIM(forth_colon)    , .immediate=0, .prev=NULL},  
    {.name=";"    , .len=1, .def=FORTH_PRIM(forth_semicolon), .immediate=1, .prev=NULL}, 
    {.name="print", .len=1, .def=FORTH_PRIM(forth_print)    , .immediate=0, .prev=NULL}, 
    {.name="+"    , .len=1, .def=FORTH_PRIM(forth_plus)     , .immediate=0, .prev=NULL}
};
#define N_INITIAL_D sizeof(forth_initial_dict)/sizeof(forth_call_t)

forth_state_t* forth_init(void) {
    forth_state_t *s = malloc(sizeof(forth_state_t)); 
    s->word = NULL; 
    
    s->dict = malloc(sizeof(forth_initial_dict));
    memcpy(s->dict, forth_initial_dict, sizeof(forth_initial_dict)); 
    for(unsigned int idx = 1; idx < N_INITIAL_D; idx++) 
        s->dict[idx].prev = &s->dict[idx-1];
    s->dict_latest = &s->dict[N_INITIAL_D-1];
    s->dsp = NULL;
    
    s->compile = 0;
    s->dbuff = malloc(sizeof(forth_call_t));
    *(s->dbuff) = FORTH_DICT_ENTRY_EMPTY;
    
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
    
    s->dsp = s->dict_latest;
    free(s->dict); 
    free(s->word);
    free(s->data);
    free(s->call);
    free(s);
    return 0;
}
