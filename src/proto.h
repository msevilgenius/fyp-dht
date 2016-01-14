#ifndef PROTO_H
#define PROTO_H

#define MSG_FMT "%X\n%X\n%s\n%d\n"
#define MSG_FMT_CONTENT "%X\n%X\n%s\n%d\n%s"

/*
find_successor:
req: find successor id
resp: successor [of id] is s_id
  --s_id needs actual address not just hash id
*/

#define REQ_SUCCESSOR
#define RESP_SUCCESSOR

/*
stabilize:
req: who is your predecessor?
resp: predecessor is id
*/

#define REQ_PREDECESSOR
#define RESP_PREDECESSOR

/*
notify:
req: notify id
resp: [ok]
*/

#define REQ_NOTIFY
#define RESP_NOTIFY // is this necessary ?

/*
check_predecessor:
req: are you there
resp: yes i am here
*/

#define REQ_ALIVE
#define RESP_ALIVE

#define MES_END ("\n\n")

/*
########\n              from
########\n              to
REQ_XXXXXX\n            message type
NNNN\n                  [content length]
ASJDGKADBJOTJGEJB...    [content]
*/


#endif // PROTO_H
