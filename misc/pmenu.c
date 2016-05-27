/*
 * pmenu: a utility for programming menus feature of windowproc on a 630 MTG
 *        each menu is bound to the window it is run in.
 * usage: pmenu [-c|-C] [-f menufile]
 *
 *	  menufile looks like:
 *		depth "item" "string"
 *	  note that the " around item and string are required. string is
 *	  optional for upper depth links.
 *	  a '\n' in the "string" will be replaced by a newline character
 */

#include <stdio.h>
#include "windowproc.h"
#include "lyrscfg.h"
#define USEAGE	"Usage: %s [-c|-C][-f menufile]\n"

#ifdef NO_INDEX
#define index strchr
#endif

main(argc, argv)
	int argc;
	char **argv;
{
	register char *ptr;
	register char *iptr;
	register char *item;
	register char *string;
	register int retval;
	int fflag = 0;
	int cflag = 0;
	int quit = 0;
	FILE *rcfile;
	char inbuf[BUFSIZ];
	char *mtgmenurc = inbuf;
	char *home;
	int count;
	int depth;
	extern char *optarg;
	extern int optind;
	extern char *getenv();
	extern char *index();
	
	if (((home = getenv("HOME")) == (char *) 0) || (*home == '\0'))
		mtgmenurc = MTGMENURC;
	else
		sprintf(inbuf, "%s/%s", home, MTGMENURC);
		
	while ((retval = getopt(argc, argv, "Ccf:")) != EOF) {
		switch(retval) {
		case 'f':
			mtgmenurc = optarg;
			fflag++;
			break;
		case 'C':
			quit++;
			/*
			 * fall through
			 */
		case 'c':
			/*
			 * clear all menus
			 */
			cflag++;
			break;
		case '?':
		default:
			fprintf(stderr, USEAGE, argv[0]);
			exit(1);
		}
	}

	if (cflag)
		printf(PRGRMENU, 0, 0, 0, "", "");
	if (quit)
		exit(0);
	if ((rcfile = fopen(mtgmenurc, "r")) == NULL) {
		if (fflag == 0)
			exit(0);
		fprintf(stderr, "%s: unable to open %s\n",argv[0],mtgmenurc);
		exit(1);
	}
	while (fgets(inbuf, BUFSIZ, rcfile) != NULL) {
		if ((ptr = index(inbuf, '"')) == (char *)0)
			continue;
		*ptr++ = '\0';
		if (((depth = atoi(inbuf)) < 0) || (depth > 3))
			continue;
		item = ptr;
		if ((ptr = index(item, '"')) == (char *)0)
			continue;
		*ptr++ = '\0';
		if ((string = index(ptr, '"')) == (char *)0)
			string = "";
		else
			string++;

		if ((ptr = index(string, '"')) != (char *)0)
			*ptr = '\0';
		for (ptr = string; *ptr != '\0'; ptr ++) {
			if (*ptr != '\\')
				continue;
			if (*(ptr+1) != 'n')
				continue;
			*ptr++ = '\n';
			for (iptr = ptr; *iptr != '\0'; iptr++)
				*iptr = *(iptr+1);
		}
		printf(PRGRMENU,strlen(item),strlen(string),depth,item,string);
	}
	exit(0);
}
