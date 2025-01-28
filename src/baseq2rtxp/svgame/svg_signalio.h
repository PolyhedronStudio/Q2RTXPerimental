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
//void SVG_SignalOut( edict_t *ent, edict_t *sender, edict_t *activator, const char *signalName, const svg_signal_argument_t *signalArguments = nullptr, const int32_t numberOfSignalArguments = 0 );
void SVG_SignalOut( edict_t *ent, edict_t *signaller, edict_t *activator, const char *signalName, const svg_signal_argument_array_t &signalArguments = {} );
