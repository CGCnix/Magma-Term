#include <pty.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include <stdlib.h>

#include <magma/logger/log.h>
#include <magma/vt.h>

#include <sys/ioctl.h>

int magma_get_pty(int *pmaster, int *pslave) {
	/* 
	 * Switched to openpty as posix_openpt
	 * was causing permission denied errors 
	 * on a friends system
	 */
	return openpty(pmaster, pslave, NULL, NULL, NULL);
}

/**
 *	@brief Get fork master and slave. Slave is invalidated after call
 *
 *	@param [in] master master fd
 *	@param [in/out] slave a pointer filled with slave fd
 */
pid_t magma_fork_pty(const int master, int *slave) {
	pid_t pid;
	int i = 0;
	/*	We use sh as its shell that doesn't use 
	 *	any fancy ANSI escape codes or want terminal 
	 *	features we do not have 
	 */
	const char *args[] = { "/bin/sh", NULL };
	/*	We use xterm here as some programs
	 *	disable some functionality such as 
	 *	not printing ANSI escape codes
	 */
	const char *env[] = { "TERM=xterm", NULL };

	pid = fork();
	if(pid == 0) {
		/*we are the child process*/
		close(master);
		setsid();
		if(ioctl(*slave, TIOCSCTTY, NULL) == -1) {
			magma_log_error("IOCTL:(TIOCSCTTY) %m\n");
			return -1;
		}

		/*std(in,out,err)*/
		for(i = 0; i < 3; i++) { 
			dup2(*slave, i);
		}
		close(*slave);	

		execve(args[0], (char**)args, (char**)env); 	
		/* we should handle this better as the master
		 * essentially continues thinking the slave 
		 * is running 
		 */
		exit(1);
	} 
	/*we are the parent process or error*/
	close(*slave);
	*slave = -1;
	
	/*0 success or -1 error*/
	return pid;
}

static int utf8_to_utf32(utf32_t *unicode, char b1, int fd) {
	uint8_t bytes[3];
	utf32_t codepoint;
	if((b1 & 0x80) == 0x00)  {
		/*UTF8*/
		*unicode = b1;
	} else if((b1 & 0xe0) == 0xc0) {
		read(fd, bytes, 1);
		codepoint = ((b1 & 0x1f) << 6) | (bytes[0] & 0x3f);
		*unicode = codepoint;
	} else if((b1 & 0xF0) == 0xe0) {
		read(fd, bytes, 2);
		codepoint = ((b1 & 0x0f) << 12) | ((bytes[0] & 0x3f) << 6) | (bytes[1] & 0x3f);
		*unicode = codepoint;
	}

	/*TODO UTF32 code points*/
	else {
	
		/*Invalid UTF*/
		return -1;
	}

	return 0;
}

void escape_color_change(int i, magma_vt_t *vt) {
	magma_log_info("Changing color to: %d\n", i);
	if(i == '1') {
		vt->fg = 0xffff0000;
	} else if(i == '4') {
		vt->fg = 0xff00ffff;
	} else {
		vt->fg = 0xfff8f8f2;
	}
}

void escape_set_attrs(uint32_t attrs, magma_vt_t *vt) {
	vt->attributes = attrs;
}

void csi_escape_handle(int fd, magma_vt_t *vt) {
	char seq[100];
	int i = 0;
	do {
		read(fd, &seq[i], 1);
		i++;
	} while(seq[i-1] != 'm');

	if(strcmp("[0m", seq) == 0) {
		escape_set_attrs(0, vt);
		escape_color_change(0, vt);
	}
	if(strcmp("[01;34m", seq) == 0) {
		escape_set_attrs(1, vt); /*Implement attributes ENUM*/
		escape_color_change('4', vt);
	}
}

void vt_read_input(magma_vt_t *magmavt) {
	uint8_t byte;
	utf32_t unicode = 0;
	read(magmavt->master, &byte, 1);

	/*ESCAPE CODE*/
	if(byte == 0x1b) {
		magma_log_info("Escape MODE\n");
		return;
	}

	/* we store the character as UTF32
	 * as it's what freetype expects
	 * and it saves us having to process
	 * the UTF8 character sequence into a
	 * UTF32 character every draw sequence
	 */

	utf8_to_utf32(&unicode, byte, magmavt->master);

	magmavt->lines[magmavt->buf_y][magmavt->buf_x].unicode = unicode;
	magmavt->lines[magmavt->buf_y][magmavt->buf_x].fg = magmavt->fg;
	magmavt->lines[magmavt->buf_y][magmavt->buf_x].attributes = magmavt->attributes;
	if(byte == 0x08) {
		magmavt->buf_x--;
	} else if(byte == 0x9) {
		magmavt->buf_x = ((magmavt->buf_x) | (8 - 1)) + 1;
	} else if(byte == '\n' || magmavt->buf_x > magmavt->cols-2) {
		magmavt->buf_y++;
		magmavt->buf_x = 0;
	} else {
		magmavt->buf_x++;
	}
	
	if(magmavt->buf_y >= magmavt->rows) {
		for(int i = 1; i < magmavt->rows; i++) {
				memmove(magmavt->lines[i-1], magmavt->lines[i], magmavt->cols * sizeof(glyph_t));
			magmavt->buf_x = 0;
		}
		magmavt->buf_y--;
	}
}
