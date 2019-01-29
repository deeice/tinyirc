#if 0
.if "0" == "1"
#endif

/* Debian: gcc -O2 -s -o tinyirc tinyirc.c -lbsd -ltermcap */

/* Configuration options */
/* please change the default server to one near you. */
//#define DEFAULTSERVER	"irc.mirbsd.org"
#define DEFAULTSERVER	"irc.freenode.org"
#define DEFAULTPORT	6667
#define COMMANDCHAR	'/'
/* each line of history adds 512 bytes to resident size */
#define HISTLEN		8

#ifdef ZIPIT_Z2 /* version */
#define RELEASE_VER	"TinyIRC MirOS"
#define RELEASE_L	RELEASE_VER " (Zipit)"
#define RELEASE_S	RELEASE_VER
#else
#if defined(__MirBSD__)
#define RELEASE_OS	"MirBSD"
#elif defined(__OpenBSD__)
#define RELEASE_OS	"OpenBSD"
#elif defined(__CYGWIN32__)
#define RELEASE_OS	"GNU/Cygwin32"
#elif defined(__gnu_linux__)
#define RELEASE_OS	"GNU/Linux"
#else
#define RELEASE_OS	"unknown OS"
#endif

#define RELEASE_VER	"TinyIRC 20110717"
#define RELEASE_L	RELEASE_VER " (" RELEASE_OS ") MirOS-contrib"
#define RELEASE_S	RELEASE_VER " MirOS"
#endif
/*-
   TinyIRC - MirOS Fork
   Copyright (C) 1994 Nathan I. Laredo <laredo@gnu.org>
   Copyright (c) 1999-2011 Thorsten Glaser <tg@mirbsd.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License Version 1
   as published by the Free Software Foundation.

   This work is provided "AS IS" and WITHOUT WARRANTY of any kind, to
   the utmost extent permitted by applicable law, neither express nor
   implied; without malicious intent or gross negligence. In no event
   may a licensor, author or contributor be held liable for indirect,
   direct, other damage, loss, or other issues arising in any way out
   of dealing in the work, even if advised of the possibility of such
   damage or existence of a defect, except proven that it results out
   of said person's immediate fault when using the work as intended.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The copyright holders consider the licence's condition of source
   publication to be fulfilled if all of the sources in the exact
   state used to build a combined binary, e.g. using crunchgen(1),
   that includes this work as part of it are published, without the
   other, independent works part of said combined binary requiring
   to be distributed under the terms of the GNU GPL.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <ctype.h>
#include <curses.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _USE_OLD_CURSES_
#include <term.h>
#endif
#include <termios.h>
#include <time.h>
#include <unistd.h>

#ifndef __RCSID
#define	__RCSID(x)	static const char __rcsid[] __attribute__((used)) = (x)
#endif

__RCSID("$MirOS: contrib/code/Snippets/tinyirc.c,v 1.41 2011/07/18 00:35:41 tg Exp $");

#ifndef __dead
#define __dead
#endif

#if defined(__CYGWIN__) || defined(WIN32)
u_int32_t arc4random(void);
uint32_t arc4random_pushb(const void *, size_t);
#define RELEASE_PAPI	"Win32"
#define dopush		arc4random_pushb
#elif defined(arc4random_pushb_fast)
#define RELEASE_PAPI	"pfast"
#define dopush		arc4random_pushb_fast
#elif defined(__MirBSD__)
#define RELEASE_PAPI	"pushb"
#define dopush		arc4random_pushb
#elif defined(arc4random_pushk)
#define RELEASE_PAPI	"pushk"
#define dopush		arc4random_pushk
#else
#define RELEASE_PAPI	"none"
#define dopush(buf,n)	arc4random_addrandom((u_char *)(buf), (int)(n))
#endif

struct dlist {
    char name[64];
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
    char *names[256];
    int namecount;
#endif
    struct dlist *next;
};
struct dlist *object = NULL, *objlist = NULL, *newobj;
u_short IRCPORT = DEFAULTPORT;
#define LINELEN 512
int sockfd, sok = 1, stdinfd, stdoutfd, histline, dumb = 0,
    maxcol, maxlin, column;
char *linein, lineout[LINELEN], *history[HISTLEN], localhost[64],
*tok_in[256], *tok_out[256], *tmp, serverdata[512], termcap[1024],
*ptr, *term, *fromhost, IRCNAME[16], inputbuf[512], beenden = 0;
char bp[4096], *cap_cm, *cap_cs, *cap_ce, *cap_so, *cap_se, *cap_dc;
#define NLASTCHAN 12
char *lastchans[NLASTCHAN];
int cursd = 0, curli = 0, curx = 0;
fd_set readfs;
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
char **names;
int namecount = 0;
char namesearch[512];
char *nameptr;
int nametab = -1;
#endif
#ifdef ZIPIT_Z2 /* timestamp */
int timestamp = 0;
#define USE_ANSICOLOR
#endif
struct timeval time_out;
struct tm *timenow;
static time_t idletimer, datenow, wasdate;
static volatile sig_atomic_t sigwinch = 0;

char null[] = "";
char s_cm[] = "cm";
char s_CM[] = "CM";
char s_so[] = "so";
char s_se[] = "se";
char s_cs[] = "cs";
char s_ce[] = "ce";
char s_dc[] = "dc";
struct termios _tty;
tcflag_t _res_iflg, _res_lflg;
#define raw() (_tty.c_lflag &= ~(ICANON | ECHO | ISIG), \
	tcsetattr(stdinfd, TCSANOW, &_tty))
#define savetty() ((void) tcgetattr(stdinfd, &_tty), \
	_res_iflg = _tty.c_iflag, _res_lflg = _tty.c_lflag)
#define resetty() (_tty.c_iflag = _res_iflg, _tty.c_lflag = _res_lflg,\
	(void) tcsetattr(stdinfd, TCSADRAIN, &_tty))

#ifdef _USE_OLD_CURSES_
#define	tputs_x		_puts
#else
#define	tputs_x(s)	(tputs(s, 0, putchar))
#endif

int my_stricmp(const char *, const char *);
struct dlist *additem(char *, struct dlist *);
struct dlist *finditem(char *, struct dlist *);
struct dlist *removeitem(char *, struct dlist *);
int makeconnect(const char *);
int sendline(void);
void updatestatus(void);
static int nop(void);
static int doerror(void);
static int doinvite(void);
static int dojoin(void);
static int dokick(void);
static int dokill(void);
static int domode(void);
static int donick(void);
static int donotice(void);
static int dopart(void);
static int dopong(void);
static int doprivmsg(void);
static int doquit(void);
static int dosquit(void);
static int dotime(void);
static int dotopic(void);
int donumeric(int);
int wordwrapout(char *, size_t);
int parsedata(void);
int serverinput(void);
void parseinput(void);
void histupdate(void);
void printpartial(int);
void userinput(void);
__dead void cleanup(int);
static void dowinch(int);
static void dowinsz(int);
int main(int, char *[]);
static void pushlastchan(char *);
#ifdef ZIPIT_Z2 /* use rand() */
#define strlcpy(a, b, c) strncpy(a, b, c)
#define strlcat(a, b, c) strcat(a, b)
#define arc4random() rand()
#define arc4hashpush(x) 
#define arc4random_addrandom(buf, n) 
#else
#define arc4hashpush(x) arc4hashpush_((const void *)(x))
void arc4hashpush_(const uint8_t *);

void arc4hashpush_(const uint8_t *buf)
{
	static uint32_t s = 0;
	register uint32_t h;
	register uint8_t c;
	uint32_t pv;

	if (!s)
		s = arc4random();

	h = s;
	while ((c = *buf++)) {
		h += c;
		++h;
		h += h << 10;
		h ^= h >> 6;
	}
	s = h;

	h += h << 10;
	h ^= h >> 6;
	h += h << 3;
	h ^= h >> 11;
	h += h << 15;

	pv = h;
	dopush(&pv, sizeof(pv));
}
#endif

int my_stricmp(const char *str1, const char *str2)
{
    int compare;

    while (*str1 != 0 && str2 != 0) {
	if (isalpha(*str1) && isalpha(*str2)) {
	    compare = *str1 ^ *str2;
	    if ((compare != 32) && (compare != 0))
		return (*str1 - *str2);
	} else {
	    if (*str1 != *str2)
		return (*str1 - *str2);
	}
	str1++;
	str2++;
    }
    return (*str1 - *str2);
}

struct dlist *additem(char *item, struct dlist *ptrx)
{
    newobj = (struct dlist *) malloc(sizeof(struct dlist));
    strlcpy(newobj->name, item, 64);
    newobj->next = ptrx;
    return newobj;
}

struct dlist *finditem(char *item, struct dlist *ptrx)
{
    while (ptrx != NULL)
	if (my_stricmp(item, ptrx->name) == 0)
	    break;
	else
	    ptrx = ptrx->next;
    return ptrx;
}

#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
static void switchitem(void)
{
  /* Now would be a good time to free all namecount names in object->names */
  /* Gotta do it before removeitem.. */
  int i;
  for (i = 0; i < namecount; i++)
    free(names[i]);
  objlist = removeitem(tok_in[2], objlist);
  if (object == NULL)
    object = objlist;
  if (object != NULL)
  {
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[1;35m"); // MAGENTA
    printf("\n\r*** Now talking in %s", object->name);
    printf("\E[0;39m"); // NORMAL
#else
    printf("\n\r*** Now talking in %s", object->name);
#endif
    /* NOTE: This would be a good place to swap in the namelist for this object. */    
    names = object->names;
    namecount = object->namecount;
  }
  else {
    names = NULL;
    namecount = 0;
  }
  wasdate = 0;
}
#endif

struct dlist *removeitem(char *item, struct dlist *ptrx)
{
    struct dlist *prev = NULL, *start = ptrx;
    while (ptrx != NULL)
	if (my_stricmp(item, ptrx->name) == 0) {
	    newobj = ptrx->next;
	    if (object == ptrx)
		object = NULL;
	    free (ptrx);
	    if (prev == NULL)
		return newobj;
	    else {
		prev->next = newobj;
		return start;
	    }
	} else {
	    prev = ptrx;
	    ptrx = ptrx->next;
	}
    return start;
}

int makeconnect(const char *hostname)
{
    struct sockaddr_in sa;
    struct hostent *hp;
    int s;

    if ((hp = gethostbyname(hostname)) == NULL)
	return -1;
    bzero(&sa, sizeof(sa));
    memmove((char *) &sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_family = hp->h_addrtype;
    sa.sin_port = htons((u_short) IRCPORT);
    if ((s = socket(hp->h_addrtype, SOCK_STREAM, 0)) < 0)
	return -1;
    if (connect(s, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
	close(s);
	return -1;
    }
    fcntl(s, F_SETFL, O_NDELAY);
    return s;
}

#if 0	/* DEBUG */
int sendline(void)
{
  printf("%s\n", lineout);
}
#else
int sendline(void)
{
    arc4hashpush(lineout);
    if (write(sockfd, lineout, strlen(lineout)) < 1)
	return 0;
    return 1;
}
#endif

void updatestatus(void)
{
    int n;
#ifdef ZIPIT_Z2
    static char buf[81];
#endif
    if (!dumb) {
	if (60 < (datenow = time(NULL)) - wasdate) {
	    wasdate = datenow;
	    timenow = localtime(&datenow);
	    tputs_x(tgoto(cap_cm, 0, maxlin - 1));
	    tputs_x(cap_so);
#ifdef ZIPIT_Z2 /* timestamp and clamp statusbar width for dvtm. */
	    n = maxcol < 80 ? maxcol : 80;
	    n = snprintf(buf,n,"%02d:%02d %s on %s : %s %s", timenow->tm_hour,
		timenow->tm_min, IRCNAME, object == NULL ||
		object->name == NULL ? "*" : object->name, RELEASE_S,
		timestamp ? "Z" : "z");
	    buf[n]=0;
	    n = printf(buf);
#else
	    n = printf("[%02d:%02d] %s on %s : %s", timenow->tm_hour,
		timenow->tm_min, IRCNAME, object == NULL ||
		object->name == NULL ? "*" : object->name, RELEASE_S);
#endif
	    for (; n < maxcol; n++)
		putchar(' ');
	    tputs_x(cap_se);
	}
    }
}

static int nop(void)
{
    return 1;
}

static int doerror(void)
{
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[1;31m"); // RED
#endif
    column = printf("*** ERROR:");
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[0;39m"); // NORMAL
#endif
    return 2;
}

static int doinvite(void)
{
    printf("*** %s (%s) invites you to join %s.",
	   tok_in[0], fromhost, &tok_in[2][1]);
    return 0;
}

static int dons(void)
{
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[1;32m"); // GREEN
#endif
    printf("\nNS <%s> (%s)\n", tok_in[0], tok_in[1]);
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[0;39m"); // NORMAL
#endif
    return 1;
}

static int dojoin(void)
{
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[1;35m"); // MAGENTA
#endif
    if (strcmp(tok_in[0], IRCNAME) == 0) {
	object = objlist = additem(tok_in[2], objlist);
	wasdate = 0;
	printf("*** Now talking in %s", object->name);
    } else
	printf("*** %s (%s) joined %s", tok_in[0], fromhost, tok_in[2]);
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
    printf("\E[0;39m"); // NORMAL
#endif

#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
  // Probably should see if name exists already.
    {
      int i, found=0;
      if (strcmp(tok_in[0], IRCNAME) == 0) {
	names = object->names;
	namecount = 0;
      }
      for (i = 0; i < namecount; i++) {
	if (!strcmp(names[i],tok_in[0])) {
	  found++;
	  break;
	}
      }
      if (!found) {
	names[namecount] = strdup(tok_in[0]);
	if (++namecount >= 256) namecount = 0; // Ran out.  uh oh.
      }
      object->namecount = namecount;
    }
#endif
    return 0;
}

static int dokick(void)
{
    printf("*** %s was kicked from %s by %s (%s)",
	   tok_in[3], tok_in[2], tok_in[0], tok_in[4]);
    if (strcmp(tok_in[3], IRCNAME) == 0) {
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
	switchitem();
#else
	objlist = removeitem(tok_in[2], objlist);
	if (object == NULL)
	    object = objlist;
	if (object != NULL)
	    printf("\n\r*** Now talking in %s", object->name);
	wasdate = 0;
#endif
    }
    return 0;
}

static int dokill(void)
{
    printf("*** %s killed by %s (%s)", tok_in[3], tok_in[0], tok_in[4]);
    return 0;
}

static int domode(void)
{
    printf("*** %s changed %s to:", tok_in[0], tok_in[2]);
    return 3;
}

static int donick(void)
{
    if (strcmp(tok_in[0], IRCNAME) == 0) {
	wasdate = 0;
	strlcpy(IRCNAME, tok_in[2], sizeof(IRCNAME));
    }
    printf("*** %s is now known as %s", tok_in[0], tok_in[2]);
    return 0;
}

static int donotice(void)
{
    if (*tok_in[2] != '#')
	column = printf("-%s-", tok_in[0]);
    else
	column = printf("-%s:%s-", tok_in[0], tok_in[2]);
    return 3;
}

static int dopart(void)
{
    printf("*** %s (%s) left %s", tok_in[0], fromhost,
	   tok_in[2]);
    if (strcmp(tok_in[0], IRCNAME) == 0) {
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
        switchitem();
#else
	objlist = removeitem(tok_in[2], objlist);
	if (object == NULL)
	    object = objlist;
	if (object != NULL)
	    printf("\n\r*** Now talking in %s", object->name);
	wasdate = 0;
#endif
    }
    return 0;
}

static int dopong(void)
{
    column = printf("*** Got PONG from %s:", tok_in[0]);
    return 3;
}

static int doprivmsg(void)
{
    if (*tok_in[3] == 1) {
	/* rudimentary CTCP handler */
	char ch, ctcp[50];
	int i = 1, skipout = 0;

	while ((ch = tok_in[3][i]) && ch != 1 && ch != ' ' && i < 50)
		ctcp[i++ - 1] = toupper(ch);
	if (i == 50 || tok_in[3][strlen(tok_in[3]) - 1] != 1)
		goto noctcp;
	ctcp[i++ - 1] = 0;
	tok_in[3][strlen(tok_in[3]) - 1] = 0;
	if (!strcmp(ctcp, "ENTROPY")) {
		dopush(serverdata, sizeof(serverdata));
		snprintf(bp, sizeof(bp),
		    "%s initiated the RANDEX protocol with %s",
		    tok_in[0], *tok_in[2] == '#' ? tok_in[2] : "you");
		column = printf("*C*");
		column = wordwrapout(bp, column);
		skipout = 1;
		snprintf(lineout, LINELEN, "NOTICE %s :\001RANDOM ",
		    tok_in[0]);
		while (strlen(lineout) < 240) {
			char buf[9];

			snprintf(buf, 9, "%08X", arc4random());
			strlcat(lineout, buf, LINELEN);
		}
		strlcat(lineout, "\001\n", LINELEN);
		sendline();
	} else if (!strcmp(ctcp, "RANDOM")) {
		dopush(serverdata, sizeof(serverdata));
		snprintf(bp, sizeof(bp),
		    "%s queried RANDEX protocol information from %s",
		    tok_in[0], *tok_in[2] == '#' ? tok_in[2] : "you");
		column = printf("*C*");
		column = wordwrapout(bp, column);
		skipout = 1;
		snprintf(lineout, LINELEN,
		    "PRIVMSG %s :\001ACTION uses %s with built-in support"
		    " for the RANDEX protocol, push API: %s\001\n", tok_in[0],
		    RELEASE_L, RELEASE_PAPI);
		sendline();
	} else if (!strcmp(ctcp, "VERSION")) {
		snprintf(lineout, LINELEN,
		    "NOTICE %s :\001VERSION %s (RANDOM=%u)\001\n",
		    tok_in[0], RELEASE_L, arc4random());
		sendline();
	} else if (!strcmp(ctcp, "CLIENTINFO")) {
		snprintf(lineout, LINELEN,
		    "NOTICE %s :\001CLIENTINFO ACTION, CLIENTINFO, ECHO, "
		    "ENTROPY, ERRMSG, PING, RANDOM, TIME, VERSION\001\n",
		    tok_in[0]);
		sendline();
	} else if (!strcmp(ctcp, "PING") ||
	    !strcmp(ctcp, "ECHO") || !strcmp(ctcp, "ERRMSG")) {
		snprintf(lineout, LINELEN,
		    "NOTICE %s :\001%s %s\001\n",
		    tok_in[0], ctcp, tok_in[3] + i);
		sendline();
	} else if (!strcmp(ctcp, "TIME")) {
		time_t t;

		time(&t);
		snprintf(lineout, LINELEN,
		    "NOTICE %s :\001TIME ", tok_in[0]);
		strftime(lineout + strlen(lineout),
		    LINELEN - strlen(lineout),
		    "%c\001\n", localtime(&t));
		sendline();
	} else if (!strcmp(ctcp, "ACTION")) {
		if (*tok_in[2] != '#')
			column = printf("[*] %s", tok_in[0]);
		else if (object != NULL && object->name != NULL &&
		    my_stricmp(object->name, tok_in[2]))
			column = printf("* %s:%s", tok_in[0], tok_in[2]);
		else
			column = printf("* %s", tok_in[0]);
		column = wordwrapout(tok_in[3] + i, column);
		skipout = 1;
	}
	if (!skipout) {
		snprintf(bp, sizeof(bp), "%s did a CTCP %s%s%s to %s",
		    tok_in[0], ctcp, tok_in[3][i] ? " " : "",
		    tok_in[3] + i, *tok_in[2] == '#' ? tok_in[2] : "you");
		column = printf("*C*");
		column = wordwrapout(bp, column);
	}
	i = 4;
	while (tok_in[i])
		column = wordwrapout(tok_in[i++], column);
	return (0);
    }
 noctcp:
    if (*tok_in[2] != '#') {
#ifdef USE_ANSICOLOR
	printf("\E[1m");  // BOLD (for private msgs)
#endif
	column = printf("*%s*", tok_in[0]);
    }
    else if (object != NULL && object->name != NULL &&
      my_stricmp(object->name, tok_in[2]))
	column = printf("<%s:%s>", tok_in[0], tok_in[2]);
    else
	column = printf("<%s>", tok_in[0]);
    return 3;
}

static int doquit(void)
{
    printf("*** %s (%s) Quit (%s)", tok_in[0], fromhost, tok_in[2]);
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
    // Probably should see if name exists already.
    {
      int i, found=0;
      for (i = 0; i < namecount; i++) {
	if (!strcmp(names[i],tok_in[0])) {
	  free(names[i]);
	  found++;
	  break;
	}
      }
      for (i++; i < namecount; i++)
	names[i-1] = names[i];
      if (found) namecount--;
      object->namecount = namecount;
    }
#endif
    return 0;
}

static int dosquit(void)
{
    return 1;
}

static int dotime(void)
{
    return 1;
}

static int dotopic(void)
{
    printf("*** %s set %s topic to \"%s\"", tok_in[0], tok_in[2],
	   tok_in[3]);
    return 0;
}

int donumeric(int num)
{
    if (num == 352) {		/* rpl_whoreply */
	column = printf("%-14s %-10s %-3s %s@%s :", tok_in[3], tok_in[7],
			tok_in[8], tok_in[4], tok_in[5]);
	return 9;
    } else if (num == 432 || num == 433) {
	char ch;
	printf("*** You've chosen an invalid nick.  Choose again.");
	tputs_x(tgoto(cap_cm, 0, maxlin));
	tputs_x(cap_ce);
	printf("New Nick? ");
	fflush(stdout);
	resetty();
	tmp = IRCNAME;
	while ((ch = getchar()) != '\n')
	    if (strlen(IRCNAME) < sizeof(IRCNAME) - 1)
		*(tmp++) = ch;
        *tmp = '\0';
	wasdate = 0;
	raw();
	snprintf(lineout, LINELEN, "NICK :%s\n", IRCNAME);
	sendline();
	tputs_x(tgoto(cap_cm, 0, maxlin));
	tputs_x(cap_ce);
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
    } else  if (num == 353) {
        char *t, *s = strdup(tok_in[5]);
	namecount = 0;
	for (t = strtok(s, " "); t; t = strtok(NULL, " ")) {
	  names[namecount] = strdup(t);
	  if (++namecount >= 256) namecount = 0; // Ran out.  uh oh.
	}
	object->namecount = namecount;
	free(s);
#if 0	/* DEBUG */
        FILE *fp;
        int i;
	fp = fopen("/tmp/ircnames.txt", "a");
	fprintf(fp, "namecount = %d\n------------\n", namecount);
	for (i = 0; tok_in[i]; i++)
	  fprintf(fp, "%s\n",tok_in[i]);
	fprintf(fp, "-------\n");
	for (i = 0; i < namecount; i++){
	  fprintf(fp, "%s\n", names[i]);
	}
	fclose(fp);
#endif
	column = printf("%s", tok_in[1]);
	return 3;
#endif
    } else {			/* all remaining numerics */
	column = printf("%s", tok_in[1]);
	return 3;
    }
    return (0);
}

#define	LISTSIZE	55
#define	DO_JOIN		12
#define	DO_MSG		18
#define	DO_PRIVMSG	30
#define	DO_QUIT		32
#define	DO_QUOTE	31
#define	DO_W		46
#define	DO_WHOIS	49
#define DO_ME		51
#define DO_DESCRIBE	52
#define DO_CTCP		53
#define DO_NOTICE	21
#define DO_NAMES	23
#define DO_NS		54
static const char *cmdlist[LISTSIZE] =
{"AWAY", "ADMIN", "CONNECT", "CLOSE", "DIE", "DNS", "ERROR", "HELP",
 "HASH", "INVITE", "INFO", "ISON", "JOIN", "KICK", "KILL", "LIST", "LINKS",
 "LUSERS", "MSG", "MODE", "MOTD", "NOTICE", "NICK", "NAMES", "NOTE", "OPER",
 "PART", "PASS", "PING", "PONG", "PRIVMSG", "QUOTE", "QUIT", "REHASH", "RESTART",
 "SERVER", "SQUIT", "STATS", "SUMMON", "TIME", "TOPIC", "TRACE", "USER",
 "USERHOST", "USERS", "VERSION", "W", "WALLOPS", "WHO", "WHOIS", "WHOWAS",
 "ME", "DESCRIBE", "CTCP", "NS"};
static int numargs[LISTSIZE] =
{1, 1, 3, 1, 1, 1, 1, 1,
 1, 2, 1, 1, 1, 3, 2, 1, 1,
 2, 2, 2, 1, 2, 1, 1, 1, 2,
 1, 1, 1, 1, 2, 0, 1, 1, 1,
 3, 2, 1, 3, 1, 2, 2, 4,
 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 0
};
static int (*docommand[LISTSIZE]) (void) =
{nop, nop, nop, nop, nop, nop, doerror, nop,
 nop, doinvite, nop, nop, dojoin, dokick, dokill, nop, nop,
 nop, nop, domode, nop, donotice, donick, nop, nop, nop,
 dopart, nop, nop, dopong, doprivmsg, nop, doquit, nop, nop,
 nop, dosquit, nop, nop, dotime, dotopic, nop, nop,
 nop, nop, nop, nop, nop, nop, nop, nop,
 nop, nop, nop, dons
};

int wordwrapout(char *ptrx, size_t count)
{
    while(ptrx != NULL) {
	if ((tmp = strchr(ptrx, ' ')) != NULL)
	    *(tmp++) = '\0';
	if (strlen(ptrx) < maxcol - count)
	    count += printf(" %s", ptrx);
	else
	    count = printf("\n\r   %s", ptrx);
	ptrx = tmp;
    }
    return count;
}

int parsedata(void)
{
    int i, found = 0;

/* take care of ping silently */
    if (serverdata[0] == 'P') {
	snprintf(lineout, LINELEN, "PONG :%s\n", &serverdata[6]);
	return sendline();
    }
    if (!dumb)
	tputs_x(tgoto(cap_cm, 0, maxlin - 2));

    tok_in[i = 0] = serverdata;
    tok_in[i]++;
    while (tok_in[i] != NULL)
	if (*tok_in[i] == ':')
	    break;
	else {
	    if ((tmp = strchr(tok_in[i], ' ')) != NULL) {
		tok_in[++i] = &tmp[1];
		*tmp = '\0';
	   } else
		tok_in[++i] = NULL;
	}
    if (tok_in[i] != NULL)
	tok_in[i]++;
    tok_in[++i] = NULL;

    if (!dumb)
	putchar('\n');
    if ((tmp = strchr(tok_in[0], '!')) != NULL) {
	fromhost = &tmp[1];
	*tmp = '\0';
    } else
	fromhost = NULL;

    column = 0;
#ifdef ZIPIT_Z2 /* timestamp */
    if (timestamp) {
      datenow = time(NULL);
      timenow = localtime(&datenow);
      printf("%02d:%02d ", timenow->tm_hour, timenow->tm_min);
    }
#endif
    if ((i = atoi(tok_in[1])))
	i = donumeric(i);
    else {
	for (i = 0; i < LISTSIZE && !found; i++)
	    found = (strcmp(cmdlist[i], tok_in[1]) == 0);
	if (found)
	    i = (*docommand[i - 1]) ();
	else
	    i = nop();
    }

#ifdef ZIPIT_Z2 /* timestamp */
    if (timestamp) 
        column += 6; // donumeric, docommand assume column starts at 0.
#endif
    if (i)
	while (tok_in[i])	/* output remainder with word wrap */
	    column = wordwrapout(tok_in[i++], column);
#ifdef USE_ANSICOLOR
    printf("\E[m"); // Not BOLD
#endif
    if (dumb)
	putchar('\n');
    if (strncmp(tok_in[1], "Closing", 7) == 0)
	return 0;
    return 1;
}

int serverinput(void)
{
    char ich;
    while (read(sockfd, &ich, 1) > 0)
	if (ich != '\n' && ich != '\r')
	    serverdata[cursd++] = ich;
	else {
	    serverdata[cursd] = 0;
	    arc4hashpush(serverdata);
	    if (cursd) {
		cursd = 0;
		return parsedata();
	    }
	}
    return 1;
}

void parseinput(void)
{
    int i = 0, j = 0, outcol = 0, found = 0;

    arc4hashpush(linein);
    while (linein[i] == ' ')
	++i;
    if (linein[i] == '\0')
	return;
    if (linein[i] == COMMANDCHAR) {
	++i;
	if (linein[i] == ' ')
		j = ++i;
	while (linein[i] == ' ')
		++i;
	if (linein[i] == '\0')
		return;
    }

    strlcpy(inputbuf, linein, 512);
    tok_out[i = 0] = strtok(inputbuf," ");
    while((tok_out[++i] = strtok(NULL, " ")) != NULL);
    if (!dumb) {
	tputs_x(tgoto(cap_cm, 0, maxlin - 2));
	putchar('\n');
    }
    if (j) {
	linein += j;
	j = 1;
	goto do_say;
    }
    if (*tok_out[0] == COMMANDCHAR) {
	tok_out[0]++;
	for (i = 0; (size_t)i < strlen(tok_out[0]) && isalpha(tok_out[0][i]); i++)
	    tok_out[0][i] = toupper(tok_out[0][i]);
	for (i = 0; i < LISTSIZE && !found; i++)
	    found = (strncmp(cmdlist[i], tok_out[0], strlen(tok_out[0])) == 0);
	i--;
	if (!found) {
	    printf("*** Invalid command");
	    return;
	}
	if (i == DO_ME) {
		if (object == NULL || object->name == NULL) {
			printf("*** Nowhere to send");
			return;
		}
		while ((*linein) && (*linein != ' '))
			++linein;
		snprintf(lineout, LINELEN, "PRIVMSG %s :%cACTION%s%c\n",
		    object->name, 1, linein, 1);
		outcol = printf("* %s", IRCNAME);
		j = 0;
		while(tok_out[++j])
		   outcol = wordwrapout(tok_out[j], outcol);
		goto parseinput_done;
	}
	if (i == DO_DESCRIBE) {
		while ((*linein) && (*linein != ' '))
			++linein;
		while ((*linein) && (*linein == ' '))
			++linein;
		tmp = linein;
		while ((*linein) && (*linein != ' '))
			++linein;
		if (!*tmp || !*linein || tmp >= linein) {
			printf("*** Nothing to send");
			return;
		}
		*linein++ = '\0';
		if (!strcmp(tmp, "*")) {
			if (object == NULL || (tmp = object->name) == NULL) {
				printf("*** Nowhere to send");
				return;
			}
		} else if (object && object->name && strcmp(tmp, object->name))
			pushlastchan(tmp);
		snprintf(lineout, LINELEN, "PRIVMSG %s :\001ACTION %s\001\n",
		    tmp, linein);
		if (*tmp == '#')
			outcol = printf("* %s:%s", IRCNAME, tmp);
		else
			outcol = printf("*-> %s: %s", tmp, IRCNAME);
		linein[-1] = ' ';
		j = 1;
		while(tok_out[++j])
			outcol = wordwrapout(tok_out[j], outcol);
		goto parseinput_done;
	}
	if (i == DO_CTCP) {
		char *tmp2;

		while ((*linein) && (*linein != ' '))
			++linein;
		while ((*linein) && (*linein == ' '))
			++linein;
		tmp = linein;
		while ((*linein) && (*linein != ' '))
			++linein;
		if (!*tmp || !*linein || tmp >= linein) {
			printf("*** Nothing to send");
			return;
		}
		*linein++ = '\0';
		tmp2 = linein;
		while (*tmp2 && *tmp2 != ' ')
			*tmp2++ = toupper(*tmp2);
		if (!strcmp(tmp, "*")) {
			if (object == NULL || (tmp = object->name) == NULL) {
				printf("*** Nowhere to send");
				return;
			}
		} else if (object && object->name && strcmp(tmp, object->name))
			pushlastchan(tmp);
		snprintf(lineout, LINELEN, "PRIVMSG %s :\001%s\001\n",
		    tmp, linein);
		snprintf(bp, sizeof(bp),
		    "Sending a CTCP %s to %s", linein, tmp);
		linein[-1] = ' ';
		column = printf("*C*");
		outcol = wordwrapout(bp, outcol);
		goto parseinput_done;
	}
	if (i == DO_JOIN)
	    if ((newobj = finditem(tok_out[1], objlist)) != NULL) {
		object = newobj;
		wasdate = 0;
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
		printf("\E[1;35m"); // MAGENTA
#endif
		printf("*** Now talking in %s", object->name);
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
		printf("\E[0;39m"); // NORMAL
#endif
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
		names = object->names;
		namecount = object->namecount;
#endif
		return;
	    }
	if (i == DO_MSG)
	    i = DO_PRIVMSG;
	if (i == DO_W) {
		snprintf(lineout, LINELEN, "WHOIS %s %s",
		    tok_out[1], tok_out[1]);
		outcol = printf("= %s", lineout);
		goto parseinput_cont;
	}
	if (i == DO_NAMES && !tok_out[1]) {
		if (object == NULL) {
			printf("*** Nowhere to send");
			return;
		}
		snprintf(lineout, LINELEN, "NAMES %s", object->name);
		outcol = printf("= %s", lineout);
		goto parseinput_cont;
	}

	if (i == DO_PRIVMSG || i == DO_JOIN || i == DO_NOTICE)
		pushlastchan(tok_out[1]);

	strlcpy(lineout, (i == DO_QUOTE) ? "" : cmdlist[i], LINELEN);
	j = 0;
#ifdef ZIPIT_Z2 /* version */
	if (i == DO_QUIT)
	    snprintf(&lineout[strlen(lineout)], LINELEN - strlen(lineout),
		" :using %s", RELEASE_L);
#else
	if (i == DO_QUIT)
	    snprintf(&lineout[strlen(lineout)], LINELEN - strlen(lineout),
		" :using %s (RANDOM=%u)", RELEASE_L, arc4random());
#endif
#ifdef USE_ANSICOLOR
	//printf("\E[1m");
#endif
	outcol = printf("= %s", lineout);
	while (tok_out[++j]) {
	   snprintf(&lineout[strlen(lineout)], LINELEN - strlen(lineout),
		   "%s%s", ((j == numargs[i] &&
		   tok_out[j + 1] != NULL) ? " :" : " "), tok_out[j]);
	   outcol = wordwrapout(tok_out[j], outcol);
	}
#ifdef USE_ANSICOLOR
	//printf("\E[m");
#endif
	if (i == DO_NS && strstr(lineout, "%s") != NULL) {
		char ch;

		strlcpy(inputbuf, lineout, 384);
		tmp = inputbuf + 384;
		tputs_x(tgoto(cap_cm, 0, maxlin));
		tputs_x(cap_ce);
		printf("Password? ");
		fflush(stdout);
		while ((ch = getchar()) != '\n')
			if (ch == 8 || ch == 127) {
				if (tmp > inputbuf + 384)
					--tmp;
			} else if (tmp < inputbuf + sizeof(inputbuf) - 2)
				*tmp++ = ch;
		*tmp = '\0';
		wasdate = 0;
		tputs_x(tgoto(cap_cm, 0, maxlin));
		tputs_x(cap_ce);
		snprintf(lineout, LINELEN, inputbuf, inputbuf + 384);
		bzero(inputbuf, sizeof(inputbuf));
	}

 parseinput_cont:
	strlcat(lineout, "\n", LINELEN);
    } else {
	j = 0;
 do_say:
	if (object == NULL) {
	    printf ("*** Nowhere to send");
	    return;
	}
	snprintf(lineout, LINELEN, "PRIVMSG %s :%s\n", object->name, linein);
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
	printf("\E[1;32m"); // GREEN
#endif
#ifdef ZIPIT_Z2 /* timestamp */
	if (timestamp) {
	  datenow = time(NULL);
	  timenow = localtime(&datenow);
	  outcol = printf("%02d:%02d > %s", 
		     timenow->tm_hour, timenow->tm_min, tok_out[j]);
	}
	else
#endif
	outcol = printf("> %s", tok_out[j]);
	while(tok_out[++j])
	   outcol = wordwrapout(tok_out[j], outcol);
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
	printf("\E[0;39m"); // NORMAL
#endif
    }
 parseinput_done:
    sendline();
    idletimer = time(NULL);
}

void histupdate(void)
{
    linein = history[histline];
    curx = curli = strlen(linein);
    tputs_x(tgoto(cap_cm, 0, maxlin));
    printf("%s", &linein[(curli / maxcol) * maxcol]);
    tputs_x(cap_ce);
}

void printpartial(int fromx)
{
    int i;
    tputs_x(tgoto(cap_cm, 0, maxlin));
    for(i = fromx; i < fromx + maxcol && i < curli; i++)
	putchar(linein[i]);
    tputs_x(cap_ce);
    tputs_x(tgoto(cap_cm, curx % maxcol, maxlin));
}

void userinput(void)
{
    int i, z;
    char ch;
    static int lasttab = -1;

    if (dumb) {
	fgets(linein, 500, stdin);
	tmp = strchr(linein, '\n');
	if (tmp != NULL)	/* get rid of newline */
	    *tmp = '\0';
	parseinput();
	putchar('\n');
    } else {
	read(stdinfd, &ch, 1);
#ifdef ZIPIT_Z2 /* Handle arrow keys */
	if (ch == 27) {				// Remap ESC codes to control chars
		FD_ZERO(&readfs);
		FD_SET(stdinfd, &readfs);
	    	time_out.tv_usec = 0;
		select(FD_SETSIZE, &readfs, NULL, NULL, &time_out);
		if (FD_ISSET(stdinfd, &readfs))
		{
		    read(stdinfd, &ch, 1); 	// Expect '['
		    FD_ZERO(&readfs);
		    FD_SET(stdinfd, &readfs);
		    time_out.tv_usec = 0;
		    select(FD_SETSIZE, &readfs, NULL, NULL, &time_out);
		    if (FD_ISSET(stdinfd, &readfs))
		      read(stdinfd, &ch, 1); 	// After '['
		}
		switch (ch) {
		case 27: ch = '\27'; break;	// ESC -> ^W Change Channel
		case 'D': ch=2; break;		// Left arrow -> ^B
		case 'C': ch=6; break;		// Right arrow -> ^F
		case 'A': ch=16; break;		// Up -> ^P
		case 'B': ch=14; break;		// Down -> ^N
                case '1': read(stdinfd, &ch, 1); ch=1;  break; // Home -> ^A
                case '4': read(stdinfd, &ch, 1); ch=5;  break; // End  -> ^E
                case '5': read(stdinfd, &ch, 1); ch=16; break; // PgUp -> ^P
                case '6': read(stdinfd, &ch, 1); ch=14; break; // PgUp -> ^N
		}
	}
#endif /* ZIPIT_Z2 Handle arrow keys */
	if (ch == '\177')
	    ch = '\10';
	if (ch != '\t')
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
	        nametab = -1;
	if (ch != '\17')
#endif
		lasttab = -1;
	switch (ch) {
	case '\1':		/* C-a */
	    if (curx >= maxcol)
	        printpartial(curx = 0);
	    else
		tputs_x(tgoto(cap_cm, curx = 0, maxlin));
	    break;
	case '\10':		/* C-h */
	    if (curx)
	case '\4':		/* C-d */
	    if (curli) {
		if ((ch == '\4') && (curx < curli))
		    curx++;
		if (curli == curx)
		    linein[(--curx)] = '\0';
		else		/* delete somewhere in the middle */
		    for (i = (--curx); i < curli; i++)
			linein[i] = linein[i + 1];
		curli--;
		if (cap_dc != NULL && curx % maxcol != maxcol - 1) {
		    tputs_x(tgoto(cap_cm, curx % maxcol, maxlin));
		    tputs_x(cap_dc);
		} 
#ifndef ZIPIT_Z2  /* BS, DEL need full redraw for long lines to refresh right.  */
		else
#endif
		    printpartial((curx / maxcol) * maxcol);
	    }
	    break;
	case '\2':		/* C-b */
	    if (curx > 0)
		curx--;
	    if (curx % maxcol == maxcol - 1)
		printpartial((curx / maxcol) * maxcol);
	    else
		tputs_x(tgoto(cap_cm, curx % maxcol, maxlin));
	    break;
	case '\3':		/* C-c */
	    beenden = 1;
	    break;
	case '\5':		/* C-e */
	    curx = curli;
	case '\14':		/* C-l */
	    dowinsz(2);
	    break;
	case '\6':		/* C-f */
	    if (curx < curli)
		curx++;
#ifdef ZIPIT_Z2 
	    if (curx % maxcol)
	      tputs_x(tgoto(cap_cm, curx % maxcol, maxlin));
	    else // Moved offscreen.  Redraw.
	      printpartial((curx / maxcol) * maxcol);
#else
	    tputs_x(tgoto(cap_cm, curx % maxcol, maxlin));
#endif
	    break;
	case '\16':		/* C-n */
	    if ((++histline) >= HISTLEN)
		histline = 0;
	    histupdate();
	    break;
	case '\20':		/* C-p */
	    if ((--histline) < 0)
		histline = HISTLEN - 1;
	    histupdate();
	    break;
	case '\25':		/* ^U */
	case '\r':
	case '\n':
	    if (!curli)
		return;
	    tputs_x(tgoto(cap_cm, 0, maxlin));
	    tputs_x(cap_ce);
	    if (ch != '\25')
		parseinput();
	    if ((++histline) >= HISTLEN)
		histline = 0;
	    curx = curli = 0;
	    linein = history[histline];
	    break;
#ifdef ZIPIT_Z2 /* ctrl-t = toggle timestamp */
	case '\24':
	    timestamp = (timestamp+1) % 2;
	    wasdate = 0;
	    break;
#endif
	case '\27':
	    if (object == NULL)
		break;
	    object = object->next;
	    if (object == NULL)
		object = objlist;
	    wasdate = 0;
	    break;
	case '\t':
#ifdef ZIPIT_Z2 /* Use tab to complete NICK, ^O for other tab. */
	    if (nametab < 0) {
	      strcpy(namesearch,linein); /* Start new completion search */
	      if (nameptr = strrchr(namesearch, ' '))
		*nameptr++ = 0; /* Search on remaining string after the space. */
	      else nameptr = namesearch; /* no spaces, do search on entire string. */
	    }
	    if (++nametab >= namecount)
	      nametab = 0;
	    if (namecount == 0)
	      nametab = -1;
	    else { /* Find next match if any, starting at nametab. */
	      int i, j, n = strlen(nameptr);
	      for (i = nametab; i < nametab+namecount; i++) {
		j = i % namecount;
		if (!strncasecmp(names[j], nameptr, n)) {
		  nametab = j;
		  break;
		}
	      }
	      /*
	      if (curx >= maxcol)
	        printpartial(curx = 0);
	      else
		tputs_x(tgoto(cap_cm, curx = 0, maxlin));
	      if (i % maxcol) 
		tputs_x(cap_ce);
	      */
	      if (nameptr == namesearch)
		snprintf(history[histline], 512, "%s: ", names[nametab]);
	      else
		snprintf(history[histline], 512, "%s %s ", namesearch, names[nametab]);
	      histupdate();
	    }
	    break;
	case '\17':		/* C-o */
#endif
	    if (++lasttab == 0) {
		if (lastchans[0] == NULL)
			break;
		if (curli) {
			if ((++histline) >= HISTLEN)
				histline = 0;
			linein = history[histline];
		}
	    }
	    for (;;) {
		if (lasttab >= NLASTCHAN)
			lasttab = 0;
		if (lastchans[lasttab])
			break;
		++lasttab;
	    }
	    snprintf(history[histline], 512, "/m %s ", lastchans[lasttab]);
	    histupdate();
	    break;
	case '\26':		/* ^V */
	    /* quote - press ^V^V^A to insert a ^A, it's invisible tho */
	    read(stdinfd, &ch, 1);
	    /* FALLTHROUGH */
	default:
	    if (curli < 499) {
		if (curli == curx) {	/* append character */
		    linein[++curli] = '\0';
		    linein[curx++] = ch;
		    putchar(ch);
		    tputs_x(cap_ce);
		} else {	/* insert somewhere in the middle */
		    for (i = (++curli); i >= curx; i--)
			linein[i + 1] = linein[i];
		    linein[curx] = ch;
		    for (i = (curx % maxcol); i < maxcol &&
			(z = (curx / maxcol) * maxcol + i) < curli; i++)
			putchar(linein[z]);
		    tputs_x(cap_ce);
		    curx++;
		}
#ifdef ZIPIT_Z2 /* If cursor not on last char of line, clear to end. */
		if (i % maxcol) 
		  tputs_x(cap_ce);
#endif
	    }
	    break;
	}
    }
}

void cleanup(int sig)
{
#ifdef ZIPIT_Z2
    tputs_x(tgoto(cap_cs, maxlin, 0));
#else
    tputs_x(tgoto(cap_cs, -1, -1));
#endif
    tputs_x(tgoto(cap_cm, 0, maxlin));
    resetty();
    printf("\ngot signal %d\n", sig);
    exit(128 + sig);
}

static void
dowinch(int sig __attribute__((unused)))
{
	sigwinch = 1;
}

static void
dowinsz(int scrupdate)
{
	struct winsize ws;
#ifdef ZIPIT_Z2
	maxcol = maxlin = -1;
	if (ioctl(stdoutfd, TIOCGWINSZ, &ws) >= 0) {
		maxcol = ws.ws_col - 0;
		maxlin = ws.ws_row - 1;
	}
	if (maxcol < 5)
		maxcol = 80;
	if (maxlin < 2)
		maxlin = 23;
#else
	maxcol = maxlin = -1;
	if (ioctl(stdoutfd, TIOCGWINSZ, &ws) >= 0) {
		maxcol = ws.ws_col - 2;
		maxlin = ws.ws_row - 1;
	}
	if (maxcol < 5)
		maxcol = 78;
	if (maxlin < 5)
		maxlin = 23;
#endif
	wasdate = 0;
	if (scrupdate) {
		tputs_x(tgoto(cap_cs, maxlin - 2, 0));
		updatestatus();
		printpartial((curx / maxcol) * maxcol);
	}
}

int
main(int argc, char *argv[])
{
    char hostname[64];
    char *ircusername, *ircgecosname;
    int i;
#ifdef ZIPIT_Z2 /* autojoin */
    int id = 0;
    char pass[200]=""; // Server password
    char keys[200]=""; // Collect comma separated channel keys.
    char chans[200]=""; // Collect comma separated channel names.
    char ident[200]=""; // NickServ password
    char *ircautojoin = getenv("IRCJOIN");
#endif

    for (i = 0; i < NLASTCHAN; ++i)
	lastchans[i] = NULL;
    stdinfd = fileno(stdin);
    stdoutfd = fileno(stdout);
    ircusername = getenv("IRCUSER");
    ircgecosname = getenv("IRCNAME");
    if (!ircusername || !ircgecosname) {
	struct passwd *userinfo;
	userinfo = getpwuid(getuid());
	ircusername = ircusername ? ircusername : userinfo->pw_name;
	ircgecosname = ircgecosname ? ircgecosname : userinfo->pw_gecos;
    }
    strlcpy(hostname, DEFAULTSERVER, sizeof(hostname));
    tmp = getenv("IRCNICK");
    strlcpy(IRCNAME, tmp ? tmp : ircusername, sizeof(IRCNAME));
#if 1 /* ZIPIT_Z2 change root IRCLOGIN name to zippy<something> */
    srand(time(NULL));
    if (tmp == NULL)
      sprintf(IRCNAME, "zippy%03d", (rand() % 1000));
#endif
    if (argc > 1) {
	for (i = 1; i < argc; i++)
	    if (argv[i][0] == '-') {
		if (argv[i][1] == 'd')
		    dumb = 1;
#ifdef ZIPIT_Z2 /* timestamp */
		else if (argv[i][1] == 't')
		  timestamp = 1;
#endif
#ifdef ZIPIT_Z2 /* autojoin */
		else if (argv[i][1] == 'i')
		  id = 1;
#endif
		else {
		    fprintf(stderr, "usage: %s %s\n", argv[0],
			    "[nick] [server] [port] [-dumb]");
		    exit(1);
		}
#ifdef ZIPIT_Z2 /* autojoin */
	    } else if (strchr(argv[i], '.') || strchr(argv[i], '/')) { 
		int port = DEFAULTPORT;
		char page[200];
		if (!strncmp(argv[i], "irc://", 6))
		  argv[i] += 6;
		if (sscanf(argv[i], "%63[^:]:%i/%199[^\n]", hostname, &port, page) == 3) 
		  {IRCPORT = port; ircautojoin = strdup(page);}
		else if (sscanf(argv[i], "%63[^/]/%199[^\n]", hostname, page) == 2)
		  ircautojoin = strdup(page);
		else if (sscanf(argv[i], "%63[^:]:%i[^\n]", hostname, &port) == 2)
		  IRCPORT = port;
		else if (sscanf(argv[i], "%63[^\n]", hostname) == 1) {}
#else
	    } else if (strchr(argv[i], '.')) {
		strlcpy(hostname, argv[i], sizeof(hostname));
#endif
	    } else if (atoi(argv[i]) > 255) {
		IRCPORT = atoi(argv[i]);
	    } else
		strlcpy(IRCNAME, argv[i], sizeof(IRCNAME));
    }
#ifdef ZIPIT_Z2 /* autojoin */
    // Maybe server could be url format: nick:passwd@server:port//channel/channel/channel
    // Autojoin any channels.  Identify if password given.
    // Could use -i to force it to ask for a nickserv identity password (getpass) 
    // Then send "msg nickserv identify <passwd>" after the NICK line.
    // 
    if (ircautojoin) {
      // do a for loop with strtok("/") for multi channel joins
      // if no leading #, &, +, or ! then add the missing #.
      // Should I handle % encoding for prefix?  ( %23 = #, %26 = & )
      // JOIN takes multiple channels separated by commas followed by comma separated keys.
      // eg. JOIN #foo,#bar fookey,barkey
      char c, *s, *p, *ch;
      for (ch = strtok(ircautojoin,"/"); ch != NULL; ch = strtok(NULL, "/")) {
	int k=0;
	for (s = strpbrk(ch, ",?&"); s; s = strpbrk(s, ",?&")){
	  c = *s; 
	  *s=0;
	  s++;
	  switch (c){
	  case ',': // option needpass,needkey,isnick,isserver
	    if (!strncmp(s, "needpass", strlen("needpass")))
	      if (p = getpass("Server Password: "))
		strncpy(pass, p, 199);
	    if (strlen(ch) && !strncmp(s, "needkey", strlen("needkey")))
	      k = 1;
	    break;
	  case '?': // Get Server Pass.  Get keys if we have a channel. 
	  case '&':
	    sscanf(s, "identify=%200[^&]",ident);
	    sscanf(s, "pass=%200[^&]",pass);
	    if (strlen(ch) && (k<2)) {
	      if (sscanf(s, "key=%200[^&]",keys+strlen(keys)) == 1)
		k = 2; // Cancel any needkey for this channel
	    }
	    break;
	  }
	}
	if (strlen(ch)) { // If we got a channel, then add it to join list.
	  if (k == 1){ // If we needkey then get it.
	    char prompt[212];
	    sprintf(prompt, "Channel %s Password: ", ch);
	    if (p = getpass(prompt))
	      snprintf(keys+strlen(keys), 199-strlen(keys),",%s", p);
	  }
	  strcat(keys, ","); // Comma separate the keys
	  if (strlen(chans)) 
	    strcat(chans, ","); // Comma separate the channels
	  if (strpbrk(ch, "#&+!") != ch)
	    strcat(chans, "#"); // Add # prefix if no prefix specified.
	  strcat(chans, ch);
	}
      }
    }
    if (id && !strlen(ident)) {
      char *p;
      if (p = getpass("NickServ Password: "))
	strncpy(ident, p, 199);
    }
#endif
#if 0	/* DEBUG */
#else
    printf("*** trying port %d of %s\n\n", IRCPORT, hostname);
    if ((sockfd = makeconnect(hostname)) < 0) {
	fprintf(stderr, "*** %s connection refused, aborting\n", hostname);
	exit(0);
    }
#endif
#ifdef ZIPIT_Z2 /* autojoin */
    // Must send "PASS <passwd>" msg *BEFORE* NICK and USER msgs.
    if (strlen(pass)) {
      snprintf(lineout, LINELEN, "PASS %s\n", pass);
      sendline();
    }
#endif
    if (gethostname(localhost, sizeof(hostname)) || !*localhost)
	strlcpy(localhost, "localhost", sizeof(hostname));
    snprintf(lineout, LINELEN, "USER %s %s %s :%s\n", ircusername,
	localhost, hostname, ircgecosname);
    sendline();
    snprintf(lineout, LINELEN, "NICK :%s\n", IRCNAME);
    sendline();
#ifdef ZIPIT_Z2 /* autojoin */
    if (strlen(ident)) {
      //snprintf(lineout, LINELEN, "MSG NickServ IDENTIFY %s\n", ident);
      snprintf(lineout, LINELEN, "NS IDENTIFY %s\n", ident);
      sendline();
      // sleep(30);
      wasdate = time(NULL) + 30; // Loop 30 secs for IDENTIFY rsp.
      do { 
        serverinput();
        datenow = time(NULL);
      }
      while (datenow < wasdate);
      wasdate = 0;
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
      printf("\E[1;32m"); // GREEN
#endif
      printf("\nIDENTIFY 30 secs is up.\n");
#ifdef USE_ANSICOLOR    /* ZIPIT_Z2 */
      printf("\E[0;39m"); // NORMAL
#endif
      printf("\n"); // This line gets eaten by the ncurses stuff below.
      if (!dumb) {
          fflush(stdout);
      }
    }
    if (strlen(chans)) {
      while (strlen(keys) && (keys[strlen(keys)-1] == ',')) // remove trailing commas.
	keys[strlen(keys)-1] = 0;
      if (strlen(keys)) 
	snprintf(lineout, LINELEN, "JOIN %s %s\n", chans, keys);
      else
	snprintf(lineout, LINELEN, "JOIN %s\n", chans);
      sendline();
    }
#endif
#if 0	/* DEBUG */
    exit(0);
#endif
    idletimer = time(NULL);
    if (!dumb) {
	ptr = termcap;
	if ((term = getenv("TERM")) == NULL) {
	    fprintf(stderr, "tinyirc: TERM not set\n");
	    exit(1);
	}
	if (tgetent(bp, term) < 1) {
	    fprintf(stderr, "tinyirc: no termcap entry for %s\n", term);
	    exit(1);
	}
	if ((cap_cm = tgetstr(s_cm, &ptr)) == NULL)
		cap_cm = tgetstr(s_CM, &ptr);
	if ((cap_so = tgetstr(s_so, &ptr)) == NULL)
		cap_so = null;
	if ((cap_se = tgetstr(s_se, &ptr)) == NULL)
		cap_se = null;
	cap_cs = tgetstr(s_cs, &ptr);
	cap_ce = tgetstr(s_ce, &ptr);
	cap_dc = tgetstr(s_dc, &ptr);
	dowinsz(0);
	if (!cap_cm || !cap_cs || !cap_ce) {
	    printf("tinyirc: sorry, no termcap cm,cs,ce: dumb mode set\n");
	    dumb = 1;
	}
	if (!dumb) {
	    signal(SIGINT, cleanup);
	    signal(SIGHUP, cleanup);
	    signal(SIGKILL, cleanup);
	    signal(SIGQUIT, cleanup);
	    signal(SIGSTOP, cleanup);
	    signal(SIGSEGV, cleanup);
	    signal(SIGWINCH, dowinch);
	    savetty();
	    raw();
	    tputs_x(tgoto(cap_cs, maxlin - 2, 0));
	    updatestatus();
	}
    }
    for (i = 0; i < HISTLEN; i++)
	history[i] = calloc(512, sizeof(char));
    linein = history[histline = 0];
    while (sok && !beenden) {
	FD_ZERO(&readfs);
	FD_SET(sockfd, &readfs);
	FD_SET(stdinfd, &readfs);
	if (!dumb) {
	    time_out.tv_sec = 61;
	    time_out.tv_usec = 0;
	}
	i = select(FD_SETSIZE, &readfs, NULL, NULL, (dumb ? NULL : &time_out));
	if (i < 0 && errno != EINTR)
	    beenden = 1;
	else if (i > 0) {
	    if (FD_ISSET(stdinfd, &readfs))
		userinput();
	    if (FD_ISSET(sockfd, &readfs))
		sok = serverinput();
	    if (!wasdate)
		updatestatus();
	} else if (sigwinch) {
	    dowinsz(1);
	    sigwinch = 0;
	} else
	    updatestatus();
	if (!dumb) {
	    tputs_x(tgoto(cap_cm, curx % maxcol, maxlin));
	    fflush(stdout);
	}
    }
    if (!dumb) {
#ifdef ZIPIT_Z2
        tputs_x(tgoto(cap_cs, maxlin, 0));
	tputs_x(tgoto(cap_cm, 0, maxlin-1));
	resetty();
	printf("\n");
#else
	tputs_x(tgoto(cap_cs, -1, -1));
	tputs_x(tgoto(cap_cm, 0, maxlin));
	resetty();
#endif
    }
    exit(0);
}

static void
pushlastchan(char *cname)
{
	int i;

	for (i = 0; i < NLASTCHAN; ++i)
		if (lastchans[i] && !strcmp(lastchans[i], cname))
			break;
	if (i < NLASTCHAN)
		cname = lastchans[i];
	else {
		if ((cname = strdup(cname)) == NULL)
			return;
		i = NLASTCHAN - 1;
		if (lastchans[i])
			free(lastchans[i]);
	}
	while (i > 0) {
		lastchans[i] = lastchans[i - 1];
		--i;
	}
	lastchans[0] = cname;
}

#if 0
.endif

PROG=		tinyirc
NOMAN=		Yes

.ifdef SMALL
CPPFLAGS+=	-D_USE_OLD_CURSES_
LDADD+=		-lotermcap
DPADD+=		${LIBOTERMCAP}
.else
LDADD+=		-ltermcap
DPADD+=		${LIBTERMCAP}
.endif

.include <bsd.prog.mk>
#endif
