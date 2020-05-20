#include "config.h"
#include "version.h"

#ifdef SHOW_SERVER_VERSION
#define EXPOSED_SERVER_SOFTWARE SERVER_SOFTWARE
#else /* SHOW_SERVER_VERSION */
#define EXPOSED_SERVER_SOFTWARE "thttpd"
#endif /* SHOW_SERVER_VERSION */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif /* HAVE_MEMORY_H */
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <stdarg.h>

#ifdef HAVE_OSRELDATE_H
#include <osreldate.h>
#endif /* HAVE_OSRELDATE_H */

#ifdef HAVE_DIRENT_H
# include <dirent.h>
# define NAMLEN(dirent) strlen((dirent)->d_name)
#else
# define dirent direct
# define NAMLEN(dirent) (dirent)->d_namlen
# ifdef HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# ifdef HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# ifdef HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

/* The connection states. */
#define CNST_FREE 0
#define CNST_READING 1
#define CNST_SENDING 2
#define CNST_PAUSING 3
#define CNST_LINGERING 4

#include "libhttpd.h"
#include "mmc.h"
#include "timers.h"
#include "match.h"
#include "tdate_parse.h"
#include "fdwatch.h"

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif
#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif
#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef SHUT_WR
#define SHUT_WR 1
#endif

#ifndef HAVE_INT64T
typedef long long int64_t;
#endif

#ifdef __CYGWIN__
#define timezone  _timezone
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/* Conditional macro to allow two alternate forms for use in the built-in
** error pages.  If EXPLICIT_ERROR_PAGES is defined, the second and more
** explicit error form is used; otherwise, the first and more generic
** form is used.
*/
#ifdef EXPLICIT_ERROR_PAGES
#define ERROR_FORM(a,b) b
#else /* EXPLICIT_ERROR_PAGES */
#define ERROR_FORM(a,b) a
#endif /* EXPLICIT_ERROR_PAGES */

#define THROTTLE_NOLIMIT -1

typedef struct {
    int conn_state;
    int next_free_connect;
    httpd_conn* hc;
    int tnums[MAXTHROTTLENUMS];         /* throttle indexes */
    int numtnums;
    long max_limit, min_limit;
    time_t started_at, active_at;
    Timer* wakeup_timer;
    Timer* linger_timer;
    long wouldblock_delay;
    off_t bytes;
    off_t end_byte_index;
    off_t next_byte_index;
    } connecttab;
static connecttab* connects;

typedef struct {
    char* pattern;
    long max_limit, min_limit;
    long rate;
    off_t bytes_since_avg;
    int num_sending;
    } throttletab;
throttletab* throttles;
int numthrottles, maxthrottles;
int num_connects, max_connects, first_free_connect;
int httpd_conn_count;
int terminate = 0;
time_t start_time, stats_time;
long stats_connections;
off_t stats_bytes;
int stats_simultaneous;

static size_t sockaddr_len( httpd_sockaddr* saP );
void handle_read( connecttab* c, struct timeval* tvP );
int check_throttles( connecttab* c );
void finish_connection( connecttab* c, struct timeval* tvP );
void clear_connection( connecttab* c, struct timeval* tvP );
void linger_clear_connection( ClientData client_data, struct timeval* nowP );
void really_clear_connection( connecttab* c, struct timeval* tvP );
void clear_throttles( connecttab* c, struct timeval* tvP );

int main(int argc, char*argv[]) {
	int port = 8989;
	char * cwd = strdup("/home/httpd1/html");
	time_t timer;
    char time_buffer[26];
    struct tm* tm_info;

	httpd_conn* hc;
	hc = calloc(sizeof(httpd_conn), 1);

	hc->hs = calloc(sizeof(httpd_server), 1);
	hc->hs->server_hostname = strdup("kali");
	hc->hs->port = port;
	hc->hs->charset = strdup("UTF-8");
	hc->hs->p3p = strdup("");
	hc->hs->max_age = -1;
	hc->hs->cwd = cwd;
	hc->hs->listen4_fd = -1;
	hc->hs->listen6_fd = -1; // TODO orig value=5
	hc->hs->no_log = 1;
	hc->hs->logfp = NULL; // TODO open fd to a log file
	hc->hs->no_symlink_check = 0;
	hc->hs->vhost = 0;
	hc->hs->global_passwd = 0;
	hc->hs->url_pattern = 0;
	hc->hs->local_pattern = 0;
	hc->hs->no_empty_referrers = 0;

	hc->initialized = 1;

	hc->mime_flag=1;
	hc->read_size = 600;
	hc->checked_idx = 77;
	hc->checked_state = 10;
	hc->method = 0;
	hc->status = 0;
	hc->bytes_to_send = 0;
    hc->maxdecodedurl = 200;
	hc->maxorigfilename = 200;
	hc->maxexpnfilename = 200;
	hc->maxencodings = 200;
	hc->maxremoteuser = 200;
	hc->maxaccept  = 200;
	hc->maxaccepte  = 200;
	hc->maxresponse = 200;

//////////////////////////////////////////////////////////
    httpd_sockaddr sa;
    socklen_t sz;

	hc->read_size = 0;
	hc->maxdecodedurl =
	    hc->maxorigfilename = hc->maxexpnfilename = hc->maxencodings =
	    hc->maxpathinfo = hc->maxquery = hc->maxaccept =
	    hc->maxaccepte = hc->maxreqhost = hc->maxhostdir =
	    hc->maxremoteuser = hc->maxresponse = 0;
#ifdef TILDE_MAP_2
	hc->maxaltdir = 0;
#endif /* TILDE_MAP_2 */
	httpd_realloc_str( &hc->decodedurl, &hc->maxdecodedurl, 1 );
	httpd_realloc_str( &hc->origfilename, &hc->maxorigfilename, 1 );
	httpd_realloc_str( &hc->expnfilename, &hc->maxexpnfilename, 0 );
	httpd_realloc_str( &hc->encodings, &hc->maxencodings, 0 );
	httpd_realloc_str( &hc->pathinfo, &hc->maxpathinfo, 0 );
	httpd_realloc_str( &hc->query, &hc->maxquery, 0 );
	httpd_realloc_str( &hc->accept, &hc->maxaccept, 0 );
	httpd_realloc_str( &hc->accepte, &hc->maxaccepte, 0 );
	httpd_realloc_str( &hc->reqhost, &hc->maxreqhost, 0 );
	httpd_realloc_str( &hc->hostdir, &hc->maxhostdir, 0 );
	httpd_realloc_str( &hc->remoteuser, &hc->maxremoteuser, 0 );
	httpd_realloc_str( &hc->response, &hc->maxresponse, 0 );
#ifdef TILDE_MAP_2
	httpd_realloc_str( &hc->altdir, &hc->maxaltdir, 0 );
#endif /* TILDE_MAP_2 */
	hc->initialized = 1;

    (void) memset( &hc->client_addr, 0, sizeof(hc->client_addr) );
    (void) memmove( &hc->client_addr, &sa, sockaddr_len( &sa ) );
    hc->read_idx = 0;
    hc->checked_idx = 0;
    hc->checked_state = CHST_FIRSTWORD;
    hc->method = METHOD_UNKNOWN;
    hc->status = 0;
    hc->bytes_to_send = 0;
    hc->bytes_sent = 0;
    hc->encodedurl = "";
    hc->decodedurl[0] = '\0';
    hc->protocol = "UNKNOWN";
    hc->origfilename[0] = '\0';
    hc->expnfilename[0] = '\0';
    hc->encodings[0] = '\0';
    hc->pathinfo[0] = '\0';
    hc->query[0] = '\0';
    hc->referrer = "";
    hc->useragent = "";
    hc->accept[0] = '\0';
    hc->accepte[0] = '\0';
    hc->acceptl = "";
    hc->cookie = "";
    hc->contenttype = "";
    hc->reqhost[0] = '\0';
    hc->hdrhost = "";
    hc->hostdir[0] = '\0';
    hc->authorization = "";
    hc->remoteuser[0] = '\0';
    hc->response[0] = '\0';
#ifdef TILDE_MAP_2
    hc->altdir[0] = '\0';
#endif /* TILDE_MAP_2 */
    hc->responselen = 0;
    hc->if_modified_since = (time_t) -1;
    hc->range_if = (time_t) -1;
    hc->contentlength = -1;
    hc->type = "";
    hc->hostname = (char*) 0;
    hc->mime_flag = 1;
    hc->one_one = 0;
    hc->got_range = 0;
    hc->tildemapped = 0;
    hc->first_byte_index = 0;
    hc->last_byte_index = -1;
    hc->keep_alive = 0;
    hc->should_linger = 0;
    hc->file_address = (char*) 0;

	struct timeval tv;
typedef struct {
    int conn_state;
    int next_free_connect;
    httpd_conn* hc;
    int tnums[MAXTHROTTLENUMS];         /* throttle indexes */
    int numtnums;
    long max_limit, min_limit;
    time_t started_at, active_at;
    Timer* wakeup_timer;
    Timer* linger_timer;
    long wouldblock_delay;
    off_t bytes;
    off_t end_byte_index;
    off_t next_byte_index;
    } connecttab;
	connecttab *c;
//////////////////////////////////////////////////////////


	FILE* fd;
	long lSize;
	char *buffer;

    FILE* logfp;
	char *logfile;

#ifdef __AFL_HAVE_MANUAL_CONTROL
  __AFL_INIT();
#endif

    openlog( argv[1], LOG_NDELAY|LOG_PID, LOG_FACILITY );
	logfile = argv[1];
	logfp = fopen( logfile, "a" );
	if ( logfp == (FILE*) 0 )
	{
	syslog( LOG_CRIT, "%.80s - %m", logfile );
	perror( logfile );
	exit( 1 );
	}
	if ( logfile[0] != '/' )
	{
	syslog( LOG_WARNING, "logfile is not an absolute path, you may not be able to re-open it" );
	(void) fprintf( stderr, "%s: logfile is not an absolute path, you may not be able to re-open it\n", argv[1] );
	}
	(void) fcntl( fileno( logfp ), F_SETFD, 1 );
	
	/*
	fd = fopen(argv[2], "rb");
	if (NULL == fd) {
		printf("Failed to read in test file. strerror: %s\n", strerror(errno));
		return(-1);
	}
	fseek( fd, 0L, SEEK_END);
	lSize = ftell( fd );
	rewind( fd );
	//hc->read_buf = calloc(lSize+3, 1);
	//read_buf = calloc(lSize, 1);
	if ( 1 != fread( hc->read_buf, lSize, 1, fd)) {
		fputs("entire read failed", stderr);
		return -1;
	}
	hc->read_idx = lSize;
	*/
	//hc->conn_fd = fd;
	httpd_realloc_str( &hc->read_buf, &hc->read_size, 600 );
	//hc->conn_fd = 0;
	hc->conn_fd = open(argv[2], 0);

	c = calloc(sizeof(connecttab), 1);
	c->hc = hc;
	c->conn_state = 1;
	c->next_free_connect = -1;
	c->numtnums = 0;
	c->max_limit = 0;
	c->min_limit = 0;
	c->started_at = 0;
	// TODO
	c->active_at = 1588981353;
	c->wakeup_timer = 0;
	c->linger_timer = 0;
	c->wouldblock_delay = 0;
	c->bytes = 0;
	c->end_byte_index = 0;
	c->next_byte_index = 0;

	gettimeofday( &tv, (struct timezone*) 0 );
	handle_read(c, &tv);
	/*
	size_t size = 15;
	size_t nmemb = 15;
	char * buf1 = calloc(32, 1);
	fread(buf1, size, nmemb, stdin);
	fprintf(stdout, "%s", buf1);
	*/

	//int rtrn = httpd_parse_request(hc);


    timer = time(NULL);
    tm_info = localtime(&timer);

    strftime(time_buffer, 26, "%H:%M:%S %d-%m-%Y", tm_info);

	/*
	printf("%d\n", hc->method);
	printf("%s\n", hc->decodedurl);
	printf("%s\n", hc->protocol);
	printf("%s\n", hc->useragent);
	printf("%s\n", time_buffer);
	*/
	fprintf(logfp, "\"%s\" \"%s\" \"%s\" \"%s\" %s\n",
		   	httpd_method_str(hc->method), hc->decodedurl, hc->protocol,
		   	hc->useragent, time_buffer);

	free(hc->hs->server_hostname);
	free(hc->hs->charset);
	free(hc->hs->p3p);
	free(hc->hs);

	//free(hc->read_buf );
	//free(hc->encodedurl);
	free(hc->decodedurl);
	free(hc->origfilename);
	free(hc->expnfilename);
	free(hc->encodings);
	free(hc->pathinfo);
	free(hc->query);
	//free(hc->referrer);
	free(hc->accept);
	free(hc->accepte);
	//free(hc->acceptl);
	//free(hc->hdrhost);
	free(hc->reqhost);
	free(hc->hostdir);
	free(hc->remoteuser);
	free(hc->response);

	free(hc->read_buf);
	free(hc);

	free(c);

	free(cwd);

return 0;
}

static size_t
sockaddr_len( httpd_sockaddr* saP )
    {
    switch ( saP->sa.sa_family )
	{
	case AF_INET: return sizeof(struct sockaddr_in);
#ifdef USE_IPV6
	case AF_INET6: return sizeof(struct sockaddr_in6);
#endif /* USE_IPV6 */
	default:
	return 0;	/* shouldn't happen */
	}
    }

void
handle_read( connecttab* c, struct timeval* tvP )
    {
    int sz;
    ClientData client_data;
    httpd_conn* hc = c->hc;

    /* Is there room in our buffer to read more bytes? */
    if ( hc->read_idx >= hc->read_size )
	{
	if ( hc->read_size > 5000 )
	    {
	    httpd_send_err( hc, 400, httpd_err400title, "", httpd_err400form, "" );
	    finish_connection( c, tvP );
	    return;
	    }
	httpd_realloc_str(
	    &hc->read_buf, &hc->read_size, hc->read_size + 1000 );
	}

    /* Read some more bytes. */
    sz = read(
	hc->conn_fd, &(hc->read_buf[hc->read_idx]),
	hc->read_size - hc->read_idx );
    if ( sz == 0 )
	{
	httpd_send_err( hc, 400, httpd_err400title, "", httpd_err400form, "" );
	finish_connection( c, tvP );
	return;
	}
    if ( sz < 0 )
	{
	/* Ignore EINTR and EAGAIN.  Also ignore EWOULDBLOCK.  At first glance
	** you would think that connections returned by fdwatch as readable
	** should never give an EWOULDBLOCK; however, this apparently can
	** happen if a packet gets garbled.
	*/
	if ( errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK )
	    return;
	httpd_send_err(
	    hc, 400, httpd_err400title, "", httpd_err400form, "" );
	finish_connection( c, tvP );
	return;
	}
    hc->read_idx += sz;
    c->active_at = tvP->tv_sec;

    /* Do we have a complete request yet? */
    switch ( httpd_got_request( hc ) )
	{
	case GR_NO_REQUEST:
	return;
	case GR_BAD_REQUEST:
	httpd_send_err( hc, 400, httpd_err400title, "", httpd_err400form, "" );
	finish_connection( c, tvP );
	return;
	}

    /* Yes.  Try parsing and resolving it. */
    if ( httpd_parse_request( hc ) < 0 )
	{
	finish_connection( c, tvP );
	return;
	}

    /* Check the throttle table */
    if ( ! check_throttles( c ) )
	{
	httpd_send_err(
	    hc, 503, httpd_err503title, "", httpd_err503form, hc->encodedurl );
	finish_connection( c, tvP );
	return;
	}

    /* Start the connection going. */
    if ( httpd_start_request( hc, tvP ) < 0 )
	{
	/* Something went wrong.  Close down the connection. */
	finish_connection( c, tvP );
	return;
	}

    /* Fill in end_byte_index. */
    if ( hc->got_range )
	{
	c->next_byte_index = hc->first_byte_index;
	c->end_byte_index = hc->last_byte_index + 1;
	}
    else if ( hc->bytes_to_send < 0 )
	c->end_byte_index = 0;
    else
	c->end_byte_index = hc->bytes_to_send;

    /* Check if it's already handled. */
    if ( hc->file_address == (char*) 0 )
	{
	/* No file address means someone else is handling it. */
	int tind;
	for ( tind = 0; tind < c->numtnums; ++tind )
	    throttles[c->tnums[tind]].bytes_since_avg += hc->bytes_sent;
	c->next_byte_index = hc->bytes_sent;
	finish_connection( c, tvP );
	return;
	}
    if ( c->next_byte_index >= c->end_byte_index )
	{
	/* There's nothing to send. */
	finish_connection( c, tvP );
	return;
	}

    /* Cool, we have a valid connection and a file to send to it. */
    c->conn_state = CNST_SENDING;
    c->started_at = tvP->tv_sec;
    c->wouldblock_delay = 0;
    client_data.p = c;

    fdwatch_del_fd( hc->conn_fd );
    fdwatch_add_fd( hc->conn_fd, c, FDW_WRITE );
    }

int
check_throttles( connecttab* c )
    {
    int tnum;
    long l;

    c->numtnums = 0;
    c->max_limit = c->min_limit = THROTTLE_NOLIMIT;
    for ( tnum = 0; tnum < numthrottles && c->numtnums < MAXTHROTTLENUMS;
	  ++tnum )
	if ( match( throttles[tnum].pattern, c->hc->expnfilename ) )
	    {
	    /* If we're way over the limit, don't even start. */
	    if ( throttles[tnum].rate > throttles[tnum].max_limit * 2 )
		return 0;
	    /* Also don't start if we're under the minimum. */
	    if ( throttles[tnum].rate < throttles[tnum].min_limit )
		return 0;
	    if ( throttles[tnum].num_sending < 0 )
		{
		syslog( LOG_ERR, "throttle sending count was negative - shouldn't happen!" );
		throttles[tnum].num_sending = 0;
		}
	    c->tnums[c->numtnums++] = tnum;
	    ++throttles[tnum].num_sending;
	    l = throttles[tnum].max_limit / throttles[tnum].num_sending;
	    if ( c->max_limit == THROTTLE_NOLIMIT )
		c->max_limit = l;
	    else
		c->max_limit = MIN( c->max_limit, l );
	    l = throttles[tnum].min_limit;
	    if ( c->min_limit == THROTTLE_NOLIMIT )
		c->min_limit = l;
	    else
		c->min_limit = MAX( c->min_limit, l );
	    }
    return 1;
    }

void
finish_connection( connecttab* c, struct timeval* tvP )
    {
    /* If we haven't actually sent the buffered response yet, do so now. */
    httpd_write_response( c->hc );

    /* And clear. */
    clear_connection( c, tvP );
    }

void
clear_connection( connecttab* c, struct timeval* tvP )
    {
    ClientData client_data;

    if ( c->wakeup_timer != (Timer*) 0 )
	{
	tmr_cancel( c->wakeup_timer );
	c->wakeup_timer = 0;
	}

    /* This is our version of Apache's lingering_close() routine, which is
    ** their version of the often-broken SO_LINGER socket option.  For why
    ** this is necessary, see http://www.apache.org/docs/misc/fin_wait_2.html
    ** What we do is delay the actual closing for a few seconds, while reading
    ** any bytes that come over the connection.  However, we don't want to do
    ** this unless it's necessary, because it ties up a connection slot and
    ** file descriptor which means our maximum connection-handling rate
    ** is lower.  So, elsewhere we set a flag when we detect the few
    ** circumstances that make a lingering close necessary.  If the flag
    ** isn't set we do the real close now.
    */
    if ( c->conn_state == CNST_LINGERING )
	{
	/* If we were already lingering, shut down for real. */
	tmr_cancel( c->linger_timer );
	c->linger_timer = (Timer*) 0;
	c->hc->should_linger = 0;
	}
    if ( c->hc->should_linger )
	{
	if ( c->conn_state != CNST_PAUSING )
	    fdwatch_del_fd( c->hc->conn_fd );
	c->conn_state = CNST_LINGERING;
	shutdown( c->hc->conn_fd, SHUT_WR );
	fdwatch_add_fd( c->hc->conn_fd, c, FDW_READ );
	client_data.p = c;
	if ( c->linger_timer != (Timer*) 0 )
	    syslog( LOG_ERR, "replacing non-null linger_timer!" );
	c->linger_timer = tmr_create(
	    tvP, linger_clear_connection, client_data, LINGER_TIME, 0 );
	if ( c->linger_timer == (Timer*) 0 )
	    {
	    syslog( LOG_CRIT, "tmr_create(linger_clear_connection) failed" );
	    exit( 1 );
	    }
	}
    else
	really_clear_connection( c, tvP );
    }

void
linger_clear_connection( ClientData client_data, struct timeval* nowP )
    {
    connecttab* c;

    c = (connecttab*) client_data.p;
    c->linger_timer = (Timer*) 0;
    really_clear_connection( c, nowP );
    }

void
really_clear_connection( connecttab* c, struct timeval* tvP )
    {
    stats_bytes += c->hc->bytes_sent;
    if ( c->conn_state != CNST_PAUSING )
	fdwatch_del_fd( c->hc->conn_fd );
    httpd_close_conn( c->hc, tvP );
    clear_throttles( c, tvP );
    if ( c->linger_timer != (Timer*) 0 )
	{
	tmr_cancel( c->linger_timer );
	c->linger_timer = 0;
	}
    c->conn_state = CNST_FREE;
    c->next_free_connect = first_free_connect;
    first_free_connect = c - connects;	/* division by sizeof is implied */
    --num_connects;
    }

void
clear_throttles( connecttab* c, struct timeval* tvP )
    {
    int tind;

    for ( tind = 0; tind < c->numtnums; ++tind )
	--throttles[c->tnums[tind]].num_sending;
    }

