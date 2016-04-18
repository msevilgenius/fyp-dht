# fyp-dht

## Requirements

- Libevent 2.0
- OpenSSL

# Building

run `make` as usual

run `make nofinger` to build with finger tables disabled

#Examples

run `ex.sh` to copy headers into the example folder and the built library to lib
`cd example`
`./build.sh`
.
to run textsend:
`./textsend <listen_port> <name> [<ip> <port>]`

to start a textsend node with the given name listenening on listen_port
if ip and port are provided it will attempt to connect to an existing node at that address,
otherwise it will create a new network.
