#Author Matthew Saum
#Copyright 2017 SURFBV
#Apache License 2.0

#INSTRUCTIONS FOR USE
#Lines 47, 48, 89, and 90 all have two things to adjust per environment
#47 and 89 are the Resource Name
#48 and 90 are the server name (of the actual NFS-linked server to tape)
#Rule Conflicts:
#Line 83 should be uncommented if PEP_OPEN_PRE is used in your policies
#this will allow those other PEPS to run even if this one is hit first.


#20 Sep 2017
#DMF interaction for iRODS, when mounted via NFS.
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#REQUIRED FILES TO BE MADE:
#two command files in ~irods/iRODS/server/bin/cmd/:
# dmget and dmattr
#both of these are short, DMF client commands used by this ruleset.
#in short, they are just a "dmget -qa $1" and a "dmattr $1 | awk '{print $1, $12}'
#but they do need to exist. Also, they can be adjusted to meet various requirements.
#see the dmget and dmattr man pages for details.
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#REQUIRED VARIABLES TO BE DEFINED:
#inside three functions (acPostProcForPut, pep_open_pre, and iarch), are variables
#that needs to be defined for functionality. They should match across the three, where applicable
#*auto=[1|0]  #1 means that irods will automatically stage data if asked to get data that is on tape
#*svr=FQ.D.N   #The FQDN of your iRODS server connected to the archive resource
#*resc=Archive #The name of your archive resource in iRODS
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#1.0- 20Sept2017-First fully functional version
#1.1- 26Sept2017-Now includes a % based feedback of staging and some code scrubbing to clean up functions.
#1.2- 29Sept2017-Bug fix in PEP enforcement on what "open" means in iRODS, display better output in iGet interrupt
#1.3- 02Oct2017- Included user input for options.
#1.4- 03Oct2017- Much cleaner feedback displays. Prevented sending redundant DMGET requests. (if already staging, etc...)
#----------------Also, functions and calls are now more appropriately named towards DMGET and DMATTR instead of dmg and attr
#1.5- 21Nov2017- Now included- an auto stage feature on iget for tape-stored data. *auto var in the PEP.
#2.0- 19Jan2017- Re-structured entire code. Far better function calling, less redundant lines, rule-conflict handling
#2.1- 25Jan2017- Re-work meta-data application. Issues with iphymv because of rule handling in general
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#TO-DO:
#Size limitations? Min/max?
#Possibly force .tar-ball of data before placed on archive resource?

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#This is our Policy Enforcement Point for preventing iRODS from reading data
#that has not been staged to disk. This is because if the data is not on disk,
#but iRODS tries to access it, DMF is flooded by 1 request every 3 seconds,
#per each file, until interrupted or data is staged.
pep_resource_open_pre(*OUT){
 on($KVPairs.resc_hier like "Archive"){
  *svr="sara-irods1.grid.surfsara.nl";
  *dma=dmattr($KVPairs.physical_path, *svr, $KVPairs.logical_path);                            #DMF meta attribute update
 # *dma="(NEW)               100%";
  *dmfs=substr(*dma, 1, 4);
  *stg=triml(*dma, "        ");
  if (
      *dmfs like "REG"
   || *dmfs like "DUL"
   || *dmfs like "MIG"
   || *dmfs like "NEW"
  ){                            #Log access if data is online
   writeLine("serverLog","$userNameClient:$clientAddr accessed ($connectOption) "++$KVPairs.logical_path++" (*dmfs) from the Archive.");
  }#if
  else if (
      *dmfs like "UNM"
   || *dmfs like "OFL"
   || *dmfs like "PAR"
  ){            #Errors out if data not staged
   #-=-=-=-=-=-=-=-=-
    #These two lines are for auto-staging
#   dmget($KVPairs.physical_path,*svr, *dmfs);
#   failmsg(-1,$KVPairs.logical_path++" is still on tape, but queued to be staged. Current data staged: *stg." );
   #-=-=-=-=-=-=-=-=-
    #This block is for not auto-staging data.
    failmsg(-1,$userNameClient++":"++$clientAddr++" tried to access ($connectOption) "++$KVPairs.logical_path++" but it was not staged from tape.");
   } #else if
  else {
   failmsg(-1,$KVPairs.logical_path++" is either not on the tape archive, or something broke internal to the system.");
  }#else
 } #on
 on($KVPairs.resc_hier not like "Archive"){
  #does nothing if not on the Archive
 } #on
 #msiGoodFailure;       #Uncomment to prevent later rule conflicts if PEP in use elsewhere
} #PEP


#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#Our iarchive rule. This is used to stage data from tape to disk manually.
#This cann be called via || irule iarch "*tar=/path/to/object/or/coll%*inp=0" "ruleExecOut"
#The two variabels are : target data, input [0|1] to check status or actually stage.
iarch(){
 *svr="sara-irods1.grid.surfsara.nl";     #Resource Server FQDN
 *resc="Archive";                                   #The name of the resource
 #Removes a trailing "/" from collections if entered.
 if(*tar like '*/'){
  *tar = trimr(*tar,'/');
 }#if
 *inp=int("*inp"); #Input flag conversion to int. Used to determin to stage data or only provide output.
 msiGetObjType(*tar, *tarCD);
 writeLine("stdout","\n\nUse the iarchive command with the \"-s\" option to stage data.");
 writeLine("stdout","Use the iarchive command with the \"-h\" for DMF STATE defintions");
 writeLine("stdout","DMF STATE  % STAGED        OBJECT NAME\n--------------------------------------------");
 if (*tarCD like '-d'){                                         #Manual staging individual data object
  msiSplitPath(*tar, *coll, *obj);
  foreach(
   *row in
   SELECT
    DATA_PATH
   where
        RESC_NAME like '*resc'
        AND COLL_NAME like '*coll'
        AND DATA_NAME like '*obj'
  ){
   *dmfs=dmattr(*row.DATA_PATH, *svr, *tar);                                  #pulls DMF attributes into meta-data
   if(  *inp==1 ){
    dmget(*row.DATA_PATH, *svr, substr(*dmfs,1,4));
    *dmfs="(UNM)"++triml(*dmfs,")");    #changes display to show UNM (prevents re-SQL-querying)
   }#if
   writeLine("stdout","*dmfs            *tar");                         #Whitespace for display
  }#foreach
 }#if
 if (*tarCD like '-c'){                                 #Recursively stage a collection
  foreach(
   *row in
   SELECT
    DATA_PATH,
    COLL_NAME,
    DATA_NAME
   where
        RESC_NAME like '*resc'
    AND COLL_NAME like '*tar%'
  ){
   *ipath=*row.COLL_NAME++"/"++*row.DATA_NAME;
   *dmfs=dmattr(*row.DATA_PATH, *svr, *ipath);
   if(*inp==1){                                          #input to only check status or actually stage
    dmget(*row.DATA_PATH, *svr, substr(*dmfs,1,4));
    *dmfs="(UNM)"++triml(*dmfs,")");                     #changes display to show UNM (prevents re-SQL-querying)
   }#if
   writeLine("stdout","*dmfs          "++*row.COLL_NAME++"/"++*row.DATA_NAME);          #Whitespace for display
  }#foreach
 }#if
 writeLine("stdout","\nWARNING!!! % STAGED may not be 100% exactly, due to bytes used vs block size storage.");
}#iarch

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#the DMGET function
#INPUT ORDER: physical path, resource server FQDN, DMF status
#Prevents sending redundant requests to DMF
dmget(*data, *svr, *dmfs){
 if(
     *dmfs not like "DUL"
  && *dmfs not like "REG"
  && *dmfs not like "UNM"
  && *dmfs not like "MIG"
 ){
  msiExecCmd("dmget", "*data", "*svr", "", "", *dmRes);
  msiGetStdoutInExecCmdOut(*dmRes,*dmStat);
  writeLine("serverLog","$userNameClient:$clientAddr- Archive dmget started on *svr:*data. Returned Status- *dmStat.");
 }#if
}#dmget

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#this dmattr rule below is meant to be running on delay to keep MetaData up to date.
#It also will be with any DMGET requests via the iarchive rules above.
#That data is: The BFID of the data on tape, and the DMF Status
#INPUT ORDER- physical path, resource server FQDN
dmattr(*data, *svr, *ipath){
 msiExecCmd("dmattr", "*data", "*svr", "", "", *dmRes);
 msiGetStdoutInExecCmdOut(*dmRes,*Out);
 # Our *Out variable looks osmething like this "109834fjksjv09sdrf+DUL+0+2014"
 # The + is a separator, and the order of the 4 values are BFID, DMF status, size of data on disk, total size of data.
 *Out=trimr(*Out,'\n');                                 #Trims the newline
 *bfid=trimr(trimr(trimr(*Out,'+'),'+'),'+');           #DMF BFID, trims from right to left, to and including the + symbol
 if(*bfid like ""){
  *bfid = "0";
 }
 *dmfs=triml(trimr(trimr(*Out,'+'),'+'),'+');           #DMF STATUS, trims up the DMF status only
 if(*dmfs like ""){
  *dmfs = "INV";
 }
 *dmt=triml(triml(triml(*Out,'+'),'+'),'+');            #trims to the total file size in DMF
 *dma=trimr(triml(triml(*Out,'+'),'+'),'+');            #trims to the available file size on disk
 if(*dmt like "0" || *Out like ""){                                     #Prevents division by zero in case of empty files.
  *dmt="1";
  *dma="1";
 }#if
 *mig=double(*dma)/double(*dmt)*100;                     #Give us a % of completed migration from tape to disk
 *dma=trimr("*mig", '.');
 msiAddKeyVal(*Keyval1,"SURF-BFID",*bfid);
 msiSetKeyValuePairsToObj(*Keyval1,*ipath,"-d");
 msiAddKeyVal(*Keyval2,"SURF-DMF",*dmfs);
 msiSetKeyValuePairsToObj(*Keyval2,*ipath,"-d");

 "(*dmfs)               *dma%";                                         #Our return sentence of status
}#dmattr
