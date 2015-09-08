#! /usr/bin/python
import mmres

# Test
reload(mmres)
print("Test resourceAvailable")
if len(mmres.STATUS) > 0:
    print("Expected empty STATUS")
if mmres.resourceAvailable('atom001', 'c') != True:
    print("Expected True")
if mmres.resourceAvailable('mmatom', 'c') != True:
    print("Expected True")
if mmres.resourceAvailable('noname', 'c') != False:
    print("Expected False")
mmres.STATUS['rc01'] = (1,'c',3)
if mmres.resourceAvailable('atom001', 'b') != False:
    print("Expected False")
if mmres.resourceAvailable('atom001', 'c') != True:
    print("Expected True")
if mmres.resourceAvailable('atom002', 'c') != True:
    print("Expected True")
print("Test Done")

# Test
reload(mmres)
print("Test leaseList")
if len(mmres.STATUS) > 0:
    print("Expected empty STATUS")
mmres.leaseList([], 'test', mmres.datetime.now(), 'hello world')
mmres.leaseList(['atom001', 'noname', 'notreal', 'mmatom'], 'test', mmres.datetime.now(), 'hello world')
if mmres.resourceAvailable('atom001', 'test') != True:
    print("Expected True")
mmres.leaseList(['atom001', 'mmatom'], 'test', mmres.datetime.now(), 'hello world')
if mmres.resourceAvailable('atom001', 'test') != False:
    print("Expected False")
if mmres.resourceAvailable('mmatom', 'test') != False:
    print("Expected False")
if mmres.resourceAvailable('atom002', 'test') != True:
    print("Expected True")
print("Test Done")

#Test
reload(mmres)
print("Test parseServerIds")
temp = mmres.parseServerIds('atom1-3')
if len(temp) != 3:
    print("Expected 3 ids")
temp = mmres.parseServerIds('mmatom')
if len(temp) != 1:
    print("Expected 1 ids")
temp = mmres.parseServerIds('atom34-34-')
if len(temp) != 1:
    print("Expected 1 ids")
print("Test Done")

#Test
reload(mmres)
print("Test parseTime")
print(mmres.parseTime('12:00AM'))
print("Test Done")

#Test
reload(mmres)
print("Test unlockList")
for i in range(1,11):
    mmres.STATUS["atom%03d" % i] = ('test', 1, 2)
for i in range(11,21):
    mmres.STATUS["atom%03d" % i] = ('LG1', 1, 2)
for i in range(21,31):
    mmres.STATUS["atom%03d" % i] = ('LG2', 1, 2)
if len(mmres.STATUS) != 30:
    print("Expected 30")
mmres.unlockList(mmres.generateRequestList(['atom1-131']), 'test', False)
if len(mmres.STATUS) != 30:
    print("Expected 30")
mmres.unlockList(mmres.generateRequestList(['atom1-5']), 'test', False)
if len(mmres.STATUS) != 25:
    print("Expected 25")
mmres.unlockList(mmres.generateRequestList(['test', 'LG1']), 'test', False)
if len(mmres.STATUS) != 25:
    print("Expected 25")
mmres.unlockList(mmres.generateRequestList(['atom10-15']), 'test', True)
if len(mmres.STATUS) != 19:
    print("Expected 19")
mmres.unlockList(mmres.generateRequestList(['LG2']), 'test', True)
if len(mmres.STATUS) != 9:
    print("Expected 9")
mmres.unlockList(mmres.generateRequestList(['test', 'LG1']), 'other', True)
if len(mmres.STATUS) != 0:
    print("Expected 0")
print("Test Done")
