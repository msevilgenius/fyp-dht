#include <openssl/sha.h>
#include <string.h>
#include "libdht.h"
#include "logging.h"

#include <stdlib.h>

hash_type get_id(const char* name)
{
    unsigned char sha_1[SHA_DIGEST_LENGTH];
    hash_type hash_id = 0;

    SHA1((unsigned char *)name, strlen(name), sha_1);
    int bytes = sizeof(hash_type);
    for (int i = 0; i < bytes; ++i){
        hash_id += ((hash_type)sha_1[i]) << 8*(bytes-(i+1)); //uff...
    }

    return hash_id;
}



