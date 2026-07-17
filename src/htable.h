#pragma once
#include <stddef.h>
#include <stdbool.h>

// linear search should be faster than hash table lookup with up to 7 items

typedef struct ht htable;

// Create hash table and return pointer to it, or NULL if out of memory.
htable *ht_create(void);

// Free memory allocated for hash table, including allocated keys.
void ht_destroy(htable *table);

// Get item with given key from hash table.
// Returns NULL if key not found.
void *ht_get(htable *table, const char *key);

// Set item with given key to value (value must not be NULL).
// If not already present in table, key is copied to newly allocated memory.
// Return address of key, or NULL if out of memory.
const char *ht_set(htable *table, const char *key, void *value);

// Return number of items in hash table.
size_t ht_length(htable *table);

// Hash table iterator: create with ht_iterator, iterate with ht_next.
typedef struct {
	const char *key;
	void *value;
	
	// Don't use these fields directly
	htable* _table;
	size_t _index;
} hti;

// Return new hash table iterator for use with ht_next
hti ht_iterator(htable *table);

// Move iterator to next item in hash table, update iterator's key
// and value to that item, and return true. If there are no more
// items, return false. Don't call ht_set during iteration.
bool ht_next(hti *it);
