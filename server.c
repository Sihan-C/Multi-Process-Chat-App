#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "util.h"
#include <signal.h>


/*
 * Identify the command used at the shell 
 */
int parse_command( char *buf ) {
	int cmd;

	if ( starts_with( buf, CMD_CHILD_PID ) )
		cmd = CHILD_PID;
	else if ( starts_with( buf, CMD_P2P ) )
		cmd = P2P;
	else if ( starts_with( buf, CMD_LIST_USERS ) )
		cmd = LIST_USERS;
	else if ( starts_with( buf, CMD_ADD_USER ) )
		cmd = ADD_USER;
	else if ( starts_with( buf, CMD_EXIT ) )
		cmd = EXIT;
	else if ( starts_with( buf, CMD_KICK ) )
		cmd = KICK;
	else
		cmd = BROADCAST;
	return cmd;
}

/*
 * List the existing users on the server shell
 */
void list_users( user_chat_box_t *users, char *message, int fd ) {
	/* 
	 * Construct a list of user names
	 * Don't forget to send the list to the requester!
	 */

	/***** Insert YOUR code *******/
	int i = 0, count = 0;
	strcpy( message, " \n\n" );
	for ( i; i < MAX_USERS; i++ ) {
		if ( users[ i ].status == 0 ) {
			//printf("Here %s\n",msg);
			strcat( message, users[ i ].name );
			strcat( message, "\n" );
			count++;
		}
	}
	if ( !count ) {
		strcpy( message, "<no users>" );
		if ( write( fd, message, MSG_SIZE ) == -1 ) {
			perror( "Failed to write" );
			exit( 0 );
		}	
	}
	else{
		if ( write( fd, message, MSG_SIZE ) == -1 ) {
			perror( "Failed to write" );
			exit( 0 );
		}
	}
	return;
}

/*
 * Add a new user
 */
void add_user( user_chat_box_t *users, char *cName, char *message, int server_fd ) {
	/***** Insert YOUR code *******/
	/* 
	 * Check if user limit reached.
	 *
	 * If limit is okay, add user, set up non-blocking pipes and
	 * notify on server shell
	 *
	 * NOTE: You may want to remove any newline characters from the name string 
	 * before adding it. This will help in future name-based search.
	 */ 
	 // Check whether there is an empty spot. If there is one, return the first empty spot's index found.
	int i = 0, flags;
	for ( i; i < MAX_USERS; i++ ) {
		if ( users[ i ].status == 1 )
			break;
	}
	if ( i >= MAX_USERS ) {
		strcpy( message, "Could not add user: Max user limit reached" );
		if ( write( server_fd, message, MSG_SIZE ) == -1 ) {
			perror( "Failed to write" );
			exit( 0 );
		}
		return;	
	}
	pid_t childpid;					
	strcpy( users[ i ].name, cName );
	users[ i ].status = 0; 	 
	int pip[ 2 ], pip1[ 2 ];
	if ( pipe( pip ) == -1 ) {
		perror( "Creating pipe failed" );
		exit( 0 );	
	}
	if ( pipe( pip1 ) == -1 ) {
		perror( "Creating pipe failed" );
		exit( 0 );	
	}
	users[ i ].ctop[ 0 ] = pip[ 0 ];
	users[ i ].ctop[ 1 ] = pip[ 1 ];
	users[ i ].ptoc[ 0 ] = pip1[ 0 ];
	users[ i ].ptoc[ 1 ] = pip1[ 1 ];
	flags = fcntl( pip[ 0 ], F_GETFL, 0 );
	fcntl( pip[ 0 ], F_SETFL, flags | O_NONBLOCK );
	childpid = fork();
	users[ i ].pid = childpid;
	if ( childpid == -1 ) {
		perror( "Failed to fork" );
		exit( 0 );
	}
	if ( childpid == 0 ) {		// Children process
		close( users[ i ].ctop[ 0 ] );
		close( users[ i ].ptoc[ 1 ] );
		int exeStatus;
		char ends0[ 10 ], ends1[ 10 ];
		sprintf( ends0, "%d", users[ i ].ctop[ 1 ] );
		sprintf( ends1, "%d", users[ i ].ptoc[ 0 ] );
		exeStatus = execl( XTERM_PATH, XTERM, "+hold", "-e", "./shell", ends0, ends1,cName, NULL );
		if ( exeStatus == -1 ) {
			perror( "execl failed" );
			exit( 0 );
		}
	}
	else {		// Parent process
		close( users[ i ].ctop[ 1 ] );
		close( users[ i ].ptoc[ 0 ] );
		sprintf( message, "Adding user %s...", cName );
		if ( write( server_fd, message, MSG_SIZE ) == -1 ) {
			perror( "Failed to write" );
			exit( 0 );
		}		
	}
	return;
}

/*
 * Broadcast message to all users. Completed for you as a guide to help with other commands :-).
 */
int broadcast_msg( user_chat_box_t *users, char *buf, int fd, char *sender ) {
	int i;
	const char *msg = "Broadcasting...", *s;
	char text[ MSG_SIZE ];

	/* Notify on server shell */
	if ( write( fd, msg, strlen( msg ) + 1 ) < 0 )
		perror( "writing to server shell" );
	
	/* Send the message to all user shells */
	s = strtok( buf, "\n" );
	sprintf( text, "%s : %s", sender, s );
	for ( i = 0; i < MAX_USERS; i++ ) {
		if ( users[ i ].status == SLOT_EMPTY ) {
			continue;
		}
		if ( write( users[ i ].ptoc[ 1 ], text, MSG_SIZE ) < 0 ) {
			perror( "write to child shell failed" );
			exit( 0 );
		}
	}
}

/*
 * Close all pipes for this user
 */
void close_pipes( int idx, user_chat_box_t *users ) {
	/***** Insert YOUR code *******/
	close( users[ idx ].ctop[ 0 ] );
	close( users[ idx ].ptoc[ 1 ] );
	return; 
}

/*
 * Cleanup single user: close all pipes, kill user's child process, kill user 
 * xterm process, free-up slot.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_user( int idx, user_chat_box_t *users ) {
	/***** Insert YOUR code *******/
	close_pipes( idx, users );
	int status, status1;
	if ( kill( users[ idx ].child_pid, 9 ) == -1 ) {
		perror( "Killing child process failed" );
		exit( 0 );
	}
	/*if ( waitpid( users[ idx ].child_pid, &status, 0 ) == -1 ) {
		perror( "Wait failed" );
		exit( 0 );
	}*/
	if ( kill( users[ idx ].pid, 9 ) == -1 ) {
		perror( "Killing child process failed" );
		exit( 0 );
	}
	if ( waitpid( users[ idx ].pid, &status1, 0 ) == -1 ) {
		perror( "Wait failed" );
		exit( 0 );
	}
	users[ idx ].status = 1;
	return;	
}

/*
 * Cleanup all users: given to you
 */
void cleanup_users( user_chat_box_t *users ) {
	int i;

	for (i = 0; i < MAX_USERS; i++) {
		if (users[i].status == SLOT_EMPTY)
			continue;
		cleanup_user( i, users );
	}
}

/*
 * Cleanup server process: close all pipes, kill the parent process and its 
 * children.
 * Remember to wait for the appropriate processes here!
 */
void cleanup_server( server_ctrl_t server_ctrl, user_chat_box_t *users )
{
	/***** Insert YOUR code *******/
	cleanup_users( users );
	close( server_ctrl.ptoc[ 1 ] );
	close( server_ctrl.ctop[ 0 ] );
	int status, status1;
	if ( kill( server_ctrl.child_pid, 9 ) == -1 ) {
		perror( "Killing child process failed" );
		exit( 0 );
	}
	/*if ( waitpid( server_ctrl.child_pid, &status, 0 ) == -1 ) {	
		perror( "Wait failed" );
		exit( 0 );
	}*/
	if ( kill( server_ctrl.pid, 9 ) == -1 ) {
		perror( "Killing child process failed" );
		exit( 0 );
	}
	if ( waitpid( server_ctrl.pid, &status1, 0 ) == -1 ) {
		perror( "Wait failed" );
		exit( 0 );
	}
	exit( 0 );
}

/*
 * Utility function.
 * Find user index for given user name.
 */
int find_user_index( user_chat_box_t *users, char *name ) {
	int i, user_idx = -1;

	if ( name == NULL ) {
		fprintf( stderr, "NULL name passed.\n" );
		return user_idx;
	}
	for ( i = 0; i < MAX_USERS; i++ ) {
		if ( users[ i ].status == SLOT_EMPTY )
			continue;
		if ( strncmp( users[ i ].name, name, strlen( name ) ) == 0 ) {
			user_idx = i;
			break;
		}
	}
	return user_idx;
}

/*
 * Utility function.
 * Given a command's input buffer, extract name.
 */
char *extract_name( int cmd, char *buf ) {
	char *s = NULL;

	s = strtok( buf, " " );
	s = strtok( NULL, " " );
	if ( cmd == P2P )
		return s;	/* s points to the name as no newline after name in P2P */
	s = strtok( s, "\n" );	/* other commands have newline after name, so remove it*/
	return s;
}

/*
 * Send personal message. Print error on the user shell if user not found.
 */
void send_p2p_msg( int idx, user_chat_box_t *users, char *usrcName, char * msg) {
	/* get the target user by name (hint: call (extract_name() and send message */
	
	/***** Insert YOUR code *******/
	int i = 0;
	char text[ MSG_SIZE ];

	for ( i; i < strlen( msg ) + 1; i++ ) {
		if ( msg[ i ] == ' ' )
			break;	
	}
	msg += ( ++i );

	sprintf( text, "%s : %s", usrcName, msg );
	if ( write( users[ idx ].ptoc[ 1 ], text, MSG_SIZE ) < 0 ) {
		perror( "write to child shell failed" );
		exit( 0 );
	}
}

int main( int argc, char **argv ) {
	
	/***** Insert YOUR code *******/
	user_chat_box_t userList[ MAX_USERS ];
	user_chat_box_t *userListPtr = userList;
	server_ctrl_t sShell;

	// Initializing status attribute for usrs array, Initialize all of them to empty.
	int i = 0;
	for ( i; i < MAX_USERS; i++ )
		userList[ i ].status = 1;

	/* open non-blocking bi-directional pipes for communication with server shell */ 
	int ends[ 2 ], ends1[ 2 ], flags;
	pipe( ends );
	pipe( ends1 );
	sShell.ctop[ 0 ] = ends[ 0 ];
	sShell.ctop[ 1 ] = ends[ 1 ];
	sShell.ptoc[ 0 ] = ends1[ 0 ];
	sShell.ptoc[ 1 ] = ends1[ 1 ];
	flags = fcntl( ends[ 0 ], F_GETFL, 0 );
	fcntl( ends[ 0 ], F_SETFL, flags | O_NONBLOCK );

	/* Fork the server shell */
		/* 
	 	 * Inside the child.
		 * Start server's shell.
	 	 * exec the SHELL program with the required program arguments.
	 	 */
	pid_t childpid;
	childpid = fork();
	if ( childpid == -1 ) {
		perror( "Failed to fork" );
		exit( 0 );
	}
	if ( childpid == 0 ) {		// Child process
		close( sShell.ctop[ 0 ] );
		close( sShell.ptoc[ 1 ] );
		int exStatus;
		char *name = "Server", ends0[ 10 ], ends1[ 10 ];
		sprintf( ends0, "%d", sShell.ctop[ 1 ] );
		sprintf( ends1, "%d", sShell.ptoc[ 0 ] );
		exStatus = execlp( "./shell" , SHELL_PROG, ends0, ends1, name, NULL );
		if ( exStatus == -1 ) {
			perror( "Failed to execute shell" );
			exit( 0 );	
		}
	}	
	/* Inside the parent. This will be the most important part of this program. */
	else {		// Parent process
		close( sShell.ctop[ 1 ] );
		close( sShell.ptoc[ 0 ] );
		sShell.pid = childpid;
		/* Start a loop which runs every 1000 usecs.
	 	 * The loop should read messages from the server shell, parse them using the 
	 	 * parse_command() function and take the appropriate actions. */
		while ( 1 ) {
			/* Let the CPU breathe */
			usleep( 1000 );

			/* 
		 	 * 1. Read the message from server's shell, if any
		 	 * 2. Parse the command
		 	 * 3. Begin switch statement to identify command and take appropriate action
		 	 *
		 	 * 		List of commands to handle here:
		 	 * 			CHILD_PID
		 	 * 			LIST_USERS
		 	 * 			ADD_USER
		 	 * 			KICK
		 	 * 			EXIT
		 	 * 			BROADCAST 
		 	 */
			char msg[ MSG_SIZE ];
			ssize_t bytesread;
			bytesread = read( sShell.ctop[ 0 ], msg, MSG_SIZE ); 
			if ( bytesread == -1 && errno != EAGAIN ) {
				perror( "Read error" );
				exit( 0 );
			}
			else if ( bytesread > 0 ) {
				int cmd = parse_command( msg );

				/* Fork a process if a user was added (ADD_USER) */
					/* Inside the child */
					/*cd pwd
				 	 * Start an xterm with shell program running inside it.
				 	 * execl(XTERM_PATH, XTERM, "+hold", "-e", <path for the SHELL program>, ..<rest of the arguments for the shell program>..);
				 	 */
				char msgCopy[ MSG_SIZE ];
				strcpy( msgCopy, msg );
				char *cName;
				cName = extract_name( cmd, msg );
				int i = 0, count = 0, index;
				char message[ MSG_SIZE ];
			 	switch( cmd ) {
			 		case 0:
			 			sShell.child_pid = atoi( cName );
						break;
					case 1:
						list_users( userList, message, sShell.ptoc[ 1 ] );
						break;	
					case 2:
						// Check whether there is an empty spot. If there is one, return the first empty spot's index found.
						add_user( userList, cName, message, sShell.ptoc[ 1 ] );
						break;
					case 3:
						cleanup_server( sShell, userList );
						break;	
					case 4:
						broadcast_msg( userList, msgCopy, sShell.ptoc[ 1 ], "Server" );
						break;
					case 6:
						index = find_user_index( userList, cName );
						cleanup_user( index, userList ); 
						break;
				}
			} 	 
			/* Back to our main while loop for the "parent" */
			/* 
		 	 * Now read messages from the user shells (ie. LOOP) if any, then:
		 	 * 		1. Parse the command
		 	 * 		2. Begin switch statement to identify command and take appropriate action
		 	 *
		 	 * 		List of commands to handle here:
		 	 * 			CHILD_PID
		 	 * 			LIST_USERS
		 	 * 			P2P
		 	 * 			EXIT
		 	 * 			BROADCAST
		 	 *
		 	 * 		3. You may use the failure of pipe read command to check if the 
		 	 * 		user chat windows has been closed. (Remember waitpid with WNOHANG 
		 	 * 		from recitations?)
		 	 * 		Cleanup user if the window is indeed closed.
		 	 */
		 	for ( i = 0; i < MAX_USERS; i++ ) {
		 		if ( userList[ i ].status == 0 ) {
			 		char usrmsg[ MSG_SIZE ];
			 		bytesread = read( userList[ i ].ctop[ 0 ], usrmsg, MSG_SIZE ); 
					if ( bytesread == -1 && errno != EAGAIN ) {
						perror( "Read error" );
						exit( 0 );
					}
					else if ( bytesread == -1 && errno == EAGAIN ) {
						continue;
			 		}
			 		else if ( bytesread > 0 ) {
			 			int cmd, receiverIndex;
			 			cmd = parse_command( usrmsg );
			 			char msgCopy1[ MSG_SIZE ];
						strcpy( msgCopy1, usrmsg );
						char *usrcName;
						usrcName = extract_name( cmd, usrmsg );
						char usrMessage[ MSG_SIZE ], p2pmsg[ MSG_SIZE ];
					 	switch( cmd ) {
					 		case 0:
					 			userList[ i ].child_pid = atoi( usrcName );
								break;
							case 1:
								list_users( userList, usrMessage, userList[ i ].ptoc[ 1 ] );
								break;	
							case 3:
								cleanup_user( i, userList );
								break;	
							case 4:
								broadcast_msg( userList, msgCopy1, sShell.ptoc[ 1 ], userList[ i ].name );
								break;
							case 5:
								receiverIndex = find_user_index( userList, usrcName );
								if ( receiverIndex == -1 ) {
									char errorMsg[ MSG_SIZE ];
									sprintf( errorMsg, "User %s does not found.", usrcName );
									if ( write( userList[ i ].ptoc[ 1 ], errorMsg, MSG_SIZE ) == -1 ) {
										perror( "write to child shell failed" );
										exit( 0 );
									}
									break;
								}
								char *s = NULL;
								s = strtok( msgCopy1, " " );
								s = msgCopy1 + strlen( s ) + 1;
								char senderName[ MSG_SIZE ];
								strcpy( senderName, userList[ i ].name );
								send_p2p_msg( receiverIndex, userList, senderName, s );
								break;
						}
			 		}
		 			int status;
		 			if ( waitpid( userList[ i ].pid, &status, WNOHANG ) == -1 ) {
		 				close_pipes( i, userList );
		 				userList[ i ].status = 1;
		 				/*
							Do not need to kill any process here since Xterm will take care that.
							Based on the observation from top command. When a seg fault happen
							all of its child process got disappeared. 
		 				*/	
		 			}
			 	}	
		 		usleep( 1000 );
			}		 	 
		}	/* while loop ends when server shell sees the \exit command */	 	 
	}
	return 0;
}
