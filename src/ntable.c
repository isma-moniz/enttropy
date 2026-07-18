#include "ntable.h"

#include <stdlib.h>
#include <stdint.h>

typedef struct {
	uint64_t key;
	void* value;
} nt_entry;

struct nt {
	nt_entry* entries;
	size_t capacity;
	size_t length;
};

#define INITIAL_CAPACITY 16
#define SPLITMIX_CONST1 0x9e3779b97f4a7c15UL
#define SPLITMIX_CONST2 0xbf58476d1ce4e5b9UL
#define SPLITMIX_CONST3 0x94d049bb133111ebUL

// Simple SplitMix64 pseudo-random generation from the provided value.
static uint64_t hash_key(uint64_t key) {
	uint64_t hash = (key += SPLITMIX_CONST1);
	hash = (hash ^ (hash >> 30)) * SPLITMIX_CONST2;
	hash = (hash ^ (hash >> 27)) * SPLITMIX_CONST3;
	return hash ^ (hash >> 31);
}

ntable* nt_create(void) {
	ntable* table = malloc(sizeof(ntable));
	if (table == NULL) {
		return NULL;
	}

	table->length = 0;
	table->capacity = INITIAL_CAPACITY;

	table->entries = calloc(table->capacity, sizeof(nt_entry));
	if (table->entries == NULL) {
		free(table);
		return NULL;
	}
	return table;
}

void nt_destroy(ntable* table) {
	free(table->entries);
	free(table);
}

void* nt_get(ntable* table, uint64_t key) {
	uint64_t hash = hash_key(key);
	size_t index = (size_t)(hash & (uint64_t)(table->capacity - 1));

	while (table->entries[index].key != 0) { // (NULL)
		if (key == table->entries[index].key) {
			return table->entries[index].value;
		}

		index++;
		if (index >= table->capacity) {
			index = 0;
		}
	}
	return NULL;
}

static uint64_t nt_set_entry (nt_entry* entries, size_t capacity,
		uint64_t key, void* value, size_t* plength) {
	
	uint64_t hash = hash_key(key);
	size_t index = (size_t)(hash & (uint64_t)(capacity - 1));

	while (entries[index].key != 0) {
		if (key == entries[index].key) {
			entries[index].value = value;
			return entries[index].key;
		}

		index++;
		if (index >= capacity) {
			index = 0;
		}
	}

	if (plength != NULL) {
		(*plength)++;
	}
	entries[index].key = key;
	entries[index].value = value;
	return key;
}

/*
 * Expand hash table to twice its current size. Return true on success,
 * false if out of memory. This function is costly, hence why its better to
 * pre-reserve large amounts of memory: this will get called less that way.
 */
static bool nt_expand(ntable* table) {
	size_t new_capacity = table->capacity * 2;
	if (new_capacity < table->capacity) {
		return false;
	}

	nt_entry* new_entries = calloc(new_capacity, sizeof(nt_entry));
	if (new_entries == NULL) {
		return false;
	}

	for (size_t i = 0; i < table->capacity; i++) {
		nt_entry entry = table->entries[i];
		if (entry.key != 0) {
			nt_set_entry(new_entries, new_capacity, 
					entry.key, entry.value, NULL);
		}
	}

	free(table->entries);
	table->entries = new_entries;
	table->capacity = new_capacity;
	return true;
}

uint64_t nt_set(ntable* table, uint64_t key, void* value) {
	if (value == NULL) {
		return NULL;
	}

	if (table->length >= table->capacity / 2) {
		if (!nt_expand(table)) {
			return NULL;
		}
	}

	return nt_set_entry(table->entries, table->capacity, 
			key, value, &table->length);
}

size_t nt_length(ntable* table) {
	return table->length;
}

nti nt_iterator(ntable* table) {
	nti it;
	it._table = table;
	it._index = 0;
	return it;
}

bool nt_next(nti* it) {
	ntable* table = it->_table;
	while (it->_index < table->capacity) {
		size_t i = it->_index++;
		if (table->entries[i].key != 0) {
			nt_entry entry = table->entries[i];
			it->key = entry.key;
			it->value = entry.value;
			return true;
		}
	}
	return false;
}
