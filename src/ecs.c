#include "ecs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <stdarg.h>

/*
 * Points an entity's component store component tuple to a new component in the stack.
 * This is called when deleting components causes a pop and swap to happen.
 */
int swap_component(entity_t* entity, componenttype_t comp_type, comp_ptr new_comp_ptr);

int ecs_init(ecs_state_t* ecs_state, uint32_t component_count, ...) {
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
			for (uint32_t j = 0; j < i; j++) {
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

	ecs_state->entcomp_map = ht_create();
	if (!ecs_state->entcomp_map) {
		printf("Malformed ecs: unable to create entcomp map.\n");
		printf("WRITE THE CLEANUPS HERE YOU LAZY BUM!\n");
		return -1;
	}
	return 0;
}

int ecs_init_entities(ecs_state_t* ecs_state) {
	ecs_state->entity_store.count = 0;
	ecs_state->entity_store.cap = INITIAL_ENTITY_CAPACITY;

	ecs_state->entity_store.entities = (entity_t*)malloc(INITIAL_ENTITY_CAPACITY * sizeof(entity_t));
	entity_t* entities = ecs_state->entity_store.entities;
	if (!entities) {
		printf("Error: could not allocate memory for entities initialization.\n");
		return -1;
	}

	for (int i = 0; i < INITIAL_ENTITY_CAPACITY; i++) {
		// entities[i].type = ...; WARNING: let's purposefully leave as garbage right now
		entity_t* ent = &entities[i];
		ent->comp_slots_cap = INITIAL_ENTITY_COMPONENT_CAPACITY;
		ent->used_comp_slots = 0;
		ent->component_mask = 0;
		ent->components = nt_create();
		if (!ent->components) {
			printf("Error: could not allocate memory for entity %d component pointers.\n", i);
			for (int j = 0; j < i; j++) {
				nt_destroy(entities[j].components);
			}
			free(ecs_state->entity_store.entities);
			ecs_state->entity_store.entities = NULL;
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
	if (ecs_state->entcomp_map) {
		free(ecs_state->entcomp_map);
		ecs_state->entcomp_map = NULL;
	}
	if (ecs_state->stack) {
		for (uint32_t i = 0; i < ecs_state->component_store.count; i++) {
			free(ecs_state->stack[i]);
		}
		free(ecs_state->stack);
		ecs_state->stack = NULL;
	}
	ecs_state->component_store.count = 0;
	if (ecs_state->entity_store.entities) {
		for (uint32_t i = 0; i < ecs_state->entity_store.count; i++) {
			nt_destroy(ecs_state->entity_store.entities[i].components);
		}
		free(ecs_state->entity_store.entities);
		ecs_state->entity_store.entities = NULL;
		ecs_state->entity_store.count = 0;
		ecs_state->entity_store.cap = 0;
	}
}

int swap_component(entity_t* entity, componenttype_t comp_type, comp_ptr new_comp_ptr) {
	if (!nt_get(entity->components, comp_type)) {
		return -1;
	} // NOTE: extra overhead, just to check if we aren't adding a 
	  // new component.
	if (!nt_set(entity->components, (uint64_t)comp_type, (void*)new_comp_ptr)) {
		return -1;
	} 
	return 0;
}

ent_id ecs_create_entity(ecs_state_t* ecs_state, entitytype_t ent_type) {
	//maybe user provided ids to leverage the enums would be better
	ent_id id = ecs_state->entity_store.count; 

	if (id == ecs_state->entity_store.cap) {
		printf("ECS: Max entities reached, reallocating\n");
		uint64_t new_size = AUGMENTATION_MULTIPLIER * ecs_state->entity_store.cap * sizeof(entity_t);
		printf("ECS: New size cap: %u entities, %lu bytes\n", AUGMENTATION_MULTIPLIER * ecs_state->entity_store.cap, new_size);
		
		ecs_state->entity_store.entities = realloc(ecs_state->entity_store.entities, new_size);
		
		if (!ecs_state->entity_store.entities) {
			printf("Error: could not reallocate entity storage!\n");
			return -1;
		}

		ecs_state->entity_store.cap *= AUGMENTATION_MULTIPLIER;
	}

	ecs_state->entity_store.count++;
	entity_t* ent_ptr = &ecs_state->entity_store.entities[id];
	ent_ptr->type = ent_type;
	return id;
}


void* ecs_get_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type) {
	entity_t* entity = &ecs_state->entity_store.entities[entity_id];
	void* component = nt_get(entity->components, (uint64_t)comp_type);
	if (!component) {
		printf("No component of type %d found for entity %d.\n", comp_type, entity_id);
	}
	return component;
}

void* ecs_get_component_replicas(ecs_state_t* ecs_state, componenttype_t comp_type) {
	return ecs_state->stack[comp_type];
}

// TODO: needs testing
void ecs_component_callback(ecs_state_t* ecs_state, componenttype_t comp_type, void (*func)(void*)) {
	uint32_t offset = 0;
	for (uint32_t i = 0; i < ecs_state->component_store.replicas[comp_type]; i++) {
		func((uint8_t*)ecs_state->stack[comp_type] + offset);
		offset += (ecs_state->component_store.sizes[comp_type]);
	}
}

bool ecs_entity_has_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id) {
	return (ecs_state->entity_store.entities[entity_id].component_mask & (1 << component_id)) != 0;
}

int ecs_add_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id, void* data, void** loc) {
	if (ecs_state->stack[component_id] == NULL) {
		// Component not initialized yet
		printf("Error: Component was not initialized yet, check component with id %u\n", component_id);
		return -1; // error
	}

	if (ecs_entity_has_component(ecs_state, entity_id, component_id)) {
		// A component replica for this entity already exists
		printf("Warning: Component with id %u already exists for entity %u", component_id, entity_id);
		return -2; // ok, but no new addition
	}
	
	// add component to "stack"
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
	void* dest = (uint8_t*)ecs_state->stack[component_id] + ecs_state->component_store.replicas[component_id] * ecs_state->component_store.sizes[component_id];

	if (memcpy(dest, data, component_size) == NULL) {
		printf("Error: couldn't copy component data to memory! Component id: %u\n", component_id);
		return -1;
	}
	ecs_state->component_store.replicas[component_id]++;
	
	// update entity
	entity_t* ent_ptr = &ecs_state->entity_store.entities[entity_id];
	ent_ptr->component_mask |= (1 << component_id);
	nt_set(ent_ptr->components, component_id, dest);

	// update entcomp_map
	ht_set(ecs_state->entcomp_map, dest, ent_ptr);

	if (loc != NULL) *loc = dest;

	return 0;
}

int ecs_remove_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id) {
	ecs_state->entity_store.entities[entity_id].component_mask &= ~(1 << component_id);
	uint32_t size = ecs_state->component_store.sizes[component_id];

	void *to_delete = ecs_get_component(ecs_state, entity_id, component_id);
	void *last_replica = ecs_state->stack[component_id] + ecs_state->component_store.replicas[component_id] * size;
	if (!memcpy(to_delete, last_replica, size)) {
		printf("Error: couldn't copy component data to memory! Component id: %u\n", component_id);
		return -1;
	}
	--ecs_state->component_store.replicas[component_id];
	entity_t* ent = (entity_t*) ht_get(ecs_state->entcomp_map, last_replica);
	if (!ent)
		return -1;
	
	nt_set(ent->components, component_id, to_delete);
}

