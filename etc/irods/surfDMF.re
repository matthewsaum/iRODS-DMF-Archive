### Author Matthew Saum
### Copyright 2019 SURFBV
### Apache License 2.0

### This is our Policy Enforcement Point for preventing iRODS from reading data
### that has not been staged to disk. This is because if the data is not on disk,
### but iRODS tries to access it, DMF is flooded by 1 request every 3 seconds,
### per each file, until interrupted or data is staged.

pep_resource_open_pre(*INSTANCE_NAME, *CONTEXT, *OUT){

  ### The iRODS server with the HSM filesystem visilbity
  *SVR="server.irods.surfsara.nl"

  ### The iRODS resource with a storage path on the HSM filesystem
  *RESC="surfArchive"

  ### If the data object sits on the HSM resource
  if(*CONTEXT.resc_hier == *RESC){
    *MDcounter= 0;        # MetaData time flag, used to prevent spam-queries to HSM 
    *DIFFTIME= 0;         # base time difference of current system time and last HSM query
    *dmfs="";             # The DMF status used to determine action

    msiGetIcatTime(*TIME, "unix");
    msiSplitPath(*CONTEXT.logical_path, *iColl, *iData) ;

    ### Pulling existing metadata if it exists
    foreach(*SURF in SELECT META_DATA_ATTR_NAME,
                            META_DATA_ATTR_VALUE
                     WHERE  COLL_NAME = *iColl
                     AND    DATA_NAME = *iData)
    {
      if(*SURF.META_DATA_ATTR_NAME == 'SURF-TIME'){
        *MDcounter= 1;
        *DIFFTIME= int(*TIME) - int(*SURF.META_DATA_ATTR_VALUE);
      }
      if(*SURF.META_DATA_ATTR_NAME == 'SURF-DMF'){
        *dmfs = *SURF.META_DATA_ATTR_VALUE;
      }
    }

    ### This block is a loop to prevent flooding the system with queries
    ### The DIFFTIME variable is how many seconds must pass before re-querying HSM

    if(*MDcounter == 0 || *DIFFTIME >= 900)

    { 
      *dma=dmattr(*CONTEXT.physical_path, *SVR, *CONTEXT.logical_path, *TIME); 
      *dmfs=substr(*dma, 1, 4);
    } # if mdcounter
    
    ### The block that checks status and permits action if status is good
    if (
        *dmfs like "REG"
       || *dmfs like "DUL"
       || *dmfs like "MIG"
       || *dmfs like "NEW"
    ){   

      ### Log access if data is online
      writeLine("serverLog","$userNameClient:"++*CONTEXT.client_addr++" accessed ($connectOption) "++*CONTEXT.logical_path++" (*dmfs) from the Archive.");

    }#  if

    ### This block is what to do if data is not staged to disk

    else if (
         *dmfs like "UNM"
      || *dmfs like "OFL"
      || *dmfs like "PAR"
      ){ 
      #-=-=-=-=-=-=-=-=-
      ### These two lines are for auto-staging 
      ### By commenting out the dmget call, you can disable auto stage
      ### but then you need to manually call the dmget via another method
      
      dmget(*CONTEXT.physical_path,*SVR, *dmfs);
      failmsg(-1,*CONTEXT.logical_path++" is still on tape, but queued to be staged." );
     
    } # else if

    else {

      failmsg(-1,*CONTEXT.logical_path++" is either not on the tape archive, or something broke internal to the system.");

    } # else
  } # if on archive resc
  
  if(*CONTEXT.resc_hier != *RESC){
    #does nothing if not on the Archive

  } # if not on resc
  
###  msiGoodFailure;       #Uncomment to prevent later rule conflicts if exact same PEP in use elsewhere
 
} # close PEP


#-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
### The DMGET function
### INPUT ORDER: physical path, resource server FQDN, DMF status
### Prevents sending redundant requests to DMF

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
### This dmattr rule below is meant to be running on delay to keep MetaData up to date.
### It also will be with any DMGET requests via the iarchive rules above.
### That data is: The BFID of the data on tape, and the DMF Status
### INPUT ORDER- physical path, resource server FQDN

dmattr(*data, *svr, *ipath, *time){

  msiExecCmd("dmattr", "*data", "*svr", "", "", *dmRes);
  msiGetStdoutInExecCmdOut(*dmRes,*Out);
  
  ## Our *Out variable looks osmething like this "109834fjksjv09sdrf+DUL+0+2014"
  ## The + is a separator, and the order of the 4 values are BFID, DMF status, size of data on disk, total size of data.
  
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
  msiAddKeyVal(*Keyval3,"SURF-TIME",str(*time));
  msiSetKeyValuePairsToObj(*Keyval3,*ipath,"-d");

  "(*dmfs)               *dma%";                                         #Our return sentence of status
}#dmattr
