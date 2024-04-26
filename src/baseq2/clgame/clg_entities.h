/********************************************************************
*
*
*	ClientGame:	General Entities Header
*
*
********************************************************************/
// Use a static entity ID on some things because the renderer relies on eid to match between meshes
// on the current and previous frames.
static constexpr int32_t RENTITIY_RESERVED_GUN = 1;
static constexpr int32_t RENTITIY_RESERVED_TESTMODEL = 2;
static constexpr int32_t RENTITIY_RESERVED_COUNT = 3;


static constexpr int32_t RENTITIY_OFFSET_LOCALENTITIES = MAX_MODELS / 2; // TODO: Increase MAX_MODELS * 2 for refresh and all that.

/**
*   @brief  For debugging problems when out-of-date entity origin is referenced.
**/
#if USE_DEBUG
void CLG_CheckEntityPresent( int32_t entityNumber, const char *what );
#endif
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
void PF_GetEntitySoundOrigin( const int32_t entityNumber, vec3_t org );
/**
*   @brief  The sound code makes callbacks to the client for entitiy position
*           information, so entities can be dynamically re-spatialized.
**/
qhandle_t PF_GetEAXBySoundOrigin( const int32_t entityNumber, vec3_t org );
/**
*	@return		A pointer to the entity bound to the client game's view. Unless STAT_CHASE is set to
*               a specific client number the current received frame, this'll point to the entity that
*               is of the local client player himself.
**/
centity_t *CLG_ViewBoundEntity( void );
