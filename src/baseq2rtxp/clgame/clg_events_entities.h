/********************************************************************
*
*
*	ClientGame: (Entity/Player State) -Events:
*
*
********************************************************************/
#pragma once

#include "shared/shared.h"

/**
*   @brief  Processes the given entity event.
*   @param  eventValue          The event value to process.
*   @param  lerpOrigin          The lerp origin of the entity generating the event.
*   @param  cent                The centity_t generating the event.
*   @param  entityNumber        The entity number of the entity generating the event.
*   @param  clientNumber        The client number of the entity generating the event, if any (-1 otherwise.).
*   @param  clientInfo          The clientinfo_t of the entity generating the event, if any (nullptr otherwise).
**/
void CLG_Events_FireEntityEvent( const int32_t eventValue, const Vector3 &lerpOrigin, centity_t *cent, const int32_t entityNumber, const int32_t clientNumber, clientinfo_t *clientInfo );
