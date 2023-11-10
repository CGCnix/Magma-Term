#pragma once

#include <unistd.h>
#include <stdint.h>


typedef struct {
	int master;

	int rows, cols;

	int buf_x;
	int buf_y;

	/*	This just won't do we need to
	 *	implement ASCII escape codes.
	 *	We need some way to store what 
	 *	color, attributes, etc... a character has
	 *	
	 *	We could store this in the character buffer directly.
	 *	But this means having to process the escape sequence again 
	 *	and again for every redraw of the buffer.
	 *
	 *	Or we could implement a structure to store all these attributes
	 *	with the character and potentially we could store the character
	 *	as UTF32 avoid the mess that is the current draw function
	 *
	 *	But I wanna talk to star before I do cause I want her opinion.
	 *	For now the terminal does "technically work". Though it's not 
	 *	memory safe not even close
	 *
	 */
	char *buf;
} magma_vt_t;


/** 
 *	@brief Get master and slave fd
 *
 *	@param [in/out] pmaster a pointer filled with master fd
 *	@param [in/out] pslave a pointer filled with slave fd
 *	@retval	 0 success
 *	@retval	-1 failed to open master slave pair
 */
int magma_get_pty(int *pmaster, int *pslave);

/**
 *	@brief fork master and slave into terminal and shell.
 *
 *	NOTE: slave fd is set to negative -1 after call and 
 *	is invalid after the call and should not be used
 *
 *	@param [in] master master fd
 *	@param [in/out] slave a pointer filled with slave fd
 *	@retval >0 success value is child pid
 *	@retval -1 fork() failed
 */
pid_t magma_fork_pty(const int master, int *slave);
void vt_read_input(magma_vt_t *magvt);
