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
 * Dump the lastlog log file in a human readable format. Shows as much       *
 * information as possible. The index is equal to the user ID (UID) of the   *
 * user that the log entry pertains to. So information about a user with the *
 * UID of 999 will be contained at index 999.                                *
 *****************************************************************************/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <lastlog.h>
#include <time.h>


/** Print a record taken from the lastlog log file
 * @param ent the entry to print
 *
 */
void print_record(struct lastlog ent){
    char str[80];
    struct tm *tmbuf;

    time_t tmptime = ent.ll_time;
    tmbuf = localtime(&tmptime);
    strftime(str, 80, "%a %b %d %H:%M:%S %z %Y", tmbuf);

    printf("ll_line: %s ll_host: %s time: %s\n",
        ent.ll_line, ent.ll_host, str);
}

/** Print a simple help message and return
 *
 */
void usage(void){
    fprintf(stderr, "Usage: last_print FILE\n");
    fprintf(stderr, "Dump the entries in the specified lastlog log file\n");
    fprintf(stderr, "   -h,?               Display this help message\n");
    fprintf(stderr, "\n");
    return;
}

int main(int argc, char *argv[]){
    int fd;
    int ret;
    int opt;
    char *fname;
    size_t index = 0;
    struct lastlog lbuf = {0};

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
        fprintf(stderr, "Try 'last_print -h' for more information\n");
        return(1);
    }

    if((fd = open(fname, O_RDONLY)) < 0){
        perror("open() error");
        return(1);
    }

    while((ret = read(fd, (void *)&lbuf, sizeof(lbuf)) == sizeof(struct lastlog))){
        printf("index: %zu ", index);
        print_record(lbuf);
        index++;
    }
    if(ret < 0){
        perror("read() error");
        close(fd);
        return(1);
    }

    close(fd);
    return(0);

}
