/*********************************************************************
*
*
*	SVGame: Signal In/Out Mechanics:
*
*
********************************************************************/
#pragma once



/**
*   @brief  Describes the value type of a signal's argument.
**/
typedef enum {
    //! Argument type wasn't set!
    SIGNAL_ARGUMENT_TYPE_NONE = 0,

    //! Boolean type.
    SIGNAL_ARGUMENT_TYPE_BOOLEAN,
    //! Integer type.
    //SIGNAL_ARGUMENT_TYPE_INTEGER,
    //! Double type.
    SIGNAL_ARGUMENT_TYPE_NUMBER,
    //! String type.
    SIGNAL_ARGUMENT_TYPE_STRING,
    //! (Nullptr) type.
    SIGNAL_ARGUMENT_TYPE_NULLPTR,
} svg_signal_argument_type_t;

/**
*   @brief  Holds a Signal's argument its key, type, as well as its value.
**/
typedef struct svg_signal_argument_s {
    //! Type.
    svg_signal_argument_type_t type;
    //! Key index name of the argument for usage within the lua table.
    const char *key;
    //! Value.
    union {
        // Boolean representation.
        bool boolean;
        //! Integer representation.
        int64_t integer;
        //! Number(double) representation.
        double number;
        //! String representation.
        const char *str;
    } value;
} svg_signal_argument_t;

//! Typedef an std::vector for varying argument counts.
typedef std::vector<svg_signal_argument_t> svg_signal_argument_array_t;

/**
*   @brief
**/
//void SVG_SignalOut( svg_base_edict_t *ent, svg_base_edict_t *sender, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_t *signalArguments = nullptr, const int32_t numberOfSignalArguments = 0 );
void SVG_SignalOut( svg_base_edict_t *ent, svg_base_edict_t *signaller, svg_base_edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments = {} );

/**
*
**/
template<typename T> inline constexpr T SVG_SignalArguments_GetValue( const svg_signal_argument_array_t &signalArguments, const char *keyName, const T defaultValue ) {
    // Iterate the arguments.
    for ( int32_t argumentIndex = 0; argumentIndex < signalArguments.size(); argumentIndex++ ) {
        // Get access to argument.
        const svg_signal_argument_t &signalArgument = signalArguments[ argumentIndex ];

        if constexpr ( std::is_floating_point_v<T> ) {
            if ( Q_strcasecmp( signalArgument.key, keyName ) && signalArgument.type == SIGNAL_ARGUMENT_TYPE_NUMBER ) {
                return signalArgument.value.number;
            }
        } else if constexpr ( std::is_integral_v<T> ) {
            #if 0
            if ( Q_strcasecmp( signalArgument.key, keyName ) && signalArgument.type == SIGNAL_ARGUMENT_TYPE_INTEGER ) {
            #else
            if ( Q_strcasecmp( signalArgument.key, keyName ) && signalArgument.type == SIGNAL_ARGUMENT_TYPE_NUMBER ) {
            #endif
                return signalArgument.value.integer;
            }
        } else if constexpr ( std::is_same_v<T, const char *> ) {
            if ( Q_strcasecmp( signalArgument.key, keyName ) && signalArgument.type == SIGNAL_ARGUMENT_TYPE_STRING ) {
                return signalArgument.value.str;
            }
        } else if constexpr ( std::is_same_v<T, bool> ) {
            if ( Q_strcasecmp( signalArgument.key, keyName ) && signalArgument.type == SIGNAL_ARGUMENT_TYPE_BOOLEAN ) {
                return signalArgument.value.boolean;
            }
        }
    }

    return defaultValue;
}