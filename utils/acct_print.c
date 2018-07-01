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
 * Dumps some basic information from the process accounting log files.       *
 * Usually found on Linux in /var/log/account/pacct                          *
 *****************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/acct.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#define acct acctv2
#elif defined(__linux__)
#define acct acct_v3
#endif

/** Print a simple help message and return
 *
 */
void usage(void){
    fprintf(stderr, "Usage: acct_print FILE\n");
    fprintf(stderr, "Dump the entries in the specified process accounting (pacct) log file\n");
    fprintf(stderr, "   -h,?               Display this help message\n");
    fprintf(stderr, "\n");
    return;
}

int main(int argc, char *argv[]){
    int opt;
    char *fname;
    struct acct acbuf;
    size_t acsize = sizeof(struct acct);
    size_t recnum = 0;
    FILE *fin;
    time_t tmptime;
    struct tm *tmbuf;
    char timestr[80];

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
        fprintf(stderr, "%s: missing FILE operand\n", argv[0]);
        fprintf(stderr, "Try 'acct_print -h' for more information\n");
        return(1);
    }

    if((fin = fopen(fname, "r")) == NULL){
        perror("fopen() error");
        return(1);
    }

    while(fread(&acbuf, acsize, 1, fin) == 1){
        recnum++;
        tmptime = acbuf.ac_btime;
        tmbuf = localtime(&tmptime);
        strftime(timestr, 80, "%a %b %d %H:%M:%S %z %Y", tmbuf);
        printf("uid: %u tty: %u command: %s time: %s\n", acbuf.ac_uid, acbuf.ac_tty,
            acbuf.ac_comm, timestr);
    }
    if(ferror(fin) && !feof(fin)){
        perror("fread() error");
        fclose(fin);
        return(1);
    }

    printf("Found %zu records\n", recnum);

    fclose(fin);
    return(0);
}
