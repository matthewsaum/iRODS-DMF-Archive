#Author Matthew Saum
#Copyright 2017 SURFsara BV
#Apache License 2.0

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
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#TO-DO:
#Size limitations? Min/max?
#Possibly force .tar-ball of data before placed on archive resource?



#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#this creates two meta-data tags, one for the DMF BFID, which is good record keeping.
# the other is required by operations here. It is our DMF status.
#This is to prevent iRODS from trying to read data on tape without being staged to disk.
acPostProcForPut {
 *resc="Archive";
 if($rescName like *resc){
  msiAddKeyVal(*Key1,"SURF-BFID","NewData");
  msiSetKeyValuePairsToObj(*Key1,$objPath,"-d");
  msiAddKeyVal(*Key2,"SURF-DMF","NewData");
  msiSetKeyValuePairsToObj(*Key2,$objPath,"-d");
  writeLine("serverLog","New Archived data, applying required meta-data");
 }#if
}#acpostprocforput

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#This is our Policy Enforcement Point for preventing iRODS from reading data
#that has not been staged to disk. This is because if the data is not on disk,
#but iRODS tries to access it, DMF is flooded by 1 request every 3 seconds,
#per each file, until interrupted.
pep_resource_open_pre(*OUT){
 #DEFINE THESE ACCORDING TO THE INSTRUCTIONS ABOVE
 *auto=1;
 *svr="your.resource.FQDN";
 *resc="Archive";
 if($KVPairs.resc_hier like *resc && $connectOption != "iput"){
  #Clean copy of the physical path and logical path
  *dpath=$KVPairs.physical_path;
  *ipath=$KVPairs.logical_path;
  #fresh update of the DMF status meta data value. Runs the dmattr function, gives us the status from thh return string.
  *dma=dmattr(*dpath, *svr);
  *mv=substr(*dma, 1, 4);
  *stg=triml(*dma, "        ");
  #Checking for DMF availability, logging if status is staged to disk.
  if ((*mv like "REG") || (*mv like "DUL") || (*mv like "MIG")){
  writeLine("serverLog","$userNameClient:$clientAddr copied *ipath (*mv) from the Archive.");
  }#if
  #If DMF status is not staged, we display the current status and error out, preventing data access.
  else{
   #We have options here. We can either auto-stage the data, or provide an error and request the user manually stage data.
   if(*auto==0){
    failmsg(-1,"*ipath is still on tape with status: (*mv). If (OFL), please use iarchive to stage to disk.");
   }
   #These two lines are a failmsg stating that the data is being migrated, and auto-staging it from tape
   if(*auto==1){
    #prevents redundant queuing in DMF
    if(*mv not like "UNM"){
     dmget(*dpath,*svr);
    } #if
    failmsg(-1,"*ipath is still on tape, but queued to be staged. Current data staged: *stg." );
   } #if
   else {
    failmsg(-1,"Archive Policy is not properly configured. Please check the archive.re file, or respectively located ruleset.");
   }#else
  }#else
 }#if
}#PEP

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#Our iarchive rule. This is used to stage data from tape to disk.
iarch(){
 #called via irule: irule iarch "*tar=/target/collection/or/object" "ruleExecOut"
 #*tar must be defined upon input
 #REQUIRED DEFINITIONS:
 #The Archive Resource Server
 *svr="your.resource.FQDN";
 #The SURFsara Archive Resource Name mapped over the NFS link
 *resc="Archive";
 #Removes a trailing "/" from collections if entered.
 if(*tar like '*/'){
  *tar = trimr(*tar,'/');
 }#if
 #Puts our input variable to an intiger for easier handling.
 *inp=int("*inp");
 #Becuase iRODS does a lot of handling based on collection or data-object type:
 msiGetObjType(*tar, *tarCD);
 #For individual data objects
 if (*tarCD like '-d'){
  msiSplitPath(*tar, *coll, *obj);
  writeLine("stdout","\n\nUse the iarchive command with the \"-s\" option to stage data.\nUse the iarchive command with the \"-h\" for DMF STATE defintions\n\nDMF STATE 	% STAGED        OBJECT NAME\n--------------------------------------------");
  #Gives us the data_path location of our object. Also requires it to be on the Archive
  foreach(*row in SELECT DATA_PATH where RESC_NAME like '*resc' AND COLL_NAME like '*coll' AND DATA_NAME like '*obj' ){
   #runs the dmattr funciton to pull DMF status and % loaded (as well as update the BFID if relevant)
   *dmfs=dmattr(*row.DATA_PATH, *svr);
   #Prevents redundant DMGET requests
   if(*inp==1 && ((substr(*dmfs,1,4) not like "DUL" && substr(*dmfs,1,4) not like  "REG" && substr(*dmfs,1,4) not like "UNM" && substr(*dmfs,1,4) not like "MIG"))){
    dmget(*row.DATA_PATH, *svr);
    *dmfs="(UNM)"++triml(*dmfs,")");
   }#if
   #THIS IS THE USER OUTPUT OF IARCHIVE STATUS
   #It is best to make this white space "3 spaces, then 3 tabs" to line everythign up nicely
   writeLine("stdout","*dmfs            *tar");
  }#foreach
  writeLine("stdout","\nWARNING!!! % STAGED may not be 100% exactly, due to bytes used vs block size storage.");
 }#if

 #recursively stages a collection
 if (*tarCD like '-c'){
 writeLine("stdout","\n\nUse the iarchive command with the \"-s\" option to stage data.\nUse the iarchive command with the \"-h\" for DMF STATE defintions\n\nDMF STATE 	% STAGED        OBJECT NAME\n--------------------------------------------");
  #Pulls all data paths for items that are on the Archive resource and within a target collection, including sub-collections.
  foreach(*row in SELECT DATA_PATH, COLL_NAME, DATA_NAME where RESC_NAME like '*resc' AND COLL_NAME like '*tar%'){
   *ipath=*row.COLL_NAME++"/"++*row.DATA_NAME;
   *dmfs=dmattr(*row.DATA_PATH, *svr);
   #Prevents redundant DMGET requests
   if(*inp==1 && ((substr(*dmfs,1,4) not like "DUL" && substr(*dmfs,1,4) not like  "REG" && substr(*dmfs,1,4) not like "UNM" && substr(*dmfs,1,4) not like "MIG"))){
    dmget(*row.DATA_PATH, *svr);
    *dmfs="(UNM)"++triml(*dmfs,")");
   }#if
   #Whitespace here should be 3 spaces, then 3 tabs, to keep everything in line with the headers.
   #It just keeps it pretty.
   writeLine("stdout","*dmfs          *ipath");
  }#foreach
  writeLine("stdout","\nWARNING!!! % STAGED may not be 100% exactly, due to bytes used vs block size storage.");
 }#if

}#iarch


#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#the DMGET function
dmget(*data, *svr){
 #This runs the DMGET command located in ~irods/iRODS/server/bin/cmd/dmget
 msiExecCmd("dmget", "*data", "*svr", "", "", *dmRes);
 msiGetStdoutInExecCmdOut(*dmRes,*dmStat);
 writeLine("serverLog","$userNameClient:$clientAddr- Archive dmget started on *svr:*data. Returned Status- *dmStat.");
}#dmget

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#this dmattr rule below is meant to be running on delay to keep MetaData up to date.
#It also will be with any DMGET requests via the iarchive rules above.
#That data is: The BFID of the data on tape, and the DMF Status
#INPUT ORDER- *target object by DATA_PATH, *archive server name
dmattr(*data, *svr){
 msiExecCmd("dmattr", "*data", "*svr", "", "", *dmRes);
 msiGetStdoutInExecCmdOut(*dmRes,*Out);
 # Our *Out variable looks osmething like this "109834fjksjv09sdrf+DUL+0+2014"
 # The + is a separator, and the order of the 4 values are BFID, DMF status, size of data on disk, total size of data.
 #trim the newline
 *Out=trimr(*Out,'\n');
 #DMF BFID, trims from right to left, to and including the + symbol
 *bfid=trimr(trimr(trimr(*Out,'+'),'+'),'+');
 #DMF STATUS, trims up the DMF status only
 *dmfs=triml(trimr(trimr(*Out,'+'),'+'),'+');
 #trims to the total file size in DMF
 *dmt=triml(triml(triml(*Out,'+'),'+'),'+');
 #trims to the available file size on disk
 *dma=trimr(triml(triml(*Out,'+'),'+'),'+');
 #prevents division by zero errors, in case "someone" uploads an empty god damned file.
 #This still gives a 100% Staged result, since TECHINCALLY 0/0 is 100%. Stupid empty files.
 if(*dmt like "0"){
  *dmt="1";
  *dma="1";
 }#if
 #Give us a % of completed migration from tape to disk
 *mig=double(*dma)/double(*dmt)*100;
 *dma=trimr("*mig", '.');
 #compares our two metadatas
 foreach(*boat in SELECT META_DATA_ATTR_NAME, META_DATA_ATTR_VALUE, COLL_NAME, DATA_NAME where DATA_PATH like *data){
  *ipath=*boat.COLL_NAME++"/"++*boat.DATA_NAME;
  *mn=*boat.META_DATA_ATTR_NAME;
  *mv=*boat.META_DATA_ATTR_VALUE;
  #Checking that BFID matches, correcting if not
  if(*mn like 'SURF-BFID' && str(*mv) not like str(*bfid)){
   msiAddKeyVal(*Keyval,*mn,*bfid);
   msiSetKeyValuePairsToObj(*Keyval,*ipath,"-d");
  }#bfid if
  #Checking that DMF Status matches, correcting if not
  if(*mn like 'SURF-DMF' && str(*mv) not like str(*dmfs)){
   msiAddKeyVal(*Keyval,*mn,*dmfs);
   msiSetKeyValuePairsToObj(*Keyval,*ipath,"-d");
  }#dmfstat if
 }#metadata
 #Our return sentence of status
 "(*dmfs)               *dma%";
}#dmattr
