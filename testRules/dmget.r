### The DMGET Query command
### It can take a space-delineated string of DATA_PATH for objects and return results
### How to run it:
### Find out your DATA_PATH of the data object in question
### iquest "select DATA_PATH where DATA_NAME = 'testFile' and DATA_COLL = '/tempZone/home/user1'"
###
### Now you can either edit the *dataPath in the rule file and call the rule:
### irule -F dmget.r
###
### Or you can just adjust the variable via the command itself:
### irule -F dmget.r "*dataPath='/nfsDirectoryPath/home/user1/testFileName /nfsDirectoryPath/home/user1/testFileTwo'"

arcStatus{

    msiExecCmd("dmget", *dataPath, "resource.server.fqdn.goes.here", "", "", *dmRes);
    msiGetStdoutInExecCmdOut(*dmRes,*Out)
    writeLine("stdout",*Out);
    writeLine("stdout","Finished running rule");
}
INPUT *dataPath="/nfsDirectoryPathToResource/home/user1/test2_5.gb"
OUTPUT ruleExecOut

