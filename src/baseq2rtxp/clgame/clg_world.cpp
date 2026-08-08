/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#include "clgame/clg_local.h"
#include "clgame/clg_entities.h"
#include "clgame/clg_precache.h"
#include "clgame/clg_world.h"


/**
*   @brief  Build the stationary collision shape represented by a dynamic client entity.
*   @param  ent         Entity whose replicated solid and bounds define the target shape.
*   @param  outShape    [out] Analytical target shape definition.
*   @param  outCenter   [out] World-space center of the target shape.
*   @return True when ent is a capsule, cylinder, or sphere; false for box/BSP targets.
*   @note   Box and BSP targets continue through the existing transformed-hull path.
**/
static bool CLG_BuildDynamicEntityShape( const centity_t *ent, cm_trace_shape_t *outShape, Vector3 *outCenter ) {
	/**
	*   Reject invalid output storage and non-primitive target entities before building shape data.
	**/
	if ( !ent || !outShape || !outCenter
		|| ( ent->current.solid != SOLID_CAPSULE
			&& ent->current.solid != SOLID_CYLINDER
			&& ent->current.solid != SOLID_SPHERE ) ) {
		return false;
	}

	/**
	*   Derive the target's horizontal radius, axial half-height, and origin-to-center offset.
	**/
	const float radius = std::max( std::fabs( ent->maxs.x ), std::fabs( ent->maxs.y ) );
	const float fullHalfHeight = std::fabs( ent->maxs.z - ent->mins.z ) * 0.5f;
	const Vector3 centerOffset = QM_Vector3Scale( QM_Vector3Add( ent->mins, ent->maxs ), 0.5f );
	*outShape = {};

	/**
	*   Match the server shape definitions: capsule caps consume the radial portion of the axial height,
	*   while cylinders retain flat ends and spheres use their largest bound component as radius.
	**/
	if ( ent->current.solid == SOLID_CAPSULE ) {
		outShape->type = SHAPE_CAPSULE;
		outShape->radius = radius;
		outShape->halfHeight = std::max( 0.0f, fullHalfHeight - radius );
	} else if ( ent->current.solid == SOLID_CYLINDER ) {
		outShape->type = SHAPE_CYLINDER;
		outShape->radius = radius;
		outShape->halfHeight = fullHalfHeight;
	} else {
		outShape->type = SHAPE_SPHERE;
		outShape->radius = std::max( radius, std::max( std::fabs( ent->mins.z ), std::fabs( ent->maxs.z ) ) );
		outShape->halfHeight = 0.0f;
	}

	*outCenter = QM_Vector3Add( ent->current.origin, centerOffset );
	return true;
}

/**
*   @brief  Sweep an analytical moving shape against one dynamic primitive entity.
*   @param  start         Moving-shape center at the beginning of the sweep.
*   @param  movingShape   Analytical shape being swept.
*   @param  end           Moving-shape center at the end of the sweep.
*   @param  clipEntity    Dynamic primitive target.
*   @return Trace result for the specified target, or a clear trace for non-primitive targets.
**/
static cm_trace_t CLG_TraceShapeAgainstDynamicEntity( const Vector3 &start, const cm_trace_shape_t &movingShape, const Vector3 &end, const centity_t *clipEntity ) {
	/**
	*   Initialize a clear trace with the client collision system's default payloads.
	**/
	cm_trace_t trace = {};
	trace.fraction = 1.0f;
	trace.endpos = end;
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*   Resolve the target's analytical shape; callers retain the transformed-hull path for other solids.
	**/
	cm_trace_shape_t targetShape = {};
	Vector3 targetCenter = {};
	if ( !CLG_BuildDynamicEntityShape( clipEntity, &targetShape, &targetCenter ) ) {
		return trace;
	}

	/**
	*   Perform the shape-vs-shape sweep and restore the target identity on a hit.
	**/
	trace = clgi.CM_AnalyticalShapeSweep( &start, &movingShape, &end, &targetCenter, &targetShape );
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();
	if ( trace.fraction < 1.0f ) {
		trace.entityNumber = clipEntity->current.number;
		trace.brushID = SG_PackEntityBrushID( clipEntity->current.number, trace.brushID );
		trace.endpos = start + ( end - start ) * trace.fraction;
		trace.contents = CONTENTS_SOLID;
	} else {
		trace.endpos = end;
	}
	return trace;
}


/**
*
*
*
*   Helper Functions:
*
*
*
**/
/**
*   @brief  Clips the trace to all entities currently in-frame.
**/
static void ClipTraceMoveToEntities( cm_trace_t *tr, const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	cm_trace_t trace = cm_trace_t();
        
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	// Iterate all solid entities in the current frame.
    for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        // Acquire the entity state.
        centity_t *ent = game.frameEntities.solids[ i ];

        // Can't trace without proper data.
        if ( ent == nullptr ) {
            continue;
        }

        // Prevent tracing against non solids.
        if ( ent->current.solid == SOLID_NOT ) {
            continue;
        }
        if ( !ent->current.modelindex ) {
            continue;
        }
        // 
        //if ( !( contentMask & CONTENTS_PLAYERCLIP ) ) { // if ( !( contentMask & CONTENTS_PLAYER ) ) {
        //    continue;
        //}
        // Skip dead clients if not specified as a player in contentMask. (Assumed to be CONTENTS_DEADMONSTER)
        if ( ent->current.number <= game.maxclients && !( contentMask & CONTENTS_PLAYER ) ) {
            continue;
        }


        // Prevent tracing against passEntity.
        if ( passEntity != nullptr && ent != nullptr && ( ent->current.number == passEntity->current.number ) ) {
            continue;
        }

        //// No need to continue if we're in all-solid.
        //if ( tr->allsolid ) {
        //    return;
        //}

        // Don't clip if we're owner of said entity.
        if ( passEntity ) {
            if ( ent->current.ownerNumber == passEntity->current.number ) {
                continue;    // Don't clip against own missiles.
            }
            if ( passEntity->current.ownerNumber == ent->current.number ) {
                continue;    // Don't clip against owner.
            }
        }
        
        //if ( !( contentMask & CONTENTS_DEADMONSTER )
        //    && ( ent->svFlags & SVF_DEADENTITY ) ) {
        //    continue;
        //}
        //if ( !( contentMask & CONTENTS_PROJECTILE )
        //    && ( ent->svFlags & SVF_PROJECTILE ) ) {
        //    continue;
        //}
        //if ( !( contentMask & CONTENTS_PLAYER ) {
        //    && ( ent->svFlags & SVF_PLAYER ) )
        //    continue;
        //}

        cm_trace_shape_t touchShape = { .type = SHAPE_AABB, .extents = Vector3(0,0,0), .radius = 0, .halfHeight = 0 };
        if ( ent->current.solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
        else if ( ent->current.solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
        else if ( ent->current.solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

        if ( touchShape.type != SHAPE_AABB ) {
            cm_trace_shape_t shapeA = { .type = SHAPE_AABB, .extents = Vector3(0,0,0), .radius = 0, .halfHeight = 0 };
            Vector3 centerStart = start;
            Vector3 centerEnd = end;
            Vector3 offset = { 0.0f, 0.0f, 0.0f };

            if ( (!mins && !maxs) || ( mins->x == 0.0f && mins->y == 0.0f && mins->z == 0.0f &&
                 maxs->x == 0.0f && maxs->y == 0.0f && maxs->z == 0.0f ) ) {
                 // point
            } else {
                shapeA.extents = QM_Vector3Scale( QM_Vector3Subtract( *maxs, *mins ), 0.5f );
                shapeA.radius = std::max(shapeA.extents.x, shapeA.extents.y);
                shapeA.halfHeight = shapeA.extents.z;

                offset = QM_Vector3Scale( QM_Vector3Add( *mins, *maxs ), 0.5f );

                centerStart = QM_Vector3Add( centerStart, offset );
                centerEnd = QM_Vector3Add( centerEnd, offset );
            }

            touchShape.extents = QM_Vector3Scale( QM_Vector3Subtract( ent->maxs, ent->mins ), 0.5f );
            touchShape.radius = std::max(touchShape.extents.x, touchShape.extents.y);
            touchShape.halfHeight = touchShape.extents.z;

            Vector3 touchCenter = QM_Vector3Add(
                ent->current.origin,
                QM_Vector3Scale( QM_Vector3Add( ent->mins, ent->maxs ), 0.5f )
            );

            trace = clgi.CM_AnalyticalShapeSweep( &centerStart, &shapeA, &centerEnd, &touchCenter, &touchShape );

            if ( trace.fraction < 1.0f ) {
                trace.entityNumber = ent->current.number;
                trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
                trace.endpos = QM_Vector3Add( start, QM_Vector3Scale( QM_Vector3Subtract( end, start ), trace.fraction ) );
                trace.contents = CONTENTS_SOLID;
            } else {
                trace.endpos = end;
            }
        } else {
            // BSP Brush Model Entity:
            mnode_t *headNode = clgi.GetEntityHullNode( ent );

            // Perform the BSP box sweep.
            trace = clgi.CM_TransformedBoxTrace( 
                &start, &end,
                mins, maxs, 
                headNode, contentMask,
                &ent->current.origin, &ent->current.angles 
            );
        }

        // Stamp entity-backed traces with the touched entity identity
        if ( trace.allsolid || trace.startsolid || trace.fraction < 1.0f ) {
            trace.entityNumber = ent->current.number;
            trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
        }

        if ( trace.allsolid || trace.fraction < tr->fraction ) {
            const int32_t oldStartSolid = tr->startsolid;
            *tr = trace;
            tr->startsolid = (bool)( (int32_t)tr->startsolid | oldStartSolid );
        } else if ( trace.startsolid ) {
            tr->startsolid = true;
        }
		// No need to continue  if we're in an all-solid.
        if ( tr->allsolid ) {
            return;
        }
    }
}

/**
*	@brief	Clip one capsule sweep against all dynamic solid entities in the current frame list.
*	@param	tr				[in,out] Accumulated best trace result to update when a closer hit is found.
*	@param	start			World-space sweep start.
*	@param	radius			Capsule radius of the moving shape.
*	@param	halfHeight		Capsule half-height of the moving shape.
*	@param	end				World-space sweep end.
*	@param	passEntity		Optional entity to ignore (self/owner filtering).
*	@param	contentMask		Contents mask controlling which entities are considered collidable.
*	@note	Uses analytical shape-vs-shape sweeps for non-brush solids and transformed hull tracing for AABB brush-model solids.
**/
static void ClipCapsuleMoveToEntities( cm_trace_t *tr, const Vector3 &start, const float radius, const float halfHeight, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize one local trace container with safe null/default surface data.
	**/
	cm_trace_t trace = cm_trace_t();
        
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	Iterate all dynamic solid entities and test each candidate against the moving capsule.
	**/
	for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        centity_t *ent = game.frameEntities.solids[ i ];

		// Skip invalid/non-solid entities immediately.
        if ( ent == nullptr || ent->current.solid == SOLID_NOT ) {
            continue;
        }
		// Skip entities that do not own a collision model.
        if ( !ent->current.modelindex ) {
            continue;
        }
		// Skip players unless caller requested player contents.
        if ( ent->current.number <= game.maxclients && !( contentMask & CONTENTS_PLAYER ) ) {
            continue;
        }
		// Skip the pass entity itself.
        if ( passEntity != nullptr && ent != nullptr && ( ent->current.number == passEntity->current.number ) ) {
            continue;
        }
		// Skip owner/owned pairs so projectiles and owners do not self-collide here.
        if ( passEntity ) {
            if ( ent->current.ownerNumber == passEntity->current.number ) {
                continue;
            }
            if ( passEntity->current.ownerNumber == ent->current.number ) {
                continue;
            }
        }
        
        cm_trace_shape_t touchShape = { .type = SHAPE_AABB, .extents = Vector3(0,0,0), .radius = 0, .halfHeight = 0 };
        if ( ent->current.solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
        else if ( ent->current.solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
        else if ( ent->current.solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

		/**
		*	For analytical primitive solids (sphere/capsule/cylinder), compute extents and
		*	entity-center offset before issuing one analytical sweep.
		**/
        if ( touchShape.type != SHAPE_AABB ) {
            touchShape.extents = QM_Vector3Scale( QM_Vector3Subtract( ent->maxs, ent->mins ), 0.5f );
            touchShape.radius = std::max(touchShape.extents.x, touchShape.extents.y);
            touchShape.halfHeight = touchShape.extents.z;

            Vector3 center = QM_Vector3Add(
                ent->current.origin,
                QM_Vector3Scale( QM_Vector3Add( ent->mins, ent->maxs ), 0.5f )
            );

            cm_trace_shape_t shapeA = { .type = SHAPE_CAPSULE, .extents = Vector3(0,0,0), .radius = radius, .halfHeight = halfHeight };
            trace = clgi.CM_AnalyticalShapeSweep( &start, &shapeA, &end, &center, &touchShape );

			// Reconstruct impact endpoint from sweep fraction when we hit.
            if ( trace.fraction < 1.0f ) {
                trace.entityNumber = ent->current.number;
                trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
                trace.endpos = QM_Vector3Add( start, QM_Vector3Scale( QM_Vector3Subtract( end, start ), trace.fraction ) );
                trace.contents = CONTENTS_SOLID;
            } else {
				// No hit; keep end point unchanged.
                trace.endpos = end;
            }
        } else {
			/**
			*	For AABB brush solids, use transformed hull capsule tracing against the entity hull.
			**/
            mnode_t *headNode = clgi.GetEntityHullNode( ent );

            trace = clgi.CM_TransformedTraceCapsule( 
                &start, &end, radius, halfHeight, headNode, contentMask, &ent->current.origin, &ent->current.angles 
            );
        }

        // Stamp entity-backed traces with the touched entity identity
        if ( trace.allsolid || trace.startsolid || trace.fraction < 1.0f ) {
            trace.entityNumber = ent->current.number;
            trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
        }

		// Keep the closest hit among all tested entities.
        if ( trace.allsolid || trace.fraction < tr->fraction ) {
            const int32_t oldStartSolid = tr->startsolid;
            *tr = trace;
            tr->startsolid = (bool)( (int32_t)tr->startsolid | oldStartSolid );
		// Preserve startsolid information even when this hit is not the closest one.
        } else if ( trace.startsolid ) {
            tr->startsolid = true;
        }
		// Early-out once we are fully enclosed.
        if ( tr->allsolid ) {
            return;
        }
    }
}

/**
*	@brief	Clip one cylinder sweep against all dynamic solid entities in the current frame list.
*	@param	tr				[in,out] Accumulated best trace result to update when a closer hit is found.
*	@param	start			World-space sweep start.
*	@param	radius			Cylinder radius of the moving shape.
*	@param	halfHeight		Cylinder half-height of the moving shape.
*	@param	end				World-space sweep end.
*	@param	passEntity		Optional entity to ignore (self/owner filtering).
*	@param	contentMask		Contents mask controlling which entities are considered collidable.
**/
static void ClipCylinderMoveToEntities( cm_trace_t *tr, const Vector3 &start, const float radius, const float halfHeight, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize one local trace container with safe null/default surface data.
	**/
	cm_trace_t trace = cm_trace_t();
        
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	Iterate all dynamic solid entities and test each candidate against the moving cylinder.
	**/
	for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        centity_t *ent = game.frameEntities.solids[ i ];

		// Skip invalid/non-solid entities immediately.
        if ( ent == nullptr || ent->current.solid == SOLID_NOT ) {
            continue;
        }
		// Skip entities that do not own a collision model.
        if ( !ent->current.modelindex ) {
            continue;
        }
		// Skip players unless caller requested player contents.
        if ( ent->current.number <= game.maxclients && !( contentMask & CONTENTS_PLAYER ) ) {
            continue;
        }
		// Skip the pass entity itself.
        if ( passEntity != nullptr && ent != nullptr && ( ent->current.number == passEntity->current.number ) ) {
            continue;
        }
		// Skip owner/owned pairs so projectiles and owners do not self-collide here.
        if ( passEntity ) {
            if ( ent->current.ownerNumber == passEntity->current.number ) {
                continue;
            }
            if ( passEntity->current.ownerNumber == ent->current.number ) {
                continue;
            }
        }
        
        cm_trace_shape_t touchShape = { .type = SHAPE_AABB, .extents = Vector3(0,0,0), .radius = 0, .halfHeight = 0 };
        if ( ent->current.solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
        else if ( ent->current.solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
        else if ( ent->current.solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

		/**
		*	For analytical primitive solids (sphere/capsule/cylinder), compute extents and
		*	entity-center offset before issuing one analytical sweep.
		**/
        if ( touchShape.type != SHAPE_AABB ) {
            touchShape.extents = QM_Vector3Scale( QM_Vector3Subtract( ent->maxs, ent->mins ), 0.5f );
            touchShape.radius = std::max(touchShape.extents.x, touchShape.extents.y);
            touchShape.halfHeight = touchShape.extents.z;

            Vector3 center = QM_Vector3Add(
                ent->current.origin,
                QM_Vector3Scale( QM_Vector3Add( ent->mins, ent->maxs ), 0.5f )
            );

            cm_trace_shape_t shapeA = { .type = SHAPE_CYLINDER, .extents = Vector3(0,0,0), .radius = radius, .halfHeight = halfHeight };
            trace = clgi.CM_AnalyticalShapeSweep( &start, &shapeA, &end, &center, &touchShape );

			// Reconstruct impact endpoint from sweep fraction when we hit.
            if ( trace.fraction < 1.0f ) {
                trace.entityNumber = ent->current.number;
                trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
                trace.endpos = QM_Vector3Add( start, QM_Vector3Scale( QM_Vector3Subtract( end, start ), trace.fraction ) );
                trace.contents = CONTENTS_SOLID;
            } else {
				// No hit; keep end point unchanged.
                trace.endpos = end;
            }
        } else {
			/**
			*	For AABB brush solids, use transformed hull cylinder tracing against the entity hull.
			**/
            mnode_t *headNode = clgi.GetEntityHullNode( ent );

            trace = clgi.CM_TransformedTraceCylinder( 
                &start, &end, radius, halfHeight, headNode, contentMask, &ent->current.origin, &ent->current.angles 
            );
        }

        // Stamp entity-backed traces with the touched entity identity
        if ( trace.allsolid || trace.startsolid || trace.fraction < 1.0f ) {
            trace.entityNumber = ent->current.number;
            trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
        }

		// Keep the closest hit among all tested entities.
        if ( trace.allsolid || trace.fraction < tr->fraction ) {
            const int32_t oldStartSolid = tr->startsolid;
            *tr = trace;
            tr->startsolid = (bool)( (int32_t)tr->startsolid | oldStartSolid );
		// Preserve startsolid information even when this hit is not the closest one.
        } else if ( trace.startsolid ) {
            tr->startsolid = true;
        }
		// Early-out once we are fully enclosed.
        if ( tr->allsolid ) {
            return;
        }
    }
}

/**
*	@brief	Clip one sphere sweep against all dynamic solid entities in the current frame list.
*	@param	tr				[in,out] Accumulated best trace result to update when a closer hit is found.
*	@param	start			World-space sweep start.
*	@param	radius			Sphere radius of the moving shape.
*	@param	end				World-space sweep end.
*	@param	passEntity		Optional entity to ignore (self/owner filtering).
*	@param	contentMask		Contents mask controlling which entities are considered collidable.
**/
static void ClipSphereMoveToEntities( cm_trace_t *tr, const Vector3 &start, const float radius, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize one local trace container with safe null/default surface data.
	**/
	cm_trace_t trace = cm_trace_t();
        
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	Iterate all dynamic solid entities and test each candidate against the moving sphere.
	**/
	for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        centity_t *ent = game.frameEntities.solids[ i ];

		// Skip invalid/non-solid entities immediately.
        if ( ent == nullptr || ent->current.solid == SOLID_NOT ) {
            continue;
        }
		// Skip entities that do not own a collision model.
        if ( !ent->current.modelindex ) {
            continue;
        }
		// Skip players unless caller requested player contents.
        if ( ent->current.number <= game.maxclients && !( contentMask & CONTENTS_PLAYER ) ) {
            continue;
        }
		// Skip the pass entity itself.
        if ( passEntity != nullptr && ent != nullptr && ( ent->current.number == passEntity->current.number ) ) {
            continue;
        }
		// Skip owner/owned pairs so projectiles and owners do not self-collide here.
        if ( passEntity ) {
            if ( ent->current.ownerNumber == passEntity->current.number ) {
                continue;
            }
            if ( passEntity->current.ownerNumber == ent->current.number ) {
                continue;
            }
        }
        
        cm_trace_shape_t touchShape = { .type = SHAPE_AABB, .extents = Vector3(0,0,0), .radius = 0, .halfHeight = 0 };
        if ( ent->current.solid == SOLID_SPHERE ) touchShape.type = SHAPE_SPHERE;
        else if ( ent->current.solid == SOLID_CAPSULE ) touchShape.type = SHAPE_CAPSULE;
        else if ( ent->current.solid == SOLID_CYLINDER ) touchShape.type = SHAPE_CYLINDER;

		/**
		*	For analytical primitive solids (sphere/capsule/cylinder), compute extents and
		*	entity-center offset before issuing one analytical sweep.
		**/
        if ( touchShape.type != SHAPE_AABB ) {
            touchShape.extents = QM_Vector3Scale( QM_Vector3Subtract( ent->maxs, ent->mins ), 0.5f );
            touchShape.radius = std::max(touchShape.extents.x, touchShape.extents.y);
            touchShape.halfHeight = touchShape.extents.z;

            Vector3 center = QM_Vector3Add(
                ent->current.origin,
                QM_Vector3Scale( QM_Vector3Add( ent->mins, ent->maxs ), 0.5f )
            );

            cm_trace_shape_t shapeA = { .type = SHAPE_SPHERE, .extents = Vector3(0,0,0), .radius = radius, .halfHeight = 0 };
            trace = clgi.CM_AnalyticalShapeSweep( &start, &shapeA, &end, &center, &touchShape );

			// Reconstruct impact endpoint from sweep fraction when we hit.
            if ( trace.fraction < 1.0f ) {
                trace.entityNumber = ent->current.number;
                trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
                trace.endpos = QM_Vector3Add( start, QM_Vector3Scale( QM_Vector3Subtract( end, start ), trace.fraction ) );
                trace.contents = CONTENTS_SOLID;
            } else {
				// No hit; keep end point unchanged.
                trace.endpos = end;
            }
        } else {
			/**
			*	For AABB brush solids, use transformed hull sphere tracing against the entity hull.
			**/
            mnode_t *headNode = clgi.GetEntityHullNode( ent );

            trace = clgi.CM_TransformedTraceSphere( 
                &start, &end, radius, headNode, contentMask, &ent->current.origin, &ent->current.angles 
            );
        }

        // Stamp entity-backed traces with the touched entity identity
        if ( trace.allsolid || trace.startsolid || trace.fraction < 1.0f ) {
            trace.entityNumber = ent->current.number;
            trace.brushID = SG_PackEntityBrushID( ent->current.number, trace.brushID );
        }

		// Keep the closest hit among all tested entities.
        if ( trace.allsolid || trace.fraction < tr->fraction ) {
            const int32_t oldStartSolid = tr->startsolid;
            *tr = trace;
            tr->startsolid = (bool)( (int32_t)tr->startsolid | oldStartSolid );
		// Preserve startsolid information even when this hit is not the closest one.
        } else if ( trace.startsolid ) {
            tr->startsolid = true;
        }
		// Early-out once we are fully enclosed.
        if ( tr->allsolid ) {
            return;
        }
    }
}

/**
*   @brief  Substituting the below 'CL_PM_Trace' implementation:
**/
const cm_trace_t CLG_Trace( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
    // Initialize to no collision for the initial trace.
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	// First Clip to world.
	trace = clgi.CM_BoxTrace( &start, &end, mins, maxs, clgi.GetEntityHullNode( nullptr ), contentMask );

    // Did we hit world? Set entity number to match with 'worldspawn' entity,
    // and return.
    //
    // Otherwise we set the entity to 'None'(-1), and continue to test and clip to 
    // the remaining server entities.
    trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;
    //if ( trace.fraction == 0 || ( passEntity != nullptr && passEntity->current.number == ENTITYNUM_WORLD ) ) {
    //    return trace;		// blocked immediately by the world
    //}

    // If we are not clipping to the world, and the trace fraction is 1.0,
    // test and clip to other solid entities.
    ClipTraceMoveToEntities( &trace, start, mins, maxs, end, passEntity, contentMask );

    // Return trace.
    return trace;
}

/**
*	@brief	Trace one capsule shape through world geometry and dynamic entities.
*	@param	start		World-space trace start.
*	@param	radius		Capsule radius.
*	@param	halfHeight	Capsule half-height.
*	@param	end			World-space trace end.
*	@param	passEntity	Optional pass-through entity to ignore during entity clipping.
*	@param	contentMask	Contents mask controlling collision filtering.
*	@return	Best collision trace result for the capsule sweep.
**/
const cm_trace_t CLG_TraceCapsule( const Vector3 &start, const float radius, const float halfHeight, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize a trace with safe default/null surface/material payloads.
	**/
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	Run capsule-vs-world trace first so world collisions remain authoritative.
	**/
	trace = clgi.CM_TraceCapsule( &start, &end, radius, halfHeight, clgi.GetEntityHullNode( nullptr ), contentMask );

	/**
	*	Tag the world hit entity id when the world trace blocked movement.
	**/
    trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

	/**
	*	Refine against dynamic entities and keep the closest overall hit.
	**/
    ClipCapsuleMoveToEntities( &trace, start, radius, halfHeight, end, passEntity, contentMask );

	/**
	*	Return the final merged trace result.
	**/
    return trace;
}

/**
*	@brief	Trace one cylinder shape through world geometry and dynamic entities.
*	@param	start		World-space trace start.
*	@param	radius		Cylinder radius.
*	@param	halfHeight	Cylinder half-height.
*	@param	end			World-space trace end.
*	@param	passEntity	Optional pass-through entity to ignore during entity clipping.
*	@param	contentMask	Contents mask controlling collision filtering.
*	@return	Best collision trace result for the cylinder sweep.
**/
const cm_trace_t CLG_TraceCylinder( const Vector3 &start, const float radius, const float halfHeight, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize a trace with safe default/null surface/material payloads.
	**/
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	Run cylinder-vs-world trace first so world collisions remain authoritative.
	**/
	trace = clgi.CM_TraceCylinder( &start, &end, radius, halfHeight, clgi.GetEntityHullNode( nullptr ), contentMask );

	/**
	*	Tag the world hit entity id when the world trace blocked movement.
	**/
    trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

	/**
	*	Refine against dynamic entities and keep the closest overall hit.
	**/
    ClipCylinderMoveToEntities( &trace, start, radius, halfHeight, end, passEntity, contentMask );

	/**
	*	Return the final merged trace result.
	**/
    return trace;
}

/**
*	@brief	Trace one sphere shape through world geometry and dynamic entities.
*	@param	start		World-space trace start.
*	@param	radius		Sphere radius.
*	@param	end			World-space trace end.
*	@param	passEntity	Optional pass-through entity to ignore during entity clipping.
*	@param	contentMask	Contents mask controlling collision filtering.
*	@return	Best collision trace result for the sphere sweep.
**/
const cm_trace_t CLG_TraceSphere( const Vector3 &start, const float radius, const Vector3 &end, const centity_t *passEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize a trace with safe default/null surface/material payloads.
	**/
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	Run sphere-vs-world trace first so world collisions remain authoritative.
	**/
	trace = clgi.CM_TraceSphere( &start, &end, radius, clgi.GetEntityHullNode( nullptr ), contentMask );

	/**
	*	Tag the world hit entity id when the world trace blocked movement.
	**/
    trace.entityNumber = trace.fraction != 1.0 ? ENTITYNUM_WORLD : ENTITYNUM_NONE;

	/**
	*	Refine against dynamic entities and keep the closest overall hit.
	**/
    ClipSphereMoveToEntities( &trace, start, radius, end, passEntity, contentMask );

	/**
	*	Return the final merged trace result.
	**/
    return trace;
}

/**
*   @brief  Will perform a clipping trace to the specified entity.
*           If clipEntity == nullptr, it'll perform a clipping trace against the World.
**/
const cm_trace_t CLG_Clip( const Vector3 &start, const Vector3 *mins, const Vector3 *maxs, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentMask ) {
    // Initialize to no collision for the initial trace.
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

    // Clip against World:
    if ( clipEntity == nullptr || clipEntity == clg_entities ) {
        trace = clgi.CM_BoxTrace( &start, &end, mins, maxs, clgi.GetEntityHullNode( nullptr ), contentMask);
        // Clip against clipEntity.
	} else {
		/**
		*   Convert the moving bounds into the analytical representation used for primitive targets.
		**/
		cm_trace_shape_t movingShape = { .type = SHAPE_AABB, .extents = {}, .radius = 0.0f, .halfHeight = 0.0f };
		Vector3 shapeStart = start;
		Vector3 shapeEnd = end;
		if ( mins && maxs ) {
			movingShape.extents = QM_Vector3Scale( QM_Vector3Subtract( *maxs, *mins ), 0.5f );
			movingShape.radius = std::max( movingShape.extents.x, movingShape.extents.y );
			movingShape.halfHeight = movingShape.extents.z;
			const Vector3 offset = QM_Vector3Scale( QM_Vector3Add( *mins, *maxs ), 0.5f );
			shapeStart = QM_Vector3Add( shapeStart, offset );
			shapeEnd = QM_Vector3Add( shapeEnd, offset );
		}

		/**
		*   Primitive entities have no guaranteed BSP hull, so use direct shape clipping for them.
		**/
		if ( clipEntity->current.solid == SOLID_CAPSULE
			|| clipEntity->current.solid == SOLID_CYLINDER
			|| clipEntity->current.solid == SOLID_SPHERE ) {
			trace = CLG_TraceShapeAgainstDynamicEntity( shapeStart, movingShape, shapeEnd, clipEntity );
		} else {
			/**
			*   Retain transformed BSP tracing for box and brush-model entities.
			**/
			mnode_t *headNode = clgi.GetEntityHullNode( clipEntity );
			if ( headNode != nullptr ) {
				trace = clgi.CM_TransformedBoxTrace(
					&start, &end,
					mins, maxs,
					headNode, contentMask,
					&clipEntity->current.origin, &clipEntity->current.angles
				);

				if ( trace.fraction < 1.0f ) {
					trace.entityNumber = clipEntity->current.number;
				}
			}
		}
	}

    return trace;
}

/**
*	@brief	Perform a capsule trace against one clip target or world.
*	@param	start		World-space trace start.
*	@param	radius		Capsule radius.
*	@param	halfHeight	Capsule half-height.
*	@param	end			World-space trace end.
*	@param	clipEntity	Optional entity to clip against; world is used when null/world entity.
*	@param	contentMask	Contents mask controlling collision filtering.
*	@return	Trace result for the requested clip target.
**/
const cm_trace_t CLG_ClipCapsule( const Vector3 &start, const float radius, const float halfHeight, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize one trace result with safe null/default surface payloads.
	**/
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	When no explicit entity is provided, trace against world geometry.
	**/
    if ( clipEntity == nullptr || clipEntity == clg_entities ) {
        trace = clgi.CM_TraceCapsule( &start, &end, radius, halfHeight, clgi.GetEntityHullNode( nullptr ), contentMask);
    } else {
		/**
		*	Resolve the target entity hull and trace in its transformed local space.
		**/
		if ( clipEntity->current.solid == SOLID_CAPSULE
			|| clipEntity->current.solid == SOLID_CYLINDER
			|| clipEntity->current.solid == SOLID_SPHERE ) {
			const cm_trace_shape_t movingShape = { .type = SHAPE_CAPSULE, .extents = {}, .radius = radius, .halfHeight = halfHeight };
			trace = CLG_TraceShapeAgainstDynamicEntity( start, movingShape, end, clipEntity );
		} else {
			mnode_t *headNode = clgi.GetEntityHullNode( clipEntity );
			if ( headNode != nullptr ) {
				trace = clgi.CM_TransformedTraceCapsule(
					&start, &end,
					radius, halfHeight,
					headNode, contentMask,
					&clipEntity->current.origin, &clipEntity->current.angles
				);
				// Tag hit entity id when the transformed trace blocked movement.
				if ( trace.fraction < 1.0f ) {
					trace.entityNumber = clipEntity->current.number;
				}
			}
		}
	}
	// Return final trace payload.
    return trace;
}

/**
*	@brief	Perform a cylinder trace against one clip target or world.
*	@param	start		World-space trace start.
*	@param	radius		Cylinder radius.
*	@param	halfHeight	Cylinder half-height.
*	@param	end			World-space trace end.
*	@param	clipEntity	Optional entity to clip against; world is used when null/world entity.
*	@param	contentMask	Contents mask controlling collision filtering.
*	@return	Trace result for the requested clip target.
**/
const cm_trace_t CLG_ClipCylinder( const Vector3 &start, const float radius, const float halfHeight, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize one trace result with safe null/default surface payloads.
	**/
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	When no explicit entity is provided, trace against world geometry.
	**/
    if ( clipEntity == nullptr || clipEntity == clg_entities ) {
        trace = clgi.CM_TraceCylinder( &start, &end, radius, halfHeight, clgi.GetEntityHullNode( nullptr ), contentMask);
    } else {
		/**
		*	Resolve the target entity hull and trace in its transformed local space.
		**/
		if ( clipEntity->current.solid == SOLID_CAPSULE
			|| clipEntity->current.solid == SOLID_CYLINDER
			|| clipEntity->current.solid == SOLID_SPHERE ) {
			const cm_trace_shape_t movingShape = { .type = SHAPE_CYLINDER, .extents = {}, .radius = radius, .halfHeight = halfHeight };
			trace = CLG_TraceShapeAgainstDynamicEntity( start, movingShape, end, clipEntity );
		} else {
			mnode_t *headNode = clgi.GetEntityHullNode( clipEntity );
			if ( headNode != nullptr ) {
				trace = clgi.CM_TransformedTraceCylinder(
					&start, &end,
					radius, halfHeight,
					headNode, contentMask,
					&clipEntity->current.origin, &clipEntity->current.angles
				);
				// Tag hit entity id when the transformed trace blocked movement.
				if ( trace.fraction < 1.0f ) {
					trace.entityNumber = clipEntity->current.number;
				}
			}
		}
	}
	// Return final trace payload.
    return trace;
}

/**
*	@brief	Perform a sphere trace against one clip target or world.
*	@param	start		World-space trace start.
*	@param	radius		Sphere radius.
*	@param	end			World-space trace end.
*	@param	clipEntity	Optional entity to clip against; world is used when null/world entity.
*	@param	contentMask	Contents mask controlling collision filtering.
*	@return	Trace result for the requested clip target.
**/
const cm_trace_t CLG_ClipSphere( const Vector3 &start, const float radius, const Vector3 &end, const centity_t *clipEntity, const cm_contents_t contentMask ) {
	/**
	*	Initialize one trace result with safe null/default surface payloads.
	**/
	cm_trace_t trace = cm_trace_t();
	trace.surface = clgi.CM_GetNullSurface();
	trace.material = clgi.CM_GetDefaultMaterial();
	trace.surface2 = clgi.CM_GetNullSurface();
	trace.material2 = clgi.CM_GetDefaultMaterial();

	/**
	*	When no explicit entity is provided, trace against world geometry.
	**/
    if ( clipEntity == nullptr || clipEntity == clg_entities ) {
        trace = clgi.CM_TraceSphere( &start, &end, radius, clgi.GetEntityHullNode( nullptr ), contentMask);
    } else {
		/**
		*	Resolve the target entity hull and trace in its transformed local space.
		**/
		if ( clipEntity->current.solid == SOLID_CAPSULE
			|| clipEntity->current.solid == SOLID_CYLINDER
			|| clipEntity->current.solid == SOLID_SPHERE ) {
			const cm_trace_shape_t movingShape = { .type = SHAPE_SPHERE, .extents = {}, .radius = radius, .halfHeight = 0.0f };
			trace = CLG_TraceShapeAgainstDynamicEntity( start, movingShape, end, clipEntity );
		} else {
			mnode_t *headNode = clgi.GetEntityHullNode( clipEntity );
			if ( headNode != nullptr ) {
				trace = clgi.CM_TransformedTraceSphere(
					&start, &end,
					radius,
					headNode, contentMask,
					&clipEntity->current.origin, &clipEntity->current.angles
				);
				// Tag hit entity id when the transformed trace blocked movement.
				if ( trace.fraction < 1.0f ) {
					trace.entityNumber = clipEntity->current.number;
				}
			}
		}
	}
	// Return final trace payload.
    return trace;
}

/**
*   @brief  Player Move specific 'PointContents' implementation:
**/
const cm_contents_t CLG_PointContents( const Vector3 &point ) {
    // Perform point contents against world.
    cm_contents_t contents = clgi.CM_PointContents( &point, clgi.GetEntityHullNode( nullptr ) );
    // We hit world, so return contents.
    if ( contents != CONTENTS_NONE ) {
        return contents;
    }
    // If we hit CONTENTS_NONE then resume to test against frame's solid entities.
    for ( int32_t i = 0; i < game.frameEntities.numSolids; i++ ) {
        // Clip against all brush entity models.
        centity_t *ent = game.frameEntities.solids[ i ];

        // BSP Brush Model Entity:
        mnode_t *headNode = clgi.GetEntityHullNode( ent );

        // Might intersect, so do an exact clip.
        contents |= clgi.CM_TransformedPointContents( &point, headNode, &ent->current.origin, &ent->current.angles );
    }

    // Et voila.
    return contents;
}