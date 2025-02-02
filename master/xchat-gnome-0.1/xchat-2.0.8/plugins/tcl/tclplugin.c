/***************************************************************************
                          tclplugin.c  -  Tcl plugin for xchat 1.9.x / 2.x.x
                          --------------------------------------------------
    begin                : Sat Nov 19 17:31:20 MST 2002
    copyright            : Copyright 2002-2003 Daniel P. Stasinski
    email                : mooooooo@avenues.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#define VERSION "1.0.49"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <tcl.h>
#include <tclDecls.h>
#include <sys/stat.h>

#ifdef WIN32
#include <windows.h>
#endif

#include "xchat-plugin.h"
#include "tclplugin.h"
#include "printevents.h"

static int nexttimerid = 0;
static int nexttimerindex = 0;
static timer timers[MAX_TIMERS];

static int initialized = 0;
static int reinit_tried = 0;
static Tcl_Interp *interp = NULL;
static xchat_plugin *ph;
static xchat_hook *raw_line_hook;
static xchat_hook *Command_TCL_hook;
static xchat_hook *Command_Source_hook;
static xchat_hook *Command_Reload_hook;
static xchat_hook *Command_Load_hook;
static xchat_hook *Event_Handler_hook;
static xchat_hook *Null_Command_hook;

static int complete_level = 0;
static t_complete complete[MAX_COMPLETES + 1];
static Tcl_HashTable cmdTablePtr;
static Tcl_HashTable aliasTablePtr;

static int nextprocid = 0x1000;
#define PROCPREFIX "__xctcl_"

static char unknown[] = {
"rename unknown iunknown\n"
"proc ::unknown {args} {\n"
"  global errorInfo errorCode\n"
"  if { [string index [lindex $args 0] 0] == \"/\" } {\n"
"    command \"[string range [join $args \" \"] 1 end]\"\n"
"  } else {\n"
"    set code [catch {uplevel iunknown $args} msg]\n"
"    if {$code == 1} {\n"
"      set new [split $errorInfo \\n]\n"
"      set new [join [lrange $new 0 [expr {[llength $new] - 8}]] \\n]\n"
"      return -code error -errorcode $errorCode -errorinfo $new $msg\n"
"    } else {\n"
"      return -code $code $msg\n"
"    }\n"
"  }\n"
"}\n"
"proc unsupported0 {from to {bytes \"\"}} {\n"
"  set b [expr {$bytes == \"\" ? \"\" : \"-size [list $bytes]\"}]\n"
"  eval [list fcopy $from $to] $b\n"
"}\n"
};

static char sourcedirs[] = {
    "set files [lsort [glob -nocomplain -join [xchatdir] \"*.tcl\"]]\n"
        "set init [lsearch -glob $files \"*/init.tcl\"]\n"
        "if { $init > 0 } {\n"
        "set initfile [lindex $files $init]\n"
        "set files [lreplace $files $init $init]\n"
        "set files [linsert $files 0 $initfile]\n" "}\n" "foreach f $files {\n" "if { [catch { source $f } errMsg] } {\n" "puts \"Tcl plugin\\tError sourcing \\\"$f\\\" ($errMsg)\"\n" "} else {\n" "puts \"Tcl plugin\\tSourced \\\"$f\\\"\"\n" "}\n" "}\n"
};

static char inlinetcl[] = {
"proc splitsrc { } {\n"
"uplevel \"scan \\$_src \\\"%\\\\\\[^!\\\\\\]!%\\\\\\[^@\\\\\\]@%s\\\" _nick _ident _host\"\n"
"}\n"
"proc ::exit { } {\n"
"puts \"Using 'exit' is bad\"\n"
"}\n"
};

static void Tcl_MyDStringAppend(Tcl_DString * ds, char *string)
{
    Tcl_DStringAppend(ds, string, strlen(string));
}

static char *InternalProcName(int value)
{
    static char result[32];
    sprintf(result, "%s%08x", PROCPREFIX, value);
    return result;
}

static int SourceInternalProc(int id, char *args, char *source)
{
    Tcl_DString ds;
    int result;
    Tcl_DStringInit(&ds);

    Tcl_MyDStringAppend(&ds, "proc ");
    Tcl_MyDStringAppend(&ds, InternalProcName(id));
    Tcl_MyDStringAppend(&ds, " { ");
    Tcl_MyDStringAppend(&ds, args);
    Tcl_MyDStringAppend(&ds, " } {\n");
    Tcl_MyDStringAppend(&ds, source);
    Tcl_MyDStringAppend(&ds, "\n}\n\n");

    result = Tcl_Eval(interp, ds.string);

    Tcl_DStringFree(&ds);

    return result;
}

static int EvalInternalProc(char *procname, int ct, ...)
{
    Tcl_DString ds;
    int result;
    va_list ap;
    char *buf;

    Tcl_DStringInit(&ds);

    Tcl_MyDStringAppend(&ds, procname);

    if (ct) {
        va_start(ap, ct);
        while (ct--) {
            if ((buf = va_arg(ap, char *)) != NULL)
                 Tcl_DStringAppendElement(&ds, buf);
            else
                Tcl_MyDStringAppend(&ds, " \"\"");
        }
        va_end(ap);
    }

    result = Tcl_Eval(interp, ds.string);

    Tcl_DStringFree(&ds);

    return result;
}


static void DeleteInternalProc(char *proc)
{
    Tcl_DString ds;

    Tcl_DStringInit(&ds);
    Tcl_MyDStringAppend(&ds, "rename ");
    Tcl_MyDStringAppend(&ds, proc);
    Tcl_MyDStringAppend(&ds, " {}");
    Tcl_Eval(interp, ds.string);
    Tcl_DStringFree(&ds);
}

static char *StrDup(char *string, int *length)
{
    char *result;

    if (string == NULL)
        return NULL;

    *length = strlen(string);
    result = Tcl_Alloc(*length + 1);
    strncpy(result, string, *length);
    result[*length] = 0;

    return result;
}

static char *myitoa(long value)
{
    static char result[32];
    sprintf(result, "%ld", value);
    return result;
}

static xchat_context *atoctx(const char *nptr)
{
    int isnum = 1;
    int x = 0;

    if (!nptr)
        return NULL;

    while (isnum && nptr[x]) {
        if (!isdigit(nptr[x++]))
            isnum--;
    }

    if (isnum && x)
        return (xchat_context *) atol(nptr);
    else
        return NULL;
}

static xchat_context *xchat_smart_context(char *arg1, char *arg2)
{
    char *server, *s, *n;
    xchat_context *result = NULL;
    xchat_context *ctx = NULL;
    xchat_context *actx = NULL;
    xchat_list *list;

    if (arg1 == NULL)
        return xchat_get_context(ph);;

    if (arg1 && arg2) {
        result = xchat_find_context(ph, arg1, arg2);
        if (result == NULL)
            result = xchat_find_context(ph, arg2, arg1);
        return result;
    } else {

        actx = atoctx(arg1);

        server = xchat_get_info(ph, "server");

        list = xchat_list_get(ph, "channels");

        if (list != NULL) {

            while (xchat_list_next(ph, list)) {

                ctx = xchat_list_str(ph, list, "context");

                if (actx) {
                    if (ctx == actx) {
                        result = ctx;
                        break;
                    }
                } else {

                    s = xchat_list_str(ph, list, "server");

                    if (xchat_list_int(ph, list, "type") == 1) {
                        if (strcasecmp(arg1, s) == 0) {
                            result = ctx;
                            break;
                        }
                        n = xchat_list_str(ph, list, "network");
                        if (n) {
                            if (strcasecmp(arg1, n) == 0) {
                                result = ctx;
                                break;
                            }
                        }
                    } else {
                        if ((strcasecmp(server, s) == 0) && (strcasecmp(arg1, xchat_list_str(ph, list, "channel")) == 0)) {
                            result = ctx;
                            break;
                        }
                    }
                }
            }

            xchat_list_free(ph, list);
        }

    }

    return result;
}

static void queue_nexttimer()
{
    int x;
    time_t then;

    nexttimerindex = 0;
    then = (time_t) 0x7fffffff;

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid) {
            if (timers[x].timestamp < then) {
                then = timers[x].timestamp;
                nexttimerindex = x;
            }
        }
    }
}

static int insert_timer(int seconds, int count, char *script)
{
    int x;
    int dummy;
    time_t now;
    char *errorInfo;
    int id;

    if (script == NULL)
        return (-1);

    id = (nextprocid++ % INT_MAX) + 1;

    now = time(NULL);

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid == 0) {
            if (SourceInternalProc(id, "", script) == TCL_ERROR) {
                errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
                xchat_printf(ph, "\0039TCL plugin\003\tERROR (timer %d) %s \n", timers[x].timerid, errorInfo);
                return (-1);
            }
            timers[x].timerid = (nexttimerid++ % INT_MAX) + 1;
            timers[x].timestamp = now + seconds;
            timers[x].count = count;
            timers[x].seconds = seconds;
            timers[x].procPtr = StrDup(InternalProcName(id), &dummy);
            queue_nexttimer();
            return (timers[x].timerid);
        }
    }

    return (-1);
}

static void do_timer()
{
    xchat_context *origctx;
    time_t now;
    int index;
    char *errorInfo;

    if (!nexttimerindex)
        return;

    now = time(NULL);

    if (now < timers[nexttimerindex].timestamp)
        return;

    index = nexttimerindex;
    origctx = xchat_get_context(ph);
    if (EvalInternalProc(timers[index].procPtr, 0) == TCL_ERROR) {
        errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
        xchat_printf(ph, "\0039TCL plugin\003\tERROR (timer) %s \n", errorInfo);
    }
    xchat_set_context(ph, origctx);

    if (timers[index].count != -1)
      timers[index].count--;

    if (timers[index].count == 0) {
      timers[index].timerid = 0;
      if (timers[index].procPtr != NULL) {
          DeleteInternalProc(timers[index].procPtr);
          Tcl_Free(timers[index].procPtr);
      }
      timers[index].procPtr = NULL;
    } else {
      timers[index].timestamp += timers[index].seconds;
    }

    queue_nexttimer();

    return;

}

static int Server_raw_line(char *word[], char *word_eol[], void *userdata)
{
    char *src, *cmd, *dest, *rest;
    char *chancmd = NULL;
    char *procList;
    Tcl_HashEntry *entry = NULL;
    xchat_context *origctx;
    int len;
    int dummy;
    char *string = NULL;
    int ctcp = 0;
    int count;
    int list_argc, proc_argc;
    char **list_argv, **proc_argv;
    int private = 0;
    char *errorInfo;

    if (word[0][0] == 0)
        return XCHAT_EAT_NONE;

    if (complete_level == MAX_COMPLETES)
        return XCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = XCHAT_EAT_PLUGIN;
    complete[complete_level].result = XCHAT_EAT_NONE;

    if (word[1][0] == ':') {
        src = word[1];
        cmd = word[2];
        dest = word[3];
        rest = word_eol[4];
    } else {
        src = "";
        cmd = word[1];
        if (word_eol[2][0] == ':') {
            dest = "";
            rest = word_eol[2];
        } else {
            dest = word[2];
            rest = word_eol[3];
        }
    }

    if (src[0] == ':')
        src++;
    if (dest[0] == ':')
        dest++;
    if (rest[0] == ':')
        rest++;

    if (rest[0] == 0x01) {
        rest++;
        if (strcasecmp("PRIVMSG", cmd) == 0) {
            if (strncasecmp(rest, "ACTION ", 7) == 0) {
                cmd = "ACTION";
                rest += 7;
            } else
                cmd = "CTCP";
        } else if (!strcasecmp("NOTICE", cmd))
            cmd = "CTCR";
        ctcp = 1;
    } else if (!strcasecmp("NOTICE", cmd) && (strchr(src, '!') == NULL)) {
        cmd = "SNOTICE";
    } else if (rest[0] == '!') {
        chancmd = word[4] + 1;
    }

    if (chancmd != NULL) {
        string = StrDup(chancmd, &dummy);
        Tcl_UtfToUpper(string);
        if ((entry = Tcl_FindHashEntry(&cmdTablePtr, string)) == NULL) {
            Tcl_Free(string);
        } else {
            cmd = chancmd;
            rest = word_eol[5];
        }
    }

    if (entry == NULL) {
        string = StrDup(cmd, &dummy);
        Tcl_UtfToUpper(string);
        entry = Tcl_FindHashEntry(&cmdTablePtr, string);
    }

    if (entry != NULL) {

        procList = Tcl_GetHashValue(entry);

        if (isalpha(dest[0]))
            private = 1;

        rest = StrDup(rest, &len);
        if (ctcp) {
            if (rest != NULL) {
                if ((len > 1) && (rest[len - 1] == 0x01))
                    rest[len - 1] = 0;
            }
        }

        if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {

            for (count = 0; count < list_argc; count++) {

                if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                    continue;

                origctx = xchat_get_context(ph);
                if (EvalInternalProc(proc_argv[1], 7, src, dest, cmd, rest, word_eol[1], proc_argv[0], myitoa(private)) == TCL_ERROR) {
                    errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
                    xchat_printf(ph, "\0039TCL plugin\003\tERROR (on %s %s) %s \n", cmd, proc_argv[0], errorInfo);
                }
                xchat_set_context(ph, origctx);

                Tcl_Free((char *) proc_argv);

                if (complete[complete_level].result)
                    break;

            }

            Tcl_Free((char *) list_argv);

        }

        Tcl_Free(rest);

    }

    if (string)
        Tcl_Free(string);

    return complete[complete_level--].result;

}

static int Print_Hook(char *word[], void *userdata)
{
    char *procList;
    Tcl_HashEntry *entry;
    xchat_context *origctx;
    int count;
    int list_argc, proc_argc;
    char **list_argv, **proc_argv;
    Tcl_DString ds;
    int x;
    char *errorInfo;

    if (complete_level == MAX_COMPLETES)
        return XCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = XCHAT_EAT_PLUGIN;
    complete[complete_level].result = XCHAT_EAT_NONE;

    if ((entry = Tcl_FindHashEntry(&cmdTablePtr, xc[(int) userdata].event)) != NULL) {

        procList = Tcl_GetHashValue(entry);

        if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {

            for (count = 0; count < list_argc; count++) {

                if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                    continue;

                origctx = xchat_get_context(ph);

                Tcl_DStringInit(&ds);

                if ((int) userdata == CHAT) {
                    Tcl_DStringAppend(&ds, word[3], strlen(word[3]));
                    Tcl_DStringAppend(&ds, "!*@", 3);
                    Tcl_DStringAppend(&ds, word[1], strlen(word[1]));
                    if (EvalInternalProc(proc_argv[1], 7, ds.string, word[2], xc[(int) userdata].event, word[4], "", proc_argv[0], "0") == TCL_ERROR) {
                        errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
                        xchat_printf(ph, "\0039TCL plugin\003\tERROR (on %s %s) %s \n", xc[(int) userdata].event, proc_argv[0], errorInfo);
                    }
                } else {
                    if (xc[(int) userdata].argc > 0) {
                        for (x = 0; x <= xc[(int) userdata].argc; x++)
                            Tcl_DStringAppendElement(&ds, word[x]);
                    }
                    if (EvalInternalProc(proc_argv[1], 7, "", "", xc[(int) userdata].event, "", ds.string, proc_argv[0], "0") == TCL_ERROR) {
                        errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
                        xchat_printf(ph, "\0039Tcl plugin\003\tERROR (on %s %s) %s \n", xc[(int) userdata].event, proc_argv[0], errorInfo);
                    }
                }

                Tcl_DStringFree(&ds);

                xchat_set_context(ph, origctx);

                Tcl_Free((char *) proc_argv);

                if (complete[complete_level].result)
                    break;

            }

            Tcl_Free((char *) list_argv);

        }
    }

    return complete[complete_level--].result;
}


static int tcl_timerexists(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int x;
    int timerid;

    BADARGS(2, 2, " schedid");

    if (Tcl_GetInt(irp, argv[1], &timerid) != TCL_OK) {
        Tcl_AppendResult(irp, "Invalid timer id", NULL);
        return TCL_ERROR;
    }

    if (timerid) {
        for (x = 1; x < MAX_TIMERS; x++) {
            if (timers[x].timerid == timerid) {
                Tcl_AppendResult(irp, "1", NULL);
                return TCL_OK;
            }
        }
    }

    Tcl_AppendResult(irp, "0", NULL);

    return TCL_OK;
}

static int tcl_killtimer(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int x;
    int timerid;

    BADARGS(2, 2, " timerid");

    if (Tcl_GetInt(irp, argv[1], &timerid) != TCL_OK) {
        Tcl_AppendResult(irp, "Invalid timer id", NULL);
        return TCL_ERROR;
    }

    if (timerid) {
        for (x = 1; x < MAX_TIMERS; x++) {
            if (timers[x].timerid == timerid) {
                timers[x].timerid = 0;
                if (timers[x].procPtr != NULL) {
                    DeleteInternalProc(timers[x].procPtr);
                    Tcl_Free(timers[x].procPtr);
                }
                timers[x].procPtr = NULL;
                break;
            }
        }
    }

    queue_nexttimer();

    return TCL_OK;
}

static int tcl_timers(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int x;
    Tcl_DString ds;
    time_t now;

    BADARGS(1, 1, "");

    now = time(NULL);

    Tcl_DStringInit(&ds);

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid) {
            Tcl_DStringStartSublist(&ds);
            Tcl_DStringAppendElement(&ds, myitoa((long)timers[x].timerid));
            Tcl_DStringAppendElement(&ds, myitoa((long)timers[x].timestamp - now));
            Tcl_DStringAppendElement(&ds, timers[x].procPtr);
	    Tcl_DStringAppendElement(&ds, myitoa((long)timers[x].seconds));
	    Tcl_DStringAppendElement(&ds, myitoa((long)timers[x].count));
            Tcl_DStringEndSublist(&ds);
        }
    }

    Tcl_AppendResult(interp, ds.string, NULL);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_timer(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int seconds;
    int timerid;
    int repeat = 0;
    int count = 0;
    int first = 1;
    char reply[32];

    BADARGS(3, 6, " ?-repeat? ?-count times? seconds {script | procname ?args?}");

    while (argc--) {
        if (strcasecmp(argv[first], "-repeat") == 0) {
            repeat++;
        } else if (strcasecmp(argv[first], "-count") == 0) {
            if (Tcl_GetInt(irp, argv[++first], &count) != TCL_OK)
                return TCL_ERROR;
        } else {
            break;
        }
        first++;
    }

    if (repeat && !count)
      count = -1;

    if (!count)
      count = 1;

    if (Tcl_GetInt(irp, argv[first++], &seconds) != TCL_OK)
        return TCL_ERROR;

    if ((timerid = insert_timer(seconds, count, argv[first++])) == -1) {
        Tcl_AppendResult(irp, "0", NULL);
        return TCL_ERROR;
    }

    sprintf(reply, "%d", timerid);

    Tcl_AppendResult(irp, reply, NULL);

    queue_nexttimer();

    return TCL_OK;
}

static int tcl_on(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int newentry;
    char *procList;
    Tcl_HashEntry *entry;
    char *token;
    int dummy;
    Tcl_DString ds;
    int index;
    int count;
    int list_argc, proc_argc;
    int id;
    char **list_argv, **proc_argv;
    char *errorInfo;

    BADARGS(4, 4, " token label {script | procname ?args?}");

    id = (nextprocid++ % INT_MAX) + 1;

    if (SourceInternalProc(id, "_src _dest _cmd _rest _raw _label _private", argv[3]) == TCL_ERROR) {
        errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
        xchat_printf(ph, "\0039Tcl plugin\003\tERROR (on %s:%s) %s \n", argv[1], argv[2], errorInfo);
        return TCL_OK;
    }

    token = StrDup(argv[1], &dummy);
    Tcl_UtfToUpper(token);

    Tcl_DStringInit(&ds);

    entry = Tcl_CreateHashEntry(&cmdTablePtr, token, &newentry);
    if (!newentry) {
        procList = Tcl_GetHashValue(entry);
        if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {
            for (count = 0; count < list_argc; count++) {
                if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                    continue;
                if (strcmp(proc_argv[0], argv[2])) {
                    Tcl_DStringStartSublist(&ds);
                    Tcl_DStringAppendElement(&ds, proc_argv[0]);
                    Tcl_DStringAppendElement(&ds, proc_argv[1]);
                    Tcl_DStringEndSublist(&ds);
                } else
                    DeleteInternalProc(proc_argv[1]);
                Tcl_Free((char *) proc_argv);
            }
            Tcl_Free((char *) list_argv);
        }
        Tcl_Free(procList);
    }

    Tcl_DStringStartSublist(&ds);
    Tcl_DStringAppendElement(&ds, argv[2]);
    Tcl_DStringAppendElement(&ds, InternalProcName(id));
    Tcl_DStringEndSublist(&ds);

    procList = StrDup(ds.string, &dummy);

    Tcl_SetHashValue(entry, procList);

    if ((strncmp(token, "XC_", 3) == 0) || (strcmp(token, "CHAT") == 0)) {
        for (index = 0; index < XC_SIZE; index++) {
            if (strcmp(xc[index].event, token) == 0) {
                if (xc[index].hook == NULL) {
                    xc[index].hook = xchat_hook_print(ph, xc[index].emit, XCHAT_PRI_NORM, Print_Hook, (void *) index);
                    break;
                }
            }
        }
    }

    Tcl_Free(token);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_off(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    char *procList;
    Tcl_HashEntry *entry;
    char *token;
    int dummy;
    Tcl_DString ds;
    int index;
    int count;
    int list_argc, proc_argc;
    char **list_argv, **proc_argv;

    BADARGS(2, 3, " token ?label?");

    token = StrDup(argv[1], &dummy);
    Tcl_UtfToUpper(token);

    Tcl_DStringInit(&ds);

    if ((entry = Tcl_FindHashEntry(&cmdTablePtr, token)) != NULL) {

        procList = Tcl_GetHashValue(entry);

        if (argc == 3) {
            if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {
                for (count = 0; count < list_argc; count++) {
                    if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                        continue;
                    if (strcmp(proc_argv[0], argv[2])) {
                        Tcl_DStringStartSublist(&ds);
                        Tcl_DStringAppendElement(&ds, proc_argv[0]);
                        Tcl_DStringAppendElement(&ds, proc_argv[1]);
                        Tcl_DStringEndSublist(&ds);
                    } else
                        DeleteInternalProc(proc_argv[1]);
                    Tcl_Free((char *) proc_argv);
                }
                Tcl_Free((char *) list_argv);
            }
        }

        Tcl_Free(procList);

        if (ds.length) {
            procList = StrDup(ds.string, &dummy);
            Tcl_SetHashValue(entry, procList);
        } else {
            Tcl_DeleteHashEntry(entry);
        }

        if (!ds.length) {
            if ((strncmp(token, "XC_", 3) == 0) || (strcmp(token, "CHAT") == 0)) {
                for (index = 0; index < XC_SIZE; index++) {
                    if (strcmp(xc[index].event, token) == 0) {
                        if (xc[index].hook != NULL) {
                            xchat_unhook(ph, xc[index].hook);
                            break;
                        }
                    }
                }
            }
        }
    }

    Tcl_Free(token);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_alias(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int newentry;
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    char *string;
    char *help = NULL;
    int dummy;
    int id;
    char *errorInfo;

    BADARGS(3, 4, " name ?help? {script | procname ?args?}");

    string = StrDup(argv[1], &dummy);
    Tcl_UtfToUpper(string);

    if (strlen(argv[argc - 1])) {

        if (argc == 4)
            help = argv[2];

        id = (nextprocid++ % INT_MAX) + 1;

        if (SourceInternalProc(id, "_cmd _rest", argv[argc - 1]) == TCL_ERROR) {
            errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
            xchat_printf(ph, "\0039Tcl plugin\003\tERROR (alias %s) %s \n", argv[1], errorInfo);
            return TCL_OK;
        }

        entry = Tcl_CreateHashEntry(&aliasTablePtr, string, &newentry);
        if (newentry) {
            aliasPtr = (alias *) Tcl_Alloc(sizeof(alias));
            if (string[0] == '@')
                aliasPtr->hook = NULL;
            else
                aliasPtr->hook = xchat_hook_command(ph, string, XCHAT_PRI_NORM, Command_Alias, help, 0);
        } else {
            aliasPtr = Tcl_GetHashValue(entry);
            DeleteInternalProc(aliasPtr->procPtr);
            Tcl_Free(aliasPtr->procPtr);
        }

        aliasPtr->procPtr = StrDup(InternalProcName(id), &dummy);

        Tcl_SetHashValue(entry, aliasPtr);

    } else {

        if ((entry = Tcl_FindHashEntry(&aliasTablePtr, string)) != NULL) {
            aliasPtr = Tcl_GetHashValue(entry);
            DeleteInternalProc(aliasPtr->procPtr);
            Tcl_Free(aliasPtr->procPtr);
            if (aliasPtr->hook)
                xchat_unhook(ph, aliasPtr->hook);
            Tcl_Free((char *) aliasPtr);
            Tcl_DeleteHashEntry(entry);	    
        }
    }

    Tcl_Free(string);

    return TCL_OK;
}

static int tcl_complete(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    BADARGS(1, 2, " ?EAT_NONE|EAT_XCHAT|EAT_PLUGIN|EAT_ALL?");

    if (argc == 2) {
        if (Tcl_GetInt(irp, argv[1], &complete[complete_level].result) != TCL_OK) {
            if (strcasecmp("EAT_NONE", argv[1]) == 0)
                complete[complete_level].result = XCHAT_EAT_NONE;
            else if (strcasecmp("EAT_XCHAT", argv[1]) == 0)
                complete[complete_level].result = XCHAT_EAT_XCHAT;
            else if (strcasecmp("EAT_PLUGIN", argv[1]) == 0)
                complete[complete_level].result = XCHAT_EAT_PLUGIN;
            else if (strcasecmp("EAT_ALL", argv[1]) == 0)
                complete[complete_level].result = XCHAT_EAT_ALL;
            else
                BADARGS(1, 2, " ?EAT_NONE|EAT_XCHAT|EAT_PLUGIN|EAT_ALL?");
        }
    } else
        complete[complete_level].result = complete[complete_level].defresult;

    return TCL_RETURN;
}

static int tcl_command(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx;
    xchat_context *ctx = NULL;
    char *string = NULL;

    BADARGS(2, 4, " ?server|network|context? ?#channel|nick? text");

    origctx = xchat_get_context(ph);

    switch (argc) {
    case 2:
        ctx = origctx;
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 4:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    string = argv[argc - 1];

    if (string[0] == '/')
        string++;

    xchat_set_context(ph, ctx);
    xchat_command(ph, string);
    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_raw(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx;
    xchat_context *ctx = NULL;
    char *string = NULL;

    BADARGS(2, 4, " ?server|network|context? ?#channel|nick? text");

    origctx = xchat_get_context(ph);

    switch (argc) {
    case 2:
        ctx = origctx;
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 4:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    string = argv[argc - 1];

    xchat_set_context(ph, ctx);
    xchat_commandf(ph, "RAW %s", string);
    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_getinfo(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    BADARGS(2, 2, " id");
    Tcl_AppendResult(irp, xchat_get_info(ph, argv[1]), NULL);
    return TCL_OK;
}

static int tcl_prefs(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    int i;
    const char *str;

    BADARGS(2, 2, " name");

    switch (xchat_get_prefs (ph, argv[1], &str, &i)) {
        case 1:
            Tcl_AppendResult(irp, str, NULL);
            break;
        case 2:
        case 3:
            Tcl_AppendResult(irp, myitoa(i), NULL);
            break;
        default:
            Tcl_AppendResult(irp, NULL);
    }

    return TCL_OK;
}

static int tcl_server(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx;
    BADARGS(1, 2, " ?server|network|context?");

    origctx = xchat_get_context(ph);

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    Tcl_AppendResult(irp, xchat_get_info(ph, "server"), NULL);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_channel(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx;
    BADARGS(1, 2, " ?server|network|context?");

    origctx = xchat_get_context(ph);

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    Tcl_AppendResult(irp, xchat_get_info(ph, "channel"), NULL);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_version(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    BADARGS(1, 1, "");
    Tcl_AppendResult(irp, xchat_get_info(ph, "version"), NULL);
    return TCL_OK;
}

static int tcl_xchatdir(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    BADARGS(1, 1, "");
    Tcl_AppendResult(irp, xchat_get_info(ph, "xchatdir"), NULL);
    return TCL_OK;
}

static int tcl_getlist(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_list *list = NULL;
    const char *name = NULL;
    const char **fields;
    const char *field;
    const char *sattr;
    int iattr;
    int i;
    Tcl_DString ds;
    xchat_context *origctx;
    xchat_context *ctx = NULL;

    origctx = xchat_get_context(ph);

    BADARGS(1, 2, " list");

    Tcl_DStringInit(&ds);

    fields = xchat_list_fields(ph, "lists");

    if (argc == 1) {
        for (i = 0; fields[i] != NULL; i++) {
            Tcl_DStringAppendElement(&ds, fields[i]);
        }
        goto done;
    }

    for (i = 0; fields[i] != NULL; i++) {
        if (strcmp(fields[i], argv[1]) == 0) {
            name = fields[i];
            break;
        }
    }

    if (name == NULL)
        goto done;

    list = xchat_list_get(ph, name);
    if (list == NULL)
        goto done;

    fields = xchat_list_fields(ph, name);

    Tcl_DStringStartSublist(&ds);
    for (i = 0; fields[i] != NULL; i++) {
        field = fields[i] + 1;
        Tcl_DStringAppendElement(&ds, field);
    }
    Tcl_DStringEndSublist(&ds);

    while (xchat_list_next(ph, list)) {

        Tcl_DStringStartSublist(&ds);

        for (i = 0; fields[i] != NULL; i++) {

            field = fields[i] + 1;

            switch (fields[i][0]) {
            case 's':
                sattr = xchat_list_str(ph, list, (char *) field);
                Tcl_DStringAppendElement(&ds, sattr);
                break;
            case 'i':
                iattr = xchat_list_int(ph, list, (char *) field);
                Tcl_DStringAppendElement(&ds, myitoa((long)iattr));
                break;
            case 'p':
                sattr = xchat_list_str(ph, list, (char *) field);
                if (strcmp(field, "context") == 0) {
                    ctx = (xchat_context *) sattr;
                    Tcl_DStringAppendElement(&ds, myitoa((long)ctx));
                } else
                    Tcl_DStringAppendElement(&ds, "*");
                break;
            default:
                Tcl_DStringAppendElement(&ds, "*");
                break;
            }
        }

        Tcl_DStringEndSublist(&ds);

    }

    xchat_list_free(ph, list);

  done:

    xchat_set_context(ph, origctx);

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    return TCL_OK;
}

/*
 * tcl_xchat_puts - stub for tcl puts command
 * This is modified from the original internal "puts" command.  It redirects
 * stdout to the current context, while still allowing all normal puts features
 */

static int tcl_xchat_puts(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    Tcl_Channel chan;
    char *string;
    int newline;
    char *channelId = NULL;
    int result;
    int mode;
    int trap_stdout = 0;

    switch (argc) {

    case 2:
        string = argv[1];
        newline = 1;
        trap_stdout = 1;
        break;

    case 3:
        if (strcmp(argv[1], "-nonewline") == 0) {
            newline = 0;
            trap_stdout = 1;
        } else {
            newline = 1;
            channelId = argv[1];
        }
        string = argv[2];
        break;

    case 4:
        if (strcmp(argv[1], "-nonewline") == 0) {
            channelId = argv[2];
            string = argv[3];
        } else {
            if (strcmp(argv[3], "nonewline") != 0) {
                Tcl_AppendResult(interp, "bad argument \"", argv[3], "\": should be \"nonewline\"", (char *) NULL);
                return TCL_ERROR;
            }
            channelId = argv[1];
            string = argv[2];
        }
        newline = 0;
        break;

    default:
        Tcl_AppendResult(interp, argv, "?-nonewline? ?channelId? string", NULL);
        return TCL_ERROR;
    }

    if (!trap_stdout && (strcmp(channelId, "stdout") == 0))
        trap_stdout = 1;

    if (trap_stdout) {
        if (newline)
            xchat_printf(ph, "%s\n", string);
        else
            xchat_print(ph, string);
        return TCL_OK;
    }

    chan = Tcl_GetChannel(interp, channelId, &mode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((mode & TCL_WRITABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", channelId, "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    }

    result = Tcl_Write(chan, string, strlen(string));
    if (result < 0) {
        goto error;
    }
    if (newline != 0) {
        result = Tcl_WriteChars(chan, "\n", 1);
        if (result < 0) {
            goto error;
        }
    }
    return TCL_OK;

  error:
    Tcl_AppendResult(interp, "error writing \"", channelId, "\": ", Tcl_PosixError(interp), (char *) NULL);

    return TCL_ERROR;
}

static int tcl_print(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx;
    xchat_context *ctx = NULL;
    char *string = NULL;

    BADARGS(2, 4, " ?server|network|context? ?#channel|nick? text");

    origctx = xchat_get_context(ph);

    switch (argc) {
    case 2:
        ctx = origctx;
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 4:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    string = argv[argc - 1];

    xchat_set_context(ph, ctx);
    xchat_print(ph, string);
    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_setcontext(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *ctx = NULL;

    BADARGS(2, 2, " context");

    ctx = xchat_smart_context(argv[1], NULL);

    CHECKCTX(ctx);

    xchat_set_context(ph, ctx);

    return TCL_OK;
}

static int tcl_findcontext(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *ctx = NULL;

    BADARGS(1, 3, " ?server|network|context? ?channel?");

    switch (argc) {
    case 1:
        ctx = xchat_find_context(ph, NULL, NULL);
        break;
    case 2:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    Tcl_AppendResult(irp, myitoa((long)ctx), NULL);

    return TCL_OK;
}

static int tcl_getcontext(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *ctx = NULL;

    BADARGS(1, 1, "");

    ctx = xchat_get_context(ph);

    Tcl_AppendResult(irp, myitoa((long)ctx), NULL);

    return TCL_OK;
}

static int tcl_channels(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    char *server, *channel;
    xchat_list *list;
    Tcl_DString ds;
    xchat_context *origctx;
    xchat_context *ctx;

    origctx = xchat_get_context(ph);

    BADARGS(1, 2, " ?server|network|context?");

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    server = (char *) xchat_get_info(ph, "server");

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "channels");

    if (list != NULL) {
        while (xchat_list_next(ph, list)) {
            if (xchat_list_int(ph, list, "type") != 2)
                continue;
            if (strcasecmp(server, xchat_list_str(ph, list, "server")) != 0)
                continue;
            channel = xchat_list_str(ph, list, "channel");
            Tcl_DStringAppendElement(&ds, channel);
        }
        xchat_list_free(ph, list);
    }


    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_servers(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    char *server;
    xchat_list *list;
    Tcl_DString ds;

    BADARGS(1, 1, "");

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "channels");

    if (list != NULL) {
        while (xchat_list_next(ph, list)) {
            if (xchat_list_int(ph, list, "type") == 1) {
                server = xchat_list_str(ph, list, "server");
                Tcl_DStringAppendElement(&ds, server);
            }
        }
        xchat_list_free(ph, list);
    }

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_queries(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    char *server, *channel;
    xchat_list *list;
    Tcl_DString ds;
    xchat_context *origctx;
    xchat_context *ctx;

    origctx = xchat_get_context(ph);

    BADARGS(1, 2, " ?server|network|context?");

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    server = (char *) xchat_get_info(ph, "server");

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "channels");

    if (list != NULL) {
        while (xchat_list_next(ph, list)) {
            if (xchat_list_int(ph, list, "type") != 3)
                continue;
            if (strcasecmp(server, xchat_list_str(ph, list, "server")) != 0)
                continue;
            channel = xchat_list_str(ph, list, "channel");
            Tcl_DStringAppendElement(&ds, channel);
        }
        xchat_list_free(ph, list);
    }

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_users(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx = NULL;
    xchat_list *list;
    Tcl_DString ds;

    BADARGS(1, 3, " ?server|network|context? ?channel?");

    origctx = xchat_get_context(ph);

    switch (argc) {
    case 1:
        ctx = origctx;
        break;
    case 2:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    xchat_set_context(ph, ctx);

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "users");

    if (list != NULL) {

        Tcl_DStringStartSublist(&ds);
        Tcl_DStringAppendElement(&ds, "nick");
        Tcl_DStringAppendElement(&ds, "host");
        Tcl_DStringAppendElement(&ds, "prefix");
        Tcl_DStringEndSublist(&ds);

        while (xchat_list_next(ph, list)) {
            Tcl_DStringStartSublist(&ds);
            Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "nick"));
            Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "host"));
            Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "prefix"));
            Tcl_DStringEndSublist(&ds);
        }

        xchat_list_free(ph, list);
    }

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_chats(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_list *list;
    Tcl_DString ds;

    BADARGS(1, 1, "");

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "dcc");

    if (list != NULL) {
        while (xchat_list_next(ph, list)) {
            switch (xchat_list_int(ph, list, "type")) {
            case 2:
            case 3:
                if (xchat_list_int(ph, list, "status") == 1)
                    Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "nick"));
                break;
            }
        }
        xchat_list_free(ph, list);
    }

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_ignores(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_list *list;
    Tcl_DString ds;
    int flags;

    BADARGS(1, 1, "");

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "ignore");

    if (list != NULL) {

        while (xchat_list_next(ph, list)) {
            Tcl_DStringStartSublist(&ds);
            Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "mask"));
            Tcl_DStringStartSublist(&ds);
            flags = xchat_list_int(ph, list, "flags");
            if (flags & 1)
                Tcl_DStringAppendElement(&ds, "PRIVMSG");
            if (flags & 2)
                Tcl_DStringAppendElement(&ds, "NOTICE");
            if (flags & 4)
                Tcl_DStringAppendElement(&ds, "CHANNEL");
            if (flags & 8)
                Tcl_DStringAppendElement(&ds, "CTCP");
            if (flags & 16)
                Tcl_DStringAppendElement(&ds, "INVITE");
            if (flags & 32)
                Tcl_DStringAppendElement(&ds, "UNIGNORE");
            if (flags & 64)
                Tcl_DStringAppendElement(&ds, "NOSAVE");
            Tcl_DStringEndSublist(&ds);
            Tcl_DStringEndSublist(&ds);
        }
        xchat_list_free(ph, list);
    }

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_dcclist(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_list *list;
    Tcl_DString ds;
    int dcctype;

    BADARGS(1, 1, "");

    Tcl_DStringInit(&ds);

    list = xchat_list_get(ph, "dcc");

    if (list != NULL) {

        while (xchat_list_next(ph, list)) {
            Tcl_DStringStartSublist(&ds);
            dcctype = xchat_list_int(ph, list, "type");
            switch (dcctype) {
            case 0:
                Tcl_DStringAppendElement(&ds, "filesend");
                break;
            case 1:
                Tcl_DStringAppendElement(&ds, "filerecv");
                break;
            case 2:
                Tcl_DStringAppendElement(&ds, "chatrecv");
                break;
            case 3:
                Tcl_DStringAppendElement(&ds, "chatsend");
                break;
            }
            switch (xchat_list_int(ph, list, "status")) {
            case 0:
                Tcl_DStringAppendElement(&ds, "queued");
                break;
            case 1:
                Tcl_DStringAppendElement(&ds, "active");
                break;
            case 2:
                Tcl_DStringAppendElement(&ds, "failed");
                break;
            case 3:
                Tcl_DStringAppendElement(&ds, "done");
                break;
            case 4:
                Tcl_DStringAppendElement(&ds, "connecting");
                break;
            case 5:
                Tcl_DStringAppendElement(&ds, "aborted");
                break;
            }

            Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "nick"));

            switch (dcctype) {
            case 0:
                Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "file"));
                break;
            case 1:
                Tcl_DStringAppendElement(&ds, (const char *) xchat_list_str(ph, list, "destfile"));
                break;
            }

            Tcl_DStringAppendElement(&ds, myitoa((long)xchat_list_int(ph, list, "size")));
            Tcl_DStringAppendElement(&ds, myitoa((long)xchat_list_int(ph, list, "resume")));
            Tcl_DStringAppendElement(&ds, myitoa((long)xchat_list_int(ph, list, "pos")));
            Tcl_DStringAppendElement(&ds, myitoa((long)xchat_list_int(ph, list, "cps")));
            Tcl_DStringAppendElement(&ds, myitoa((long)xchat_list_int(ph, list, "address32")));
            Tcl_DStringAppendElement(&ds, myitoa((long)xchat_list_int(ph, list, "port")));
            Tcl_DStringEndSublist(&ds);
        }
        xchat_list_free(ph, list);
    }

    Tcl_AppendResult(irp, ds.string, NULL);

    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_away(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx;
    BADARGS(1, 2, " ?server|network|context?");

    origctx = xchat_get_context(ph);

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    Tcl_AppendResult(irp, xchat_get_info(ph, "away"), NULL);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_host(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx;
    BADARGS(1, 2, " ?server|network|context?");

    origctx = xchat_get_context(ph);

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    Tcl_AppendResult(irp, xchat_get_info(ph, "host"), NULL);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_network(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx;
    BADARGS(1, 2, " ?server|network|context?");

    origctx = xchat_get_context(ph);

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    Tcl_AppendResult(irp, xchat_get_info(ph, "network"), NULL);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_me(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx;
    BADARGS(1, 2, " ?server|network|context?");

    origctx = xchat_get_context(ph);

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        xchat_set_context(ph, ctx);
    }

    Tcl_AppendResult(irp, xchat_get_info(ph, "nick"), NULL);

    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_topic(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    xchat_context *origctx, *ctx = NULL;
    BADARGS(1, 3, " ?server|network|context? ?channel?");

    origctx = xchat_get_context(ph);

    switch (argc) {
    case 1:
        ctx = origctx;
        break;
    case 2:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    xchat_set_context(ph, ctx);
    Tcl_AppendResult(irp, xchat_get_info(ph, "topic"), NULL);
    xchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_xchat_nickcmp(ClientData cd, Tcl_Interp * irp, int argc, char *argv[])
{
    BADARGS(3, 3, " string1 string2");
    Tcl_AppendResult(irp, myitoa((long)xchat_nickcmp(ph, argv[1], argv[2])), NULL);
    return TCL_OK;
}

static int Command_Alias(char *word[], char *word_eol[], void *userdata)
{
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    xchat_context *origctx;
    int dummy;
    char *string;
    char *errorInfo;

    if (complete_level == MAX_COMPLETES)
        return XCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = XCHAT_EAT_ALL;
    complete[complete_level].result = XCHAT_EAT_NONE;

    string = StrDup(word[1], &dummy);

    Tcl_UtfToUpper(string);

    if ((entry = Tcl_FindHashEntry(&aliasTablePtr, string)) != NULL) {
        aliasPtr = Tcl_GetHashValue(entry);
        origctx = xchat_get_context(ph);
        if (EvalInternalProc(aliasPtr->procPtr, 2, string, word_eol[2]) == TCL_ERROR) {
            errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
            xchat_printf(ph, "\0039Tcl plugin\003\tERROR (alias %s) %s \n", string, errorInfo);
        }
        xchat_set_context(ph, origctx);
    }

    Tcl_Free(string);

    return complete[complete_level--].result;
}

static int Null_Command_Alias(char *word[], char *word_eol[], void *userdata)
{
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    xchat_context *origctx;
    int dummy;
    char *channel;
    char *string;
    Tcl_DString ds;
    char *errorInfo;
    static int recurse = 0;

    if (recurse)
        return XCHAT_EAT_NONE;

    if (complete_level == MAX_COMPLETES)
        return XCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = XCHAT_EAT_ALL;
    complete[complete_level].result = XCHAT_EAT_NONE;

    recurse++;

    channel = xchat_get_info(ph, "channel");
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "@", 1);
    Tcl_DStringAppend(&ds, channel, strlen(channel));
    string = StrDup(ds.string, &dummy);
    Tcl_DStringFree(&ds);

    Tcl_UtfToUpper(string);

    if ((entry = Tcl_FindHashEntry(&aliasTablePtr, string)) != NULL) {
        aliasPtr = Tcl_GetHashValue(entry);
        origctx = xchat_get_context(ph);
        if (EvalInternalProc(aliasPtr->procPtr, 2, string, word_eol[1]) == TCL_ERROR) {
            errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
            xchat_printf(ph, "\0039Tcl plugin\003\tERROR (alias %s) %s \n", string, errorInfo);
        }
        xchat_set_context(ph, origctx);
    }

    Tcl_Free(string);

    recurse--;

    return complete[complete_level--].result;
}

static int Command_TCL(char *word[], char *word_eol[], void *userdata)
{
    char *errorInfo;
    if (Tcl_Eval(interp, word_eol[2]) == TCL_ERROR) {
        errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
        xchat_printf(ph, "\0039Tcl plugin\003\tERROR: %s\n", errorInfo);
    } else
        xchat_printf(ph, "\0039Tcl plugin\003\tRESULT: %s\n", Tcl_GetStringResult(interp));
    return XCHAT_EAT_ALL;
}

static int Command_Source(char *word[], char *word_eol[], void *userdata)
{
    char *xchatdir;
    Tcl_DString ds;
    struct stat dummy;
    int len;
    char *errorInfo;

    if (!strlen(word_eol[2]))
        return XCHAT_EAT_NONE;

    len = strlen(word[2]);

    if (len > 4 && strcasecmp(".tcl", word[2] + len - 4) == 0) {

        xchatdir = xchat_get_info(ph, "xchatdir");

        Tcl_DStringInit(&ds);

        if (stat(word_eol[2], &dummy) == 0) {
            Tcl_DStringAppend(&ds, word_eol[2], strlen(word_eol[2]));
        } else {
            if (!strchr(word_eol[2], '/')) {
                Tcl_DStringAppend(&ds, xchatdir, strlen(xchatdir));
                Tcl_DStringAppend(&ds, "/", 1);
                Tcl_DStringAppend(&ds, word_eol[2], strlen(word_eol[2]));
            }
        }

        if (Tcl_EvalFile(interp, ds.string) == TCL_ERROR) {
            errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
            xchat_printf(ph, "\0039Tcl plugin\003\tERROR: %s\n", errorInfo);
        } else
            xchat_printf(ph, "\0039Tcl plugin\003\tSourced %s\n", ds.string);

        Tcl_DStringFree(&ds);

        return XCHAT_EAT_XCHAT;

    } else
        return XCHAT_EAT_NONE;

}

static int Command_Reload(char *word[], char *word_eol[], void *userdata)
{
    Tcl_Plugin_DeInit();
    Tcl_Plugin_Init();
    xchat_print(ph, "\0039Tcl plugin\003\tRehashed\n");
    return XCHAT_EAT_ALL;
}

static int TCL_Event_Handler(void *userdata)
{
    Tcl_DoOneEvent(TCL_DONT_WAIT);
    do_timer();
    return 1;
}

static void Tcl_Plugin_Init()
{
    int x;
    char *xchatdir;

    interp = Tcl_CreateInterp();

    Tcl_FindExecutable(NULL);

    Tcl_Init(interp);

    nextprocid = 0x1000;

    Tcl_CreateCommand(interp, "alias", tcl_alias, NULL, NULL);
    Tcl_CreateCommand(interp, "away", tcl_away, NULL, NULL);
    Tcl_CreateCommand(interp, "channel", tcl_channel, NULL, NULL);
    Tcl_CreateCommand(interp, "channels", tcl_channels, NULL, NULL);
    Tcl_CreateCommand(interp, "chats", tcl_chats, NULL, NULL);
    Tcl_CreateCommand(interp, "command", tcl_command, NULL, NULL);
    Tcl_CreateCommand(interp, "complete", tcl_complete, NULL, NULL);
    Tcl_CreateCommand(interp, "dcclist", tcl_dcclist, NULL, NULL);
    Tcl_CreateCommand(interp, "findcontext", tcl_findcontext, NULL, NULL);
    Tcl_CreateCommand(interp, "getcontext", tcl_getcontext, NULL, NULL);
    Tcl_CreateCommand(interp, "getinfo", tcl_getinfo, NULL, NULL);
    Tcl_CreateCommand(interp, "getlist", tcl_getlist, NULL, NULL);
    Tcl_CreateCommand(interp, "host", tcl_host, NULL, NULL);
    Tcl_CreateCommand(interp, "ignores", tcl_ignores, NULL, NULL);
    Tcl_CreateCommand(interp, "killtimer", tcl_killtimer, NULL, NULL);
    Tcl_CreateCommand(interp, "me", tcl_me, NULL, NULL);
    Tcl_CreateCommand(interp, "network", tcl_network, NULL, NULL);
    Tcl_CreateCommand(interp, "on", tcl_on, NULL, NULL);
    Tcl_CreateCommand(interp, "off", tcl_off, NULL, NULL);
    Tcl_CreateCommand(interp, "nickcmp", tcl_xchat_nickcmp, NULL, NULL);
    Tcl_CreateCommand(interp, "print", tcl_print, NULL, NULL);
    Tcl_CreateCommand(interp, "prefs", tcl_prefs, NULL, NULL);
    Tcl_CreateCommand(interp, "::puts", tcl_xchat_puts, NULL, NULL);
    Tcl_CreateCommand(interp, "queries", tcl_queries, NULL, NULL);
    Tcl_CreateCommand(interp, "raw", tcl_raw, NULL, NULL);
    Tcl_CreateCommand(interp, "server", tcl_server, NULL, NULL);
    Tcl_CreateCommand(interp, "servers", tcl_servers, NULL, NULL);
    Tcl_CreateCommand(interp, "setcontext", tcl_setcontext, NULL, NULL);
    Tcl_CreateCommand(interp, "tab", tcl_channel, NULL, NULL);
    Tcl_CreateCommand(interp, "timer", tcl_timer, NULL, NULL);
    Tcl_CreateCommand(interp, "timerexists", tcl_timerexists, NULL, NULL);
    Tcl_CreateCommand(interp, "timers", tcl_timers, NULL, NULL);
    Tcl_CreateCommand(interp, "topic", tcl_topic, NULL, NULL);
    Tcl_CreateCommand(interp, "users", tcl_users, NULL, NULL);
    Tcl_CreateCommand(interp, "version", tcl_version, NULL, NULL);
    Tcl_CreateCommand(interp, "xchatdir", tcl_xchatdir, NULL, NULL);

    Tcl_InitHashTable(&cmdTablePtr, TCL_STRING_KEYS);
    Tcl_InitHashTable(&aliasTablePtr, TCL_STRING_KEYS);

    for (x = 0; x < MAX_TIMERS; x++) {
        timers[x].timerid = 0;
        timers[x].procPtr = NULL;
    }

    for (x = 0; x < XC_SIZE; x++)
        xc[x].hook = NULL;

    xchatdir = xchat_get_info(ph, "xchatdir");

    if (Tcl_Eval(interp, unknown) == TCL_ERROR) {
        xchat_printf(ph, "Error sourcing internal 'unknown' (%s)\n", Tcl_GetStringResult(interp));
    }

    if (Tcl_Eval(interp, sourcedirs) == TCL_ERROR) {
        xchat_printf(ph, "Error sourcing internal 'sourcedirs' (%s)\n", Tcl_GetStringResult(interp));
    }

    if (Tcl_Eval(interp, inlinetcl) == TCL_ERROR) {
        xchat_printf(ph, "Error sourcing internal 'inlinetcl' (%s)\n", Tcl_GetStringResult(interp));
    }
}

static void Tcl_Plugin_DeInit()
{
    int x;
    char *procPtr;
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;

    /* Be sure to free all the memory for ON and ALIAS entries */

    entry = Tcl_FirstHashEntry(&cmdTablePtr, &search);
    while (entry != NULL) {
        procPtr = Tcl_GetHashValue(entry);
        Tcl_Free(procPtr);
        entry = Tcl_NextHashEntry(&search);
    }

    Tcl_DeleteHashTable(&cmdTablePtr);

    entry = Tcl_FirstHashEntry(&aliasTablePtr, &search);
    while (entry != NULL) {
        aliasPtr = Tcl_GetHashValue(entry);
        Tcl_Free(aliasPtr->procPtr);
        if (aliasPtr->hook)
            xchat_unhook(ph, aliasPtr->hook);
        Tcl_Free((char *) aliasPtr);
        entry = Tcl_NextHashEntry(&search);
    }

    Tcl_DeleteHashTable(&aliasTablePtr);

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid) {
            timers[x].timerid = 0;
            if (timers[x].procPtr != NULL)
                Tcl_Free(timers[x].procPtr);
            timers[x].procPtr = NULL;
            break;
        }
    }

    for (x = 0; x < XC_SIZE; x++) {
        if (xc[x].hook != NULL) {
            xchat_unhook(ph, xc[x].hook);
            xc[x].hook = NULL;
        }
    }

    Tcl_DeleteInterp(interp);
}

static void banner()
{
    xchat_printf(ph, "Tcl plugin for XChat - Version %s\n", VERSION);
    xchat_print(ph, "Copyright 2002-2003 Daniel P. Stasinski\n");
    xchat_print(ph, "http://www.scriptkitties.com/tclplugin/\n");
}

int xchat_plugin_init(xchat_plugin * plugin_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char *arg)
{
#ifdef WIN32
    HINSTANCE lib;
#endif

    ph = plugin_handle;

#ifdef WIN32
    lib = LoadLibraryA(TCL_DLL);
    if (!lib) {
        xchat_print(ph, "You must have ActiveTCL installed in order to run Tcl scripts.\n" "http://aspn.activestate.com/ASPN/Tcl/\n" "Make sure Tcl's bin directory is in your PATH.\n\n");
        return 0;
    }
    FreeLibrary(lib);
#endif

    if (initialized != 0) {
        banner();
        xchat_print(ph, "Tcl plugin already loaded");
        reinit_tried++;
        return 0;
    }
    initialized = 1;

    *plugin_name = "Tcl";
    *plugin_desc = "Tcl scripting interface";
    *plugin_version = VERSION;

    Tcl_Plugin_Init();

    raw_line_hook = xchat_hook_server(ph, "RAW LINE", XCHAT_PRI_NORM, Server_raw_line, NULL);
    Command_TCL_hook = xchat_hook_command(ph, "tcl", XCHAT_PRI_NORM, Command_TCL, 0, 0);
    Command_Source_hook = xchat_hook_command(ph, "source", XCHAT_PRI_NORM, Command_Source, 0, 0);
    Command_Reload_hook = xchat_hook_command(ph, "Reload", XCHAT_PRI_NORM, Command_Reload, 0, 0);
    Command_Load_hook = xchat_hook_command(ph, "LOAD", XCHAT_PRI_NORM, Command_Source, 0, 0);
    Event_Handler_hook = xchat_hook_timer(ph, 100, TCL_Event_Handler, 0);
    Null_Command_hook = xchat_hook_command(ph, "", XCHAT_PRI_NORM, Null_Command_Alias, "", 0);

    banner();
    xchat_print(ph, "Tcl interface loaded\n");

    return 1;                   /* return 1 for success */
}

int xchat_plugin_deinit()
{
    if (reinit_tried) {
        reinit_tried--;
        return 1;
    }

    xchat_unhook(ph, raw_line_hook);
    xchat_unhook(ph, Command_TCL_hook);
    xchat_unhook(ph, Command_Source_hook);
    xchat_unhook(ph, Command_Reload_hook);
    xchat_unhook(ph, Command_Load_hook);
    xchat_unhook(ph, Event_Handler_hook);
    xchat_unhook(ph, Null_Command_hook);

    Tcl_Plugin_DeInit();

    xchat_print(ph, "Tcl interface unloaded\n");
    initialized = 0;

    return 1;
}
