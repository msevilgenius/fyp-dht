#include <openssl/sha.h>
#include <string.h>
#include "libdht.h"

#include <stdlib.h>

hash_type get_id(const char* name)
{
    unsigned char sha_1[SHA_DIGEST_LENGTH];
    hash_type hash_id = 0;

    SHA1((unsigned char *)name, strlen(name), sha_1);
    char sha_1_4[sizeof(hash_type)+1];
    strncpy(sha_1_4, sha_1, sizeof(hash_type))
    sha_1_4[sizeof(hash_type)] = '\0';

    hash_id = strtol(sha_1_4, NULL, 16);

    return hash_id;
}

//
int two_to_the_n(int n)
{
    if (n < 1) return 1;
    return 2 << (n-1);
}

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
