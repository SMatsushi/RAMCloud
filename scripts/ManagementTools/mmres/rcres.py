#! /usr/bin/python

"""RC Server Resource Lease Interface

Usage:
  rcres status [-l] [<cluster>]
  rcres lease <time> <server_ids>... [-m MSG]
  rcres unlease [<server_ids>...] 
  rcres permalock <server_ids>... [-m MSG]
  rcres unlock <ids>...
  rcres --help
  
  rcres ls [-l] [<cluster>]
  rcres l <time> <server_ids>... [-m MSG]
  rcres ul [<server_ids>...] 
  rcres pl <server_ids>... [-m MSG]
  rcres upl <ids>...

Arguments:
  <time>        Lease expiration time (ex: 6pm, 6:00pm, 18:00, 24h, 1d).
  <server_ids>  List of server ids to lease or lock; (ex: rc1-20, rcmonster).
  <ids>         List of ids to unlock; can be servers, users, or lockgroups.
  <cluster>     Name of cluster whoes status will be printed (ex: rc, misc)

Options:
  -h --help              Show this help message and exit.
  --version              Show version.
  -l --list              Print status in list view.
  -m, --message MSG      Add message to lock or lease [default: ].
"""

__author__ = "Collin Lee (cstlee)"
__version__ = "0.2"

from docopt import docopt
import pickle
from datetime import datetime, date, timedelta
from console import getTerminalSize
import math
import re
from getpass import getuser
from subprocess import call
from lockfile import LockFile
import time

RESFILE = '/usr/local/rcres/db/activeLeaseDB'
ADMINLIST = ['ongaro', 'stutsman', 'cstlee']

CANONFORMAT = {}
SERVERS = {}
CANONFORMAT['rc'] = "rc%02d"
SERVERS['rc'] = tuple([CANONFORMAT['rc'] % i for i in range(1,81)])
SERVERS['misc'] = tuple(['rcmaster', 'rcmonster', 'rcnfs'])

STATUS = {}
LOCKGROUPS = set()

def readResFile():
    global STATUS
    try:
        with open(RESFILE, 'r') as ifile:
            STATUS = pickle.load(ifile)
    except:
        pass

def flushResFile():
    try:
        with open(RESFILE, 'w') as ofile:
            pickle.dump(STATUS, ofile)
    except:
        print("ERROR: Unable to persist request.")

def printCompactStatus(cluster):
    serverList = SERVERS[cluster]
    idlen = max([len(str(s)) for s in serverList])
    formatString = "%d" % idlen
    formatString = "  %" + formatString + "s[ %s ]  "
    (width, height) = getTerminalSize()
    elemLen = 2 + idlen + 5 + 2
    columnCount = width / elemLen;
    columnCount = max(1, columnCount)
    listLen = int(math.ceil(len(serverList)/float(columnCount)))
    for i in range(listLen):
        line = ""
        for j in range(columnCount):
            index = i + j * listLen
            if index < len(serverList):
                serverId = serverList[index]
                token = " "
                if serverId in STATUS:
                    if STATUS[serverId][1] == 'PERMA-LOCKED':
                        token = 'P'
                    else:
                        token = 'L'
                line += formatString % (str(serverList[index]),token)
        print(line)

def printListStatus(cluster):
    serverList = SERVERS[cluster]
    idlen = max([len(str(s)) for s in serverList])
    formatString = "%d" % idlen
    formatString = "%10s  %" + formatString + "s  [%16.16s]  %s"
    for serverId in serverList:
        owner = ""
        expiration = ""
        message = ""
        if serverId in STATUS:
            (owner, expiration, message) = STATUS[serverId]
            if expiration == 'PERMA-LOCKED':
                expiration = 'PERMA-LOCKED  '
            else:
                expiration = str(expiration)
        print(formatString % (owner, serverId, expiration, message))

def printStatus(cluster, listMode):
    if listMode:
        printListStatus(cluster)
    else:
        printCompactStatus(cluster)
        
def status(listMode, cluster):
    printed = False
    
    if cluster:
        cluster = cluster.lower()
    
    if not cluster or cluster == "rc":
        # RCXX Machines
        print("RCXX Machines:")
        print("==============")
        printStatus("rc", listMode)
        print("")
        printed = True
    
    if not cluster or cluster == "misc":
        # MISC Machines
        print("MISC Machines:")
        print("==============")
        printStatus("misc", listMode)
        print("")
        printed = True
    
    if not printed:
        print("Unable to complete request: "
              "No cluster named %s." % cluster)

def resourceAvailable(serverId, user):
    for cluster in SERVERS:
        if serverId in SERVERS[cluster]:
            if serverId not in STATUS or STATUS[serverId][0] == user:
                return True
    return False

def leaseList(serverIds, owner, expiration, message):
    global STATUS
    unavailable = []
    for serverId in serverIds:
        if not resourceAvailable(serverId, owner):
            unavailable.append(serverId)
    if len(unavailable) > 0:
        print("Unable to complete request: "
              "The following servers are unavailable - %s." % str(unavailable))
        return
    for serverId in serverIds:
        STATUS[serverId] = (owner, expiration, message)
    print("ACQUIRED: %s" % serverIds) 

def cleanSTATUS():
    global STATUS
    global LOCKGROUPS
    expiredLeases = []
    now = datetime.now()
    for serverId in STATUS:
        (owner, expiration, msg) = STATUS[serverId]
        if expiration == 'PERMA-LOCKED':
            LOCKGROUPS.add(int(owner.lstrip('LG')))
        elif expiration < now:
            expiredLeases.append(serverId)
    for serverId in expiredLeases:
        del STATUS[serverId]
            
def unlockList(idList, user, force):
    global STATUS
    unlockList = []
    deniedList = []
    for serverId in STATUS:
        # Check if this is a unlock target
        if serverId in idList or STATUS[serverId][0] in idList:
            # Check if this user is prohibited from unlocking this target
            if STATUS[serverId][0] != user and not force:
                deniedList.append(serverId)
            else:
                unlockList.append(serverId)
    if len(deniedList) > 0:
        print("Unable to complete request: "
              "Permission to unlease the following servers denied - %s." % str(deniedList))
        return
    for serverId in unlockList:
        del STATUS[serverId]
    print("FREED: %s" % unlockList)

def parseServerIds(serverIds):
    serverIdList = []
    isRange = re.compile('^\D+\d+\-\d+$')
    splitter = re.compile('\D+')
    if isRange.match(serverIds):
        prefix = splitter.match(serverIds).group()
        numbers = splitter.split(serverIds)
        first = min(int(numbers[1]), int(numbers[2]))
        last = max(int(numbers[1]), int(numbers[2]))
        for i in range(first, last+1):
            serverIdList.append("%s%d" % (prefix, i))
    else:
        serverIdList.append(serverIds)
    cleanedServerIdList = []
    for serverId in serverIdList:
        if re.compile('^\D+\d+$').match(serverId):
            prefix = splitter.match(serverId).group()
            number = int(splitter.split(serverId)[1])
            if prefix in CANONFORMAT:
                cleanedServerIdList.append(CANONFORMAT[prefix] % number)
                continue
        cleanedServerIdList.append(serverId)
    return cleanedServerIdList

def generateRequestList(server_ids):
    serverIdList = []
    for sIds in server_ids:
        serverIdList += parseServerIds(sIds)
    return serverIdList

def parseTime(timeStr):
    timeStr = timeStr.upper()
    if re.compile('^\d?\d(:\d\d)?((AM)|(PM))?$').match(timeStr):
        formatStr = ""
        if re.compile(':\d\d').search(timeStr):
            formatStr = ":%M"
        if re.compile('(AM)|(PM)$').search(timeStr):
            formatStr = "%I" + formatStr + "%p"
        else:
            formatStr = "%H" + formatStr
        expiration = datetime.strptime(str(date.today()) + ' ' + timeStr, "%Y-%m-%d " + formatStr)
        if expiration < datetime.now():
            expiration += timedelta(1)
        return expiration
    elif re.compile('^\d+(\.\d+)?H$').match(timeStr):
        timeStr = timeStr.rstrip('H')
        hours = float(timeStr)
        expiration = datetime.now() + timedelta(hours/24.0)
        return expiration
    elif re.compile('^\d+(\.\d+)?D$').match(timeStr):
        timeStr = timeStr.rstrip('D')
        days = float(timeStr)
        expiration = datetime.now() + timedelta(days)
        return expiration
    print("Unable to complete request: "
          "Unknown time format - %s." % str(timeStr))
    return None

def rcres(user, args):
    isAdmin = False
    if user in ADMINLIST:
        isAdmin = True
 
    if args['lease'] or args['l']:
        expiration = parseTime(args['<time>'])
        if expiration:
            serverIdList = generateRequestList(args['<server_ids>'])
            leaseList(serverIdList,
                      user,
                      expiration,
                      args['--message'])
    elif args['unlease'] or args['ul']:
        serverIdList = generateRequestList(args['<server_ids>'])
        if len(serverIdList) == 0:
            serverIdList.append(user)
        unlockList(serverIdList, user, False)
    elif args['permalock'] or args['pl']:
        if not isAdmin:
            print("Unable to complete: request: "
                  "User %s is not admin." % user)
            return
        lockGroupId = 1
        if len(LOCKGROUPS) != 0:
            lockGroupId = max(LOCKGROUPS) + 1
        expiration = 'PERMA-LOCKED'
        serverIdList = generateRequestList(args['<server_ids>'])
        leaseList(serverIdList,
                  "LG%d" % lockGroupId,
                  expiration,
                  args['--message'])
    elif args['status'] or args['ls']:
        status(args['--list'], args['<cluster>'])
    elif args['unlock'] or args['upl']:
        if not isAdmin:
            print("Unable to complete: request: "
                  "User %s is not admin." % user)
            return
        idList = generateRequestList(args['<ids>'])
        unlockList(idList, user, True)

def main(args):
    lock = LockFile(RESFILE)
    with lock:
        readResFile()
        cleanSTATUS()
        rcres(getuser(), args)
        flushResFile()

if __name__ == '__main__':
    arguments = docopt(__doc__, version='rcres %s' % __version__)
    main(arguments)
