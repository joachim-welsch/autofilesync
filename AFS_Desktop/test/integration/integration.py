import sys, string, os, shutil, msvcrt as m
from shutil import copy2

def wait():
    m.getch()

def touch(fname):
    with open(fname, 'a'):
        try:                     # Whatever if file was already existing
            os.utime(fname, None)  # => Set current time anyway
        except OSError:
            pass  # File deleted between open() and os.utime() calls
            
def run():
##    os.system(r"C:\Entwicklung\build-autofilesync-Desktop_Qt_5_11_2_MSVC2017_64bit-Debug\debug\AutoFileSync.exe console");
    os.system(r"C:/Entwicklung/build-autofilesync-Desktop_Qt_5_11_2_MSVC2017_64bit-Release/release/autofilesync.exe console");

testrootdir = r"C:/Entwicklung/autofilesync/test"

def copy_cfg(name):
##    source = r"C:/Entwicklung/autofilesync/test/\resources/" + name
    source = testrootdir + "/resources/" + name
    dest = r"C:/Users/Welsch/autofilesync.xml"
    copy2(source, dest)


fname1 = r"test1.txt"
fname2 = r"test2.txt"
fname3 = r"test3.txt"
fname4 = r"test4.txt"
fname5 = r"test5.txt"

def clearScenario():
    shutil.rmtree('/Entwicklung/autofilesync/test/test/', ignore_errors=True)
    
def getFileScenarioSrcDir():
    return r"C:/Entwicklung/autofilesync/test/test/file/src"
	
def getFileScenarioDestDir():
    return r"C:/Entwicklung/autofilesync/test/test/file/dest"

def getDirScenarioSrcDir():
    return r"C:/Entwicklung/autofilesync/test/test/dir/src"
	
def getDirScenarioDestDir():
    return r"C:/Entwicklung/autofilesync/test/test/dir/dest"
	
respath = r"C:/Entwicklung/autofilesync/test/resources/"

def fileScenarioSrcPathname():
    return getFileScenarioSrcDir() + "/" + fname1

def fileScenarioDestPathname():
    return getFileScenarioDestDir() + "/" + fname1
    
def createFileScenario():
    src = getFileScenarioSrcDir()
    if not os.path.exists(src):
        os.makedirs(src)
    dest = getFileScenarioDestDir()
    if not os.path.exists(dest):
        os.makedirs(dest)
    copy2(respath + "/" + fname1, fileScenarioDestPathname())
    copy2(respath + "/" + fname1, fileScenarioSrcPathname())

def createDirScenario():
	src = getDirScenarioSrcDir()
	if not os.path.exists(src):
		os.makedirs(src)
	dest = getDirScenarioDestDir()
	if not os.path.exists(dest):
		os.makedirs(dest)
	copy2(respath + "/" + fname2, getDirScenarioSrcDir() + "/" + fname1)
	copy2(respath + "/" + fname1, getDirScenarioDestDir() + "/" + fname1)
	copy2(respath + "/" + fname4, getDirScenarioSrcDir() + "/" + fname2)
	copy2(respath + "/" + fname3, getDirScenarioDestDir() + "/" + fname2)
	
def checksize():
    sizeSrc = os.path.getsize(fileScenarioSrcPathname())
    sizeDest = os.path.getsize(fileScenarioDestPathname())
    if (sizeSrc != sizeDest):
        return 1
    else:
        return 0

def checkSize(errmsg):
    sizeSrc = os.path.getsize(fileScenarioSrcPathname())
    sizeDest = os.path.getsize(fileScenarioDestPathname())
    if (sizeSrc != sizeDest):
        print(errmsg)
        return 1
    else:
        return 0

def checkcontent():
    str1 = open(fileScenarioSrcPathname(), 'r').read()
    str2 = open(fileScenarioDestPathname(), 'r').read()
    if (str1 != str2):
        return 1
    else:
        return 0

def checkContent(errmsg):
    str1 = open(fileScenarioSrcPathname(), 'r').read()
    str2 = open(fileScenarioDestPathname(), 'r').read()
    if (str1 != str2):
        print(errmsg)
        return 1
    else:
        return 0

def checktime():
    timeSrc = os.path.getmtime(fileScenarioSrcPathname())
    timeDest = os.path.getmtime(fileScenarioDestPathname())
    if (timeSrc != timeDest):
        return 1
    else:
        return 0
    
def checkTime(errmsg):
    timeSrc = os.path.getmtime(fileScenarioSrcPathname())
    timeDest = os.path.getmtime(fileScenarioDestPathname())
    if (timeSrc != timeDest):
        ##print("checktime failed: ", timeSrc, "!=", timeDest)
        print(errmsg)
        return 1
    else:
        return 0

    
def test_twowaysSourceModified(check):
    clearScenario()
    copy_cfg("twoways1.xml");
    createFileScenario()
    touch(fileScenarioSrcPathname())
    if check:
        return checktime() == False
    else:
        run();
        return checkTime("test_twowaysSourceModified: Zeitstempel nach Sync nicht gleich")
        

def test_twowaysDestinationModified(check):
    clearScenario()
    copy_cfg("twoways1.xml");
    createFileScenario()
    touch(fileScenarioDestPathname())
    if check:
        return checktime() == False
    else:
        run();
        return checkTime("test_twowaysDestinationModified: Zeitstempel nach Sync nicht gleich")

def test_destTimeNewer(check):
    clearScenario()
    copy_cfg("file1.xml");
    createFileScenario()
    copy2(respath + "/" + fname2, fileScenarioDestPathname())
    if check:
        return checktime() == False
    else:
        run();
        return checkTime("test_destTimeNewer: Zeitstempel nach Sync nicht gleich")

def test_SourceModifiedTime(check):
    clearScenario()
    copy_cfg("file1.xml");
    createFileScenario()
    copy2(respath + "/" + fname3, fileScenarioSrcPathname())
    if check:
        return checktime() == False
    else:
        run();
        return checkTime("test_SourceModifiedTime: Zeitstempel nach Sync nicht gleich")
    
def test_SourceModifiedSize(check):
    clearScenario()
    copy_cfg("file1.xml");
    createFileScenario()
    copy2(respath + "/" + fname3, fileScenarioSrcPathname())
    copy2(respath + "/" + fname4, fileScenarioDestPathname())
    if check:
        return checksize() == False
    else:
        run();
        return checkSize("test_SourceModifiedSize: File size nach Sync nicht gleich")

def test_SourceModifiedContent(check):
    clearScenario()
    copy_cfg("file1.xml");
    createFileScenario()
    copy2(respath + "/" + fname3, fileScenarioSrcPathname())
    copy2(respath + "/" + fname5, fileScenarioDestPathname())
    if check:
        return checkcontent() == False
    else:
        run();
        return checkContent("test_SourceModifiedContent: File content nach Sync nicht gleich")

testcount = 0
errorcount = 0

def test_file():
	global errorcount
	global testcount
	errorcount += test_twowaysSourceModified(True)
	errorcount += test_twowaysSourceModified(False)
	testcount += 1
	errorcount += test_twowaysDestinationModified(True)
	errorcount += test_twowaysDestinationModified(False)
	testcount += 1
	errorcount += test_twowaysDestinationModified(True)
	errorcount += test_twowaysDestinationModified(False)
	testcount += 1
	errorcount += test_destTimeNewer(True)
	errorcount += test_destTimeNewer(False)
	testcount += 1
	errorcount += test_SourceModifiedTime(True)
	errorcount += test_SourceModifiedTime(False)
	testcount += 1
	errorcount += test_SourceModifiedSize(True)
	errorcount += test_SourceModifiedSize(False)
	testcount += 1
	errorcount += test_SourceModifiedContent(True)
	errorcount += test_SourceModifiedContent(False)
	testcount += 1

def checkFileTime(fn):
	srcfname = testrootdir + "/test/dir/src/" + fn
	destfname = testrootdir + "/test/dir/dest/" + fn
	timeSrc = os.path.getmtime(srcfname)
	timeDest = os.path.getmtime(destfname)
	return timeSrc == timeDest


def test_dirSourceModified(check):
	clearScenario()
	copy_cfg("dir1.xml");
	createDirScenario()
	if check:
		return checkFileTime("test1.txt") == False and checkFileTime("test2.txt") == False
	else:
		run();
		if checkFileTime("test1.txt") == True and checkFileTime("test2.txt") == True:
			return 0
		else:
			print("test_dirSourceModified: Dateien sind nicht gleich")
			return 1

def test_dirSourceModifiedDesDoesntExist(check):
	clearScenario()
	copy_cfg("dir1.xml");
	createDirScenario()
	shutil.rmtree('C:/Entwicklung/autofilesync/test/test/dir/dest', ignore_errors=True)
	if check:
		if os.path.exists('C:/Entwicklung/autofilesync/test/test/dir/dest'):
			print("test_dirSourceModifiedDesDoesntExist: Pfad existiert doch!")
			return 1
		else:
			return 0
	else:
		run();
		if checkFileTime("test1.txt") == True and checkFileTime("test2.txt") == True:
			return 0
		else:
			print("test_dirSourceModifiedDesDoesntExist: Dateien sind nicht gleich")
			return 1
			
def test_dir():
	global errorcount
	global testcount
	errorcount += test_dirSourceModified(True)
	errorcount += test_dirSourceModified(False)
	testcount += 1
	errorcount += test_dirSourceModifiedDesDoesntExist(True)
	errorcount += test_dirSourceModifiedDesDoesntExist(False)
	testcount += 1


	
####### main #########
test_file()
##test_dir()

print(testcount, " Tests und ", errorcount, " Fehler")


#wait();
