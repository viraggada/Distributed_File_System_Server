ECEN 5273: Network Systems: Programming Assignment 3

Author: Virag Gada

Date: 11/12/2017

Goal: To create a Dstributed File System for reliable and secure file storage.

Programming Language - C

File structure:
      ./README.md
      ./server.c
      ./dfs.conf
      ./client.c
      ./dfc.conf
      ./dfc1.conf
      ./Makefile


Commands:
get [file]: On the client side we send the name of the file we want to get.
            The server checks if it has the file, if not it sends an "error"
            message or it sends the file size that the client should accept.

put [file]: We send the put command to the server and then open the file on
            client side, if the file does not exist we send the "error" message
            or the actual file size to expect. The server handles this accordingly.

list: Client sends this command to check the files available on the server side.

mkdir [folder]: Command to create a folder on the server

Execution:
Server side - Run make.
              Run the servers using ./dfs DFS1 10001
                                    ./dfs DFS2 10002
                                    ./dfs DFS3 10003
                                    ./dfs DFS4 10004

Client side - Run the client using ./dfc dfc.conf
