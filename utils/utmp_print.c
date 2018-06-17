/*****************************************************************************
 * Copyright 2018 <spcnvdrr@protonmail.com>                                  *
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
 * Print some basic information from a system's utmp/wtmp/btmp Shows a       *
 * little more information than other commands.                              *
 *****************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <utmp.h>
#include <time.h>

const char *type[] = {
    "EMPTY", "RUN_LVL", "BOOT_TIME", "NEW_TIME", "OLD_TIME",
    "INIT_PROCESS", "LOGIN_PROCESS", "USER_PROCESS", "DEAD_PROCESS",
    "ACCOUNTING", ""
};


/** Print some basic info. about the given utmp record
 * @param rec the utmp structure to print information from
 *
 */
void print_record(struct utmp rec){
    time_t time = rec.ut_tv.tv_sec;
    struct tm *tmbuf = localtime(&time);
    char strbuf[250];
    strftime(strbuf, 250, "%a %b %d %H:%M:%S %z %Y", tmbuf);
    printf("type: %-13s user: %-10s line: %-8s host: %-10s pid: %-4u  ",
        type[rec.ut_type], rec.ut_user, rec.ut_line, rec.ut_host, rec.ut_pid);
    printf("time: %s\n", strbuf);

}

/** Print a simple help message and return
 *
 */
void usage(void){
    fprintf(stderr, "Usage: utmp_print FILE\n");
    fprintf(stderr, "Dump the entries in the specified utmp/wtmp/btmp log file\n");
    fprintf(stderr, "   -h,?               Display this help message\n");
    fprintf(stderr, "\n");
    return;
}

int main(int argc, char *argv[]){
    int fin;
    int opt;
    char *fname;
    struct utmp tmp;
    size_t num = 0;
    size_t size = sizeof(struct utmp);

    while((opt = getopt(argc, argv, "h?")) != -1){
        switch(opt){
            case 'h':
                /* fall through */
            case '?':
                usage();
                return(0);
                break;
            default:
                usage();
                break;
        }
    }

    /* Get the file name from argv */
    fname = argv[optind];

    /* Check if the user passed the file name */
    if(fname == NULL){
        fprintf(stderr, "%s: missing file operand\n", argv[0]);
        fprintf(stderr, "Try 'utmp_print -h' for more information\n");
        return(1);
    }

    if((fin = open(fname, O_RDONLY)) < 0){
        fprintf(stderr, "%s: failed to open %s: %s\n", argv[0], fname,
            strerror(errno));
        return(1);
    }

    /* While we continue to read records, print the record */
    while(read(fin, &tmp, size) == (ssize_t)size){
        printf("%zu: ", ++num);
        print_record(tmp);
    }

    printf("Found %zu records\n", num);

    close(fin);
    return(0);
}
