#pragma once

#include <unistd.h>
#include <stdint.h>


typedef uint32_t utf32_t;

typedef struct {
	utf32_t unicode;

	uint32_t attributes;
	/* we dont support true color yet
	 * but lets just have the correct 
	 * color width in case we need to 
	 * implement it
	 */
	uint32_t fg, bg;
} glyph_t;

typedef glyph_t *line_t;

typedef struct {
	int master;

	int rows, cols;

	int buf_x;
	int buf_y;

	uint32_t fg;
	uint32_t bg;
	uint32_t attributes;
	
	line_t *lines;
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
