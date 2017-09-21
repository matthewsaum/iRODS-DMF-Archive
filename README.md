# iRODS-DMF-Archive
This contains the command files, the rule set, and a command-ish alias to easily link iRODS to a DMF Tape Library.
This is for iRODS 4.1.10. I cannot say what will happen on 4.2.x.
The DMF version we use is 6.7, via NFSv4. 


# Prior to installing these files
* You will need an iRODS resource server (could be your iCAT too).
* You will need NFS connectivity to the DMF CXFS system from this iRODS instance.
* You will need the dmf-client tools installed on this iRODS instance.
* You will need a resource to have been created in iRODS over the NFS directorys.
* You will need an iRODS service account that can own data in the DMF CXFS NFS mount point. 

For the service account, I used our central LDAP database here. It worked fine across both server ends.

All in all, easy stuff if you have your DMF admins around to talk to directly.

# With those pre-requisites taken care of...
Install the cmd files into your respective server/bin/cmd directory in irods.
 - typically this is /var/lib/irods/iRODS/server/bin/cmd/
 
Install the rule file into your iRODS instance, add it to the rulebase set in server_config.json
 - typically this is /etc/irods/
 - Note: If you have several instances, the ruleset needs installed on all of them. 
 
 Install the iarchive alias to /bin/cmd/ and give execute permissions to everyone. 
 Or maybe your users are comfy with irule. I'm new to this, so I have no idea what is standard.
  - note: this needs installed anywhere users will be running icommands.
  
  I use two pieces of meta-data to track info:  our BFID to easily locate data on tape in emergency, and our DMF status.
 
# Testing
For simplicity, my resource is called "Archive". When data is put into the Archive resource, it will bounce off the resource server connected via NFS and go straight to the CXFS system in DMF. This means the required storage space on a dedicated iRODS link is fairly low. Once data is on the DMF system, various policies will copy it to tape eventually. Mine is roughly "within an hour". The DMF disk space is a cache, frequently purged to allow staging. Ours is done by "last accessed" data goes first. This makes it possible that the inode is visible by iRODS, but the data is not since it is not on disk anymore. In this case, iRODS sends a request every few seconds, per file, until interrupted. So we interrupt access to non-staged data before that in our rules. We also give users the ability to queue up staging the data from tape to disk again, via the "iarch" function of the ruleset.

# Legal Details
Author- Matthew Saum (SURFsara)
License Copyright 2017 SURFsara BV

Licensed under the Apache License, Version 2.0 (the “License”); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an “AS IS” BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
