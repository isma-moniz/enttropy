#include "ecs.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h> 
#include <stdarg.h>

int ecs_init(ecs_state_t* ecs_state, uint32_t component_count, ...) {
	if (!(ecs_state->stack = (void**)malloc(component_count * sizeof(void*)))) {
		printf("Error: could not allocate memory for ecs_state\n");
		return -1;
	}

	// component store data
	ecs_state->component_store.count = component_count;

	if (!(ecs_state->component_store.sizes = (uint32_t*)malloc(component_count * sizeof(uint32_t)))) {
		printf("Error: could not allocate memory for component sizes\n");
		goto stage1_err;
	}

	if (!(ecs_state->component_store.replicas = (uint32_t*)malloc(component_count * sizeof(uint32_t)))) {
		printf("Error: could not allocate memory for component replica numbers\n");
		goto stage2_err;
	}

	if (!(ecs_state->component_store.slots = (uint32_t*)malloc(component_count * sizeof(uint32_t)))) {
		printf("Error: could not allocate memory for component slot counts\n");
		goto stage3_err;
	}

	uint32_t cleanup_index;	
	va_list args;
	va_start(args, component_count);
	for (uint32_t i = 0; i < component_count; i++) {
		uint32_t component_size = va_arg(args, uint32_t);
		ecs_state->component_store.sizes[i] = component_size;
		ecs_state->component_store.slots[i] = INITIAL_REPLICA_CAPACITY;
		ecs_state->component_store.replicas[i] = 0;

		ecs_state->stack[i] = (void*)malloc(INITIAL_REPLICA_CAPACITY * component_size);
		if (!ecs_state->stack[i]) {
			printf("Error: could not allocate memory for replicas of component with id: %d\n", i);
			goto stage4_err;
		}
	}

	va_end(args);

	ecs_state->entcomp_map = ht_create();
	if (!ecs_state->entcomp_map) {
		printf("Error: could not allocate memory to create entcomp map.\n");
		for (uint32_t i = 0; i < component_count; ++i) {
			free(ecs_state->stack[i]);
		}

		free(ecs_state->stack);
		ecs_state->stack = NULL;
		free(ecs_state->component_store.slots);
		ecs_state->component_store.slots = NULL;
		goto stage3_err;
	}
	return 0;

	// Error cleanup
stage4_err:
	for (uint32_t i = 0; i < cleanup_index; ++i) {
		free(ecs_state->stack[i]);
	}
	free(ecs_state->stack);
	ecs_state->stack = NULL;
	free(ecs_state->component_store.slots);
	ecs_state->component_store.slots = NULL;
stage3_err:
	free(ecs_state->component_store.replicas);
	ecs_state->component_store.replicas = NULL;
stage2_err:
	free(ecs_state->component_store.sizes);
	ecs_state->component_store.sizes = NULL;
stage1_err:
	free(ecs_state->stack);
	ecs_state->stack = NULL;
	return -1;
}

// Sets up individual entity's memory contents
int setup_entity(ent_ptr entt) {
	// entt->type = ...; WARNING: GARBAGE
	// entt->id = ...; WARNING: GARBAGE
	entt->comp_mask = 0;
	entt->components = nt_create();
	if (!entt->components) {
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

	// Setup individual entities
	for (int i = 0; i < INITIAL_ENTITY_CAPACITY; i++) {
		entity_t* ent = &entities[i];
		// setup the initially allocated entities, but not the ones after the first expansion
		// I do this to save a little space, as many of the newly allocated entities might not even
		// be used.
		if (setup_entity(ent) < 0) {
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

/*
int swap_component(ent_ptr entity, componenttype_t comp_type, comp_ptr new_comp_ptr) {
	if (!nt_get(entity->components, comp_type)) {
		return -1;
	} // NOTE: extra overhead, just to check if we aren't adding a 
	  // new component.
	if (!nt_set(entity->components, (uint64_t)comp_type, (void*)new_comp_ptr)) {
		return -1;
	} 
	return 0;
}
*/

ent_id ecs_create_entity(ecs_state_t* ecs_state, entitytype_t ent_type) {
	//maybe user provided ids to leverage the enums would be better
	ent_id id = ecs_state->entity_store.count;  // this will become a problem once I implement entity deletion
	entitystore_t *ent_store = &ecs_state->entity_store;

	if (id == ent_store->cap) {
		printf("ECS: Max entities reached, reallocating\n");
		uint64_t prev_size = ent_store->cap * sizeof(entity_t);
		uint64_t new_size = AUGMENTATION_MULTIPLIER * prev_size;
		uint32_t new_cap = ent_store->cap * AUGMENTATION_MULTIPLIER;

		if (new_size < prev_size || new_cap < ent_store->cap) {
			printf("Error: size augmentation overflow in ecs_create_entity.\n");
			return -1;
		}
		printf("ECS: New size cap: %u entities, %lu bytes\n", AUGMENTATION_MULTIPLIER * ent_store->cap, new_size);
		
		ent_store->entities = realloc(ent_store->entities, new_size);
		
		if (!ent_store->entities) {
			printf("Error: could not reallocate entity storage!\n");
			return -1;
		}

		ent_store->cap = new_cap;
	}

	ent_ptr entt = &ent_store->entities[id];
	// individual setup after first expansion
	if (ent_store->cap > INITIAL_ENTITY_CAPACITY) {
		if (setup_entity(entt) < 0) {
			printf("Error: could not allocate memory for entity %d component pointers.\n", ent_store->count);
			return -1;
		}
	}
	entt->type = ent_type;
	++ent_store->count;
	return id;
}

bool ecs_entity_has_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type) {
	return (ecs_state->entity_store.entities[entity_id].comp_mask & (1 << comp_type)) != 0;
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

int ecs_add_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type, void* data, void** loc) {
	if (comp_type > ecs_state->component_store.count) {
		// Component not initialized yet
		printf("Error: Component was not initialized, check component type id %u\n", comp_type);
		printf("All component types should be provided when initializing the ECS.\n");
		return -1; // error
	}

	if (ecs_entity_has_component(ecs_state, entity_id, comp_type)) {
		// A component replica for this entity already exists
		printf("Warning: Component with id %u already exists for entity %u. Skipping.\n", comp_type, entity_id);
		return -2; // ok, but no new addition
	}
	
	// add component to "stack"
	uint32_t replicas = ecs_state->component_store.replicas[comp_type];
	uint32_t slots = ecs_state->component_store.slots[comp_type];
	if (replicas == slots) {
		size_t prev_size = ecs_state->component_store.slots[comp_type] * ecs_state->component_store.sizes[comp_type];
		size_t new_size = prev_size * AUGMENTATION_MULTIPLIER;
		uint32_t slots_new = ecs_state->component_store.slots[comp_type] * AUGMENTATION_MULTIPLIER;

		if (new_size < prev_size || slots_new < slots) {
			printf("Error: size augmentation overflow in ecs_add_component.\n");
			return -1;
		}

		ecs_state->stack[comp_type] = realloc(ecs_state->stack[comp_type], new_size);

		if (!ecs_state->stack[comp_type]) {
			printf("Error: could not reallocate entity storage!\n");
			return -1;
		}

		ecs_state->component_store.slots[comp_type] = slots_new;
		printf("Allocated new slots for component %d!\n", comp_type);
	}

	size_t component_size = ecs_state->component_store.sizes[comp_type];
	void* dest = (uint8_t*)ecs_state->stack[comp_type] + ecs_state->component_store.replicas[comp_type] * ecs_state->component_store.sizes[comp_type];

	if (memcpy(dest, data, component_size) == NULL) {
		printf("Error: couldn't copy component data to memory! Component id: %u\n", comp_type);
		return -1;
	}
	ecs_state->component_store.replicas[comp_type]++;
	
	// update entity
	entity_t* ent_ptr = &ecs_state->entity_store.entities[entity_id];
	ent_ptr->comp_mask |= (1 << comp_type);
	nt_set(ent_ptr->components, comp_type, dest);

	// update entcomp_map
	ht_set(ecs_state->entcomp_map, dest, ent_ptr);

	if (loc != NULL)
		*loc = dest;

	return 0;
}

int ecs_remove_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type) {
	ecs_state->entity_store.entities[entity_id].comp_mask &= ~(1 << comp_type);
	uint32_t component_size = ecs_state->component_store.sizes[comp_type];

	void *to_delete = ecs_get_component(ecs_state, entity_id, comp_type);
	void *last_replica = ecs_state->stack[comp_type] + ecs_state->component_store.replicas[comp_type] * component_size;
	if (!memcpy(to_delete, last_replica, component_size)) {
		printf("Error: couldn't copy component data to memory! Component id: %u\n", comp_type);
		return -1;
	}
	--ecs_state->component_store.replicas[comp_type];
	entity_t* ent = (entity_t*) ht_get(ecs_state->entcomp_map, last_replica);
	if (!ent)
		return -1;
	
	nt_set(ent->components, comp_type, to_delete);
	return 0;
}

int ecs_remove_component_by_ptr(ecs_state_t* ecs_state, comp_ptr component, componenttype_t comp_type) {
	componentstore_t *comp_store = &ecs_state->component_store;
	ent_ptr owner_entt = (ent_ptr)ht_get(ecs_state->entcomp_map, component);
	if (!owner_entt) {
		printf("Error: could not find owner entity.\n");
		return -1;
	}
	owner_entt->comp_mask &= ~(1 << comp_type);
	uint32_t component_size = comp_store->sizes[comp_type];

	void* last_replica = ecs_state->stack[comp_type] + comp_store->replicas[comp_type] * component_size;
	if (!memcpy(component, last_replica, component_size)) {
		printf("Error: couldn't copy component data to memory! Component id: %u\n", comp_type);
	}
	--comp_store->replicas[comp_type];
	ent_ptr entt = (ent_ptr) ht_get(ecs_state->entcomp_map, last_replica);
	if (!entt) {
		printf("Couldn't find owner of relocated replica of type %u.\n", comp_type);
		return -1;
	}

	if (!nt_set(entt->components, comp_type, component)) {
		printf("Error: couldn't update entity has table.\n");
		return -1;
	}
	return 0;
}

