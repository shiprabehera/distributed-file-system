### Distributed File System

This distributed file system is written in C that allows client to store and retrieve files on multiple servers.

To build the client run : make dfc. To start the client run : "./dfc dfc.conf"
To build the server run : make dfs
To start 4 servers, run the following on different terminals:
/dfs DFS1 10001
/dfs DFS2 10002
/dfs DFS3 10003
/dfs DFS4 10004

It contains the following files: 

1. README.md
2. dfs-client.c  
3. dfs-server.txt 
4. dfs.conf
4. dfc.conf
5. Makefile

**LIST:**

	Usage: list

	Purpose: Inquires what files are stored on DFS servers, and print file names under the Username directory on DFS server. Also identifies if file pieces on DFS servers are enough to reconstruct the original file. If pieces are not enough (means some ervers are not available) then “[incomplete]” is added to the end of the file. 

**GET:**

	Usage: get [filename.ext]

	Purpose: Downloads all available pieces of a file from all available DFS servers, if the file can be reconstructed then write the file into your working folder. If not, then print “File is incomplete.”

**PUT:**

	Usage: put [filename.ext] 

	Purpose: Uploads file onto DFSs using same scheme. If 2 or more servers are down, this will throw an error saying not enough servers are offline to handle the request.

**MKDIR:**

	Usage: mkdir dir_name 

	Purpose: Makes subfolders on all servers for that user.

**Others:**
1. Traffic optimization is implemented such that GET command does not fetch data from all servers, but only from the sufficient number of servers.
2. Data encryption implemented using xor approach.
