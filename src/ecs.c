#include "ecs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h> // why the FUCK are memset and memcpy here??
#include <stdarg.h>

int ecs_init(ecs_state_t* ecs_state, uint32_t component_count, ...) {
	// components
	if (!(ecs_state->stack = (void**)malloc(component_count * sizeof(void*)))) {
		printf("Error: could not allocate memory for ecs_state\n");
	}
	
	ecs_state->component_store.count = component_count;

	if (!(ecs_state->component_store.sizes = (uint32_t*)malloc(component_count * sizeof(uint32_t)))) {
		printf("Error: could not allocate memory for component sizes\n");
		free(ecs_state->stack);
		ecs_state->stack = NULL;
		return -1;
	}

	if (!(ecs_state->component_store.replicas = (uint32_t*)malloc(component_count * sizeof(uint32_t)))) {
		printf("Error: could not allocate memory for component replica numbers\n");
		free(ecs_state->component_store.sizes);
		free(ecs_state->stack);
		ecs_state->component_store.sizes = NULL;
		ecs_state->stack = NULL;
		return -1;
	}

	if (!(ecs_state->component_store.slots = (uint32_t*)malloc(component_count * sizeof(uint32_t)))) {
		printf("Error: could not allocate memory for component slot counts\n");
		free(ecs_state->component_store.sizes);
		free(ecs_state->component_store.replicas);
		free(ecs_state->stack);
		ecs_state->component_store.sizes = NULL;
		ecs_state->stack = NULL;
		ecs_state->component_store.replicas = NULL;
		return -1;
	}

	va_list args;
	va_start(args, component_count);
	for (uint32_t i = 0; i < component_count; i++) {
		uint32_t component_size = va_arg(args, uint32_t);
		ecs_state->component_store.sizes[i] = component_size;
		ecs_state->component_store.slots[i] = INITIAL_REPLICA_CAPACITY;
		ecs_state->component_store.replicas[i] = 0;
		//NOTE: prereserving the space might be an optimization to do less mallocs. profile and see.
		ecs_state->stack[i] = (void*)malloc(INITIAL_REPLICA_CAPACITY * component_size);
		if (!ecs_state->stack[i]) {
			printf("Error: could not allocate memory for replicas of component with id: %d\n", i);
			free(ecs_state->component_store.replicas);
			free(ecs_state->component_store.slots);
			free(ecs_state->component_store.sizes);
			for (int j = 0; j < i; j++) {
				free(ecs_state->stack[j]);
			}
			free(ecs_state->stack);
			ecs_state->component_store.replicas = NULL;
			ecs_state->component_store.sizes = NULL;
			ecs_state->component_store.slots = NULL;
			ecs_state->stack = NULL;
			return -1;
		}
	}

	va_end(args);
	return 0;
}

int ecs_init_entities(ecs_state_t* ecs_state) {
	// entity store
	ecs_state->entity_store.count = 0;
	ecs_state->entity_store.cap = INITIAL_ENTITY_CAPACITY;
	ecs_state->entity_store.component_masks = (uint32_t*)malloc(INITIAL_ENTITY_CAPACITY * sizeof(uint32_t));

	if (!ecs_state->entity_store.component_masks) {
		printf("Error: could not allocate memory for entity masks\n");
		return -1;
	}

	if (!(ecs_state->entity_store.entity_types = (entitytype_t*)malloc(INITIAL_ENTITY_CAPACITY * sizeof(entitytype_t)))) {
		printf("Error: could not allocate memory for entity types\n");
		free(ecs_state->entity_store.component_masks);
		ecs_state->entity_store.component_masks = NULL;
		return -1;
	}

	if (!(ecs_state->entity_store.components = (comp_ptr**)malloc(INITIAL_ENTITY_CAPACITY * sizeof(comp_ptr*)))) {
		printf("Error: could not allocate memory for component pointers\n");
		free(ecs_state->entity_store.component_masks);
		free(ecs_state->entity_store.entity_types);
		ecs_state->entity_store.component_masks = ecs_state->entity_store.entity_types = NULL;
		return -1;
	}

	for (int i = 0; i < INITIAL_ENTITY_CAPACITY; i++) {
		if (!(ecs_state->entity_store.components[i] = (comp_ptr*)malloc(INITIAL_ENTITY_COMPONENT_CAPACITY * sizeof(comp_ptr)))) {
			printf("Error: could not allocate memory for component pointers for entity %d\n", i);
			free(ecs_state->entity_store.component_masks);
			free(ecs_state->entity_store.entity_types);
			for (int j = 0; j < i; j++) {
				free(ecs_state->entity_store.components[j]);
			}
			free(ecs_state->entity_store.components);
			ecs_state->entity_store.component_masks = ecs_state->entity_store.entity_types = NULL;
			ecs_state->entity_store.components = NULL;
			return -1;
		}
	}

 	return 0;
}

void ecs_destroy(ecs_state_t* ecs_state) {
	if (ecs_state->component_store.sizes) {
		free(ecs_state->component_store.sizes);
		ecs_state->component_store.sizes = NULL;
	}
	if (ecs_state->component_store.replicas) {
		free(ecs_state->component_store.replicas);
		ecs_state->component_store.replicas = NULL;
	}
	if (ecs_state->component_store.slots) {
		free(ecs_state->component_store.slots);
		ecs_state->component_store.slots = NULL;
	}
	if (ecs_state->stack) {
		for (uint32_t i = 0; i < ecs_state->component_store.count; i++) {
			free(ecs_state->stack[i]);
			ecs_state->stack[i] = NULL;
		}
		ecs_state->component_store.count = 0;
		free(ecs_state->stack);
		ecs_state->stack = NULL;
	}
	if (ecs_state->entity_store.components) {
		for (uint32_t i = 0; i < ecs_state->entity_store.count; i++) {
			ecs_state->entity_store.used_comp_slots[i] = 0;
			ecs_state->entity_store.comp_slots_cap[i] = INITIAL_ENTITY_COMPONENT_CAPACITY;
			ecs_state->entity_store.components[i] = realloc(ecs_state->entity_store.components[i], INITIAL_ENTITY_COMPONENT_CAPACITY * sizeof (comp_ptr));
		}
	}
}

void ecs_destroy_entities(ecs_state_t* ecs_state) {
	ecs_state->entity_store.count = 0;
	ecs_state->entity_store.cap = 0;
	if (ecs_state->entity_store.component_masks) {
		free(ecs_state->entity_store.component_masks);
		ecs_state->entity_store.component_masks = NULL;
	}
	if (ecs_state->entity_store.entity_types) {
		free(ecs_state->entity_store.entity_types);
		ecs_state->entity_store.entity_types = NULL;
	}	
}

ent_id ecs_create_entity(ecs_state_t* ecs_state, entitytype_t ent_type) {
	//maybe user provided ids to leverage the enums would be better
	ent_id id = ecs_state->entity_store.count; 

	if (id == ecs_state->entity_store.cap) {
		printf("ECS: Max entities reached, reallocating\n");
		uint64_t new_size = AUGMENTATION_MULTIPLIER * ecs_state->entity_store.cap * sizeof(uint32_t);
		printf("ECS: New size cap: %u entities, %lu bytes\n", AUGMENTATION_MULTIPLIER * ecs_state->entity_store.cap, new_size);
		
		ecs_state->entity_store.component_masks = realloc(ecs_state->entity_store.component_masks, new_size);
		//WARNING: uncomment below if you decide to change entitytype_t to some other size, such as int
		ecs_state->entity_store.entity_types = realloc(ecs_state->entity_store.entity_types, new_size /* * (sizeof(entitytype_t)/sizeof(uint32_t))*/);
	
		if (!ecs_state->entity_store.component_masks || !ecs_state->entity_store.entity_types) {
			printf("Error: could not reallocate entity storage!\n");
			return -1;
		}

		ecs_state->entity_store.cap *= AUGMENTATION_MULTIPLIER;
	}

	ecs_state->entity_store.count++;
	ecs_state->entity_store.component_masks[id] = 0;
	ecs_state->entity_store.entity_types[id] = ent_type;

	// zero out all components for this entity, so we can know if this entity has them later
	// TODO: figure out a better solution lmao
	for (uint32_t i = 0; i < ecs_state->component_store.count; i++) {
		uint32_t size = ecs_state->component_store.sizes[i];
		void* comp = (uint8_t*)ecs_state->stack[i] + size * id;
		memset(comp, 0, size);
	}
	return id;
}

void* ecs_get_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id) {
	return (uint8_t*)ecs_state->stack[component_id] + (ecs_state->component_store.sizes[component_id] * entity_id);
}

void* ecs_get_component_replicas(ecs_state_t* ecs_state, uint32_t component_id) {
	return (uint8_t*)ecs_state->stack[component_id];
}

// TODO: needs testing
void ecs_component_callback(ecs_state_t* ecs_state, uint32_t component_id, void (*func)(void*)) {
	uint32_t offset = 0;
	for (uint32_t i = 0; i < ecs_state->component_store.replicas[component_id]; i++) {
		func((uint8_t*)ecs_state->stack[component_id] + offset);
		offset += (ecs_state->component_store.sizes[component_id]);
	}
}

bool ecs_has_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id) {
	return (ecs_state->entity_store.component_masks[entity_id] & (1 << component_id)) != 0;
}

int ecs_add_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id, void* data) {
	if (ecs_state->stack[component_id] == NULL) {
		// Component not initialized yet
		printf("Error: Component was not initialized yet, check component with id %u\n", component_id);
		return -1; // error
	}

	if (ecs_has_component(ecs_state, entity_id, component_id)) {
		// A component replica for this entity already exists
		printf("Warning: Component with id %u already exists for entity %u", component_id, entity_id);
		return -2; // ok, but no new addition
	}
	
	if (ecs_state->component_store.replicas[component_id] == ecs_state->component_store.slots[component_id]) {
		size_t new_size = ecs_state->component_store.slots[component_id] * 
			AUGMENTATION_MULTIPLIER * ecs_state->component_store.sizes[component_id];

		ecs_state->stack[component_id] = realloc(ecs_state->stack[component_id], new_size);

		if (!ecs_state->stack[component_id]) {
			printf("Error: could not reallocate entity storage!\n");
			return -1;
		}

		ecs_state->component_store.slots[component_id] *= AUGMENTATION_MULTIPLIER;
		printf("Allocated new slots for component %d!\n", component_id);
	}

	size_t component_size = ecs_state->component_store.sizes[component_id];
	void* dest = ecs_get_component(ecs_state, entity_id, component_id);

	if (memcpy(dest, data, component_size) == NULL) {
		printf("Error: couldn't copy component data to memory! Component id: %u\n", component_id);
		return -1;
	}

	ecs_state->entity_store.component_masks[entity_id] |= (1 << component_id);
	ecs_state->component_store.replicas[component_id]++;

	return 0;
}

void ecs_remove_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id) {
	ecs_state->entity_store.component_masks[entity_id] &= ~(1 << component_id);
	//TODO: implement deallocation
	//WARNING: No deallocation made
}

