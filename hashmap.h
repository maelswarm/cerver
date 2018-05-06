
typedef unsigned (*HFUNC)(char *);

typedef struct hmap *HMAP_PTR;


#define NAIVE_HFUNC 0
#define BASIC_WEIGHTED_HFUNC 1

#define DEFAULT_HFUNC_ID BASIC_WEIGHTED_HFUNC

#define TSIZE_PWR2 0
#define TSIZE_PRIME 1
#define TSIZE_PRIME_P1 2

#define DEFAULT_TSIZE_POLICY TSIZE_PWR2

#define DEFAULT_INIT_SIZE 128

#define DEFAULT_LFACTOR (0.75)


/**
* Creates and initializes a hash map data structure with
*   given initial table size and specified load factor.
*
* \param init_tsize specifies the desired initial 
*    table size.  If zero is passed, a default table
*    size is used.
*
* \param lfactor specifies the desired maximum load factor;
*    if zero or a negative number is passed, a default 
*    load factor is used.
* \returns HMAP_PTR giving a handle to an initialized 
*    empty hash map.
*/
extern HMAP_PTR hmap_create(unsigned init_tsize, double lfactor);

/**
* same as hmap_create except allows caller to specify
*   a "table size policy" which is one of the following:
*
*        TSIZE_PWR2     table size is power-of-2
*        TSIZE_PRIME    table size is a prime roughly
*                       half between two adjacent
*                       powers of two.
*        TSIZE_PRIME_P1 table size is 1 + (a prime
*                       roughly half between two 
*                       adjacent powers of two)
*
*   TSIZE_PRIME_P1 exists primarily for experimental
*   purposes -- e.g., vs the prime table size policy.
*      
*/
extern HMAP_PTR hmap_create_size_policy(unsigned init_tsize, double lfactor, int tsize_mode);

/**
* \returns number of distinct keys in the map
*/
extern int hmap_size(HMAP_PTR map);

/**
* prints a crude ascii profile of the distribution
*   of keys to table entries.
*   Shows collisions.
*/
extern void hmap_display(HMAP_PTR map); 

/**
* sets hash function as specified by hfunc_id to one
*   of the pre-defined hash functions.
* if table not empty, operation fails and returns 0.
* See #defines for NAIVE_HFUNC and BASIC_WEIGHTED_HFUNC 
*   for example
*/
extern int hmap_set_hfunc(HMAP_PTR map, int hfunc_id); 

/**
* sets hash function to user-specified hfunc if table 
*    empty (and returns 1).
* if table non-empty, hash function is unchanged and
*    zero is returned.
* \param desc is a string descibing the hash function.
*/
extern int hmap_set_user_hfunc(HMAP_PTR map, HFUNC hfunc, char *desc);


/**
* determines if the given key is in the table and returns
*   0/1 accordingly.
* does not do anything with the value field.
*/
extern int hmap_contains(HMAP_PTR map, char *key);


/**
* if given key is in table, corresponding value is returned.
* If not, NULL is returned.
*
* Note:  key could be in the table with NULL as its 
*    corresponding value (as specified by the client).
*    In this case, the caller cannot distinguish between 
*    a NULL value and the key not being in the table.
*
*    hmap_contains should be used in such cases
*/
extern void *hmap_get(HMAP_PTR map, char *key);

/**
* sets the value associated with key to the given value
*   (val).
*
* If key not already in the table, a new entry is
*   created.  
* \returns previous value associated with key (if any);
*   NULL if key not previously present.
*/
extern void * hmap_set(HMAP_PTR map, char *key, void *val);

/**
* inserts the given key into the hmap.  Associated
*   value set to NULL.
*
* if key already in table, no modifications occur.
*
* equivalent to hmap_set(map, key, NULL) hmap_insert does
*   not return anything.
*
* Exists for convenience / clarity in applications using
*   hmap for set representation.
*
*/
extern void hmap_insert(HMAP_PTR map, char *key);


/**
* Removes entry corresponding to given key (if any).
* \returns previously associated value (if key not
*    in table, NULL is returned.)
*/
extern void *hmap_remove(HMAP_PTR map, char *key); 


/**
* Prints statistical information about the map.
*
* currently unimplemented
*/
extern void hmap_print_stats(HMAP_PTR map); 

/**
* Returns array of all n values in hmap (each value as a void *)
*
*/
extern void ** hmap_extract_values(HMAP_PTR map); 


/**
* Deallocates all memory internally allocated for the map
*
* If free_vals_flag is true, the associated values are also
*   deallocated (e.g., to save the client the trouble
*   of deallocating them).  If client did not dynamically
*   allocate values, then free_vals_flag should always be
*   false!
*
*/
extern void hmap_free(HMAP_PTR map, int free_vals_flag); 




