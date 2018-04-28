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

int main(int argc, char *argv[]){
    int fin;
    struct utmp tmp;
    size_t size = sizeof(struct utmp);
    size_t num = 0;

    if(argc != 2){
        fprintf(stderr, "Usage: %s <u/w/btmp logfile>\n", argv[0]);
        fprintf(stderr, "Dump the entries in the u/w/btmp log files\n\n");
        return(1);
    }

    if((fin = open(argv[1], O_RDONLY)) < 0){
        perror("open error");
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
