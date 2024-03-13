/********************************************************************
*
*
*   Collision Model Entity Key/Value Definitions:
*
*
********************************************************************/
#pragma once


//! key / value pair sizes
#define     MAX_KEY         32
#define     MAX_VALUE       1024

/**
*   @brief  Entity key-value pair parsed types.
**/
typedef enum cm_entity_parsed_type_s {
    //! A value of one or more characters is available.
    ENTITY_STRING = 0x1,

    //! An integer value is available.
    ENTITY_INTEGER = 0x2,
    //! A float value is available.
    ENTITY_FLOAT = 0x4,

    //!A two component vector value is available.
    ENTITY_VECTOR2 = 0x8,
    //! A three component vector value is available.
    ENTITY_VECTOR3 = 0x10,
    //! A four component vector is available.
    ENTITY_VECTOR4 = 0x20,
} cm_entity_parsed_type_t;

/**
*   @brief  Contains the data of each key/value pair entry for the parsed bsp entity string results.
**/
typedef struct cm_entity_s {
    //! A bitmask of the valid entity parsed value types for this key value pair.
    cm_entity_parsed_type_t parsed_type;

    //! The entity pair key.
    char key[ MAX_KEY ];

    /**
    *   @brief      The entity pair value, as a string.
    *   @remarks    This will always be a null-termianted C string.
    **/
    char string[ MAX_VALUE ];

    /**
    *   @brief      The entity pair value, as a nullable string pointer.
    *   @remarks    This will be `NULL` if no string was present.
    **/
    char *nullable_string;

    //! @brief The entity pair value, as an integer.
    int32_t integer;

    /**
    *   Float and Vector pair value members.Use a union to save space.
    **/
    // !The entity pair value, as a float.
    float value;

    //! The entity pair value represented as floating point component vectors.
    union {
        //! The entity pair value, as a two component vector.
        vec2_t vec2;
        //! The entity pair value, as a three component vector.
        vec3_t vec3;
        //! The entity pair value, as a four component vector.
        vec4_t vec4;
    };

    //! The next entity pair in this entity, or `NULL`.
    struct cm_entity_s *next;
} cm_entity_t;