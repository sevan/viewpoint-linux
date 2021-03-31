/*
 * pstree.c - display process tree
 *
 * Copyright (C) 1993-2002 Werner Almesberger
 * Copyright (C) 2002-2020 Craig Small <csmall@dropbear.xyz>
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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <pwd.h>
#include <dirent.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <langinfo.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <locale.h>

#include "i18n.h"
#include "comm.h"

#ifdef WITH_SELINUX
#include <dlfcn.h>
#include <selinux/selinux.h>
#else
typedef void* security_context_t; /* DUMMY to remove most ifdefs */
#endif                                /*WITH_SELINUX */

extern const char *__progname;

#define PROC_BASE    "/proc"

#if defined(__FreeBSD_kernel__) || defined(__FreeBSD__)
#define ROOT_PID 0
#else
#define ROOT_PID 1
#endif /* __FreeBSD__ */

/* UTF-8 defines by Johan Myreen, updated by Ben Winslow */
#define UTF_V        "\342\224\202"        /* U+2502, Vertical line drawing char */
#define UTF_VR        "\342\224\234"        /* U+251C, Vertical and right */
#define UTF_H        "\342\224\200"        /* U+2500, Horizontal */
#define UTF_UR        "\342\224\224"        /* U+2514, Up and right */
#define UTF_HD        "\342\224\254"        /* U+252C, Horizontal and down */

#define VT_BEG        "\033(0\017"        /* use graphic chars */
#define VT_END        "\033(B"        /* back to normal char set */
#define VT_V        "x"                /* see UTF definitions above */
#define VT_VR        "t"
#define VT_H        "q"
#define VT_UR        "m"
#define        VT_HD        "w"

#define THREAD_FORMAT        "{%.*s}"        /* Format for thread names */

enum ns_type {
    CGROUPNS = 0,
    IPCNS,
    MNTNS,
    NETNS,
    PIDNS,
    USERNS,
    UTSNS,
    TIMENS,
    NUM_NS
};

enum color_type {
    COLOR_NONE = 0,
    COLOR_AGE,
    NUM_COLOUR
};

static const char *ns_names[] = {
    [CGROUPNS] = "cgroup",
    [IPCNS] = "ipc",
    [MNTNS] = "mnt",
    [NETNS] = "net",
    [PIDNS] = "pid",
    [USERNS] = "user",
    [UTSNS] = "uts",
    [TIMENS] = "time",
};

typedef struct _proc {
    char comm[COMM_LEN + 2 + 1]; /* add another 2 for thread brackets */
    char **argv;                /* only used : argv[0] is 1st arg; undef if argc < 1 */
    int argc;                        /* with -a   : number of arguments, -1 if swapped    */
    pid_t pid;
    pid_t pgid;
    uid_t uid;
    ino_t ns[NUM_NS];
    char flags;
    double age;
    struct _child *children;
    struct _proc *parent;
    struct _proc *next;
} PROC;

/* For flags above */
#define PFLAG_HILIGHT   0x01
#define PFLAG_THREAD    0x02

typedef struct _child {
    PROC *child;
    struct _child *next;
} CHILD;

struct ns_entry {
    ino_t number;
    CHILD *children;
    struct ns_entry *next;
};

static struct {
    const char *empty_2;        /*    */
    const char *branch_2;        /* |- */
    const char *vert_2;                /* |  */
    const char *last_2;                /* `- */
    const char *single_3;        /* --- */
    const char *first_3;        /* -+- */
} sym_ascii = {
"  ", "|-", "| ", "`-", "---", "-+-"}

, sym_utf = {
"  ",
        UTF_VR UTF_H,
        UTF_V " ",
        UTF_UR UTF_H, UTF_H UTF_H UTF_H, UTF_H UTF_HD UTF_H}, sym_vt100 = {
"  ",
        VT_BEG VT_VR VT_H VT_END,
        VT_BEG VT_V VT_END " ",
        VT_BEG VT_UR VT_H VT_END,
        VT_BEG VT_H VT_H VT_H VT_END, VT_BEG VT_H VT_HD VT_H VT_END}

, *sym = &sym_ascii;

static PROC *list = NULL;

struct age_to_color {
    unsigned int age_seconds;
    char *color;
};

struct age_to_color age_to_color[] = {
    { 60, "\033[32m"},
    {3600,   "\033[33m"},
    {0,	    "\033[31m"}
    };

/* The buffers will be dynamically increased in size as needed. */
static int capacity = 0;
static int *width = NULL;
static int *more = NULL;

static int print_args = 0, compact = 1, user_change = 0, pids = 0, pgids = 0,
    show_parents = 0, by_pid = 0, trunc = 1, wait_end = 0, ns_change = 0,
    thread_names = 0, hide_threads = 0;
static int show_scontext = 0;
static int output_width = 132;
static int cur_x = 1;
static char last_char = 0;
static int dumped = 0;                /* used by dump_by_user */
static int charlen = 0;                /* length of character */
static enum color_type color_highlight = COLOR_NONE;

const char *get_ns_name(enum ns_type id) {
    if (id >= NUM_NS)
        return NULL;
    return ns_names[id];
}

static enum ns_type get_ns_id(const char *name) {
    int i;

    for (i = 0; i < NUM_NS; i++)
        if (!strcmp(ns_names[i], name))
            return i;
    return NUM_NS;
}

static int verify_ns(enum ns_type id)
{
    char filename[50];
    struct stat s;

    snprintf(filename, 50, "/proc/%i/ns/%s", getpid(), get_ns_name(id));

    return stat(filename, &s);
}

static inline void new_proc_ns(PROC *ns_task)
{
    struct stat st;
    char buff[50];
    pid_t pid = ns_task->pid;
    int i;

    for (i = 0; i < NUM_NS; i++) {
        snprintf(buff, sizeof(buff), "/proc/%i/ns/%s", pid,
                 get_ns_name(i));
        if (stat(buff, &st)) {
            ns_task->ns[i] = 0;
            continue;
        }
        ns_task->ns[i] = st.st_ino;
    }
}

static void find_ns_and_add(struct ns_entry **root, PROC *r, enum ns_type id)
{
    struct ns_entry *ptr, *last = NULL;
    CHILD *tmp_child, **c;

    for (ptr = *root; ptr; ptr = ptr->next) {
        if (ptr->number == r->ns[id])
            break;
        last = ptr;
    }

    if (!ptr) {

        if (!(ptr = malloc(sizeof(*ptr)))) {
            perror("malloc");
            exit(1);
        }

        memset(ptr, 0, sizeof(*ptr));
        ptr->number = r->ns[id];
        if (*root == NULL)
            *root = ptr;
        else
            last->next = ptr;
    }

    /* move the child to under the namespace's umbrella */
    for (c = &ptr->children; *c; c = &(*c)->next)
        ;

    if (!(*c = malloc(sizeof(CHILD)))) {
        perror("malloc");
        exit(1);
    }

    (*c)->child = r;
    (*c)->next = NULL;

    /* detaching from parent */
    if (r->parent) {
        for (c = &r->parent->children; *c; c = &(*c)->next) {
            if ((*c)->child == r) {
                tmp_child = (*c)->next;
                free(*c);
                *c = tmp_child;
                break;
            }
        }
        r->parent = NULL;
    }

}

static PROC *find_proc(pid_t pid);
static void sort_by_namespace(PROC *r, enum ns_type id, struct ns_entry **root)
{
    CHILD *walk, *next;

    /* first run, find the first process */
    if (!r) {
        r = find_proc(1);
        if (!r)
            return;
    }

    if (r->parent == NULL || r->parent->ns[id] != r->ns[id])
        find_ns_and_add(root, r, id);

    walk = r->children;
    while (walk) {
        next = walk->next;
        sort_by_namespace(walk->child, id, root);
        walk = next;
    }
}

static void fix_orphans(void);

/*
 * Determine the correct output width, what we use is:
 */
static int get_output_width(void)
{
    char *ep, *env_columns;
    struct winsize winsz;

    env_columns = getenv("COLUMNS");
    if (env_columns && *env_columns) {
        long t;
        t = strtol(env_columns, &ep, 0);
        if (!*ep && (t > 0) && (t < 0x7fffffffL))
            return (int)t;
    }
    if (ioctl(1, TIOCGWINSZ, &winsz) >= 0)
        if (winsz.ws_col)
            return winsz.ws_col;
    return 132;

}

/*
 * Allocates additional buffer space for width and more as needed.
 * The first call will allocate the first buffer.
 *
 * index  the index that will be used after the call
 *        to this function.
 */
static void ensure_buffer_capacity(int index)
{
    if (index >= capacity) {
        if (capacity == 0)
            capacity = 100;
        else
            capacity *= 2;
        if (!(width = realloc(width, capacity * sizeof(int)))) {
            perror("realloc");
            exit(1);
        }
        if (!(more = realloc(more, capacity * sizeof(int)))) {
            perror("realloc");
            exit(1);
        }
    }
}

/*
 * Frees any buffers allocated by ensure_buffer_capacity.
 */
static void free_buffers()
{
    if (width != NULL) {
        free(width);
        width = NULL;
    }
    if (more != NULL) {
        free(more);
        more = NULL;
    }
    capacity = 0;
}

static void free_children(CHILD *children)
{
    CHILD *walk, *next;

    walk = children;
    while (walk != NULL) {
        next = walk->next;
        free(walk);
        walk = next;
    }
}

static void free_proc()
{
    PROC *walk, *next;
    walk = list;
    while (walk != NULL) {
        next = walk->next;
        free_children(walk->children);
        if (walk->argv) {
            free(walk->argv[0]);
            free(walk->argv);
        }
        free(walk);
        walk = next;
    }
    list = NULL;
}

static void free_namespace(struct ns_entry **nsroot)
{
    struct ns_entry *walk, *next;
    walk = *nsroot;
    while (walk != NULL) {
        next = walk->next;
        free_children(walk->children);
        free(walk);
        walk = next;
    }
    *nsroot = NULL;
}

static void out_char(char c)
{
    if (charlen == 0) {                /* "new" character */
        if ((c & 0x80) == 0) {
            charlen = 1;        /* ASCII */
        } else if ((c & 0xe0) == 0xc0) {        /* 110.. 2 bytes */
            charlen = 2;
        } else if ((c & 0xf0) == 0xe0) {        /* 1110.. 3 bytes */
            charlen = 3;
        } else if ((c & 0xf8) == 0xf0) {        /* 11110.. 4 bytes */
            charlen = 4;
        } else {
            charlen = 1;
        }
        cur_x++;                /* count first byte of whatever it is only */
    }
    charlen--;
    if (!trunc || cur_x <= output_width)
        putchar(c);
    else {
        if (trunc && (cur_x == output_width + 1))
            putchar('+');
    }
}


static void out_string(const char *str)
{
    while (*str)
        out_char(*str++);
}


static int out_int(int x)
{                                /* non-negative integers only */
    int digits, div;

    digits = 0;
    for (div = 1; x / div; div *= 10)
        digits++;
    if (!digits)
        digits = 1;
    for (div /= 10; div; div /= 10)
        out_char('0' + (x / div) % 10);
    return digits;
}

/*
 * Print the security context of the current process. This is largely lifted
 * from pr_context from procps ps/output.c
 */
static void out_scontext(const PROC *current)
{
    static void (*my_freecon)(char*) = 0;
    static int (*my_getpidcon)(pid_t pid, char **context) = 0;
    static int selinux_enabled = 0;
    char *context;

#ifdef WITH_SELINUX
    static int (*my_is_selinux_enabled)(void) = 0;
    static int tried_load = 0;

    if(!my_getpidcon && !tried_load){
    void *handle = dlopen("libselinux.so.1", RTLD_NOW);
    if(handle) {
	my_freecon = dlsym(handle, "freecon");
	if(dlerror())
	    my_freecon = 0;
	dlerror();
	my_getpidcon = dlsym(handle, "getpidcon");
	if(dlerror())
	    my_getpidcon = 0;
	my_is_selinux_enabled = dlsym(handle, "is_selinux_enabled");
	if(dlerror())
	    my_is_selinux_enabled = 0;
	else
	    selinux_enabled = my_is_selinux_enabled();
    }
    tried_load++;
    }
#endif /* WITH_SELINUX */

    out_string("`");
    
    if (my_getpidcon && selinux_enabled && !my_getpidcon(current->pid, &context)) {
	out_string(context);
	my_freecon(context);
    } else {
        FILE *file;
        char path[50];
        char readbuf[BUFSIZ+1];
	int num_read;
        snprintf(path, sizeof path, "/proc/%d/attr/current", current->pid);
        if ( (file = fopen(path, "r")) != NULL) {
            if (fgets(readbuf, BUFSIZ, file) != NULL) {
		num_read = strlen(readbuf);
		readbuf[num_read-1] = '\0';
                out_string(readbuf);
            }
      }
      out_string("'");
    }
}

static void out_newline(void)
{
    if (last_char && cur_x == output_width)
        putchar(last_char);
    last_char = 0;
    putchar('\n');
    cur_x = 1;
}

static void reset_color(void)
{
    if (color_highlight != COLOR_NONE) {
        char *str = "\033[0m";
        while (*str) putchar(*str++);
    }
}

static void print_proc_color(const int process_age)
{
    struct age_to_color *p;
    switch(color_highlight) {
    case COLOR_AGE:
        for(p=age_to_color; p->age_seconds != 0; p++)
        if (process_age < p->age_seconds) break;

        char *str = p->color;
        while (*str) putchar(*str++);
        break;
    default:
        break;
    }
}

static PROC *find_proc(pid_t pid)
{
    PROC *walk;

    for (walk = list; walk; walk = walk->next) {
        if (walk->pid == pid)
            return walk;
    }
    return NULL;
}

static PROC *new_proc(const char *comm, pid_t pid, uid_t uid)
{
    PROC *new;

    if (!(new = malloc(sizeof(PROC)))) {
        perror("malloc");
        exit(1);
    }

    strncpy(new->comm, comm, COMM_LEN+2);
    new->comm[COMM_LEN+1] = '\0';     /* make sure nul terminated*/
    new->pid = pid;
    new->uid = uid;
    new->flags = 0;
    new->argc = 0;
    new->argv = NULL;
    new->children = NULL;
    new->parent = NULL;
    new->next = list;
    new_proc_ns(new);
    return list = new;
}


static void add_child(PROC * parent, PROC * child)
{
    CHILD *new, **walk;
    int cmp;

    if (!(new = malloc(sizeof(CHILD)))) {
        perror("malloc");
        exit(1);
    }
    new->child = child;
    for (walk = &parent->children; *walk; walk = &(*walk)->next)
        if (by_pid) {
            if ((*walk)->child->pid > child->pid)
                break;
        } else if ((cmp = strcmp((*walk)->child->comm, child->comm)) > 0) {
            break; }
        else if (!cmp && (*walk)->child->uid > child->uid)
            break;
    new->next = *walk;
    *walk = new;
}


static void set_args(PROC * this, const char *args, int size)
{
    char *start;
    int i;

    if (!size) {
        this->argc = -1;
        return;
    }
    this->argc = 0;
    for (i = 0; i < size - 1; i++)
        if (!args[i]) {
            this->argc++;
            /* now skip consecutive NUL */
            while(!args[i] && (i < size -1 ))
                i++;
        }
    if (!this->argc)
        return;
    if (!(this->argv = malloc(sizeof(char *) * this->argc))) {
        perror("malloc");
        exit(1);
    }
    start = strchr(args, 0) + 1;
    size -= start - args;
    if (!(this->argv[0] = malloc((size_t) size))) {
        perror("malloc");
        exit(1);
    }
    start = memcpy(this->argv[0], start, (size_t) size);
    for (i = 1; i < this->argc; i++)
        this->argv[i] = start = strchr(start, 0) + 1;
}

static void
rename_proc(PROC *this, const char *comm, uid_t uid)
{
    PROC *tmp_child, *parent;
    CHILD **walk;

    strncpy(this->comm, comm, COMM_LEN+2);
    this->comm[COMM_LEN+1] = '\0';
    this->uid = uid;

    /* Re-sort children in parent, now we have a name */
    if (!by_pid && this->parent) {
        parent = this->parent;
        for (walk = &parent->children; *walk; walk = &(*walk)->next) {
            if (
                ((*walk)->next != NULL) &&
                strcmp((*walk)->child->comm, (*walk)->next->child->comm) > 0 ) {
                tmp_child = (*walk)->child;
                (*walk)->child = (*walk)->next->child;
                (*walk)->next->child = tmp_child;
            }
        }
    }
}

static void
add_proc(const char *comm, pid_t pid, pid_t ppid, pid_t pgid, uid_t uid,
         const char *args, int size, char isthread, double process_age_sec)
{
    PROC *this, *parent;

    if (!(this = find_proc(pid)))
        this = new_proc(comm, pid, uid);
    else {
        rename_proc(this, comm, uid);
    }
    if (args)
        set_args(this, args, size);
    if (pid == ppid)
        ppid = 0;
    this->pgid = pgid;
    this->age = process_age_sec;
    if (isthread)
      this->flags |= PFLAG_THREAD;
    if (!(parent = find_proc(ppid))) {
        parent = new_proc("?", ppid, 0);
    }
    if (pid != 0) {
      add_child(parent, this);
      this->parent = parent;
    }
}


static int tree_equal(const PROC * a, const PROC * b)
{
    const CHILD *walk_a, *walk_b;
    int i;

    if (strcmp(a->comm, b->comm))
        return 0;
    if (user_change && a->uid != b->uid)
        return 0;
    if (ns_change) {
        for (i = 0; i < NUM_NS; i++)
            if (a->ns[i] != b->ns[i])
                return 0;
    }
    for (walk_a = a->children, walk_b = b->children; walk_a && walk_b;
         walk_a = walk_a->next, walk_b = walk_b->next)
        if (!tree_equal(walk_a->child, walk_b->child))
            return 0;
    return !(walk_a || walk_b);
}

static int
out_args(char *mystr)
{
  char *here;
  int strcount=0;
  char tmpstr[5];

  for (here = mystr; *here; here++) {
    if (*here == '\\') {
      out_string("\\\\");
      strcount += 2;
    } else if (*here >= ' ' && *here <= '~') {
      out_char(*here);
      strcount++;
    } else {
      sprintf(tmpstr, "\\%03o", (unsigned char) *here);
      out_string(tmpstr);
      strcount += 4;
    }
  } /* for */
  return strcount;
}

static void
dump_tree(PROC * current, int level, int rep, int leaf, int last,
          uid_t prev_uid, int closing)
{
    CHILD *walk, *next, *tmp_child, **scan;
    const struct passwd *pw;
    int lvl, i, add, offset, len, swapped, info, count, comm_len, first;
    const char *tmp, *here;

    assert(closing >= 0);
    if (!current)
        return;
    if (!leaf)
        for (lvl = 0; lvl < level; lvl++) {
            for (i = width[lvl] + 1; i; i--)
                out_char(' ');
            out_string(lvl ==
                       level -
                       1 ? last ? sym->last_2 : sym->branch_2 : more[lvl +
                                                                     1] ?
                       sym->vert_2 : sym->empty_2);
        }

    if (rep < 2)
        add = 0;
    else {
        add = out_int(rep) + 2;
        out_string("*[");
    }
    print_proc_color(current->age);
    if ((current->flags & PFLAG_HILIGHT) && (tmp = tgetstr("md", NULL)))
        tputs(tmp, 1, putchar);
    swapped = info = print_args;
    if (swapped && current->argc < 0)
        out_char('(');
    comm_len = out_args(current->comm);
    offset = cur_x;
    if (pids) {
        out_char(info++ ? ',' : '(');
        (void) out_int(current->pid);
    }
    if (pgids) {
        out_char(info++ ? ',' : '(');
        (void) out_int(current->pgid);
    }
    if (user_change && prev_uid != current->uid) {
        out_char(info++ ? ',' : '(');
        if ((pw = getpwuid(current->uid)))
            out_string(pw->pw_name);
        else
            (void) out_int(current->uid);
    }
    if (ns_change && current->parent) {
        for (i = 0; i < NUM_NS; i++) {
            if (current->ns[i] == 0 || current->parent->ns[i] == 0)
                continue;
            if (current->ns[i] != current->parent->ns[i]) {
                out_char(info++ ? ',' : '(');
                out_string(get_ns_name(i));
            }
        }
    }
    if (show_scontext) {
        out_char(info++ ? ',' : '(');
        out_scontext(current);
    }
    if ((swapped && print_args && current->argc < 0) || (!swapped && info))
        out_char(')');
    if ((current->flags & PFLAG_HILIGHT) && (tmp = tgetstr("me", NULL)))
        tputs(tmp, 1, putchar);
    if (print_args) {
        for (i = 0; i < current->argc; i++) {
            if (i < current->argc - 1)        /* Space between words but not at the end of last */
                out_char(' ');
            len = 0;
            for (here = current->argv[i]; *here; here++)
                len += *here >= ' ' && *here <= '~' ? 1 : 4;
            if (cur_x + len <=
                output_width - (i == current->argc - 1 ? 0 : 4) || !trunc)
              out_args(current->argv[i]);
            else {
                out_string("...");
                break;
            }
        }
    }
    reset_color();
    if (show_scontext || print_args || !current->children)
    {
        while (closing--)
            out_char(']');
        out_newline();
    }
    ensure_buffer_capacity(level);
    more[level] = !last;

    if (show_scontext || print_args)
    {
        width[level] = swapped + (comm_len > 1 ? 0 : -1);
        count=0;
        first=1;
        for (walk = current->children; walk; walk = next) {
          next = walk->next;
          count=0;
          if (compact && (walk->child->flags & PFLAG_THREAD)) {
            scan = &walk->next;
            while (*scan) {
              if (!tree_equal(walk->child, (*scan)->child)) {
                scan = &(*scan)->next;
              } else {
                if (next == *scan)
                  next = (*scan)->next;
                count++;
                tmp_child = (*scan)->next;
                free(*scan);
                *scan = tmp_child;
              }
            }
            dump_tree(walk->child, level + 1, count + 1,
                  0, !next, current->uid, closing+ (count ? 2 : 1));
                 //closing + (count ? 1 : 0));
          } else {
          dump_tree(walk->child, level + 1, 1, 0, !walk->next,
                      current->uid, 0);
          }
        }
        return;
    }
    width[level] = comm_len + cur_x - offset + add;
    if (cur_x >= output_width && trunc) {
        out_string(sym->first_3);
        out_string("+");
        out_newline();
        return;
    }
    first = 1;
    for (walk = current->children; walk; walk = next) {
        count = 0;
        next = walk->next;
        if (compact) {
            scan = &walk->next;
            while (*scan)
                if (!tree_equal(walk->child, (*scan)->child))
                    scan = &(*scan)->next;
                else {
                    if (next == *scan)
                        next = (*scan)->next;
                    count++;
                    tmp_child = (*scan)->next;
                    free(*scan);
                    *scan = tmp_child;
                }
        }
        if (first) {
            out_string(next ? sym->first_3 : sym->single_3);
            first = 0;
        }
        dump_tree(walk->child, level + 1, count + 1,
                  walk == current->children, !next, current->uid,
                  closing + (count ? 1 : 0));
    }
}


static void dump_by_user(PROC * current, uid_t uid)
{
    const CHILD *walk;

    if (!current)
        return;

    if (current->uid == uid) {
        if (dumped)
            putchar('\n');
        dump_tree(current, 0, 1, 1, 1, uid, 0);
        dumped = 1;
        return;
    }
    for (walk = current->children; walk; walk = walk->next)
        dump_by_user(walk->child, uid);
}

static void dump_by_namespace(struct ns_entry *root)
{
    struct ns_entry *ptr = root;
    CHILD *c;
    char buff[14];

    for ( ; ptr; ptr = ptr->next) {
        snprintf(buff, sizeof(buff), "[%li]\n", (long int)ptr->number);
        out_string(buff);
        for (c = ptr->children; c; c = c->next)
            dump_tree(c->child, 0, 1, 1, 1, 0, 0);
    }
}

static void trim_tree_by_parent(PROC * current)
{
  if (!current)
    return;

  PROC * parent = current->parent;

  if (!parent)
    return;

  free_children(parent->children);
  parent->children = NULL;
  add_child(parent, current);
  trim_tree_by_parent(parent);
}

static double
uptime()
{
    char * savelocale;
    char buf[2048];
    FILE* file;
    if (!(file=fopen( PROC_BASE "/uptime", "r"))) {
        fprintf(stderr, "pstree: error opening uptime file\n");
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

static char* get_threadname(const pid_t pid, const int tid, const char *comm)
{
    FILE *file;
    char *thread_comm, *endcomm, *threadname;
    char *path = NULL;
    int len, nbytes;
    char readbuf[BUFSIZ + 1];

    if (! (threadname = malloc(COMM_LEN + 2 + 1))) {
        exit(2);
    }
    if (!thread_names) {
        sprintf(threadname, THREAD_FORMAT, COMM_LEN, comm);
        return threadname;
    }
    len = snprintf(NULL, 0, "%s/%d/task/%d/stat", PROC_BASE, pid, tid);
    if (len < 0)
        exit(2);
    len++;
    path = malloc(len);
    if (path == NULL)
        exit(2);
    nbytes = snprintf(path, len, "%s/%d/task/%d/stat", PROC_BASE, pid, tid);
    if (nbytes < 0 || nbytes >= len)
        perror("get_threadname: snprintf");
    if ( (file = fopen(path, "r")) != NULL) {
        if (fgets(readbuf, BUFSIZ, file) != NULL) {
            if ((thread_comm = strchr(readbuf, '('))
                && (endcomm = strrchr(thread_comm, ')'))) {
                ++thread_comm;
                *endcomm = '\0';
                sprintf(threadname, THREAD_FORMAT, COMM_LEN, thread_comm);
                (void) fclose(file);
                free(path);
                return threadname;
            }
        }
        fclose(file);
    }
    free(path);

    /* Fall back to old method */
    sprintf(threadname, THREAD_FORMAT, COMM_LEN, comm);
    return threadname;
}

/*
 * read_proc now uses a similar method as procps for finding the process
 * name in the /proc filesystem. My thanks to Albert and procps authors.
 */
static void read_proc(void)
{
  DIR *dir;
  struct dirent *de;
  FILE *file;
  struct stat st;
  char *path, *comm;
  char *buffer;
  size_t buffer_size;
  char readbuf[BUFSIZ + 1];
  char *tmpptr, *endptr;
  pid_t pid, ppid, pgid;
  int fd, size;
  int empty;
  unsigned long long proc_stt_jf = 0;
  double process_age_sec = 0;

  if (trunc)
    buffer_size = output_width + 1;
  else
    buffer_size = BUFSIZ + 1;

  if (!print_args)
    buffer = NULL;
  else if (!(buffer = malloc(buffer_size))) {
    perror("malloc");
    exit(1);
  }
  if (!(dir = opendir(PROC_BASE))) {
    perror(PROC_BASE);
    exit(1);
  }
  empty = 1;
  while ((de = readdir(dir)) != NULL) {
    pid = (pid_t) strtol(de->d_name, &endptr, 10);
    if (endptr != de->d_name && endptr[0] == '\0') {
      if (! (path = malloc(strlen(PROC_BASE) + strlen(de->d_name) + 10)))
        exit(2);
      sprintf(path, "%s/%d/stat", PROC_BASE, pid);
      if ((file = fopen(path, "r")) != NULL) {
        empty = 0;
        sprintf(path, "%s/%d", PROC_BASE, pid);
        if (stat(path, &st) < 0) {
          (void) fclose(file);
          free(path);
          continue;
        }
        size = fread(readbuf, 1, BUFSIZ, file);
        if (ferror(file) == 0) {
          readbuf[size] = 0;
          /* commands may have spaces or ) in them.
           * so don't trust anything from the ( to the last ) */
          if ((comm = strchr(readbuf, '('))
            && (tmpptr = strrchr(comm, ')'))) {
            ++comm;
            *tmpptr = 0;
            /* We now have readbuf with pid and cmd, and tmpptr+2
             * with the rest */
            /*printf("tmpptr: %s\n", tmpptr+2); */
            if (sscanf(tmpptr + 2, "%*c %d %d %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %*s %Lu",
                &ppid, &pgid, &proc_stt_jf) == 3) {
              DIR *taskdir;
              struct dirent *dt;
              char *taskpath;
              int thread;

	      process_age_sec = process_age(proc_stt_jf);
              /* handle process threads */
              if (! hide_threads) {
                if (! (taskpath = malloc(strlen(path) + 10)))
                  exit(2);
                sprintf(taskpath, "%s/task", path);

                if ((taskdir = opendir(taskpath)) != 0) {
                  /* if we have this dir, we're on 2.6 */
                  while ((dt = readdir(taskdir)) != NULL) {
                    if ((thread = atoi(dt->d_name)) != 0) {
                      if (thread != pid) {
                        char *threadname;
                        threadname = get_threadname(pid, thread, comm);
                        if (print_args)
                          add_proc(threadname, thread, pid, pgid, st.st_uid,
                              threadname, strlen (threadname) + 1, 1,
			      process_age_sec);
                        else
                          add_proc(threadname, thread, pid, pgid, st.st_uid,
                              NULL, 0, 1, 
			      process_age_sec);
                        free(threadname);
                      }
                    }
                  }
                  (void) closedir(taskdir);
                }
                free(taskpath);
              }

              /* handle process */
              if (!print_args)
                add_proc(comm, pid, ppid, pgid, st.st_uid, NULL, 0, 0,
			process_age_sec);
              else {
                sprintf(path, "%s/%d/cmdline", PROC_BASE, pid);
                if ((fd = open(path, O_RDONLY)) < 0) {
          /* If this fails then the process is gone.  If a PID
		   * was specified on the command-line then we might
		   * not even be interested in the current process.
		   * There's no sensible way of dealing with this race
		   * so we might as well behave as if the current
		   * process did not exist.  */
                    (void) fclose(file);
                    free(path);
                    continue;
                }
                if ((size = read(fd, buffer, buffer_size)) < 0) {
                    /* As above. */
                    close(fd);
                    (void) fclose(file);
                    free(path);
                    continue;
                }
                (void) close(fd);
                /* If we have read the maximum screen length of args,
                 * bring it back by one to stop overflow */
                if (size >= (int)buffer_size)
                  size--;
                if (size)
                  buffer[size++] = 0;
                add_proc(comm, pid, ppid, pgid, st.st_uid,
                     buffer, size, 0, process_age_sec);
              }
            }
          }
        }
        (void) fclose(file);
      }
      free(path);
    }
  }
  (void) closedir(dir);
  fix_orphans();
  if (print_args)
    free(buffer);
  if (empty) {
    fprintf(stderr, _("%s is empty (not mounted ?)\n"), PROC_BASE);
    exit(1);
  }
}


/* When using kernel 3.3 with hidepid feature enabled on /proc
 * then we need fake root pid and gather all the orphan processes
 * that is, processes with no known parent
 * As we cannot be sure if it is just the root pid or others missing
 * we gather the lot
 */
static void fix_orphans(void)
{
    PROC *root, *walk;

    if (!(root = find_proc(ROOT_PID))) {
        root = new_proc("?", ROOT_PID, 0);
    }
    for (walk = list; walk; walk = walk->next) {
        if (walk->pid == 1 || walk->pid == 0)
            continue;
        if (walk->parent == NULL) {
            add_child(root, walk);
            walk->parent = root;
        }
    }
}


static void usage(void)
{
    fprintf(stderr, _(
             "Usage: pstree [-acglpsStTuZ] [ -h | -H PID ] [ -n | -N type ]\n"
             "              [ -A | -G | -U ] [ PID | USER ]\n"
             "   or: pstree -V\n"));
    fprintf(stderr, _(
             "\n"
             "Display a tree of processes.\n\n"));
    fprintf(stderr, _(
             "  -a, --arguments     show command line arguments\n"
             "  -A, --ascii         use ASCII line drawing characters\n"
             "  -c, --compact-not   don't compact identical subtrees\n"));
    fprintf(stderr, _(
	     "  -C, --color=TYPE    color process by attribute\n"
	     "                      (age)\n"));
    fprintf(stderr, _(
             "  -g, --show-pgids    show process group ids; implies -c\n"
             "  -G, --vt100         use VT100 line drawing characters\n"));
    fprintf(stderr, _(
             "  -h, --highlight-all highlight current process and its ancestors\n"
             "  -H PID, --highlight-pid=PID\n"
             "                      highlight this process and its ancestors\n"
             "  -l, --long          don't truncate long lines\n"));
    fprintf(stderr, _(
             "  -n, --numeric-sort  sort output by PID\n"
             "  -N TYPE, --ns-sort=TYPE\n"
             "                      sort output by this namespace type\n"
             "                              (cgroup, ipc, mnt, net, pid, time, user, uts)\n"
             "  -p, --show-pids     show PIDs; implies -c\n"));
    fprintf(stderr, _(
             "  -s, --show-parents  show parents of the selected process\n"
             "  -S, --ns-changes    show namespace transitions\n"
             "  -t, --thread-names  show full thread names\n"
             "  -T, --hide-threads  hide threads, show only processes\n"));
    fprintf(stderr, _(
             "  -u, --uid-changes   show uid transitions\n"
             "  -U, --unicode       use UTF-8 (Unicode) line drawing characters\n"
             "  -V, --version       display version information\n"));
    fprintf(stderr, _(
             "  -Z, --security-context\n"
             "                      show security attributes\n"));
    fprintf(stderr, _("\n"
              "  PID    start at this PID; default is 1 (init)\n"
              "  USER   show only trees rooted at processes of this user\n\n"));
    exit(1);
}

void print_version()
{
    fprintf(stderr, _("pstree (PSmisc) %s\n"), VERSION);
    fprintf(stderr,
            _
            ("Copyright (C) 1993-2020 Werner Almesberger and Craig Small\n\n"));
    fprintf(stderr,
            _("PSmisc comes with ABSOLUTELY NO WARRANTY.\n"
              "This is free software, and you are welcome to redistribute it under\n"
              "the terms of the GNU General Public License.\n"
              "For more information about these matters, see the files named COPYING.\n"));
}


int main(int argc, char **argv)
{
    PROC *current;
    const struct passwd *pw;
    struct ns_entry *nsroot = NULL;
    pid_t pid, highlight;
    char termcap_area[1024];
    char *termname, *endptr;
    int c, pid_set = 0;
    enum ns_type nsid = NUM_NS;

    struct option options[] = {
        {"arguments", 0, NULL, 'a'},
        {"ascii", 0, NULL, 'A'},
        {"compact-not", 0, NULL, 'c'},
        {"color", 1, NULL, 'C'},
        {"vt100", 0, NULL, 'G'},
        {"highlight-all", 0, NULL, 'h'},
        {"highlight-pid", 1, NULL, 'H'},
        {"long", 0, NULL, 'l'},
        {"numeric-sort", 0, NULL, 'n'},
        {"ns-sort", 1, NULL, 'N' },
        {"show-pids", 0, NULL, 'p'},
        {"show-pgids", 0, NULL, 'g'},
        {"show-parents", 0, NULL, 's'},
        {"ns-changes", 0, NULL, 'S' },
        {"thread-names", 0, NULL, 't'},
        {"hide-threads", 0, NULL, 'T'},
        {"uid-changes", 0, NULL, 'u'},
        {"unicode", 0, NULL, 'U'},
        {"version", 0, NULL, 'V'},
        {"security-context", 0, NULL, 'Z'},
        { 0, 0, 0, 0 }
    };

    output_width = get_output_width();
    pid = ROOT_PID;
    highlight = 0;
    pw = NULL;

#ifdef ENABLE_NLS
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
#endif

    if (!strcmp(__progname, "pstree.x11"))
        wait_end = 1;

    /*
     * Attempt to figure out a good default symbol set.  Will be overriden by
     * command-line options, if given.
     */

    if (isatty(1) && !strcmp(nl_langinfo(CODESET), "UTF-8")) {
        /* Use UTF-8 symbols if the locale's character set is UTF-8. */
        sym = &sym_utf;
    } else if (isatty(1) && (termname = getenv("TERM")) &&
               (strlen(termname) > 0) &&
               (setupterm(NULL, 1 /* stdout */ , NULL) == OK) &&
               (tigetstr("acsc") != NULL) && (tigetstr("acsc") != (char *)-1)) {
        /*
         * Failing that, if TERM is defined, a non-null value, and the terminal
         * has the VT100 graphics charset, use it.
         */
        /* problems with VT100 on some terminals, making this ascci
         * for now
         */
        sym = &sym_ascii;
    } else {
        /* Otherwise, fall back to ASCII. */
        sym = &sym_ascii;
    }

    while ((c =
            getopt_long(argc, argv, "aAcC:GhH:nN:pglsStTuUVZ", options,
                        NULL)) != -1)
        switch (c) {
        case 'a':
            print_args = 1;
            break;
        case 'A':
            sym = &sym_ascii;
            break;
        case 'c':
            compact = 0;
            break;
        case 'C':
	    if (strcasecmp("age", optarg) == 0) {
		color_highlight = COLOR_AGE;
	    } else {
		usage();
	    }
            break;
        case 'G':
            sym = &sym_vt100;
            break;
        case 'h':
            if (highlight)
                usage();
            if (getenv("TERM")
                && tgetent(termcap_area, getenv("TERM")) > 0)
                highlight = getpid();
            break;
        case 'H':
            if (highlight)
                usage();
            if (!getenv("TERM")) {
                fprintf(stderr, _("TERM is not set\n"));
                return 1;
            }
            if (tgetent(termcap_area, getenv("TERM")) <= 0) {
                fprintf(stderr, _("Can't get terminal capabilities\n"));
                return 1;
            }
            if (!(highlight = atoi(optarg)))
                usage();
            break;
        case 'l':
            trunc = 0;
            break;
        case 'n':
            by_pid = 1;
            break;
        case 'N':
            nsid = get_ns_id(optarg);
            if (nsid == NUM_NS)
                 usage();
            if (verify_ns(nsid)) {
                 fprintf(stderr,
                         _("procfs file for %s namespace not available\n"),
                         optarg);
                 return 1;
            }
            break;
        case 'p':
            pids = 1;
            compact = 0;
            break;
        case 'g':
            pgids = 1;
            compact = 0;
            break;
        case 's':
            show_parents = 1;
            break;
        case 'S':
            ns_change = 1;
            break;
        case 't':
            thread_names = 1;
            break;
        case 'T':
            hide_threads = 1;
            break;
        case 'u':
            user_change = 1;
            break;
        case 'U':
            sym = &sym_utf;
            break;
        case 'V':
            print_version();
            return 0;
        case 'Z':
            show_scontext = 1;
            break;
        default:
            usage();
        }
    if (optind == argc - 1) {
        if (isdigit(*argv[optind])) {
            pid = (pid_t) strtol(argv[optind++], &endptr, 10);
            pid_set = 1;
            if (endptr[0] != '\0')
                usage();
        } else if (!(pw = getpwnam(argv[optind++]))) {
            fprintf(stderr, _("No such user name: %s\n"),
                    argv[optind - 1]);
            return 1;
        }
    }
    if (optind != argc)
        usage();
    read_proc();
    for (current = find_proc(highlight); current;
         current = current->parent)
        current->flags |= PFLAG_HILIGHT;

    if(show_parents && pid_set == 1) {
      trim_tree_by_parent(find_proc(pid));

      pid = ROOT_PID;
    }

    if (nsid != NUM_NS) {
        sort_by_namespace(NULL, nsid, &nsroot);
        dump_by_namespace(nsroot);
    } else if (!pw)
        dump_tree(find_proc(pid), 0, 1, 1, 1, 0, 0);
    else {
        dump_by_user(find_proc(ROOT_PID), pw->pw_uid);
        if (!dumped) {
            fprintf(stderr, _("No processes found.\n"));
            return 1;
        }
    }
    free_buffers();
    free_proc();
    free_namespace(&nsroot);
    if (wait_end == 1) {
        fprintf(stderr, _("Press return to close\n"));
        (void) getchar();
    }

    return 0;
}
