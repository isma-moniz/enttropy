# enttropy - Simple entity component system (ECS) written in C 

I started writing this entity component system for a game I'm making and decided to post my development efforts here.
Feel free to drop me issues, advice, suggestions, insults and whatnot.

My initial implementation was EXTREMELY similar to user logmoon's entity component system available here https://github.com/logmoon/C-Entity-Component-System/.

The only reason I'm even creating this repo is because I want to heavily customise and improve it.

## Initial problems & Roadmap

Right now the ecs will break if entity id is bigger than allocated slots. The entities and the allocated slots
are closely coupled, because an entity's spot in a component is essentially the offset of its id * size. It's not hard
to see how this can bring problems. It would also be very dumb (although it would work) to simply allocate a component spot for every
single entity on every single component and a waste of space. This is by all means a possible mitigation, but we can do better.

Idea 1: have each component hold an entity id. If order is enforced (i.e. there may be many entities who don't have a certain component,
but the ones that do appear in non-decreasing order in the component store), we can implement reasonably fast lookup for a certain entity's component.

Idea 2 (better): Alternatively (it really would be cool to do both tho) make the entities hold some sort of pointer to their components. Maybe have each
entity start with a list of 5 pointers to fill up with their components. If they need more allocate double. similar to how we're already doing.
While this incurs in a little more overhead in terms of memory, i actually believe it makes up for the huge memory we would be using if
every entity allocated one of every entity. This provides with much much faster lookup, while preserving also the semantics of an ECS, where
the components are closely packed together and you can iterate them efficiently.

