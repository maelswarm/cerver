#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashmap.h"


unsigned Coeff1 = 27;

int Pwrs_2[] = {
	16, 32, 64, 128, 256,
	512, 1024, 2048, 4096, 8192,
	16384, 32768, 65536, 131072, 262144,
	524288, 1048576, 2097152, 4194304, 8388608,
	16777216, 33554432, 67108864, 134217728, 268435456,
	536870912, 1073741824, 2147483648
};

int Primes[] = {
	53, 97, 193, 389, 769,
	1543, 3079, 6151, 12289, 24593,
	49157, 98317, 196613, 393241, 786433,
	1572869, 3145739, 6291469, 12582917, 25165843,
	50331653, 100663319, 201326611, 402653189, 805306457,
	1610612741};

int Primes_P1[] = {
	53+1, 97+1, 193+1, 389+1, 769+1,
	1543+1, 3079+1, 6151+1, 12289+1, 24593+1,
	49157+1, 98317+1, 196613+1, 393241+1, 786433+1,
	1572869+1, 3145739+1, 6291469+1, 12582917+1, 25165843+1,
	50331653+1, 100663319+1, 201326611+1, 402653189+1, 805306457+1,
	1610612741+1};



int *SizeTables[] = {Pwrs_2, Primes, Primes_P1};

char *SizePolicies[] = {"POWERS-OF-TWO", "PRIMES", "PRIMES_PLUS_1"};


/******** STRUCTS AND TYPEDEFS *********/

typedef struct node_struct {
    struct node_struct *next;
    char *key;
    void *val;
    unsigned hval;
}NODE;

typedef struct {
    NODE *members;
    int n;
} TBL_ENTRY;

struct hmap {
    TBL_ENTRY *tbl;
    int *tsize_tbl;
    char *tsize_policy;
    int tsize_idx;
    int tsize;
    int n; 
    double lfactor;
    int max_n;
    HFUNC hfunc;
    char *hfunc_desc;
};

typedef struct {
	HFUNC hfunc;
	char *description;
} HFUNC_STRUCT;

/******** END STRUCTS AND TYPEDEFS *********/

/******** LIBRARY SUPPLIED HASH FUNCTIONS *****/

static unsigned h0(char *s) {
unsigned h = 0;

  while(*s != '\0'){
	h += *s;
	s++;
  }
  return h;
}


static unsigned int h1(char *s) {
unsigned h=0;

  while(*s != '\0') {
	h = h*Coeff1 + *s;
	s++;
  }
  return h;
}
/******** END LIBRARY SUPPLIED HASH FUNCTIONS *****/



/***** GLOBALS TO THIS FILE ******/

static HFUNC_STRUCT HashFunctions[] = 
	{ 
		{h0, "naive char sum"},
		{h1, "weighted char sum"}
	};


static int NumHFuncs = sizeof(HashFunctions)/sizeof(HFUNC_STRUCT);

/***** END GLOBALS ******/

/***** FORWARD DECLARATIONS *****/
static int match(char *key, unsigned hval, NODE *p); 
static NODE **get_node_pred(HMAP_PTR map, char *key); 
static TBL_ENTRY *create_tbl_array(int tsize); 
static void resize(HMAP_PTR map); 
static void free_lst(NODE *l, int free_vals); 
static void add_front(TBL_ENTRY *entry, NODE *p);
/***** END FORWARD DECLARATIONS *****/


/***** BEGIN hmap FUNCTIONS ******/


HMAP_PTR hmap_create(unsigned init_tsize, double lfactor){

  return hmap_create_size_policy(init_tsize, lfactor,
				 DEFAULT_TSIZE_POLICY);

}

HMAP_PTR hmap_create_size_policy(unsigned init_tsize, 
			double lfactor, int tsize_mode){
HMAP_PTR map = malloc(sizeof(struct hmap));
int idx;

  map->n = 0;
  if(lfactor <= 0)
	lfactor = DEFAULT_LFACTOR;
  if(init_tsize <= 0)
	init_tsize = DEFAULT_INIT_SIZE;
  
  map->lfactor = lfactor;

  map->tsize_tbl = SizeTables[tsize_mode];
  map->tsize_policy = SizePolicies[tsize_mode];
  idx = 0;
  while(map->tsize_tbl[idx] < init_tsize)
	idx++;

  map->tsize = map->tsize_tbl[idx];
  map->tsize_idx = idx;
  map->max_n = map->tsize * lfactor;

  map->hfunc = HashFunctions[DEFAULT_HFUNC_ID].hfunc;
  map->hfunc_desc = HashFunctions[DEFAULT_HFUNC_ID].description;

  map->tbl = create_tbl_array(map->tsize);

  map->n = 0;
 
  return map;
}

void hmap_set_coeff(unsigned new_coeff){

  Coeff1 = new_coeff;

}
  

int hmap_size(HMAP_PTR map) {
  return map->n;
}

void hmap_display(HMAP_PTR map) {
int i, j;
  
  for(i=0; i<map->tsize; i++) {
      printf("|-|");
      for(j=0; j<map->tbl[i].n; j++) 
	  printf("X");
      printf("\n");
  }
}
int hmap_set_hfunc(HMAP_PTR map, int hfunc_id) {
  if(map->n > 0) {
	fprintf(stderr, 
	  "warning:  attempt to change hash function on non-empty table\n");
	return 0;
  }
  if(hfunc_id < 0 || hfunc_id >= NumHFuncs) {
	fprintf(stderr, 
	  "warning:  invalid hash function id %i\n", hfunc_id);
	return 0;
  }
  map->hfunc = HashFunctions[hfunc_id].hfunc;
  map->hfunc_desc = HashFunctions[hfunc_id].description;
  return 1;
}

int hmap_set_user_hfunc(HMAP_PTR map, HFUNC hfunc, char *desc) {
  if(map->n > 0) {
	fprintf(stderr, 
	  "warning:  attempt to change hash function on non-empty table\n");
	return 0;
  }
  map->hfunc = hfunc;
  if(desc == NULL)
    map->hfunc_desc = "user-supplied hash function";
  else 
    map->hfunc_desc = desc;
  return 1;
}



int hmap_contains(HMAP_PTR map, char *key) {
NODE **pp;
  pp = get_node_pred(map, key);
  return (*pp == NULL ? 0 : 1);
}

void *hmap_get(HMAP_PTR map, char *key) {
NODE **pp, *tmp;
  pp = get_node_pred(map, key);
  
    if (*pp!=NULL) {
        tmp = *pp;
        while (tmp->next!=NULL) {
            tmp = tmp->next;
            if (strlen(tmp->val)==strlen(key)) {
            }
        }
    }
    
  return (*pp == NULL ? NULL : (*pp)->val);
}

  

	

  

void * hmap_set(HMAP_PTR map, char *key, void *val){
unsigned h;
int idx;
NODE *p, **pp;

  pp = get_node_pred(map, key);
  p = *pp;

  if(p == NULL) {  // key not present
     char *key_clone;
     char *val_clone;

     key_clone = malloc( (strlen(key) + 1)*sizeof(char));
     strcpy(key_clone, key);
      
     val_clone = malloc((strlen(val) + 1)*sizeof(char));
     strcpy(val_clone, val);

     map->n++;
     if(map->n > map->max_n) 
	resize(map);
     h = map->hfunc(key);
     idx = h % map->tsize;

     p = malloc(sizeof(NODE));

     p->key = key_clone;
     p->val = val_clone;
     p->hval = h;

     add_front(&(map->tbl[idx]), p);
     return NULL;
  }
  else {
      char *key_clone;
      char *val_clone;
      
      key_clone = malloc( (strlen(key) + 1)*sizeof(char));
      strcpy(key_clone, key);
      
      val_clone = malloc((strlen(val) + 1)*sizeof(char));
      strcpy(val_clone, val);
      
      map->n++;
      if(map->n > map->max_n)
          resize(map);
      h = map->hfunc(key);
      idx = h % map->tsize;
      
      p = malloc(sizeof(NODE));
      
      p->key = key_clone;
      p->val = val_clone;
      p->hval = h;
      
      add_front(&(map->tbl[idx]), p);
      return NULL;
  }
}

void hmap_insert(HMAP_PTR map, char *key) {

  if(hmap_contains(map, key))
	return;
  hmap_set(map, key, NULL);

}

void *hmap_remove(HMAP_PTR map, char *key) {
NODE *p, **pp;

  pp = get_node_pred(map, key);
  p = *pp;
  if(p == NULL){
	return NULL;
  }
  else {
	unsigned idx;
	void *val = p->val;

	*pp = p->next;
	free(p->key);
	free(p);

  	idx = (p->hval) % map->tsize;
	map->tbl[idx].n--;
	map->n--;
	return val;
  }
}
  

static int max_len(HMAP_PTR map) {
int i;
int max = 0;

  for(i=0; i<map->tsize; i++) 
	if(map->tbl[i].n > max)
		max = map->tbl[i].n;
  
  return max;
}

static void histogram(HMAP_PTR map) {
int max = max_len(map);
int cutoff;
int freq[max+1];
int i, len ;

  for(i=0; i<=max; i++)
    freq[i]=0;

  for(i=0; i<map->tsize; i++) {
	len = map->tbl[i].n;
	freq[len]++;
  }
  printf("\n\n   DISTRIBUTION OF LIST LENGTHS\n\n");

  printf("NBUCKETS:   ");
  for(len=0; len <= max; len++) {
	printf("%7i",  freq[len]);
  }
  printf("\n");
  printf("------------");
  for(len=0; len <= max; len++) {
	printf("-------");
  }
  printf("\n");
  printf("LIST-LEN:   ");
  for(len=0; len <= max; len++) {
	printf("%7i",  len);
  }
  printf("\n\n");
  
}

static double avg_cmps(HMAP_PTR map) {
int i;
double total = 0;

  for(i=0; i<map->tsize; i++) {
	int ni = map->tbl[i].n;
	total += (ni*(ni+1))/2;
  }
  return total/map->n;
}

void hmap_print_stats(HMAP_PTR map) {


    printf("######## TABLE STATS ##########\n");

    printf("   hash-func:             %s \n", map->hfunc_desc);
    printf("   tsize-policy:          %s \n", map->tsize_policy);
    printf("   tblsize:               %i \n", map->tsize);
    printf("   numkeys:               %i \n", map->n);
    printf("   max-collisions:        %i \n", max_len(map));
    printf("   avg-cmps-good-lookup:  %f \n", avg_cmps(map));

    histogram(map);

    printf("###### END TABLE STATS ##########\n");

}

void ** hmap_extract_values(HMAP_PTR map) {
int i, k;
NODE *p;
void **values = malloc(map->n * (sizeof(void *)));

  k=0;
  for(i=0; i<map->tsize; i++) {
	p = map->tbl[i].members;
	while(p != NULL) {
		values[k] = p->val;
		k++;
		p = p->next;
	}
  }
  return values;
}

void hmap_free(HMAP_PTR map, int free_vals) {
int i;

  for(i=0; i<map->tsize; i++) 
	free_lst(map->tbl[i].members, free_vals);
  free(map->tbl);
  map->tbl = NULL;
  free(map);
}


/**** UTILITY FUNCTIONS *******/

static int match(char *key, unsigned hval, NODE *p) {
  return (p->hval == hval && strcmp(key, p->key)==0);
}

static NODE **get_node_pred(HMAP_PTR map, char *key) {
unsigned h;
int i;
NODE **pp;
  h = map->hfunc(key);
  i = h % map->tsize;

  pp =&(map->tbl[i].members); 
  while( *pp != NULL) {
      if(match(key, h, *pp)) {
          return pp;
      }
    pp = &((*pp)->next);
  }
  return pp;
}

static void add_front(TBL_ENTRY *entry, NODE *p) {
  entry->n++;
  p->next = entry->members;
  entry->members = p;
}

static TBL_ENTRY *create_tbl_array(int tsize) {
int i;
TBL_ENTRY *tbl;
NODE *p;

  tbl = malloc(tsize * sizeof(TBL_ENTRY));
  for(i=0; i<tsize; i++) {
	tbl[i].members = NULL;
	tbl[i].n = 0;
  }
  return tbl;
}

static void resize(HMAP_PTR map) {
int ntsize;
TBL_ENTRY *ntbl;
NODE *nxt, *p;
unsigned h;
int i, idx;

  map->tsize_idx++;
  ntsize = map->tsize_tbl[map->tsize_idx];
  ntbl = create_tbl_array(ntsize);

  for(i=0; i<map->tsize; i++) {
    p = map->tbl[i].members;
    while(p != NULL) {
	nxt = p->next;
  	idx = p->hval % ntsize;
	add_front(&ntbl[idx], p);
	p = nxt;
    }
  }
  free(map->tbl);
  map->tbl = ntbl;
  map->tsize = ntsize;
  map->max_n = (int)(ntsize * map->lfactor);

}
static void free_lst(NODE *l, int free_vals) {
  if(l == NULL) return;
  free_lst(l->next, free_vals );
  free(l->key);
  if(free_vals &&  l->val != NULL)
	free(l->val);
  free(l);
}
/**** END UTILITY FUNCTIONS *******/

