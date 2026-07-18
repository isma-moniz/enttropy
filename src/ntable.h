#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// ntable is a specialized hash table for non-uniform uint64_t keys.
// NOTE: keys CANNOT be 0, as ntable relies on NULL detection

// linear search should be faster than hash table lookup with up to 7 items

typedef struct nt ntable;

// Create hash table and return pointer to it, or NULL if out of memory.
ntable *nt_create(void);

// Free memory allocated for hash table, including allocated keys.
void nt_destroy(ntable *table);

// Get item with given key from hash table.
// Returns NULL if key not found.
void *nt_get(ntable *table, uint64_t key);

// Set item with given key to value (value must not be NULL).
// If not already present in table, key is copied to newly allocated memory.
// Return address of key, or NULL if out of memory.
uint64_t nt_set(ntable *table, uint64_t key, void *value);

// Return number of items in hash table.
size_t nt_length(ntable *table);

// Hash table iterator: create with ht_iterator, iterate with ht_next.
typedef struct {
	uint64_t key;
	void *value;
	
	// Don't use these fields directly
	ntable* _table;
	size_t _index;
} nti;

// Return new hash table iterator for use with ht_next
nti nt_iterator(ntable *table);

// Move iterator to next item in hash table, update iterator's key
// and value to that item, and return true. If there are no more
// items, return false. Don't call ht_set during iteration.
bool nt_next(nti *it);
