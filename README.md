# SelRej

This program allows a user to connect to a server and download files from it. It simulates packet loss with a with a user provided error rate and recovers these lost packets with using selective reject.\
\
To run the program, first navigate to the "code" folder and call make.\
\
Then startup the server with the command:
```
./server [error-rate] [optional: port number]
```
Note: if you do not specify a port number, one will be generated and displayed for you.\
\
Start the client and issue a download request with:
```
./rcopy [from-file] [to-file] [window-size] [buffer-size] [error-rate] [host-name] [port-number]
```
The specified from-file must exist in the same directory as the server.\
Higher error rates may result in a timeout.
