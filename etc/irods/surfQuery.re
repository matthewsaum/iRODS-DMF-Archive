#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
#Our iarchive rule. This is used to stage data from tape to disk manually.
#This cann be called via || irule iarch "*tar=/path/to/object/or/coll%*stage=0" "ruleExecOut"
#The two variabels are : target data, input [0|1] to check status or actually stage.
iarch(){
  *resc="RESOURCE_NAME";                                   #The name of the resource

  #Find our resource location (FQDN)
  foreach(
    *r in SELECT
      RESC_LOC
    WHERE
      RESC_NAME = *resc
  ){
    *svr=*r.RESC_LOC;
  }

 #Removes a trailing "/" from collections if entered.
  if(*tar like '*/'){
    *tar = trimr(*tar,'/');
  }#if
  *stage=int("*stage"); #Input flag conversion to int. Used to determin to stage data or only provide output.
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
        WHERE
          RESC_NAME like '*resc'
      AND COLL_NAME like '*coll'
      AND DATA_NAME like '*obj'
    ){
      *dmfs=dmattr(*row.DATA_PATH, *svr, *tar);                                  #pulls DMF attributes into meta-data

      if(  *stage==1 ){
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
  
  msiExecCmd("dmattr", *data, *svr, "", "", *dmRes);
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

  if(*dmt like "0" || *Out like ""){                     #Prevents division by zero in case of empty files.
    *dmt="1";
    *dma="1";
  }#if

  *mig=double(*dma)/double(*dmt)*100;                    #Give us a % of completed migration from tape to disk
  *dma=trimr("*mig", '.');


  msiAddKeyVal(*Keyval1,"SURF-BFID",*bfid);
  msiSetKeyValuePairsToObj(*Keyval1,*ipath,"-d");
  msiAddKeyVal(*Keyval2,"SURF-DMF",*dmfs);
  msiSetKeyValuePairsToObj(*Keyval2,*ipath,"-d");
  
  "(*dmfs)               *dma%";                         #Our return sentence of status
}#dmattr
