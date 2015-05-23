/* more02.c - version 0.2 of more 
 * read adn print 24 lines then pause for a few special commands
 * feature of version 0.2: reads from /dev/tty fro commands
 */

#include <stdio.h>
#define PAGELEN	24
#define LINELEN 512

void do_more(FILE *);
int see_more(FILE *);

int main( int argc, char *argv[])
{
	FILE	*fp;
	if ( argc == 1 )
		do_more( stdin );
	else
		while ( --argc )
			if ( (fp = fopen( * ++argv, "r")) != NULL)
			{
				do_more(fp);
				fclose(fp);
			}
			else
				exit(1);
	return 0;
}

/* 
 * read PAGELEN lines, then call see_more() for further instructions 
 */
void do_more( FILE *fp)
{
	char	line[LINELEN];
	int 	num_of_lines = 0;
	int 	see_more(FILE *), reply;
	FILE 	*fp_tty;
	fp_tty = fopen("/dev/tty", "r");	/* NEW: cmd stream */
	if ( fp_tty == NULL )			/* if open fails  */
		exit(1);			/* no use in running */
	while( fgets( line, LINELEN, fp)){	/* more input */
		if( num_of_lines == PAGELEN) {	/* full screen? */
			reply = see_more(fp_tty);/* New: pass FILE */
			if ( reply == 0)	/* n: done */
				break;
			num_of_lines -= reply;	/* reset count */
		}
		if (fputs(line, stdout ) == EOF) /* show line */
			exit(1);
		num_of_lines++;
	}
}


/*
 * print message, wait for response, return # of lines to advance 
 * q means no, space means yes, CR means one line
 */
int see_more(FILE *cmd)
{
	int	c;
	printf("\033[7m more? \033[m");		/* reverse on a vt100 */
	while( ( c = getc(cmd)) != EOF )	/* NEW: reads from tty */
	{
		if( c == 'q' )		/* q -> N  */
			return 0;
		if( c == ' ' )		/* ' ' => next page */
			return PAGELEN; /* how many to show */
		if( c == '\n' ) 	/* Enter key => 1 line */
			return 1;
	}
	return 0;
}

