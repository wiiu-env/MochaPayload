#!/usr/bin/env python3

import atexit
import os
import readline

import tcp_log_server

command_map = {
    "aroma": {
        "help": None,
        "plugins": {
            "details"    : None, # aroma plugins details
            "heap_usage" : None, # aroma plugins heap_usage
            "help"       : None, # aroma plugins help
            "list"       : None, # aroma plugins list
        },
    },
    "cos": {
        "bootperf"    : None, # cos bootperf
        "coretrace"   : None, # cos coretrace
        "cpuusage"    : None, # cos cpuusage [c] [p]
        "crashdump"   : {     # cos crashdump [0|1]
            "0": None,
            "1": None
        },
        "ctxdump"     : None, # cos ctxdump addr
        "debug"       : None, # cos debug
        "disasm"      : None, # cos disasm addr [words]
        "fault"       : None, # cos fault [cmd]
        "fslogdump"   : None, # cos fslogdump
        "getsym"      : None, # cos getsym addr
        "heapcheck"   : None, # cos heapcheck addr
        "heapdump"    : None, # cos heapdump addr
        "heaps"       : None, # cos heaps
        "help"        : None, # cos help
        "intstats"    : None, # cos intstats [coreId]
        "iodump"      : None, # cos iodump [coreId]
        "iostats"     : None, # cos iostats [coreId]
        "kill"        : None, # cos kill
        "killrestart" : None, # cos killrestart
        "kpanic"      : None, # cos kpanic
        "kstats"      : None, # cos kstats [coreId]
        "launch"      : None, # cos launch hi lo
        "launchex"    : None, # cos launchex tid [args]
        "logdump"     : None, # cos logdump
        "logsave"     : None, # cos logsave
        "memdump"     : None, # cos memdump addr [words]
        "modules"     : None, # cos modules
        "ospanic"     : None, # cos ospanic
        "osstats"     : None, # cos osstats [coreId]
        "runonthread" : None, # cos runonthread [r]
        "sdkversion"  : None, # cos sdkversion
        "setdabr"     : None, # cos setdabr addr [r] [w]
        "setfslog"    : {     # cos setfslog [0|1|2]
            "0": None,
            "1": None,
            "2": None,
        },
        "setiabr"     : None, # cos setiabr addr
        "setword"     : None, # cos setword addr value
        "slowlaunch"  : None, # cos slowlaunch tid [args]
        "stacktrace"  : None, # cos stacktrace addr
        "tcheck"      : None, # cos tcheck
        "threads"     : None, # cos threads
        "timestamp"   : {     # cos timestamp [1|2|3|4]
            "1": None,
            "2": None,
            "3": None,
            "4": None,
        },
        "ttrace"      : None, # cos ttrace
    },
    "iosu": {
        "help"     : None,
        "reboot"   : None,
        "reload"   : None,
        "shutdown" : None,
    },
}

def get_candidates_for(text, tokens):
    # first navigate using all tokens except the last
    node = command_map
    for token in tokens[:-1]:
        if token in node:
            node = node[token]
            if node is None:
                return []
        else: # if user typed garbage, return no candidates
            return []
    # all keys that start with text are candidates
    return [x for x in node.keys() if x.startswith(text)]

def iopshell_completer(text, state):
    tokens = readline.get_line_buffer().split(" ")
    candidates = get_candidates_for(text, tokens)
    if candidates and state < len(candidates):
        return candidates[state]
    return None

def prepare_readline():
    readline.read_init_file()
    readline.set_completer_delims(" ")
    readline.set_completer(iopshell_completer)
    # Keep a persistent command history.
    hist_filename = ".wiiu_shell_history"
    try:
        from xdgenvpy.xdgenv import XDG
        xdg = XDG()
        # place history file in ~/.config/
        hist_path = os.path.join(xdg.XDG_CONFIG_HOME, hist_filename)
    except:
        # fallback is local directory
        hist_path = hist_filename
    try:
        print(f"Saving readline history in {hist_path}")
        readline.read_history_file(hist_path)
        readline.set_history_length(1000)
    except FileNotFoundError:
        pass
    atexit.register(readline.write_history_file, hist_path)
    readline.set_auto_history(True)

def main():
    prepare_readline()
    tcp_log_server.main()

if __name__ == "__main__":
    main()
