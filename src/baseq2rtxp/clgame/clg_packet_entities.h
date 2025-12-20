/********************************************************************
*
*
*	ClientGame: Packet Entities Header.
* 
*	Each Packet Entity comes with a specific Entity Type set for it
*	in the nextState. Most are added by CLG_PacketEntity_AddGeneric.
*	
*	However for various distinct ET_* types there exist specific
*	submethods to handle those. This should bring some structure to
*	CLG_AddPacketEntities :-)
*
*
********************************************************************/
#pragma once



/**
*	@brief	Will add all packet entities to the current frame's view refdef
**/
void CLG_AddPacketEntities( void );



/**
* 
* 
* 
*	Submethods for Entity Types encountered CLG_AddPacketEntities:
* 
* 
* 
**/
/**
*	@brief	Will setup the refresh entity for the ET_BEAM centity with the nextState.
**/
void CLG_PacketEntity_AddBeam( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_GENERIC centity with the nextState.
**/
void CLG_PacketEntity_AddGeneric( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_GIB centity with the nextState.
**/
void CLG_PacketEntity_AddGib( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_ITEM centity with the nextState.
**/
void CLG_PacketEntity_AddItem( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_MONSTER centity with the nextState.
**/
void CLG_PacketEntity_AddMonster( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_PLAYER centity with the nextState.
**/
void CLG_PacketEntity_AddPlayer( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_PUSHER centity with the nextState.
**/
void CLG_PacketEntity_AddPusher( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
/**
*	@brief	Will setup the refresh entity for the ET_SPOTLIGHT centity with the nextState.
**/
void CLG_PacketEntity_AddSpotlight( centity_t *packetEntity, entity_t *refreshEntity, entity_state_t *nextState );
