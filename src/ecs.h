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

#define INITIAL_REPLICA_CAPACITY 10
#define INITIAL_ENTITY_CAPACITY 10
#define INITIAL_ENTITY_COMPONENT_CAPACITY 4 // x2 will be 8, x2 will be 16, x2 will be 32
#define AUGMENTATION_MULTIPLIER 2

typedef uint32_t ent_id;
typedef uint8_t* comp_ptr;
typedef uint32_t entitytype_t; // this is a placeholder for an item of an enum of entity types you might have
typedef uint32_t componenttype_t; // similar but for components.

// TODO: hashmap please...
typedef struct {
	componenttype_t type;
	comp_ptr component;
} comp_tuple;

// idea: whenever we destroy an entity, put their components in this type of objects.
// when we create new components, substitute the components at these first before allocating new stuff, effectively reciclying them :)
typedef struct {
	componenttype_t type;
	comp_ptr component;
} recycle_component;

/*
 * Note: may have to increase the size of the mask as more components are added.
 */
typedef struct {
	entitytype_t type; // will have garbage initially - not much use right now, might remove
	uint32_t component_mask; // bitmask with components this entity has
	comp_tuple* components; // list of pointers to the components this entity has, including comp types
	uint8_t used_comp_slots; // used comp_ptr slots for this entity
	uint8_t comp_slots_cap; // allocated comp_ptr slots for this entity
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
	recycle_component* recycle_bin;
	entitystore_t entity_store;
	componentstore_t component_store;
} ecs_state_t;

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
 * Destroys the entity component system by freeing data related to 
 * components. Data relating to entities is not touched and must be freed by a call
 * to ecs_destroy_entities.
 */
void ecs_destroy(ecs_state_t* ecs_state);

/*
 * Destroys the entities in the entity component system, leaving the core untouched.
 * If you wish to completely destroy the component system, an additional call to ecs_destroy 
 * must be done.
 */
void ecs_destroy_entities(ecs_state_t* ecs_state);

/*
 * Creates a new entity by reallocating the necessary containers if 
 * storage cap is exceeded and incrementing entity count.
 *
 * Right now the id is just the entity count. I think some integration with the
 * gamedefs enums would be advantageous.
 */ 
ent_id ecs_create_entity(ecs_state_t* ecs_state, entitytype_t ent_type);

/*
 * Returns a void* to the instance of component *component_id* belonging to entity 
 * *entity_id*
 */
void* ecs_get_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id);

/*
 * Returns a pointer to the beginning of the component replicas
 * of a provided component id. Does NOT enforce the limit, i.e.
 * the caller must check for number of replicas if he plans to iterate.
 */
void* get_component_replicas(ecs_state_t* ecs_state, uint32_t component_id);

void ecs_component_callback(ecs_state_t* ecs_state, uint32_t component_id, void (*func)(void*));

bool ecs_has_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id);

int ecs_add_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id, void* data);

void ecs_remove_component(ecs_state_t* ecs_state, ent_id entity_id, uint32_t component_id);
