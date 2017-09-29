#SURFsara-Matthew Saum
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
#*svr and *resc, found in "acpostProcForPut", "pep_resource_open_pre", "delay", and "iarch"
#These are the same defintions, needed by separate functions.
#*svr="The name of the iRODS resource server connected to the archive"
#*resc="the name of the archive resource"
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#1.0- 20Sept2017-First fully functional version
#1.1- 26Sept2017-Now includes a % based feedback of staging and some code scrubbing to clean up functions.
#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#TO-DO:
#Size limitations? Min/max?
#Possibly force .tar-ball of data before placed on archive resource?
#User Prompt for DMF status only- to check status of data between disk and tape?




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
 *svr="your.resource.FQDN";
 *resc="Archive";
 if($KVPairs.resc_hier like *resc && $connectOption != "iput"){
  #Clean copy of the physical path and logical path
  *dpath=$KVPairs.physical_path;
  *ipath=$KVPairs.logical_path;
  #fresh update of the DMF status meta data value. Runs the attr function, gives us the status from teh return string.
  *mv=substr(attr(*dpath, *svr), 1, 4);
  #Checking for DMF availability, logging if status is staged to disk.
  if ((*mv like "REG") || (*mv like "DUL")){
   writeLine("serverLog","$userNameClient:$clientAddr copied *dpath (*mv) from the Archive.");
  }#if
  #If DMF status is not staged, we display the current status and error out, preventing data access.
  else{
   cut;
   msiExit("-1","Data requested is still on tape. Please use iarchive to stage to disk.");
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
 #This runs our target from user input to trim any trailing "/" and verify absolute paths
 #Errors out, stating that absolute paths must be used.
 if(*tar not like '/*'){
  cut;
  msiExit("-1","*tar is not an absolute path. Please retry with absolute path.");
 }#if
 #Removes a trailing "/" from collections if entered.
 if(*tar like '*/'){
  *tar = trimr(*tar,'/');
 }#if

 #Becuase iRODS does a lot of handling based on collection or data-object type:
 msiGetObjType(*tar, *tarCD);
 #For individual data objects
 if (*tarCD like '-d'){
  msiSplitPath(*tar, *coll, *obj);
  #Gives us the data_path location of our object. Also requires it to be on the Archive
  foreach(*row in SELECT DATA_PATH where RESC_NAME like '*resc' AND COLL_NAME like '*coll' AND DATA_NAME like '*obj' ){
   #runs the DMGET Staging and iget function
   dmg(*row.DATA_PATH, *svr);
   *dmfs=attr(*row.DATA_PATH, *svr);
   writeLine("stdout","*tar is currently in state: *dmfs. Queued for staging to disk. Only REG or DUL may be accessed.");
  }#foreach
 }#if

 #recursively stages a collection
 if (*tarCD like '-c'){
  #Pulls all data paths for items that are on the Archive resource and within a target collection, including sub-collections.
  foreach(*row in SELECT DATA_PATH, COLL_NAME, DATA_NAME where RESC_NAME like '*resc' AND COLL_NAME like '*tar%'){
   *ipath=*row.COLL_NAME++"/"++*row.DATA_NAME;
   dmg(*row.DATA_PATH, *svr);
   *dmfs=attr(*row.DATA_PATH, *svr);
   writeLine("stdout","*ipath is currently in state: *dmfs. Queued for staging to disk. Only REG or DUL may be accessed.");
  }#foreach
 }#if

}#iarch


#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#the DMGET function
dmg(*data, *svr){
 #This runs the DMGET command located in ~irods/iRODS/server/bin/cmd/dmget
 msiExecCmd("dmget", "*data", "*svr", "", "", *dmRes);
 msiGetStdoutInExecCmdOut(*dmRes,*dmStat);
 writeLine("serverLog","$userNameClient:$clientAddr- Archive dmget started on *svr:*data. Returned Status- *dmStat.");
}#dmg

#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#this Attr rule below is meant to be running on delay to keep MetaData up to date.
#It also will be with any DMGET requests via the iarchive rules above.
#That data is: The BFID of the data on tape, and the DMF Status
#INPUT ORDER- *target object by DATA_PATH, *archive server name
attr(*data, *svr){
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
 "(*dmfs) with *dma% staged from tape";
}#attr
