/********************************************************************
*
*
*    ServerGame: Nav3 Core Types - Implementation
*
*
********************************************************************/
#include "svgame/nav3/nav3_types.h"


/**
*
*
*    Nav3 Runtime Type Helpers:
*
*
**/
/**
*    @brief  Return a readable publication-state label.
*    @param  publishState  State to convert.
*    @return Stable string label for diagnostics.
**/
const char *SVG_Nav3_RuntimePublishStateName( const nav3_runtime_publish_state_t publishState ) {
	switch ( publishState ) {
	case nav3_runtime_publish_state_t::Empty:
		return "empty";
	case nav3_runtime_publish_state_t::StubPending:
		return "stub_pending";
	case nav3_runtime_publish_state_t::Published:
		return "published";
	case nav3_runtime_publish_state_t::Count:
	default:
		break;
	}
	return "unknown";
}
