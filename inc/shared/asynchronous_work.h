/********************************************************************
*
*
*   Asynchronous shared worker struct:
*
*
********************************************************************/
#pragma once


QEXTERN_C_OPEN

/**
*	@brief	Struct representing a unit of asynchronous work to be processed on the main thread.
**/
typedef struct asyncwork_s {
	void ( *work_cb )( void * );
	void ( *done_cb )( void * );
	void *cb_arg;
	struct asyncwork_s *next;
} asyncwork_t;

// Extern C
QEXTERN_C_CLOSE