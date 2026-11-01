#pragma once
#include <cstdint>

/**
*	@brief	Issue an owner-thread revision from the current service epoch.
*	@return	Nonzero identity; overflow drains readers and starts a new epoch.
*	@note	Transient navigation identities are never serialized with entities.
**/
uint64_t Nav_IssueIdentityRevisionMainThread( void );
/**
*	@brief	Read the epoch invalidating all identities from a controlled service reset.
**/
uint64_t Nav_IdentityEpochMainThread( void );
