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

#include "libhttpd.h"
#include "mmc.h"
#include "timers.h"
#include "match.h"
#include "tdate_parse.h"

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

static size_t sockaddr_len( httpd_sockaddr* saP );

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
	//httpd_realloc_str( &hc->read_buf, &hc->read_size, 500 );
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
	
	fd = fopen(argv[2], "rb");
	if (NULL == fd) {
		printf("Failed to read in test file. strerror: %s\n", strerror(errno));
		return(-1);
	}
	fseek( fd, 0L, SEEK_END);
	lSize = ftell( fd );
	rewind( fd );
	//hc->read_buf = calloc(lSize+3, 1);
	hc->read_buf = calloc(lSize, 1);
	if ( 1 != fread( hc->read_buf, lSize, 1, fd)) {
		fputs("entire read failed", stderr);
		return -1;
	}
	//lSize += 3L;
	//hc->read_buf = strcat(hc->read_buf, "\x0d\x0a");
	hc->read_idx = lSize;

	int rtrn = httpd_parse_request(hc);

    timer = time(NULL);
    tm_info = localtime(&timer);

    strftime(time_buffer, 26, "%H:%M:%S %d-%m-%Y", tm_info);

	fprintf(logfp, "%d \"%s\" \"%s\" \"%s\" \"%s\" %d %s\n", hc->method, hc->read_buf, hc->decodedurl, 
					hc->protocol, hc->useragent, rtrn, time_buffer);

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

	free(cwd);

return rtrn;
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
