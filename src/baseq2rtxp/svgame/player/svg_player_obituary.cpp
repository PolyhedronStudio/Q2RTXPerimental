/********************************************************************
*
*
*	ServerGame: Generic Client EntryPoints
*
*
********************************************************************/
#include "svgame/svg_local.h"



/**
*   @brief  
**/
void SVG_Client_Obituary( edict_t *self, edict_t *inflictor, edict_t *attacker ) {
    sg_means_of_death_t meansOfDeath = MEANS_OF_DEATH_UNKNOWN;

    // WID: TODO: In the future, use a gamemode callback for testing if it was friendly fire.
    if ( coop->value && attacker->client ) {
        self->meansOfDeath = self->meansOfDeath | MEANS_OF_DEATH_FRIENDLY_FIRE;
    }

    // WID: TODO: Move to gamemode specific callback.
    // WID: TODO: Enable for SP? Might be fun. 
    //! Only perform Means of Death in DM or Coop mode.
    if ( deathmatch->value || coop->value ) {
        // Store whether it was a friendly fire or not.
        const bool isFriendlyFire = ( self->meansOfDeath & MEANS_OF_DEATH_FRIENDLY_FIRE );
        // Make sure to remove friendly fire flag so our switch statements actually work.
        meansOfDeath = self->meansOfDeath & ~MEANS_OF_DEATH_FRIENDLY_FIRE;

        // Main death message.
        const char *message = nullptr;
        // Appened to main message when set, after first appending attacker->client->name.
        const char *message2 = "";

        // Determine message based on 'environmental/external' influencing events:
        switch ( meansOfDeath ) {
        case MEANS_OF_DEATH_SUICIDE:
            message = "suicides";
            break;
        case MEANS_OF_DEATH_FALLING:
            message = "fell to death";
            break;
        case MEANS_OF_DEATH_CRUSHED:
            message = "imploded by crush";
            break;
        case MEANS_OF_DEATH_WATER:
            message = "went to swim with the fishes";
            break;
        case MEANS_OF_DEATH_SLIME:
            message = "took an acid bath";
            break;
        case MEANS_OF_DEATH_LAVA:
            message = "burned to hell by lava";
            break;
        case MEANS_OF_DEATH_EXPLOSIVE:
            message = "exploded to gibs";
            break;
        case MEANS_OF_DEATH_EXPLODED_BARREL:
            message = "bumped into an angry barrel";
            break;
        case MEANS_OF_DEATH_EXIT:
            message = "'found' a way out";
            break;
        case MEANS_OF_DEATH_LASER:
            message = "ran into a laser";
            break;
        case MEANS_OF_DEATH_SPLASH:
        case MEANS_OF_DEATH_TRIGGER_HURT:
            message = "was in the wrong place";
            break;
        }

        // Messages for when the attacker is actually ourselves inflicting damage.
        if ( attacker == self ) {
            //switch (meansOfDeath) {
            // WID: Left as example.
            //  case MOD_HELD_GRENADE:
            //      message = "tried to put the pin back in";
            //      break;
            //  default:
            //      if (IsNeutral(self))
            //          message = "killed itself";
            //      else if (IsFemale(self))
            //          message = "killed herself";
            //      else
            message = "killed himself";
            //      break;
            //}
        }

        // Print the Obituary Message if we have one.
        if ( message ) {
            gi.bprintf( PRINT_MEDIUM, "%s %s.\n", self->client->pers.netname, message );
            // If in deathmatch, decrement our score.
            if ( self->client && deathmatch->value ) {
                self->client->resp.score--;
            }
            // Reset enemy pointer.
            self->enemy = nullptr;
            // Exit.
            return;
        }

        // Assign attacker as our enemy.
        self->enemy = attacker;
        // Messages for whne the attacker is a client:
        if ( attacker && attacker->client ) {
            switch ( meansOfDeath ) {
            case MEANS_OF_DEATH_HIT_FIGHTING:
                message = "was fisted by";
                break;
            case MEANS_OF_DEATH_HIT_PISTOL:
                message = "was shot down by";
                message2 = "'s pistol";
                break;
            case MEANS_OF_DEATH_HIT_SMG:
                message = "was shot down by";
                message2 = "'s smg";
                break;
            case MEANS_OF_DEATH_HIT_RIFLE:
                message = "was shot down by";
                message2 = "'s rifle";
                break;
            case MEANS_OF_DEATH_HIT_SNIPER:
                message = "was shot down by";
                message2 = "'s sniper";
                break;
            case MEANS_OF_DEATH_TELEFRAGGED:
                message = "got teleport fragged by";
                message2 = "'s personal space";
                break;
            default:
                break;
            }
            // Inform the generated message to the public.
            if ( message ) {
                const char *netNameSelf = self->client->pers.netname;
                const char *netNameAttacker = attacker->client->pers.netname;
                gi.bprintf( PRINT_MEDIUM, "%s %s %s%s.\n", netNameSelf, message, netNameAttacker, message2 );
                // Make sure to decrement score.
                if ( coop->value || deathmatch->value ) {
                    if ( isFriendlyFire ) {
                        attacker->client->resp.score--;
                    } else {
                        attacker->client->resp.score++;
                    }
                }
                // Exit.
                return;
            }
        }
    }

    // We're unlikely to reach this point, however..
    gi.bprintf( PRINT_MEDIUM, "%s died.\n", self->client->pers.netname );
    if ( coop->value || deathmatch->value ) {
        self->client->resp.score--;
    }
}