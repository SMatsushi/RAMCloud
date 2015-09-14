#! /opt/local/bin/python
import rcres

# Test
reload(rcres)
print("Test resourceAvailable")
if len(rcres.STATUS) > 0:
    print("Expected empty STATUS")
if rcres.resourceAvailable('rc01', 'c') != True:
    print("Expected True")
if rcres.resourceAvailable('rcmaster', 'c') != True:
    print("Expected True")
if rcres.resourceAvailable('noname', 'c') != False:
    print("Expected False")
rcres.STATUS['rc01'] = (1,'c',3)
if rcres.resourceAvailable('rc01', 'b') != False:
    print("Expected False")
if rcres.resourceAvailable('rc01', 'c') != True:
    print("Expected True")
if rcres.resourceAvailable('rc02', 'c') != True:
    print("Expected True")
print("Test Done")

# Test
reload(rcres)
print("Test leaseList")
if len(rcres.STATUS) > 0:
    print("Expected empty STATUS")
rcres.leaseList([], 'test', rcres.datetime.now(), 'hello world')
rcres.leaseList(['rc01', 'noname', 'notreal', 'rcmaster'], 'test', rcres.datetime.now(), 'hello world')
if rcres.resourceAvailable('rc01', 'test') != True:
    print("Expected True")
rcres.leaseList(['rc01', 'rcmaster'], 'test', rcres.datetime.now(), 'hello world')
if rcres.resourceAvailable('rc01', 'test') != False:
    print("Expected False")
if rcres.resourceAvailable('rcmaster', 'test') != False:
    print("Expected False")
if rcres.resourceAvailable('rc02', 'test') != True:
    print("Expected True")
print("Test Done")

#Test
reload(rcres)
print("Test parseServerIds")
temp = rcres.parseServerIds('rc1-3')
if len(temp) != 3:
    print("Expected 3 ids")
temp = rcres.parseServerIds('rcmaster')
if len(temp) != 1:
    print("Expected 1 ids")
temp = rcres.parseServerIds('rc34-34-')
if len(temp) != 1:
    print("Expected 1 ids")
print("Test Done")

#Test
reload(rcres)
print("Test parseTime")
print(rcres.parseTime('12:00AM'))
print("Test Done")

#Test
reload(rcres)
print("Test unlockList")
for i in range(1,11):
    rcres.STATUS["rc%02d" % i] = ('test', 1, 2)
for i in range(11,21):
    rcres.STATUS["rc%02d" % i] = ('LG1', 1, 2)
for i in range(21,31):
    rcres.STATUS["rc%02d" % i] = ('LG2', 1, 2)
if len(rcres.STATUS) != 30:
    print("Expected 30")
rcres.unlockList(rcres.generateRequestList(['rc1-80']), 'test', False)
if len(rcres.STATUS) != 30:
    print("Expected 30")
rcres.unlockList(rcres.generateRequestList(['rc1-5']), 'test', False)
if len(rcres.STATUS) != 25:
    print("Expected 25")
rcres.unlockList(rcres.generateRequestList(['test', 'LG1']), 'test', False)
if len(rcres.STATUS) != 25:
    print("Expected 25")
rcres.unlockList(rcres.generateRequestList(['rc10-15']), 'test', True)
if len(rcres.STATUS) != 19:
    print("Expected 19")
rcres.unlockList(rcres.generateRequestList(['LG2']), 'test', True)
if len(rcres.STATUS) != 9:
    print("Expected 9")
rcres.unlockList(rcres.generateRequestList(['test', 'LG1']), 'other', True)
if len(rcres.STATUS) != 0:
    print("Expected 0")
print("Test Done")