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
 * Dump the faillog file which contains information about the last time a    *
 * user failed to login. Like the lastlog file, entries are index by the     *
 * user ID. The file on Linux is usually found at: /var/log/faillog          *
 *****************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

/* The format of the faillog struct. Stolen from the
 * shadow source code. */
struct faillog{
    short fail_cnt;         /* failures since last success */
    short fail_max;         /* failures before turning account off */
    char fail_line[12];     /* last failure occured here */
    time_t fail_time;       /* last failure occured then */
    /*
     * If nonzero, the account will be re-enabled if there are no
     * failures for fail_locktime seconds since last
     * failure.
     */
    long fail_locktime;
};

/** Print a simple help message and return
 *
 */
void usage(void){
    fprintf(stderr, "Usage: fail_print FILE\n");
    fprintf(stderr, "Dump the entries in the specified lastlog log file\n");
    fprintf(stderr, "   -h,?               Display this help message\n");
    fprintf(stderr, "\n");
    return;
}

int main(int argc, char *argv[]){
    int fd;
    int opt;
    char *fname;
    char str[80];
    time_t tmptime;
    size_t index = 0;
    struct tm *tmbuf;
    struct faillog fbuf = {0};

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
        fprintf(stderr, "Try 'fail_print -h' for more information\n");
        return(1);
    }

    if((fd = open(fname, O_RDONLY)) < 0){
        perror("open() error");
        return(1);
    }

    while((read(fd, (void *)&fbuf, sizeof(fbuf)) == sizeof(fbuf))){
        tmptime = fbuf.fail_time;
        tmbuf = localtime(&tmptime);
        strftime(str, 80, "%a %b %d %H:%M:%S %z %Y", tmbuf);

        printf("index: %zu fail_cnt: %d fail_max: %d fail_line: %s "
        " fail_time: %s fail_locktime: %ld\n",
        index, fbuf.fail_cnt, fbuf.fail_max, fbuf.fail_line, str,
        fbuf.fail_locktime);

        index++;
    }

    close(fd);
    return(0);

}
