/*
 * Heavy inspiration from https://github.com/logmoon/C-Entity-Component-System/.
 * Huge thanks to user logmoon.
 *
 * NOTE:
 * Imagine you have 100 fighting units on the battlefield, but only 10 of them changed their positions. 
 * Instead of using a normal system and updating all 100 entities depending on the position, 
 * you can use a reactive system which will only update the 10 changed units. So efficient.
 *
 * This would be cool to implement. Experimenting with offsets, etc.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ntable.h"
#include "htable.h"

#define INITIAL_REPLICA_CAPACITY 10
#define INITIAL_ENTITY_CAPACITY 10
#define INITIAL_ENTITY_COMPONENT_CAPACITY 4 // x2 will be 8, x2 will be 16, x2 will be 32
#define AUGMENTATION_MULTIPLIER 2

typedef uint32_t ent_id; // 32 bit identifier uint
typedef uint32_t entitytype_t; // this is a placeholder for an item of an enum of entity types you might have
typedef uint32_t componenttype_t; // similar but for components.
typedef uint32_t component_mask; // for convenient checking

typedef struct {
	ent_id id; // attributed on initialization
	entitytype_t type; // will have garbage initially - not much use right now, might remove
	component_mask comp_mask;	
	ntable* components; // hash table of component type to component_ptr. TODO: what if has two components of same type?
} entity_t;

typedef struct {
	entity_t* entities; // list of entities
	uint32_t count; // number of created entities
	uint32_t cap; // maximum number of entities allowed in storage
} entitystore_t;

// since all components will probably be used (and only once), 
// we can index them with the component enum
typedef struct {
	uint32_t count; // number of components	
	uint32_t* sizes; // stores the size of each component
	uint32_t* replicas; // stores the number of replicas of each component (filled slots)
	uint32_t* slots; // stores the number of allocated slots (vacant or not) for each component
} componentstore_t;

typedef struct {
	void** stack; // list of pointers to component replica sets
	entitystore_t entity_store;
	componentstore_t component_store;
	htable* entcomp_map; // maps component replica pointers to their owner entities.
} ecs_state_t;

typedef void* comp_ptr;
typedef entity_t* ent_ptr;

/*
 * Initializes the Entity Component System on the provided
 * ecs_state_t state. 
 *
 * This involves allocating state stack space for 
 * the provided number of components, storing their sizes and number of replicas,
 * as well as allocating space for those replicas.
 * 
 */
int ecs_init(ecs_state_t* ecs_state, uint32_t component_count, ...);

/*
 * Initializes entity storage on the provided Entity Component System state.
 * 
 * This involves setting the capacity for entities to an initial value, allocating
 * space for their masks, and for the entity ids in query_result.
 *
 */
int ecs_init_entities(ecs_state_t* ecs_state);

/*
 * Destroys the entirety of the entity component system by freeing data related to 
 * components and entities alike. Effectively resets the ECS to a state where you can 
 * call the init functions on it again to reuse it.
 */
void ecs_destroy(ecs_state_t* ecs_state);

/*
 * Creates a new entity by reallocating the necessary containers if 
 * storage cap is exceeded and incrementing entity count.
 *
 * Right now the id is just the entity count. Starts with 0 (zero). 
 */ 
ent_id ecs_create_entity(ecs_state_t* ecs_state, entitytype_t ent_type);

/*
 * Returns a void* to the instance of component *comp_type* belonging to entity 
 * *entity_id*
 */

comp_ptr ecs_get_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type);

/*
 * Returns a pointer to the beginning of the component replicas
 * of a provided component id. Does NOT enforce the limit, i.e.
 * the caller must check for number of replicas if he plans to iterate.
 */
void* get_component_replicas(ecs_state_t* ecs_state, componenttype_t comp_type);

void ecs_component_callback(ecs_state_t* ecs_state, componenttype_t comp_type, void (*func)(void*));

// WARNING: this is relying on entity id being the count - fine while no entity deletion, dangerous afterwards
bool ecs_entity_has_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type);

// WARNING: this is relying on entity id being the count - fine while no entity deletion, dangerous afterwards
int ecs_add_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type, void* data, void** loc);

// WARNING: this is relying on entity id being the count - fine while no entity deletion, dangerous afterwards
int ecs_remove_component(ecs_state_t* ecs_state, ent_id entity_id, componenttype_t comp_type);

int ecs_remove_component_by_ptr(ecs_state_t* ecs_state, comp_ptr component, componenttype_t comp_type);
