
// Game related types.
#include "svgame/svg_local.h"
// Save related types.
#include "svgame/svg_save.h"



/**
*	@brief	Constructor for the svg_save_funcptr_instance_t class.
**/
svg_save_funcptr_instance_t::svg_save_funcptr_instance_t( const char *name, svg_save_funcptr_type_t type, void *ptr ) :
    name( name ),
    saveAbleType( type ),
    ptr( ptr ) {
    // Generate the hashed version of the actual name string.
    hashedName = Q_HashCaseInsensitiveString( name );

    // Add this instance to the linked list.  
    if ( !head ) {
        head = this;
    } else {
        previous = head;
        head = this;
    }

    //gi.dprintf( "Adding saveable function pointer ID(#%d), name(\"%s\")", saveAbleTypeID.GetID(), name);
}

/**
*	@brief	Gets the ptr which matches the passed name as well as the actual type of function pointer
**/
inline svg_save_funcptr_instance_t *svg_save_funcptr_instance_t::GetForNameType( const char *name, svg_save_funcptr_type_t type ) {
    // Start at the head.
    svg_save_funcptr_instance_t *current = head;
    // Iterate while we got an instance.
    while ( current ) {
        // Compare by hashed name
        if ( current->hashedName == Q_HashCaseInsensitiveString( name ) ) {
            // Compare the actual name and type.
            if ( !strcmp( current->name, name ) && current->saveAbleType == type ) {
                return current;
            }
        }

        current = current->previous;
    }
    return nullptr;
}
/**
*	@brief	Gets a ptr to the saveable instance which matches the passed name as well as the actual address function pointer
**/
inline svg_save_funcptr_instance_t *svg_save_funcptr_instance_t::GetForPointerType( svg_save_funcptr_type_t type, void *ptr ) {
    // Start at the head.
    svg_save_funcptr_instance_t *current = head;
    // Iterate while we got an instance.
    while ( current ) {
        // Compare by hashed name
        if ( current->saveAbleType == type ) {
            // Compare the actual name and type.
            if ( current->ptr == ptr ) {
                return current;
            }
        }
        current = current->previous;
    }
    return nullptr;
}
/**
*	@brief	Gets a ptr to the saveable instance which matches the actual address function pointer.
**/
svg_save_funcptr_instance_t *svg_save_funcptr_instance_t::GetForPointer( void *ptr ) {
    // Start at the head.
    svg_save_funcptr_instance_t *current = head;
    // Iterate while we got an instance.
    while ( current ) {
        // Compare the actual name and type.
        if ( current->ptr == ptr ) {
            return current;
        }
        current = current->previous;
    }
    return nullptr;
}
/**
*	@brief	Gets a ptr to the saveable instance which matches the passed saveAbleTypeID
**/
svg_save_funcptr_instance_t *svg_save_funcptr_instance_t::GetForTypeID( const size_t funcPtrID ) {
    // Start at the head.
    svg_save_funcptr_instance_t *current = head;
    // Iterate while we got an instance.
    while ( current ) {
        // Compare by hashed name
        //if ( current->saveAbleType == type ) {
            // Compare the actual name and type.
        if ( current->saveAbleTypeID.GetID() == funcPtrID ) {
            return current;
        }
        //}
        current = current->previous;
    }
    return nullptr;
}