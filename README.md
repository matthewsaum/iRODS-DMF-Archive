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
These are the install instructions for your iRODS instance, with (default locations) listed as well.

On the resource connected to your archive:
- install the dmattr and dmget command files to the appropriate location (/var/lib/irods/iRODs/server/bin/cmd/)
- install the archive.re rule policy to the appropriate location (/etc/irods/)
- If users connect to this resource as well, install the iarchive alias script somehow. (/usr/bin)
 
On all other iCAT and Resource servers:
- install the archive.re rule policy to the appropriate location (/etc/irods/)
- If users connect to this resource as well, install the iarchive alias script somehow. (/usr/bin)
 
On any icommand nodes, or anywhere else users log in:
- Install the iarchive alias script somehow. (/usr/bin)
  
  I use two pieces of meta-data to track info:  our BFID on tape and our DMF status. The BFID is optional, but good recordkeeping. The DMF status is what prevents iRODS from flooding the system with requests. It is important that these meta-data names be unique, without risk of being overwritting by other policy enforcement.
 
# Testing
For simplicity, my resource is called "Archive". When data is put into the Archive resource, it will bounce off the resource server connected via NFS and go straight to the CXFS system in DMF. This means the required storage space on a dedicated iRODS link is fairly low. Once data is on the DMF system, various policies will copy it to tape eventually. Mine is roughly "within an hour". The DMF disk space is a cache, frequently purged to allow staging. Ours is done by "last accessed" data is deleted first. This makes it possible that the inode is visible by iRODS, but the data itself is not since it is not on disk anymore. In this case, iRODS sends a request every few seconds, per file, until interrupted. So we interrupt access to non-staged data before that in our rules. We also give users the ability to queue up staging the data from tape to disk again, via the "iarch" function of the ruleset.

# Legal Details
Author- Matthew Saum (SURFsara)

License Copyright 2017 SURFsara BV

Licensed under the Apache License, Version 2.0 (the “License”); you may not use this file except in compliance with the License. You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an “AS IS” BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
