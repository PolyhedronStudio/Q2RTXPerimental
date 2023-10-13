#include "shared/shared.h"
#include "shared/list.h"
#include "common/common.h"
#include "common/files.h"
#include "system/hunk.h"
#include "format/md2.h"
#if USE_MD3
#include "format/md3.h"
#endif
#include "format/sp2.h"
#include "format/iqm.h"

#include "common/models.h"
#include "common/precache_context.h"

model_t *MOD_Alloc( precache_context_t *precacheContext ) {
	model_t *model;
	int i;

	for ( i = 0, model = precacheContext->models; i < precacheContext->numModels; i++, model++ ) {
		if ( !model->type ) {
			break;
		}
	}

	if ( i == precacheContext->numModels ) {
		if ( precacheContext->numModels == PRECACHE_MAX_MODELS ) {
			return NULL;
		}
		precacheContext->numModels++;
	}

	return model;
}

model_t *MOD_Find( precache_context_t *precacheContext, const char *name ) {
	model_t *model;
	int i;

	for ( i = 0, model = precacheContext->models; i < precacheContext->numModels; i++, model++ ) {
		if ( !model->type ) {
			continue;
		}
		if ( !FS_pathcmp( model->name, name ) ) {
			return model;
		}
	}

	return NULL;
}

model_t *MOD_ForHandle( precache_context_t *precacheContext, qhandle_t h ) {
	model_t *model;

	if ( !h ) {
		return nullptr;
	}

	Q_assert( h > 0 && h <= precacheContext->numModels );
	model = &precacheContext->models[ h - 1 ];
	if ( !model->type ) {
		return nullptr;
	}

	return model;
}

void MOD_FreeUnused( precache_context_t *precacheContext ) {
	model_t *model;
	int i;

	for ( i = 0, model = precacheContext->models; i < precacheContext->numModels; i++, model++ ) {
		if ( !model->type ) {
			continue;
		}
		if ( model->registration_sequence == precacheContext->registration_sequence ) {
			// make sure it is paged in
			Com_PageInMemory( model->hunk.base, model->hunk.cursize );
		} else {
			// don't need this model
			Hunk_Free( &model->hunk );
			memset( model, 0, sizeof( *model ) );
		}
	}
}

void MOD_FreeAll( precache_context_t *precacheContext ) {
	model_t *model;
	int i;

	for ( i = 0, model = precacheContext->models; i < precacheContext->numModels; i++, model++ ) {
		if ( !model->type ) {
			continue;
		}

		Hunk_Free( &model->hunk );
		memset( model, 0, sizeof( *model ) );
	}

	precacheContext->numModels = 0;
}