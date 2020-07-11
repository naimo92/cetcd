#ifndef __CETCD_H
#define __CETCD_H

#include "thread_pool.h"
#include "sds/sds.h"

enum etcd_op_type{
    ETCD_OP_PUT = 1,//kv/put
    ETCD_OP_GET,//kv/range
    ETCD_OP_DEL,//kv/deleterange
    ETCD_OP_WATCH,//watch
    ETCD_OP_TXN,//kv/txn
    ETCD_OP_LEASE_CREATE,//lease/grant
    ETCD_OP_MAX,
};

typedef struct{
    enum etcd_op_type type;
    bool sync;
    int done;
    sds url;//curl url
    sds postdata;//curl_postdata

    size_t (*curl_callback)(void *contents, size_t size, size_t nmemb, void *userdata);//curl_callback
}thread_args;

int etcd_put(const char *key, const void *value, int v_len, int ignore, int64_t lease_id, int prev_kv, int ignore_value);
int etcd_get();

#endif