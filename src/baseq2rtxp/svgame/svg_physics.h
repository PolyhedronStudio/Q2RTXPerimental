/*********************************************************************
*
*
*	SVGame: (Entity/MoveType-Specific Mechanic -) Physics:
* 
* 
* 	- Physics functions here are used by various movement types to handle collision detection and response.
*	- Physics traces will use the clip masks to determine what entities to collide with during movement.* 	- Physics functions here are used by various movement types to handle collision detection and response.
* 	- Clip Masks are used to determine what types of entities an entity will collide with during movement traces.
*
*	- Pusher entities are SOLID_BSP by default, and MOVETYPE_PUSH.
*		- These are used for moving platforms, doors, elevators, etc.
* 		- Do not obey gravity, and do not interact with each other or trigger fields.
* 		- They will block normal movement, and push other entities when they move into them. 
*		  Optionally crushing them if they can't move out of the way.
*	- Trigger entities are SOLID_TRIGGER by default, and MOVETYPE_NONE.
* 		- These are used for invisible trigger volumes that fire events when other entities touch them.
*	- Bonus items are SOLID_TRIGGER by default, and MOVETYPE_TOSS.
* 		- These are used for items that can be picked up by players when touched.
*	- Corpses are SOLID_NOT by default, and MOVETYPE_TOSS.
* 		- These are used for dead bodies that can still move around, but do not collide with other entities.
*	- Crates/Boxes are SOLID_BBOX by default, and MOVETYPE_BOUNCE.
* 		- These are used for physics objects that can be pushed around and collide with other entities.
*	- These are used for AI-controlled characters that can walk around and collide with other entities.
*		- Walking NPCs/Monsters are SOLID_BBOX by default, and MOVETYPE_STEP.
*		- Flying/Floating(Swimming) NPCs/Monsters are SOLID_BBOX by default, and MOVETYPE_FLY.
* 
* 
********************************************************************/
#pragma once



// Predeclarations.
struct svg_base_edict_t;



/**
*
*
*
*	Generic Physics Functions:
*
*
*
**/
/**
*	@brief	Applies a frame's worth of the gravity into the direction of the gravity vector for this entity.
* 	@param	ent The entity to apply gravity to.
**/
void SVG_AddGravity( svg_base_edict_t *ent );

/**
*	@brief	Ensure that an entity's velocity does not exceed sv_maxvelocity.
*	@param	ent	The entity to check.
**/
void SVG_ClampEntityMaxVelocity( svg_base_edict_t *ent );
/**
*	@brief Redirects the input velocity vector 'in' along the plane defined by the 'normal' vector,
*	@param in The input velocity vector to be clipped.
*	@param normal The normal vector defining the clipping plane.
*	@param out The output velocity vector after clipping.
*	@param overbounce The overbounce factor to apply during clipping.
*	@return The blocked flags indicating the type of clipping that occurred.
**/
const int32_t SVG_Physics_ClipVelocity( Vector3 &in, vec3_t normal, Vector3 &out, const double overbounce );



/**
*
*
*
*	Entity (Clip -)Test/Tracing:
*
*
*
**/
/**
*	@brief	Fetch the clipMask for this entity; certain modifiers affect the clipping behavior of objects.
* 	@param	ent The entity to get the clip mask for.
* 	@return The contents mask to use for clipping traces against this entity.
**/
const cm_contents_t SVG_GetClipMask( const svg_base_edict_t *ent );

/**
*	@brief	Will test the entity's current position to see if it is	obstructed by anything.
*	@param	ent The entity to test.
*	@return	nullptr if not obstructed, otherwise the entity that is obstructing it.
*	@note	In case of the trace yielding ENTITYNUM_NONE, the 'world' entity is returned instead.
**/
svg_base_edict_t *SVG_TestEntityPosition( const svg_base_edict_t *ent );
/**
*	@brief	Two entities have collided; run their touch functions.
*	@param	e1 The first entity.
*	@param	trace The trace result containing information about the collision.
*	@note	The entity in the trace is the second entity.
**/
void SVG_Impact( svg_base_edict_t *e1, svg_trace_t *trace );



/**
*
*
*
*	PushMove:
*
*
*
**/
/**
*	@brief	Will attempt to push an entity by the specified vector, handling collisions and impacts.
*	@param	ent The entity to push.
* 	@param	push The vector to push the entity by.
* 	@return The trace result of the push operation.
* 	@note	If the entity is blocked during the push, its position will be set to the end position of the trace.
* 			If the entity is removed during the impact handling, it will not be re-linked.
**/
svg_trace_t SVG_PushEntity( svg_base_edict_t *ent, const Vector3 &push );
/**
*	@brief	Objects need to be moved back on a failed push, otherwise riders would continue to slide.
*	@param	pusher The entity that is pushing.
* 	@param	move The movement vector.
* 	@param	amove The angular movement vector.
* 	@return	true if the push was successful, false if blocked.
**/
const bool SVG_PushMover( svg_base_edict_t *pusher, const Vector3 &move, const Vector3 &amove );