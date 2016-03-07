#ifndef PROTO_H
#define PROTO_H

#define MSG_FMT "%c%08X"
#define MSG_FMT_CONTENT "%c%08X%s"

/* msg body node data : "%X\n%X\n%X\n"

IDXXXXXX
IPXXXXXX
PORT

*/

/*
find_successor:
req: find successor id
resp: successor [of id] is s_id
  --s_id needs actual address not just hash id
*/

#define MSG_T_SUCC_REQ 'S'
#define MSG_T_SUCC_REP 's'

/*
stabilize:
req: who is your predecessor?
resp: predecessor is id
*/

#define MSG_T_PRED_REQ 'P'
#define MSG_T_PRED_REP 'p'

/*
notify:
req: notify id
resp: [ok]
*/

#define MSG_T_NOTIF 'N'
#define MSG_T_NOTIFIED 'n' // is this necessary ?

/*
check_predecessor:
req: are you there
resp: yes i am here
*/

#define MSG_T_ALIVE_REQ 'A'
#define MSG_T_ALIVE_REP 'a'

#define MSG_T_NODE_MSG 'M'

#define MSG_T_UNKNOWN '0'

#define LEN_STR_BYTES 8
/*
 * TLV proto pls        what                    size
R                       message type            1
XXXXXXXX                content length          4
ASJDGKADBJOTJGEJB...    [content]
*/


#endif // PROTO_H
