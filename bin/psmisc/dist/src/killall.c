/*
 * killall.c - kill processes by name or list PIDs
 *
 * Copyright (C) 1993-2002 Werner Almesberger
 * Copyright (C) 2002-2021 Craig Small <csmall@dropbear.xyz>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <pwd.h>
#include <regex.h>
#include <ctype.h>
#include <assert.h>

#ifdef WITH_SELINUX
#include <dlfcn.h>
#include <selinux/selinux.h>
#endif /*WITH_SELINUX*/

#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif /* HAVE_LOCALE_H */

#include "i18n.h"
#include "comm.h"
#include "signals.h"

#define PROC_BASE "/proc"
#define MAX_NAMES (int)(sizeof(unsigned long)*8)

#define TSECOND "s"
#define TMINUTE "m"
#define THOUR   "h"
#define TDAY    "d" 
#define TWEEK   "w"
#define TMONTH  "M" 
#define TYEAR   "y"

#define TMAX_SECOND 31536000
#define TMAX_MINUTE 525600  
#define TMAX_HOUR   8760    
#define TMAX_DAY    365     
#define TMAX_WEEK   48      
#define TMAX_MONTH  12      
#define TMAX_YEAR   1       

#define ER_REGFAIL -1
#define ER_NOMEM   -2
#define ER_UNKWN   -3
#define ER_OOFRA   -4

static pid_t opt_ns_pid = 0;

static int verbose = 0, exact = 0, interactive = 0, reg = 0,
           quiet = 0, wait_until_dead = 0, process_group = 0,
           ignore_case = 0;
static long younger_than = 0, older_than = 0;

typedef struct NAMEINFO {
    const char *name;
    int name_length;
    struct stat st;
} NAMEINFO;

static int
ask (char *name, pid_t pid, const int signal)
{
    int res;
    size_t len;
    char *line;

    line = NULL;
    len = 0;

    do {
        if (signal == SIGTERM)
            printf (_("Kill %s(%s%d) ? (y/N) "), name, process_group ? "pgid " : "",
                    pid);
        else
            printf (_("Signal %s(%s%d) ? (y/N) "), name, process_group ? "pgid " : "",
                    pid);

        fflush (stdout);

        if (getline (&line, &len, stdin) < 0)
            return 0;
        /* Check for default */
        if (line[0] == '\n') {
            free(line);
            return 0;
        }
        res = rpmatch(line);
        if (res >= 0) {
            free(line);
            return res;
        }
    } while(1);
    /* Never should get here */
}

static double
uptime()
{
    char * savelocale;
    char buf[2048];
    FILE* file;
    if (!(file=fopen( PROC_BASE "/uptime", "r"))) {
        fprintf(stderr, "killall: error opening uptime file\n");    
        exit(1);
    }
    savelocale = setlocale(LC_NUMERIC,"C");
    if (fscanf(file, "%2047s", buf) == EOF) perror("uptime");
    fclose(file);
    setlocale(LC_NUMERIC,savelocale);
    return atof(buf);
}

/* process age from jiffies to seconds via uptime */
static double process_age(const unsigned long long jf)
{
    double age;
    double sc_clk_tck = sysconf(_SC_CLK_TCK);
    assert(sc_clk_tck > 0);
    age = uptime() - jf / sc_clk_tck;
    if (age < 0L)
        return 0L;
    return age;
}

/* returns requested time interval in seconds, 
   negative indicates error has occurred
   */
static long
parse_time_units(const char* age)
{
    char *unit;
    long num;

    num = strtol(age,&unit,10);
    if (age == unit) /* no digits found */
        return -1;
    if (unit[0] == '\0') /* no units found */
        return -1;

    switch(unit[0]) {
    case 's':
        return num;
    case 'm':
        return (num * 60);
    case 'h':
        return (num * 60 * 60);
    case 'd':
        return (num * 60 * 60 * 24);
    case 'w':
        return (num * 60 * 60 * 24 * 7);
    case 'M':
        return (num * 60 * 60 * 24 * 7 * 4);
    case 'y':
        return (num * 60 * 60 * 24 * 7 * 4 * 12);
    }
    return -1;
}

enum ns_type {
    IPCNS = 0,
    MNTNS,
    NETNS,
    PIDNS,
    USERNS,
    UTSNS
};

static const char *ns_names[] = {
    [IPCNS] = "ipc",
    [MNTNS] = "mnt",
    [NETNS] = "net",
    [PIDNS] = "pid",
    [USERNS] = "user",
    [UTSNS] = "uts",
};

#define NUM_NS 6

const char *get_ns_name(int id) {
    if (id >= NUM_NS)
        return NULL;
    return ns_names[id];
}

static int get_ns(pid_t pid, int id) {
    struct stat st;
    char buff[50];
    snprintf(buff, sizeof(buff), "/proc/%i/ns/%s", pid, get_ns_name(id));
    if (stat(buff, &st))
        return 0;
    else
        return st.st_ino;
}

static int
match_process_uid(pid_t pid, uid_t uid)
{
    char buf[128];
    uid_t puid;
    FILE *f;
    int re = -1;

    snprintf (buf, sizeof buf, PROC_BASE "/%d/status", pid);
    if (!(f = fopen (buf, "r")))
        return 0;

    while (fgets(buf, sizeof buf, f))
    {
        if (sscanf (buf, "Uid:\t%d", &puid))
        {
            re = uid==puid;
            break;
        }
    }
    fclose(f);
    if (re==-1)
    {
        fprintf(stderr, _("killall: Cannot get UID from process status\n"));
        exit(1);
    }
    return re;
}

/* Match on the given scontext to the process context
 * Return 0 on a match
 */
static int
match_process_context(const pid_t pid, const regex_t *scontext)
{
    static void (*my_freecon)(char*) = NULL;
    static int (*my_getpidcon)(pid_t pid, char **context) = NULL;
    static int selinux_enabled = 0;
    char *lcontext;
    int retval = 1;

#ifdef WITH_SELINUX
    static int tried_load = 0;
    static int (*my_is_selinux_enabled)(void) = NULL;

    if(!my_getpidcon && !tried_load){
        void *handle = dlopen("libselinux.so.1", RTLD_NOW);
        if(handle) {
            my_freecon = dlsym(handle, "freecon");
        if(dlerror())
            my_freecon = NULL;
        my_getpidcon = dlsym(handle, "getpidcon");
        if(dlerror())
            my_getpidcon = NULL;
        my_is_selinux_enabled = dlsym(handle, "is_selinux_enabled");
        if(dlerror())
            my_is_selinux_enabled = 0;
        else
            selinux_enabled = my_is_selinux_enabled();
        }
    tried_load++;
    }
#endif /* WITH_SELINUX */

    if (my_getpidcon && selinux_enabled && !my_getpidcon(pid, &lcontext)) {
        retval = (regexec(scontext, lcontext, 0, NULL, 0) ==0);
	my_freecon(lcontext);
    } else {
        FILE *file;
        char path[50];
        char readbuf[BUFSIZ+1];
        snprintf(path, sizeof path, "/proc/%d/attr/current", pid);
        if ( (file = fopen(path, "r")) != NULL) {
            if (fgets(readbuf, BUFSIZ, file) != NULL) {
                retval = (regexec(scontext, readbuf, 0, NULL, 0)==0);
             }
	    fclose(file);
        }
    }
    return retval;
}


static void
free_regexp_list(regex_t *reglist, int names)
{
    int i;
    for (i = 0; i < names; i++)
        regfree(&reglist[i]);
    free(reglist);
}

static regex_t *
build_regexp_list(int names, char **namelist)
{
    int i;
    regex_t *reglist;
    int flag = REG_EXTENDED|REG_NOSUB;

    if (!(reglist = malloc (sizeof (regex_t) * names)))
    {
        perror ("malloc");
        exit (1);
    }

    if (ignore_case)
        flag |= REG_ICASE;

    for (i = 0; i < names; i++)
    {
        if (regcomp(&reglist[i], namelist[i], flag) != 0) 
        {
            fprintf(stderr, _("killall: Bad regular expression: %s\n"), namelist[i]);
            free_regexp_list(reglist, i);
            exit (1);
        }
    }
    return reglist;
}

static NAMEINFO *
build_nameinfo(const int names, char **namelist)
{
    int i;
    NAMEINFO *ni = NULL;
    if ( (ni = malloc(sizeof(NAMEINFO) * names)) == NULL)
        return NULL;

    for (i = 0; i < names; i++) 
    {
        ni[i].name = namelist[i];
        ni[i].st.st_dev = 0;
        if (!strchr (namelist[i], '/'))
        {
            ni[i].name_length = strlen (namelist[i]);
        }
        else if (stat (namelist[i], &(ni[i].st)) < 0)
        {
            perror (namelist[i]);
            free(ni);
            return NULL;
        }
    }
    return ni;
}

static int
load_process_name_and_age(char *comm, double *process_age_sec,
                          const pid_t pid, int load_age)
{
    FILE *file;
    char *path;
    char buf[1024];
    char *startcomm, *endcomm;
    unsigned lencomm;
    *process_age_sec = 0;

    if (asprintf (&path, PROC_BASE "/%d/stat", pid) < 0)
        return -1;
    if (!(file = fopen (path, "r")))
    {
        free(path);
        return -1;
    }
    free (path);
    if (fgets(buf, 1024, file) == NULL)
    {
        fclose(file);
        return -1;
    }
    fclose(file);
    if ( NULL == ( startcomm = strchr(buf, '(')))
       return -1;
    startcomm++;
    if ( NULL == ( endcomm = strrchr(startcomm, ')')))
	return -1;
    lencomm = endcomm - startcomm;
    if (lencomm > COMM_LEN -1)
        lencomm = COMM_LEN -1;
    strncpy(comm, startcomm, lencomm);
    comm[lencomm] = '\0';

    endcomm += 2; // skip ") "
    if (load_age)
    {
        unsigned long long proc_stt_jf = 0;
        if (sscanf(endcomm, "%*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %Lu",
                   &proc_stt_jf) != 1)
        {
            return -1;
        }
        *process_age_sec = process_age(proc_stt_jf);
    }
    return lencomm;
}

static int
load_proc_cmdline(const pid_t pid, const char *comm, char **command, int *got_long)
{
    FILE *file;
    char *path, *p, *command_buf;
    int cmd_size = 128;
    int okay;

    if (asprintf (&path, PROC_BASE "/%d/cmdline", pid) < 0)
        return -1;
    if (!(file = fopen (path, "r")))
    {
        free (path);
        return -1;
    }
    free(path);

    if ( (command_buf = (char *)malloc (cmd_size)) == NULL)
        exit(1);

    while (1)
    {
        /* look for actual command so we skip over initial "sh" if any */

        /* 'cmdline' has arguments separated by nulls */
        for (p=command_buf; ; p++)
        {
            int c;
            if (p == (command_buf + cmd_size))
            {
                char *new_command_buf;
                int cur_size = cmd_size;
                cmd_size *= 2;
                new_command_buf = (char *)realloc(command_buf, cmd_size);
                if (!new_command_buf) {
                    if (command_buf)
                        free(command_buf);
                    exit (1);
                }
                command_buf = new_command_buf;
                p = command_buf + cur_size;
            }
            c = fgetc(file);
            if (c == EOF || c == '\0')
            {
                *p = '\0';
                break;
            } else {
                *p = c;
            }
        }
        if (strlen(command_buf) == 0) {
            okay = 0;
            break;
        }
        p = strrchr(command_buf,'/');
        p = p ? p+1 : command_buf;
        if (strncmp(p, comm, COMM_LEN-1) == 0) {
            okay = 1;
            if (!(*command = strdup(p))) {
                free(command_buf);
                exit(1);
            }
            break;
        }
    }
    (void) fclose(file);
    free(command_buf);
    command_buf = NULL;

    if (exact && !okay)
    {
        if (verbose)
            fprintf (stderr, _("killall: skipping partial match %s(%d)\n"),
                     comm, pid);
        *got_long = okay;
        return -1;
    }
    *got_long = okay;
    return 0;
}

static pid_t *
create_pid_table(int *max_pids, int *pids)
{
    pid_t self, *pid_table;
    int pid;
    DIR *dir;
    struct dirent *de;

    self = getpid ();
    if (!(dir = opendir (PROC_BASE)))
    {
        perror (PROC_BASE);
        exit (1);
    }
    *max_pids = 256;
    pid_table = malloc (*max_pids * sizeof (pid_t));
    if (!pid_table)
    {
        perror ("malloc");
        exit (1);
    }
    *pids = 0;
    while ( (de = readdir (dir)) != NULL)
    {
        if (!(pid = (pid_t) atoi (de->d_name)) || pid == self)
            continue;
        if (*pids == *max_pids)
        {
            if (!(pid_table = realloc (pid_table, 2 * *pids * sizeof (pid_t))))
            {
                perror ("realloc");
                exit (1);
            }
            *max_pids *= 2;
        }
        pid_table[(*pids)++] = pid;
    }
    (void) closedir (dir);
    return pid_table;
}

#define strcmp2(A,B,I) (I? strcasecmp((A),(B)):strcmp((A),(B)))
#define strncmp2(A,B,L,I) (I? strncasecmp((A),(B),(L)):strncmp((A),(B),(L)))
static int match_process_name(
    const char *proc_comm,
    const int comm_len,
    const char *proc_cmdline,
    const char *match_name,
    const int match_len,
    const int got_long
                         )
{
    /* process is old length but matching longer */
    if (comm_len == OLD_COMM_LEN - 1 && match_len >= OLD_COMM_LEN - 1)
    {
        if (got_long)
        {
            return (0 == strncmp2 (match_name, proc_cmdline, OLD_COMM_LEN - 1,
                                   ignore_case));
        } else {
            return (0 == strncmp2 (match_name, proc_comm, OLD_COMM_LEN - 1,
                                   ignore_case));
        }
    }

    if (comm_len == COMM_LEN - 1 && match_len >= COMM_LEN - 1)
    {
        if (got_long)
        {
            return (0 == strncmp2 (match_name, proc_cmdline, COMM_LEN - 1,
                                   ignore_case));
        } else {
            return (0 == strncmp2 (match_name, proc_comm, COMM_LEN - 1,
                                   ignore_case));
        }
    }
    /* Not old new COMM_LEN so we match all of it */
    if (got_long)
    {
        return (0 == strcmp2 (match_name, proc_cmdline, ignore_case));
    }
    return (0 == strcmp2 (match_name, proc_comm, ignore_case));
}

static int
kill_all(int signal, int name_count, char **namelist, struct passwd *pwent, 
         regex_t *scontext )
{
    struct stat st;
    NAMEINFO *name_info = NULL;
    char *path, comm[COMM_LEN];
    char *command = NULL;
    pid_t *pid_table, *pid_killed;
    pid_t *pgids = NULL;
    int i, j, length, got_long, error;
    int pids, max_pids, pids_killed;
    unsigned long found;
    regex_t *reglist = NULL;
    long ns_ino = 0;

    if (opt_ns_pid)
        ns_ino = get_ns(opt_ns_pid, PIDNS);

    if (name_count && reg)
        reglist = build_regexp_list(name_count, namelist);
    else
        if ( (name_info = build_nameinfo(name_count, namelist)) == NULL)
            exit(1);

    pid_table = create_pid_table(&max_pids, &pids);
    found = 0;
    pids_killed = 0;
    pid_killed = malloc (max_pids * sizeof (pid_t));
    if (!pid_killed)
    {
        perror ("malloc");
        exit (1);
    }
    if (process_group)
    {
        pgids = calloc (pids, sizeof (pid_t));
        if (!pgids)
        {
            perror ("malloc");
            exit (1);
        }
    }
    got_long = 0;
    for (i = 0; i < pids; i++)
    {
        pid_t id;
        int found_name = -1;
        double process_age_sec = 0;
        /* match by UID */
        if (pwent && match_process_uid(pid_table[i], pwent->pw_uid)==0)
            continue;
        if (opt_ns_pid && ns_ino && ns_ino != get_ns(pid_table[i], PIDNS))
            continue;

	if (scontext && match_process_context(pid_table[i], scontext) == 0)
	    continue;

        length = load_process_name_and_age(comm, &process_age_sec, pid_table[i], (younger_than||older_than));
        if (length < 0)
            continue;

        /* test for process age, if required */
        if ( younger_than && (process_age_sec > younger_than ) )
            continue;
        if ( older_than   && (process_age_sec < older_than ) )
            continue;

        got_long = 0;
        if (command) {
            free(command);
            command = NULL;
        }
        if (length == COMM_LEN - 1)
            if (load_proc_cmdline(pid_table[i], comm, &command, &got_long) < 0)
                continue;

        /* match by process name */
        for (j = 0; j < name_count; j++)
        {
            if (reg)
            {
                if (regexec (&reglist[j], got_long ? command : comm, 0, NULL, 0) != 0)
                    continue;
            }
            else /* non-regex */
            {
                if (!name_info[j].st.st_dev)
                {
                    if (!match_process_name(comm, length, command, namelist[j],
                                            name_info[j].name_length, got_long))
                        continue;

                } else {
                    int ok = 1; 
                    if (asprintf (&path, PROC_BASE "/%d/exe", pid_table[i]) < 0)
                        continue;
                    if (stat (path, &st) < 0) 
                        ok = 0;
                    else if (name_info[j].st.st_dev != st.st_dev ||
                             name_info[j].st.st_ino != st.st_ino)
                    {
                        /* maybe the binary has been modified and std[j].st_ino
                         * is not reliable anymore. We need to compare paths.
                         */
                        size_t len = strlen(namelist[j]);
                        char *linkbuf = malloc(len + 1);

                        if (!linkbuf ||
                            readlink(path, linkbuf, len + 1) != (ssize_t)len ||
                            memcmp(namelist[j], linkbuf, len))
                            ok = 0;
                        free(linkbuf);
                    }
                    free(path);
                    if (!ok)
                        continue;
                }
            } /* non-regex */
            found_name = j;
            break;
        }  
        if (name_count && found_name==-1)
            continue;  /* match by process name faild */

        /* check for process group */
        if (!process_group)
            id = pid_table[i];
        else
        {
            int j;

            id = getpgid (pid_table[i]);
            pgids[i] = id;
            if (id < 0)
            {
                fprintf (stderr, "killall: getpgid(%d): %s\n",
                         pid_table[i], strerror (errno));
            }
            for (j = 0; j < i; j++)
                if (pgids[j] == id)
                    break;
            if (j < i)
                continue;
        }    
        if (interactive && !ask (comm, id, signal))
            continue;
        if (kill (process_group ? -id : id, signal) >= 0)
        {
            if (verbose)
                fprintf (stderr, _("Killed %s(%s%d) with signal %d\n"), got_long ? command :
                         comm, process_group ? "pgid " : "", id, signal);
            if (found_name >= 0)
                /* mark item of namelist */
                found |= 1UL << found_name;
            pid_killed[pids_killed++] = id;
        }
        else if (errno != ESRCH || interactive)
            fprintf (stderr, "%s(%d): %s\n", got_long ? command :
                     comm, id, strerror (errno));
    }
    if (command)
        free(command);
    if (reglist)
        free_regexp_list(reglist, name_count);
    free(pgids);
    if (!quiet)
        for (i = 0; i < name_count; i++)
            if (!(found & (1UL << i)))
                fprintf (stderr, _("%s: no process found\n"), namelist[i]);
    if (name_count)
        /* killall returns a zero return code if at least one process has 
         * been killed for each listed command. */
        error = found == ((1UL << (name_count - 1)) | ((1UL << (name_count - 1)) - 1)) ? 0 : 1;
    else
        /* in nameless mode killall returns a zero return code if at least 
         * one process has killed */
        error = pids_killed ? 0 : 1;
    /*
     * We scan all (supposedly) killed processes every second to detect dead
     * processes as soon as possible in order to limit problems of race with
     * PID re-use.
     */
    while (pids_killed && wait_until_dead)
    {
        for (i = 0; i < pids_killed;)
        {
            if (kill (process_group ? -pid_killed[i] : pid_killed[i], 0) < 0 &&
                errno == ESRCH)
            {
                pid_killed[i] = pid_killed[--pids_killed];
                continue;
            }
            i++;
        }
        sleep (1);        /* wait a bit longer */
    }
    free(pid_killed);
    free(pid_table);
    free(name_info);
    return error;
}


static void
usage (const char *msg)
{
    if (msg != NULL)
        fprintf(stderr, "%s\n", msg);
    fprintf(stderr, _(
                      "Usage: killall [OPTION]... [--] NAME...\n"));
    fprintf(stderr, _(
                      "       killall -l, --list\n"
                      "       killall -V, --version\n\n"
                      "  -e,--exact          require exact match for very long names\n"
                      "  -I,--ignore-case    case insensitive process name match\n"
                      "  -g,--process-group  kill process group instead of process\n"
                      "  -y,--younger-than   kill processes younger than TIME\n"
                      "  -o,--older-than     kill processes older than TIME\n"
                      "  -i,--interactive    ask for confirmation before killing\n"
                      "  -l,--list           list all known signal names\n"
                      "  -q,--quiet          don't print complaints\n"
                      "  -r,--regexp         interpret NAME as an extended regular expression\n"
                      "  -s,--signal SIGNAL  send this signal instead of SIGTERM\n"
                      "  -u,--user USER      kill only process(es) running as USER\n"
                      "  -v,--verbose        report if the signal was successfully sent\n"
                      "  -V,--version        display version information\n"
                      "  -w,--wait           wait for processes to die\n"
                      "  -n,--ns PID         match processes that belong to the same namespaces\n"
                      "                      as PID\n"));

    fprintf(stderr, _(
                      "  -Z,--context REGEXP kill only process(es) having context\n"
                      "                      (must precede other arguments)\n"));
    fputc('\n', stderr);
    exit(1);
}


void print_version()
{
    fprintf(stderr, "killall (PSmisc) %s\n", VERSION);
    fprintf(stderr, _(
                      "Copyright (C) 1993-2021 Werner Almesberger and Craig Small\n\n"));
    fprintf(stderr, _(
                      "PSmisc comes with ABSOLUTELY NO WARRANTY.\n"
                      "This is free software, and you are welcome to redistribute it under\n"
                      "the terms of the GNU General Public License.\n"
                      "For more information about these matters, see the files named COPYING.\n"));
}

static int
have_proc_self_stat (void)
{
    char filename[128];
    struct stat isproc;
    pid_t pid = getpid();

    snprintf(filename, sizeof(filename), PROC_BASE"/%d/stat", (int) pid);
    return stat(filename, &isproc) == 0;
}

int
main (int argc, char **argv)
{
    char *name;
    int sig_num;
    int optc;
    int myoptind;
    int skip_error=0;
    struct passwd *pwent = NULL;
    char yt[COMM_LEN];
    char ot[COMM_LEN];

    //int optsig = 0;

    struct option options[] = {
        {"exact", 0, NULL, 'e'},
        {"ignore-case", 0, NULL, 'I'},
        {"process-group", 0, NULL, 'g'},
        {"younger-than", 1, NULL, 'y'},
        {"older-than", 1, NULL, 'o'},
        {"interactive", 0, NULL, 'i'},
        {"list-signals", 0, NULL, 'l'},
        {"quiet", 0, NULL, 'q'},
        {"regexp", 0, NULL, 'r'},
        {"signal", 1, NULL, 's'},
        {"user", 1, NULL, 'u'},
        {"verbose", 0, NULL, 'v'},
        {"wait", 0, NULL, 'w'},
        {"ns", 1, NULL, 'n' },
        {"context", 1, NULL, 'Z'},
        {"version", 0, NULL, 'V'},
        {0,0,0,0 }};


    /* Setup the i18n */
#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif
    char *scontext = NULL;
    regex_t scontext_reg;

    if ( argc < 2 ) usage(NULL); /* do the obvious thing... */

    name = strrchr (*argv, '/');
    if (name)
        name++;
    else
        name = *argv;
    sig_num = SIGTERM;


    opterr = 0;
    while ( (optc = getopt_long_only(argc,argv,"egy:o:ilqrs:u:vwZ:VIn:",options,NULL)) != -1) {
            switch (optc) {
            case 'e':
                exact = 1;
                break;
            case 'g':
                process_group = 1;
                break;
            case 'y':
                strncpy(yt, optarg, sizeof yt -1);
                yt[sizeof yt -1] = '\0';
                if ( 0 >= (younger_than = parse_time_units(yt) ) )
                    usage(_("Invalid time format"));
                break;
            case 'o':
                strncpy(ot, optarg, sizeof ot - 1);
                ot[sizeof ot -1] = '\0';
                if ( 0 >= (older_than = parse_time_units(ot) ) )
                    usage(_("Invalid time format"));
                break;
            case 'i':
                interactive = 1;
                break;
            case 'l':
                list_signals();
                return 0;
                break;
            case 'q':
                quiet = 1;
                break;
            case 'r':
                reg = 1;
                break;
            case 's':
                sig_num = get_signal (optarg, "killall");
                break;
            case 'u':
                if (!(pwent = getpwnam(optarg))) {
                    fprintf (stderr, _("Cannot find user %s\n"), optarg);
                    exit (1);
                }
                break;
            case 'v':
                verbose = 1;
                break;
            case 'w':
                wait_until_dead = 1;
                break;
            case 'I':
                /* option check is optind-1 but sig name is optind */
                if (strcmp(argv[optind-1],"-I") == 0 || strncmp(argv[optind-1],"--",2) == 0) {
                    ignore_case = 1;
                } else {
                    sig_num = get_signal (argv[optind]+1, "killall");
                    skip_error=optind;
                }
                break;
            case 'V':
                /* option check is optind-1 but sig name is optind */
                if (strcmp(argv[optind-1],"-V") == 0 || strncmp(argv[optind-1],"--",2) == 0) {
                    print_version();
                    return 0;
                } else {
                    sig_num = get_signal (argv[optind]+1, "killall");
                    skip_error=optind;
                }
                break;
            case 'n': {
                long num;
                char *end = NULL;
                errno = 0;
                num = strtol(optarg, &end, 10);
                if (errno != 0 || optarg == end || end == NULL)
                    usage(_("Invalid namespace PID"));
                opt_ns_pid = (pid_t) num;
            }
                break;
            case 'Z': 
                scontext=optarg;
                if (regcomp(&scontext_reg, scontext, REG_EXTENDED|REG_NOSUB) != 0) {
                    fprintf(stderr, _("Bad regular expression: %s\n"), scontext);
                    exit (1);
                }
                break;
            case '?':
                if (skip_error == optind)
                    break;
                /* Sigh, this is a hack because -ve could be -version or
                 * -verbose */
                if (strncmp(argv[optind-1], "-ve", 3) == 0) {
                    verbose=1;
                    exact=1;
                    break;
                }
                /* Signal names are in uppercase, so check to see if the argv
                 * is upper case */
                if (argv[optind-1][1] >= 'A' && argv[optind-1][1] <= 'Z') {
                    sig_num = get_signal (argv[optind-1]+1, "killall");
                } else {
                    /* Might also be a -## signal too */
                    if (argv[optind-1][1] >= '0' && argv[optind-1][1] <= '9') {
                        sig_num = atoi(argv[optind-1]+1);
                    } else {
                            usage(NULL);
                    }
                }
                break;
            }
        }
        myoptind = optind;
        if ((argc - myoptind < 1) && pwent==NULL && scontext==NULL) 
                usage(NULL);

        if (argc - myoptind > MAX_NAMES) {
            fprintf (stderr, _("killall: Maximum number of names is %d\n"),
                     MAX_NAMES);
            exit (1);
        }
        if (!have_proc_self_stat()) {
            fprintf (stderr, _("killall: %s lacks process entries (not mounted ?)\n"),
                     PROC_BASE);
            exit (1);
        }
        argv = argv + myoptind;
        return kill_all(sig_num,argc - myoptind, argv, pwent, 
                        scontext ? &scontext_reg : NULL);
    }
