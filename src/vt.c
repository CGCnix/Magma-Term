#include <pty.h>
#include <stdint.h>
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

void vt_read_input(magma_vt_t *magmavt) {
	uint8_t byte;

	read(magmavt->master, &byte, 1);

	magmavt->buf[magmavt->buf_x] = byte;
	
}
