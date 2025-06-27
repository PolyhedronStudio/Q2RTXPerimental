/********************************************************************
*
*
*
*	CppLib: Base Library for the C++ Portion of Q2RTXPerimental
*
*	shEntity: Base Class Object, all entities inherit from.
*
*
*
********************************************************************/
#pragma once

/**
*	@brief	Base Entity Class.
**/
struct shEntity { 
	//! Default Constructor.
	[[nodiscard]] shEntity() = default;
	//! Destructor.
	[[nodiscard]] virtual ~shEntity() = default;
};