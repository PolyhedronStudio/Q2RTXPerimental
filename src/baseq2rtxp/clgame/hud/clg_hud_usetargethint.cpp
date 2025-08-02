/********************************************************************
*
*
*   Client 'Heads Up Display':
*
*
********************************************************************/
#include "clgame/clg_local.h"

#include "sharedgame/sg_shared.h"
#include "sharedgame/sg_usetarget_hints.h"

#include "clgame/clg_hud.h"
#include "clgame/clg_main.h"
#include "clgame/clg_screen.h"




//! Used in various places.
extern const bool SCR_ShouldDrawPause();

/**
*
*
*
*
*   HUD UseTarget Hint Display:
*
*
*
*
**/
/**
*   @brief  Cheesy, cheap, utility. I hate strings, don't you? :-)
**/
static void Q_std_string_replace_all( std::string &str, const std::string &from, const std::string &to ) {
    if ( from.empty() )
        return;
    size_t start_pos = 0;
    while ( ( start_pos = str.find( from, start_pos ) ) != std::string::npos ) {
        str.replace( start_pos, from.length(), to );
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}
/**
*   @brief  Cheesy, cheap, utility. I hate strings, don't you? :-)
**/
std::vector<std::string_view> Q_std_string_split( std::string_view buffer, const std::string_view delimiter = " " ) {
    std::vector<std::string_view> result;
    std::string_view::size_type pos;

    while ( ( pos = buffer.find( delimiter ) ) != std::string_view::npos ) {
        auto match = buffer.substr( 0, pos );
        if ( !match.empty() ) {
            result.push_back( match );
        }
        buffer.remove_prefix( pos + delimiter.size() );
    }

    if ( !buffer.empty() ) {
        result.push_back( buffer );
    }

    return result;
}

/**
*   @brief  Will look for [command] in the string, and replace it by the matching [KEY] value.
*   @note   Operates on the data contained in the reference of hudToken.
**/
static const bool HUD_FormatToken_KeyForCommand( std::string_view &stringToken, hud_usetarget_hint_token_t &hudToken ) {
    // Does it begin with a '[' ?
    const bool startWithAngleBracket = stringToken.starts_with( "[" );
    // Does it end with a '[' ?
    const bool endWithAngleBracket = stringToken.ends_with( "]" );

    // If both are valid, acquire us the keyname for the action.
    if ( startWithAngleBracket && endWithAngleBracket ) {
        // Get action name.
        size_t firstAngleBracketPos = hudToken.value.find_first_of( '[' ) + 1;
        size_t nextAngleBracketPos = hudToken.value.find_first_of( ']' ) - 1;
        hudToken.value = hudToken.value.substr( firstAngleBracketPos, nextAngleBracketPos );

        const std::string_view actionName = hudToken.value;

        // Invalid/empty action name.
        if ( hudToken.value.empty() ) {
            return false;
        }

        // Find the keyname for the given action.
        std::string keyName = clgi.Key_GetBinding( hudToken.value.c_str() );
        // Failure if keyname == ""
        if ( keyName == "" ) {
            return false;
        }

        // Transform keyname to uppercase.
        std::transform( keyName.begin(), keyName.end(), keyName.begin(), ::toupper );

        // Replace our stringToken by [keyName]
        hudToken.value = std::string( "[" + keyName + "]" );
        hudToken.type = HUD_TOKEN_TYPE_TOKEN_KEYNAME;

        // True.
        return true;
    }

    return false;
}
/**
*   @brief  Will look for (+Action) or (-Action) in the string, in which case it is token tagged as ACTIVE/DEACTIVATE
*   @note   Operates on the data contained in the reference of hudToken.
**/
static const bool HUD_FormatToken_ForAction( std::string_view &stringToken, hud_usetarget_hint_token_t &hudToken ) {
    // Does it begin with a '(' ?
    const bool startWithCurlyBrace = hudToken.value.starts_with( "(" );
    // Does it end with a ')' ?
    const bool endWithCurlyBrace = hudToken.value.ends_with( ")" );

    // If both are valid, acquire us the keyname for the action.
    if ( startWithCurlyBrace && endWithCurlyBrace ) {
        // Get action name.
        size_t firstCurlyBracePos = hudToken.value.find_first_of( '(' ) + 1;
        size_t nextCurlyBracePos = hudToken.value.find_first_of( ')' ) - 1;
        // Determine whether it is a + or -, thus Activate or Deactivate.
        size_t firstOfPlus = hudToken.value.find_first_of( '+' );
        size_t firstOfMinus = hudToken.value.find_first_of( '-' );
        hudToken.value = hudToken.value.substr( firstCurlyBracePos, nextCurlyBracePos );

        // Invalid/empty action name.
        if ( hudToken.value.empty() ) {
            return false;

        }
        //clgi.Print( PRINT_DEVELOPER, "%s\n", hudToken.value.c_str() );

        if ( firstOfMinus == std::string::npos && firstOfPlus == std::string::npos ) {
            hudToken.value = hudToken.value.substr( 0, hudToken.value.size() ); // hudToken.value.substr( 1, hudToken.value.size() - 2 );
            // TODO: WID: Yeah??
            hudToken.type = HUD_TOKEN_TYPE_NOTE; // Or _REGULAR??
            // If we found a plus, redo the string value.
        } else if ( firstOfPlus != std::string::npos ) {
            hudToken.value = hudToken.value.substr( firstOfPlus, hudToken.value.size() ),
                hudToken.type = HUD_TOKEN_TYPE_ACTION_ACTIVATE;
            // If we found a minus, redo the string value.
        } else if ( firstOfMinus != std::string::npos ) {
            hudToken.value = hudToken.value.substr( firstOfMinus, hudToken.value.size() ),
                hudToken.type = HUD_TOKEN_TYPE_ACTION_DEACTIVATE;
            // Otherwise it is of type 'NOTE', use for referring to 'things'.
        } else {

        }

        // True.
        return true;
    }

    return false;
}

/**
*   @brief  Parse the usetarget info string properly, replace [+usetarget] etc.
**/
static const std::vector<hud_usetarget_hint_token_t> HUD_FormatUseTargetHintString( const std::string &useTargetHintString ) {
    // Actual final resulting tokens.
    std::vector<hud_usetarget_hint_token_t> hintTokens;

    // First split up the string by spaces right into a vector of strings. I know, inefficient.
    std::vector<std::string_view> stringTokens = Q_std_string_split( useTargetHintString, " " );
    // Iterate all tokens and determine their type, then push them back into the hintTokens vector.
    for ( std::string_view &stringToken : stringTokens ) {
        // The type of token, default to normal.
        hud_usetarget_hint_token_t hudToken = {
            .value = std::string( stringToken ),
            .type = HUD_TOKEN_TYPE_NORMAL,
        };
        // Format for a keys action. [+usetarget] etc.
        if ( HUD_FormatToken_KeyForCommand( stringToken, hudToken ) ) {

            // Format for an Action('Activate/Deactivate') defined by (+ActionName)/(-ActionName).
        } else if ( HUD_FormatToken_ForAction( stringToken, hudToken ) ) {

        }
        // Push back the token into the vector.
        hintTokens.push_back( hudToken );
    }

    // Return tokens.
    return hintTokens;
}

/**
*   @brief  Takes care of the actual drawing of specified targetHintInfo.
**/
void HUD_DrawTargetHintInfo( hud_usetarget_hint_t *targetHintInfo ) {
    // Clear the original color.
    clgi.R_ClearColor();
    clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );

    // Determine center of screen.
    const int32_t x = ( clg_hud.hud_scaled_width/*- scr.pause_width */ ) / 2;
    const int32_t y = ( clg_hud.hud_scaled_height /*- scr.pause_height */ ) / 2;

    // Current draw offset X axis.
    int32_t xOffset = 0;
    const int32_t yOffset = CHAR_HEIGHT * 4.5;
    // Iterate to acquire total width.. sight lol.
    const int32_t xWidth = HUD_GetTokenVectorDrawWidth( targetHintInfo->hintStringTokens );
    // X Start location.
    int32_t xDrawLocation = x - ( xWidth / 2 );
    // Iterate.
    for ( auto &hintStringToken : targetHintInfo->hintStringTokens ) {
        // Draw piece, special color if it is the keyname.
        if ( hintStringToken.type == HUD_TOKEN_TYPE_TOKEN_KEYNAME ) {
            // Clear the original color.
            clgi.R_ClearColor();
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawAltString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else if ( hintStringToken.type == HUD_TOKEN_TYPE_ACTION_ACTIVATE ) {
            // Set color.
            clgi.R_SetColor( U32_GREEN );
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else if ( hintStringToken.type == HUD_TOKEN_TYPE_ACTION_DEACTIVATE ) {
            // Set color.
            clgi.R_SetColor( U32_RED );
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else if ( hintStringToken.type == HUD_TOKEN_TYPE_NOTE ) {
            // Set color.
            //clgi.R_SetColor( clg_hud.colors.ORANGE2 );
            clgi.R_SetColor( U32_YELLOW );
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        } else {
            // Clear the original color.
            clgi.R_ClearColor();
            // Set Alpha.
            clgi.R_SetAlpha( targetHintInfo->alpha * scr_alpha->value );
            // Draw it.
            HUD_DrawString( xDrawLocation + xOffset, y + yOffset, hintStringToken.value.c_str() );
        }
        // Increment the piece its width to the offset, including a space.(one more character)
        xOffset += ( HUD_GetStringDrawWidth( hintStringToken.value.c_str() ) + 1 ) + CHAR_WIDTH;
    }
}

/**
*   @brief  Handles detecting changes in the most recent UseTargetHint entity we're targeting,
*           as well as shuffling the hints whilst easing them in/out.
**/
void CLG_HUD_DrawUseTargetHintInfos() {
    //! Don't display if paused.
    if ( SCR_ShouldDrawPause() ) {
        return;
    }

    // Get QMTime for realtime.
    const QMTime realTime = QMTime::FromMilliseconds( clgi.GetRealTime() );

    // Get the useTargetHintIDs from the stats.
    const int32_t currentID = clgi.client->frame.ps.stats[ STAT_USETARGET_HINT_ID ];
    const int32_t lastID = clgi.client->oldframe.ps.stats[ STAT_USETARGET_HINT_ID ];

    //
    // New ID, shuffle current to last, reassign current to fresh new current value.
    //
    if ( currentID != lastID ) {
        // Is it the same currentID as the one we got set?
        if ( currentID == clg_hud.targetHints.currentID ) {
            // Do nothing here.
        // It is a new currentID which we are not displaying.
        } else {
            // Get the useTargetHint information that matches the ID.
            const sg_usetarget_hint_t *currentUseTargetHint = SG_UseTargetHint_GetByID( currentID );

            // Move the current 'top' slot its hud target hint data into the 'bottom' slot.
            clg_hud.targetHints.hints[ HUD_BOTTOM_TARGET_HINT ] = clg_hud.targetHints.hints[ HUD_TOP_TARGET_HINT ];
            // Initiate a new ease.
            clg_hud.targetHints.hints[ HUD_BOTTOM_TARGET_HINT ].easeState = QMEaseState::new_ease_state( realTime, HUD_TARGET_INFO_EASE_DURATION );
            // Store it as our new lastID.
            clg_hud.targetHints.lastID = clg_hud.targetHints.hints[ HUD_BOTTOM_TARGET_HINT ].hintID;

            // It is a valid usetarget.
            if ( currentUseTargetHint && currentUseTargetHint->hintString && ( currentUseTargetHint->index != clg_hud.targetHints.currentID ) ) {
                // Apply new top ID.
                clg_hud.targetHints.hints[ HUD_TOP_TARGET_HINT ] = {
                    .hintID = clg_hud.targetHints.currentID = currentID,
                    .hintString = currentUseTargetHint->hintString,
                    .hintStringTokens = HUD_FormatUseTargetHintString( currentUseTargetHint->hintString ),
                    .easeState = QMEaseState::new_ease_state( realTime + HUD_TARGET_INFO_EASE_DURATION, HUD_TARGET_INFO_EASE_DURATION ),
                    .alpha = 0
                };
            } else {
                // Store lastID.
                clg_hud.targetHints.lastID = ( clg_hud.targetHints.currentID & HUD_TARGETHINT_TOGGLE_BIT ) ^ HUD_TARGETHINT_TOGGLE_BIT;
                // Apply new top ID.
                clg_hud.targetHints.hints[ HUD_TOP_TARGET_HINT ] = {
                    .hintID = clg_hud.targetHints.currentID = currentID,
                    .hintString = "",
                    .hintStringTokens = {},
                    .easeState = QMEaseState::new_ease_state( realTime, 0_ms ),
                    .alpha = 0
                };
            }
        }
    }

    // Iterate the use target hints, ease them if necessary, and render them in order.
    for ( int32_t i = 0; i < HUD_MAX_TARGET_HINTS; i++ ) {
        // Get pointer to info data.
        hud_usetarget_hint_t *targetHintInfo = &clg_hud.targetHints.hints[ i ];

        // The head info always is new, thus eases in. (It looks better if we apply EaseOut so.)
        if ( i == HUD_TOP_TARGET_HINT ) {
            targetHintInfo->alpha = targetHintInfo->easeState.EaseInOut( realTime, QM_ExponentialEaseInOut );
            // The others however, ease out. (It looks better if we apply EaseIn so.)
        } else {
            targetHintInfo->alpha = 1.0 - targetHintInfo->easeState.EaseInOut( realTime, QM_ExponentialEaseInOut );
        }

        // Reset R color.
        clgi.R_ClearColor();
        // Draw the info.
        HUD_DrawTargetHintInfo( targetHintInfo );

        // Reset R color.
        clgi.R_ClearColor();
    }
}