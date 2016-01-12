#!/usr/bin/env python
# Copyright (c) 2010 Stanford University
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR(S) DISCLAIM ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL AUTHORS BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""Misc utilities and variables for Python scripts."""

import commands
import contextlib
import os
import random
import re
import shlex
import signal
import subprocess
import sys
import time

__all__ = ['sh', 'captureSh', 'Sandbox', 'getDumpstr', 'getHosts', 'getOldMasterHost', 'getClientHost']

def sh(command, bg=False, **kwargs):
    """Execute a local command."""

    kwargs['shell'] = True
    if bg:
        return subprocess.Popen(command, **kwargs)
    else:
        subprocess.check_call(command, **kwargs)

def captureSh(command, **kwargs):
    """Execute a local command and capture its output."""

    kwargs['shell'] = True
    kwargs['stdout'] = subprocess.PIPE
    p = subprocess.Popen(command, **kwargs)
    output = p.communicate()[0]
    if p.returncode:
        raise subprocess.CalledProcessError(p.returncode, command)
    if output.count('\n') and output[-1] == '\n':
        return output[:-1]
    else:
        return output

class Sandbox(object):
    """A context manager for launching and cleaning up remote processes."""
    class Process(object):
        def __init__(self, host, command, kwargs, sonce, proc,
                     ignoreFailures, kill_on_exit, server_process):

            self.host = host
            self.command = command
            self.kwargs = kwargs
            self.sonce = sonce
            self.proc = proc
            self.ignoreFailures = ignoreFailures
            self.kill_on_exit = kill_on_exit
            self.server_process = server_process

        def __repr__(self):
            return repr(self.__dict__)

    def __init__(self, cleanup=True):
        # cleanup indicates whether this this Sandbox needs to clean up
        # processes that are currently running as part of this run of
        # clusterperf or not.
        self.processes = []
        self.cleanup = cleanup

    def rsh(self, host, command, locator=None, ignoreFailures=False,
            is_server=False, kill_on_exit=True, bg=False, **kwargs):

        """Execute a remote command.

        @return: If bg is True then a Process corresponding to the command
                 which was run, otherwise None.
        """
        checkHost(host)
        if bg:
            sonce = ''.join([chr(random.choice(range(ord('a'), ord('z'))))
                             for c in range(8)])

            server_process = is_server

            if is_server:
                # Assumes scripts are at same path on remote machine
                sh_command = ['ssh', host,
                              '%s/serverexec' % scripts_path,
                              host, os.getcwd(), "'%s'" % locator,
                              "'%s'" % command]
            else:
                # Assumes scripts are at same path on remote machine
                sh_command = ['ssh', host,
                              '%s/regexec' % scripts_path, sonce,
                              os.getcwd(), "'%s'" % command]

            p = subprocess.Popen(sh_command, **kwargs)
            process = self.Process(host, command, kwargs, sonce,
                                   p, ignoreFailures, kill_on_exit,
                                   server_process)

            self.processes.append(process)
            return process
        else:
            sh_command = ['ssh', host,
                          '%s/remoteexec.py' % scripts_path,
                          "'%s'" % command, os.getcwd()]
            subprocess.check_call(sh_command, **kwargs)
            return None

    def kill(self, process):
        """Kill a remote process started with rsh().

        @param process: A Process corresponding to the command to kill which
                        was created with rsh().
        """
        killer = subprocess.Popen(['ssh', process.host,
                                   '%s/killpid' % scripts_path,
                                    process.sonce])
        killer.wait()
        try:
            process.proc.kill()
        except:
            pass
        process.proc.wait()
        self.processes.remove(process)

    def restart(self, process):
        self.kill(process)
        self.rsh(process.host, process.command, process.ignoreFailures, True, **process.kwargs)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, exc_tb):
        with delayedInterrupts():
            killers = []
            for p in self.processes:
                # If this sandbox does not require a cleanup of its processes
                # now, don't do it. Currently only servers are started in the
                # context of such objects. They will be reaped later by some
                # object in whose context, this object was created.
                if not self.cleanup:
                    to_kill = '0'
                    killers.append(subprocess.Popen(['ssh', p.host,
                                        '%s/killserver' % scripts_path,
                                        to_kill, os.getcwd(), p.host]))
                # invoke killpid only for processes that are not servers.
                # server processes will be killed by killserver outside this
                # loop below.
                elif not p.server_process:
                    # Assumes scripts are at same path on remote machine
                    killers.append(subprocess.Popen(['ssh', p.host,
                                                     '%s/killpid' % scripts_path,
                                                     p.sonce]))

            if self.cleanup:
                chost = getHosts()[-1] # coordinator
                killers.append(subprocess.Popen(['ssh', chost[0],
                                    '%s/killcoord' % scripts_path]))

                path = '%s/logs/shm' % os.getcwd()
                files = ""
                try:
                    files = sorted([f for f in os.listdir(path)
                        if os.path.isfile( os.path.join(path, f) )])
                except:
                    pass

                # kill all the servers that are running
                for mhost in files:
                    if mhost != 'README' and not mhost.startswith("cluster"):
                        to_kill = '1'
                        killers.append(subprocess.Popen(['ssh', mhost.split('_')[0],
                                            '%s/killserver' % scripts_path,
                                            to_kill, os.getcwd(), mhost]))
                try:
                    os.remove('%s/logs/shm/README' % os.getcwd())
                    # remove the file that represents the name of the cluster.
                    # This is used so that new backups can be told whether
                    # or not to read data from their disks

                    for fname in os.listdir(path):
                        if fname.startswith("cluster"):
                            os.remove(os.path.join(path, fname))
                except:
                    pass

            for killer in killers:
                killer.wait()
        # a half-assed attempt to clean up zombies
        for p in self.processes:
            try:
                p.proc.kill()
            except:
                pass
            p.proc.wait()

    def checkFailures(self):
        """Raise exception if any process has exited with a non-zero status."""
        for p in self.processes:
            if (p.ignoreFailures == False):
                rc = p.proc.poll()
                if rc is not None and rc != 0:
                    # raise subprocess.CalledProcessError(rc, p.command)
                    # don't raise exception because the test may involve intentional
                    # crashing of the coordinator or master/backup servers
                    pass

@contextlib.contextmanager
def delayedInterrupts():
    """Block SIGINT and SIGTERM temporarily."""
    quit = []
    def delay(sig, frame):
        if quit:
            print ('Ctrl-C: Quitting during delayed interrupts section ' +
                   'because user insisted')
            raise KeyboardInterrupt
        else:
            quit.append((sig, frame))
    sigs = [signal.SIGINT, signal.SIGTERM]
    prevHandlers = [signal.signal(sig, delay)
                    for sig in sigs]
    try:
        yield None
    finally:
        for sig, handler in zip(sigs, prevHandlers):
            signal.signal(sig, handler)
        if quit:
            raise KeyboardInterrupt(
                'Signal received while in delayed interrupts section')

# This stuff has to be here, rather than at the beginning of the file,
# because config needs some of the functions defined above.
from config import *
import config
__all__.extend(config.__all__)

def getDumpstr():
    """Returns an instance of Dumpstr for uploading reports.

    You should set dumpstr_base_url in your config file if you want to use this
    to upload reports. See the dumpstr README for more info. You might be able
    to find that README on the web at
    https://github.com/ongardie/dumpstr/blob/master/README.rst
    """

    from dumpstr import Dumpstr
    try:
        url = config.dumpstr_base_url
    except AttributeError:
        d = Dumpstr("")
        def error(*args, **kwargs):
            raise Exception(
                "You did not set your dumpstr_base_url "
                "in localconfig.py, so you can't upload reports.")
        d.upload_report = error
        return d
    else:
        return Dumpstr(url)

def atomHostList(hostIdsToUse):
    """ Helper routine for ATOM (Micro Modular Server) Cluster.

    Input a list of hostIds in 'hostIdsToUse' and
    returns a complete list 'atomHosts' with numerical addresses
    (ip, Mac, hostname, server number).
    
    If hostIdsToUse == 0 (empty), 
    it returns a list with all available hosts from the host database.
    """
    valid_entry = re.compile('^([\d.]+)\s+(atom(\d\d\d)[am]?)\s+#\s*(\S+)\s+')
    hosts_db = "/etc/hosts"
    host_info = {}
    file = open(hosts_db)
    for line in file:
        m = valid_entry.search(line)
        if m:
            (ip, host, host_id, mac) = m.group(1,2,3,4)
            host_id = int(host_id)
            if not host_id in host_info:
                host_info[host_id] = {}
            if re.search("a$", host):
                host_info[host_id]['mac']  = mac
            if re.search("\d$", host):
                host_info[host_id]['host'] = host
                host_info[host_id]['ip']   = ip

    if not hostIdsToUse:
        hostIdsToUse = sorted(list(host_info.keys()))
    # Create a complete host lists for micro moduler server
    atomHosts =[]
    for host_id in hostIdsToUse:
        info = host_info[int(host_id)]
        atomHosts.append((info['host'], info['ip'], host_id, info['mac']))
    return atomHosts
    
def getHosts():
    """Returns a list of the hosts available for servers or clients.

    A global valiable 'cluster_type' defined in config.py switches
       the designaged cluster type:
       rc_cluseter (default) and atom_cluster are supported.

    RCCluster : when cluster_type is rc_cluster or undefined.
     Each entry consists of a name for the host (for ssh), an IP address
     to use for creating service locators, and an id for generating
     Ethernet addresses.

    AtomCluster : when cluster_type is atom_cluster
     Each entry consists of a name for the host (for ssh), an IP address,
     an id for generating Ethernet addrss, and Ethernet Mac Address.

    By default, the function will return a list generated from servers
    locked by the current user in rcres (an RAMCloud internal utility).
    If rcres is not available, a custom list can be defined in
    localconfig.py (see below and the wiki for additional instructions).
    In the event that rcres is available and a custom list is defined,
    the function will validate the custom list against rcres.

    
    Example for constructing a custom list in localconfig.py:
    hosts = []
    for i in range(1, 61):
        hosts.append(('rc%02d' % i,
                      '192.168.1.%d' % (100 + i),
                      i))


    For Atom server, hosts are just a list of 3 digit id numbers:
    for i in range(1, 61):
        hosts.append(('%03d' % i)

    """
    # Find servers locked by user via rcres
    if config.cluster_type == 'atom_cluster':
        rcresOutput = commands.getoutput('mmres ls -l | grep "$(whoami)" | '
                             'cut -c13-19 | grep "atom[0-9]" | cut -c5-7')
    else:
        # rc cluster
        rcresOutput = commands.getoutput('rcres ls -l | grep "$(whoami)" | '
                             'cut -c13-16 | grep "rc[0-9]" | cut -c3-4')

    rcresFailed = re.match(".*not found.*", rcresOutput)

    # If hosts overridden in localconfig.py, check that all servers are locked
    if 'hosts' in globals():
        requstedUnlockedHosts = []
        if config.cluster_type == 'atom_cluster':
            for host in hosts:
                if str("%03d" % host) not in rcresOutput.split():
                    requstedUnlockedHosts.append(host)
        else:
            for host in hosts:
                if str("%02d" % host[2]) not in rcresOutput.split():
                    requstedUnlockedHosts.append(host[0])

        if not rcresFailed and len(requstedUnlockedHosts) > 0:
            raise Exception ("Manually defined hosts list in localconfig.py includes the "
                "following servers not locked by user in rcres:\r\n\t%s"
                             % requstedUnlockedHosts)

        return hosts
    # hosts has not been overridden, check that rcres has some servers for us
    else:
        if rcresFailed:
            raise Exception ("config.py could not invoke rcres (%s);\r\n"
                "\tplease specify a custom hosts list in scripts/localconfig.py" % rcresOutput)

        if len(rcresOutput) == 0:
            raise Exception ("config.py found 0 rcXX servers locked in rcres;\r\n"
                "\tcheck your locks or specify a custom hosts list in scripts/localconfig.py")

    # Everything checks out, build list
    serverList = []
    if config.cluster_type == 'atom_cluster':
        for hostNum in rcresOutput.split():
            i = int(hostNum)
            serverList.append('%03d' % i)
        serverList = atomHostList(serverList)
    else:
        for hostNum in rcresOutput.split():
            i = int(hostNum)
            serverList.append(('rc%02d' % i,
                         '192.168.1.%d' % (100 + i),
                         i))
    return serverList

def getOldMasterHost():
    """Returns old_master_host if defined in config.py or localconfig.py.
    Otherwise, returns the last server from getHosts()
    """
    if config.old_master_host:
        return config.old_master_host
    else:
        return getHosts()[-1]

def getClientHost():
    """Returns old_master_host if defined in config.py or localconfig.py.
    Otherwise, returns the second to the last server from getHosts()
    """
    if config.cluster_type == 'atom_cluster':
        return getHosts()[-2]
    else:
        return getOldMasterHost()

def checkHost(host):
    """Returns True when the host specified is either locked via rcres or in
    cases where rcres is unavailable, is specified in the user's
    scripts/localconfig.py. If both conditions are false, an exception is raised.
    This function is intended to be invoked as a check right before executing a
    command on the target server.
    """
    serverList = getHosts()
    for server in serverList:
        if host == server[0]:
            return True

    # Server was not found in the valid list, let's check what the problem may be
    if config.cluster_type == 'atom_cluster':
        rcres_cmd = 'mmres'
    else:
        rcres_cmd = 'rcres'

    rcresOutput = commands.getoutput('%s ls' % rcres_cmd)
    rcresFailed = re.match(".*not found.*", rcresOutput)
    if rcresFailed:
        raise Exception ("Attempted to use a host (%s) that is not present in the "
            "current user's scripts/localconfig.py" % host)
    else:
        raise Exception ("Attempted to use a host (%s) that is not locked by the "
            "current user in %s" % (host, rcres_cmd))
