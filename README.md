# Redact - A Linux log wiping program

Redact is a simple Linux program to remove some or all of a user's activity
from common system log files. This program is different than most log
wipers currently available in that it avoids common pitfalls when
destroying log entries. Some wipe programs simply overwrite the matching
log entries with zeros. However, there are programs that check the integrity
of the system's log files by searching for these NUL entries and alert
the administrator when found. NUL log entries are avoided by creating a new
log file and entirely omitting specific entries which leaves no trace that
the log files have been tampered with. The same permissions and user/group
owners are applied to the new log file to avoid incorrect permissions or
owners. Finally, the old log file is replaced with the newly sanitized one.
The old log file is securely deleted before being replaced with the new
log file. There is also an option to only wipe entries created in the last
N days, e.g. the last 30 days. This allows only a portion of a user's activity
to be wiped from the log file. Also, unlike other log wiping programs, there
is an option to back up the unmodified log files before wiping log entries.
Below is a list of the common Linux log files that this program currently
supports.

**This program is still undergoing development!**

This program is currently released as a beta version and as such is
currently undergoing active development. New features are still being added
and functionality is constantly being improved.

**Common log file location - Description**

* /var/log/wtmp - Records all logins and logouts

* /var/log/utmp - Records who is currently using the system

* /var/log/btmp - Contains information about failed login attempts

* /var/log/lastlog - A database of the most recent login for each user

* /var/log/faillog - A database of the last failed login attempt for each user

* /var/log/account/pacct - Contains a record of programs executed by each user

* /var/log/auth.log - Logs usage of the system's authorization systems, e.g. sshd and PAM

**What Redact does not do**

Note that while this program modifies the current log files to remove some or
all traces of a given username/host it does NOT modify old caches of those log
files. For example, this program does not and cannot wipe gzipped backups
of old log files created by logrotate. Also, there maybe multiple log files
because old copies are renamed in the format LOGNAME.1 and these files
are not wiped by default either. However, these files can be wiped by setting
the appropriate constants in the program (WTMPFILE, UTMPFILE, etc.) and
recompiling it. Finally, this program currently does not support wiping log
reports, such as wtmp.report that are sometimes generated.


**Getting Started**

Get a copy of the source code and change into the redact directory.

    cd redact

Then set the location of the relevant log files by editing the defines
at the top of the redact.c file (WTMPFILE, UTMPFILE, etc.) which is
located in the src directory. If a defined log file is missing, either
point it at a blank regular text file you created somewhere or leave it
as the default and ignore the error messages about failure to open the
file. The default definitions should work for most Linux distributions,
but some may need to be changed. Use your favorite text editor to do so.

    vim ./src/redact.c

From the redact directory, run make to build the program

    make

Then run the program with the -h or -? options for help.

    ./redact -h

An example of using redact to wipe all log entries in the wtmp log file
pertaining to the user named john

    ./redact -w john

Wipe all log entries in the wtmp log file pertaining to the user john that
were created within the last 30 days. Also with verbose mode enabled.

    ./redact -v -w -d 30 john

When finished, run the following command to delete the compiled program
and any intermediary object files

    make remove


**Installing**

There is no install target in the Makefile to install the binary on a system.
This is intentional.


**Utilities**

The utils folder contains several programs that are useful for examining
log files. To compile them just change in the utils directory and run
`make` These programs show more/different information usually than their
standard counterparts, e.g. who, w, etc.


**To Do**

- [x] Add a backup option to back up the original log files
- [x] Add option to only wipe entries created in the last N days
- [ ] ???


**Contributing**

Pull requests, new feature suggestions, and bug reports/issues are
welcome.


**Versioning**

This project uses semantic versioning 2.0. Version numbers follow the
MAJOR.MINOR.PATCH format.


**License**

This project is licensed under the 3-Clause BSD License also known as the
*"New BSD License"* or the *"Modified BSD License"*. A copy of the license
can be found in the LICENSE file. A copy can also be found at the
[Open Source Institute](https://opensource.org/licenses/BSD-3-Clause)
