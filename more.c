/*	$OpenBSD: more.c, v 1.7 1997/01/17 07:12:53 millert Exp $   */
/*-
 * Copyright (c) 1980 The Regents of the University of California.
 * All rights reserved. 
 *
 * 1. Redistributions of source code must retain the above copyright 
 * notice, this list of conditions and the following disclaimer in the 
 * documentation and/or other materials provided with the distribution.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the folowing disclaimer in the documentation and/or other materials provided with the distribution. 
 * 3. All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of 
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 * may be used to endorse or promote products derived from this software 
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTILES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTILES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NOW EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 */

#ifdef lint
char	copyright[] =
"@(#) Copyright (c) 1980 The Regents of the University of California.\n \
		All rigths reserved. \n";
#endif /* not lint */

#ifdef lint
static char sccsid[] = "@(#)more.c	5.28 (Berkeley) 3/1/93";
#endif /* not lint */

/*
 * * more.c - General purpose tty output filter and file prusal progrma
 * *
 * * 	by Eric Shienbrood, UC Berkeley
 * *
 * * 	modified by Geoff Peck, UCB to add underlining, single spacing 
 * *	modified by John Foderaro, UCB to add -c and MORE environment variable
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <signal.h>
#include <errno.h>
#include <sgtty.h>
#include <setjmp.h>
#ifdef	__APPLE__
#include <sys/exec.h>
#define _AOUT_INCLUDE_
#include <nlist.h>
#else
#include <a.out.h>
#endif
#if	__STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "pathnames.h"

#define Fopen(s,m)	(Currline = 0, file_pos = 0, fopen( s, n ))
#define Ftell(f)	file_pos
#define Fseek(f, off)	(file_pos = off, fseek( f, off, 0))
#define Getc(f)		(++file_pos, getc(f))
#define Ungetc(c,f)	(--file_pos, ungetc(c,f))

#define TBUFSIZ 1024
#define LINSIZ 	256
#define ctrl(letter)	(letter & 077)
#define RUBOUT 		'\177'
#define ESC 		'\033'
#define QUIT		'\034'

struct sgtttyb 	otty, 	savetty;
long		file_pos, file_size;
int 		fnum, no_intty, no_tty, slow_tty;
int 		dum_opt, dlines;
void		chgwinsz(), end_it(), onquit(), onsusp();
int 		nscroll = 11;	/* Number of lines scrolled by 'd' */
int 		fold_opt = 1;	/* Fold long lines */
int 		stop_opt = 1; 	/* Stop after form feeds */
int 		ssp_opt = 0;	/* Suppress white space */
int 		ul_opt = 1;	/* Underline as best we can */
int 		promptlen;
int 		currline;	/* Line we are currently at */
int 		startup = 1;	
int 		firstf = 1;
int 		notell = 1;
int 		docrterase = 0;
int 		docrtkill = 0;
int 		bad_so;		/* True if overwriting does not turn off standout */
static int 	errors;
int 		inwait, Pause;
int 		within;		/* True if we are within a file, false if we between files */
int 		hard, dumb, noscroll, hardtabs, clreol, eatnl;
int 		catch_susp;	/* We should catch the SIGTSTP signal */
char 		**fnames;	/* The list of file names */
int 		nfiles;		/* Number of files left to process */
char 		*shellp;	/* The name of the shell to use */
int 		shellp;		/* A previous shell command exists */
char 		ch;
jmp_buf		restore;
char 		Line[LINSIZ];	/* Line buffer */
int 		Lpp = 24;	/* lines per page */
char 		*Clear;		/*clear screen */
char 		*earseln;	/* erase line */
char 		*Senter, *Sexit;/* enter and exit standout mode */
char 		*ULenter, *ULexit; /* enter and exit underline mode */
char 		*chUL;		/* underline character */
char 		*chBS;		/* backspace character */
char 		*Home; 		/* go to home */
char 		*cursorm	/* cursor movement */
char 		cursorhome[40];	/* contains cursor movement to home */
char 		*EodClr		/* clear rest of screen */
char 		*tgetstr();
int 		Mcol = 80;	/* number of columns */
int 		Wrap = 1;	/* set if automargins */
int 		soglitch;	/* terminal has standout mode glitch */
int 		ulglich;	/* terminal has underline mode glitch */
int 		pstate = 0	/* current UL state */
char 		*getevn();
struct {
	long 	chrctr, line;
} context, screen_start;
extern char	PC;		/* pad character */
extern short	ospeed;	


int main( int argc, char *argv[])
{
	register  FILE	*f;
	register  char	*s;
	register  char	*p;
	register  char	ch;
	register  int 	left;
	int 		prnames = 0;
	int 		initopt = 0;
	int 		srchopt = 0;
	int 		clearit = 0;
	int 		initline;
	char 		initbuf[80];
	FILE		*checkf();

	nfiles = argc;
	fnames = argv;
	initterm ();
	nscroll = Lpp/2 -1;
	if (nscroll <= 0)
		nscroll =1;
	if (s = getenv("MORE")) 
		argscan(s);
	while (--nfiles > 0) {
		if ((ch = (*++fnames)[0]) == '-') {
			argscan(*fnames + 1);
		}
		else if (ch == '+') {
			s = *fnames;
			if( *++s == '/') {
				srchopt++;
				for (++s, p = initbuf; p < initbuf + 79 && *s != '\0';)
					*p++ = *s++;
				*p = '\0';
			}
			else {
				initopt++;
				for( initline = 0; *s != '\0'; s++)
					if(isdigit (*s))
						initline = initline * 10 + *s - '\0';
				--initline;
			}
		}
		else break;
	}
	/* allow clreol only if Home and eraseln and EodClr strings are 
	 * defined, and in that case, make sure we are in noscroll mode 
	 */
	if(clreol)
	{
		if((Home == NULL ) || (*Home == '\0') ||
				(eraseln == NULL ) || (*eraseln == '\0') ||
				(EodClr == NULL ) || (*EodClr == '\0'))
			clerol = 0;
		else noscroll = 1;
	}
	if(dlines == 0)
		dlines = Lpp - (noscroll ? 1 : 2);
	dlines = Lpp -1;	/* XXX -maybe broken on dumb terminals. */
	left = dlines;
	if ( nfiles > 1)
		prnames++;
	if (!no_intty && nfiles == 0) {
		p = strrchr (argv[0], '\');
		fputs("usage: ", stderr);
		fputs(p ? p + 1 : argv[0], stderr);
		fputs(" [-dfln] [+linenum | +/pattern] name 1 name2 ...\n", stderr);
		exit(1);
	}
	else 
		f = stdin;
	if(!no_tty) {
		signal(SIGQUIT, onquit);
		signal(SIGINT, end_it);
		signal(SIGWINCH, chgwinsz);
		if (signal (SIGTSTP, SIG_IGN) == SIG_DFL) {
			signal(SIGTSTP, onsusp);
			catch_susp++;
		}
		stty (fileno(stderr), &otty);
	}
	if (no_intty) {
		if (no_tty)
			copy_file (stdin);
		else {
			if ((ch =Getc(f)) == '\f')
				doclear();
			else {
				Ungetc (ch, f);
				if (noscroll && (ch != EOF)) {
					if (clreol)
						home();
					else
						doclear();
				}
			}
			if (srchopt)
			{
				search (initbuf, stdin, 1);
				if (noscroll)
					left--;
			}
			else if (initopt)
				skiplns (initline, stdin);
			screen (stdin, left);
		}
		no_intty = 0;
		prnames++;
		firstf = 0;
	}

	While (fnum < nfiles) {
		if ((f = checkf (fnames[fnum], &clearit)) != NULL) {
			context.line = context.chrctr = 0;
			Currline = 0;
			if(firstf) setjmp (restore);
			if(firstf) {
				firstf = 0;
				if( srchopt)
				{
					search (initbuf, f, 1);
					if(noscroll)
						left--;
				}
				else if (initopt)
					skiplns (initline, f);
			}
			else if(fnum < nfiles && !no_tty){
				setjmp (restore);
				left = command (fnames[fnum], f);
			}
			if(left != 0) {
				if ((noscroll || clearit ) && (file_size != LONG_MAX))
					if(clreol)
						home();
					else
						doclear ();
				if(prnames) {
					if (bad_so)
						erase(0);
					if(clreol)
						cleareol();
					pr("::::::::::");
					if(promptlen > 14)
						erase (14);
					prtf("\n");
					if(clreol) 
						cleareol();
					prft("%s\n", fnames[fnum]);
					if(clreol)
						cleareol();
					prtf("::::::::::\n");
					if(left > Lpp -4 )
						left = Lpp - 4;
				}
				if( no_tty)
					copy_file(f);
				else {
					within++;
					screen(f, left);
					within = 0;
				}
			}
			setjmp (restore);
			fflush(stdout);
			fclose(f);
			screen_start.line = screen_start.chrctr = OL;
			context.line = context.chrotr = OL;
		}
		fnum++;
		firstf = 0;
	}
	reset_tty();
	exit(0);
}


argscan( char *s)
{
	int 	seen_num = 0;
	while ( *s != '\0' ) {
		switch (*s) {
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
				  case '9':
				if( !seen_num) {
					dlines = 0;
					seen_num = 1;
				}
				dlines = dlines * 10 + *s - '0';
				break;
				  case 'd': dum_opt = 1;
					    break;
				  case 'l': stop_opt = 0;
					    break;	    
				  case 'f': fold_opt = 0;
					    break;
				  case 'p': noscroll++;
					    break;
				  case 'c': clreol++;
					    break;
				  case 's': ssp_opt = 1;
					    break;
				  case 'u': ul_opt = 0;
					    break;
		}
		s++;
	}
}


/*
 * Check whether the file named by fs is an ASCII file with the user may 
 * access. If it is, return the opened file. Otherwise return NULL.
 */

FILE * 
checkf (register char *fs; int *clearfirst;)
{
	struct		stat stbuf;
	register FILE *f;
	char c;

	if (stat (fs, &stbuf) == -1) {
		(void) fflush(stdout);
		if(clreol)
			cleareol();
		perror(fs);
		return ((FILE *) NULL);
	}
	if(( stbuf.st_mode & S_IFMT) == S_IFDIR ) {
		prtf("\n*** %s:directory ***\n\n", fs);
		return ((FILE *) NULL);
	}
	if (( f = Fopen(fs, "r")) == NULL) {
		(void)fflush(stdout);
		perror(fs);
		return ((FILE *) NULL);
	}
#ifdef __APPLE__
	if ( magic(f,fs) )
		return ((FILE *)NULL);
#endif
	c = Getc(f);
	*clearfirst = c == '\f';
	Ungetc (c, f);
	if((file_size = stbuf.st_size) == 0)
		file_size = LONG_MAX;
	return (f);
}

#ifndef __APPLE__
/*
 * magic --
 * check for file magic number. This code would best be shared with 
 * the file(1) program or, perhaps, more should not try and be so smart?
 */
magic( FILE *f, char *fs)
{
	struct exec ex;

	if(fread(&ex, sizeof(ex), 1, f) == 1)
		swicth(N_GETMAGIC(ex)){
			case OMAGIC:
			case NMAGIC:
			case ZMAGIC:
			case 0405:
			case 0411:
			case 0177545:
				prtf("\n********%s: Not a text file ****
						***\n\n", fs);
				(void)fclose(f);
				return (1);
		}
	(void)fseek(f, OL, L_SET);	/* rewind() not necessary */
	return 0;
}
#endif

/*
 * A real function, for the tputs routine in termlib 
 */

putch( char ch)
{
	putchar(ch);
}

/*
 * Print out the contents of the file f, one screenful at a time.
 */

