#ifndef PROTO_H
#define PROTO_H

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


#endif // PROTO_H