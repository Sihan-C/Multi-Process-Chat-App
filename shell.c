#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include "util.h"


/*
	Read a line from stdin.
*/
char *sh_read_line( void ) {
	char *line = NULL;
	size_t bufsize = 0;
	getline( &line, &bufsize, stdin );
	return line;
}

/*
	Check if the line is empty (no data; just spaces or return)
*/
int is_empty( char *line ) {
	while ( *line != '\0' ) {
		if ( !isspace( *line ) )
			return 0;
		line++;
	}
	return 1;
}

/*
	Do stuff with the shell input here.
*/
int sh_handle_input( char *line, int fd_toserver ) {
	
	/***** Insert YOUR code *******/
	
 	/* Check for \seg command and create segfault */
	if ( starts_with( line, CMD_SEG ) ) {
		char *s = NULL;
		*s = 'c';

	}
	
	/* Write message to server for processing */
	if ( is_empty( line ) )
		return 0;
	char msg[ MSG_SIZE ];
	strncpy( msg, line, strlen( line ) - 1 );	// Exclude the newline character. 
	msg[ strlen( line ) - 1 ] = '\0';
	if ( write( fd_toserver, msg, MSG_SIZE ) == -1 ) {
		perror( "Write failed" );
		exit( 0 );
	}
	return 1;
}


/*
	Start the main shell loop:
	Print prompt, read user input, handle it.
*/
int sh_start( char *name, int fd_toserver ) {
	print_prompt( name );
	char *line;
	line = sh_read_line();
	int retVal = 0;
	retVal = sh_handle_input( line, fd_toserver );
	return retVal;
}

int main( int argc, char **argv ) {
	
	/***** Insert YOUR code *******/
	
	/* Extract pipe descriptors and name from argv */
	char *name = argv[ 3 ];
	int fd_fromserver = atoi( argv[ 2 ] ), fd_toserver = atoi( argv[ 1 ] );	
	int stoc[ 2 ]; 
	if ( pipe( stoc ) == -1 ) {
		perror( "pipe error" );
		exit( 0 );
	}

	/* Fork a child to read from the pipe continuously */
	pid_t childpid;
	if ( ( childpid = fork() ) == -1 ) {
		perror( "Failed to fork" );
		exit( 0 );
	}

	/*
	 * Once inside the child
	 * look for new data from server every 1000 usecs and print it
	 */ 
	if ( childpid == 0 ) {		// Child process
		close( stoc[ 0 ] );
		
		/* 
			buff is used to store an indicator message to tell parent process it has finished 
			printing to stdout. H does not have any specific meaning.
		*/
		char buff[ 5 ] = "H", buff1[ 5 ]; 		
		while ( 1 ) {
			char msg[ MSG_SIZE ];
			ssize_t bytesread;
			bytesread = read( fd_fromserver, msg, MSG_SIZE );
			if ( bytesread == -1 && errno != EAGAIN ) {
				perror( "Read error" );
				exit( 0 );
			}
			else if ( bytesread > 0 ) {
				printf( "%s\n", msg );
			}
			usleep( 1000 );
		}	
	}	
	
	/* Inside the parent
	 * Send the child's pid to the server for later cleanup
	 * Start the main shell loop
	 */
	else {		// Parent process
		close( stoc[ 1 ] );
		char cpid[ 10 ], dest[ MSG_SIZE ] = "\\child_pid ";
		sprintf( cpid, "%d", childpid );
		strcat( dest, cpid );
		if ( write( fd_toserver, dest, MSG_SIZE ) == -1 ) {
			perror( "Write failed" );
			exit( 0 );
		}
		
		while ( 1 ) {
			// Same as child, buff1 only used to store an indicator message.
			char buff[ 5 ], buff1[ 5 ] = "Guo";		
			int retVal = 0, status;
			retVal = sh_start( name, fd_toserver );
		}	
	}
	return 0; 
}


