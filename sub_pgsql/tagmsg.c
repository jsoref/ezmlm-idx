/*$Id$*/
#include "stralloc.h"
#include "scan.h"
#include "fmt.h"
#include "strerr.h"
#include "cookie.h"
#include "slurp.h"
#include "errtxt.h"
#include "subscribe.h"
#include "makehash.h"
#include "idx.h"
#include <unistd.h>
#include <libpq-fe.h>

static stralloc line = {0};
static stralloc key = {0};
static char hash[COOKIE];
static char strnum[FMT_ULONG];	/* message number as sz */

void tagmsg(const char *dir,		/* db base dir */
	    unsigned long msgnum,	/* number of this message */
	    const char *seed,		/* seed. NULL ok, but less entropy */
	    const char *action,	/* to make it certain the cookie differs from*/
				/* one used for a digest */
	    char *hashout,		/* calculated hash goes here */
	    unsigned long bodysize,
	    unsigned long chunk)
/* This routine creates a cookie from num,seed and the */
/* list key and returns that cookie in hashout. The use of sender/num and */
/* first char of action is used to make cookie differ between messages,   */
/* the key is the secret list key. The cookie will be inserted into       */
/* table_cookie where table and other data is taken from dir/sql. We log  */
/* arrival of the message (done=0). */
{
  PGresult *result;
  PGresult *result2; /* Need for dupicate check */
  const char *table = (char *) 0;
  const char *ret;
  unsigned int i;

  strnum[fmt_ulong(strnum,msgnum)] = '\0';	/* message nr ->string*/

    switch(slurp("key",&key,32)) {
      case -1:
	strerr_die3sys(111,FATAL,ERR_READ,"key: ");
      case 0:
	strerr_die3x(100,FATAL,"key",ERR_NOEXIST);
    }
    cookie(hash,key.s,key.len,strnum,seed,action);
    for (i = 0; i < COOKIE; i++)
      hashout[i] = hash[i];

  if ((ret = opensql(dir,&table))) {
    if (*ret) strerr_die2x(111,FATAL,ret);
    return;				/* no sql => success */

  } else {
    if (chunk >= 53L) chunk = 0L;	/* sanity */

	/* INSERT INTO table_cookie (msgnum,cookie) VALUES (num,cookie) */
	/* (we may have tried message before, but failed to complete, so */
	/* ER_DUP_ENTRY is ok) */
    if (!stralloc_copys(&line,"INSERT INTO ")) die_nomem();
    if (!stralloc_cats(&line,table)) die_nomem();
    if (!stralloc_cats(&line,"_cookie (msgnum,cookie,bodysize,chunk) VALUES ("))
      die_nomem();
    if (!stralloc_cats(&line,strnum)) die_nomem();
    if (!stralloc_cats(&line,",'")) die_nomem();
    if (!stralloc_catb(&line,hash,COOKIE)) die_nomem();
    if (!stralloc_cats(&line,"',")) die_nomem();
    if (!stralloc_catb(&line,strnum,fmt_ulong(strnum,bodysize)))
      die_nomem();
    if (!stralloc_cats(&line,",")) die_nomem();
    if (!stralloc_catb(&line,strnum,fmt_ulong(strnum,chunk))) die_nomem();
    if (!stralloc_cats(&line,")")) die_nomem();
    
    if (!stralloc_0(&line)) die_nomem();
    result = PQexec(psql,line.s);
    if (result == NULL)
      strerr_die2x(111,FATAL,PQerrorMessage(psql));
    if (PQresultStatus(result) != PGRES_COMMAND_OK) { /* Possible tuplicate */
      if (!stralloc_copys(&line,"SELECT msgnum FROM ")) die_nomem();
      if (!stralloc_cats(&line,table)) die_nomem();	  
      if (!stralloc_cats(&line,"_cookie WHERE msgnum = ")) die_nomem();
      if (!stralloc_catb(&line,strnum,fmt_ulong(strnum,msgnum))) 
	die_nomem();
      /* Query */
      if (!stralloc_0(&line)) die_nomem();
      result2 = PQexec(psql,line.s);
      if (result2 == NULL)
	strerr_die2x(111,FATAL,PQerrorMessage(psql));
      if (PQresultStatus(result2) != PGRES_TUPLES_OK)
	strerr_die2x(111,FATAL,PQresultErrorMessage(result2));
      /* No duplicate, return ERROR from first query */
      if (PQntuples(result2)<1) 
	strerr_die2x(111,FATAL,PQresultErrorMessage(result));
      PQclear(result2);
    }
    PQclear(result);

    if (! (ret = logmsg(dir,msgnum,0L,0L,1))) return;	/* log done=1*/
    if (*ret) strerr_die2x(111,FATAL,ret);
  }

  return;
}
