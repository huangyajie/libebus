/*
 * ebus - easy bus implementation,used for interprocess communication
 *
 * Copyright (C) 2023-2024 huang <https://github.com/huangyajie>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef _EBUS_H_
#define _EBUS_H_

#ifdef __cpluscplus
extern "C" {
#endif

#include "ebus_msg.h"
#include "rbtree.h"

#define EBUS_UNIX_PATH  "/tmp/ebus.sock"


typedef void (*method_cb)(struct ebus_ctx* ctx,int session,void* msg,int msg_sz);

struct ebus_method
{
    char method_name[METHOD_NAME_LEN];
    method_cb func;
};

struct ebus_ctx
{
    int fd;
    char service[SERVICE_NAME_LEN];
    struct ebus_method* mts;
    int mts_cnt;
    int session;  //通信会话
    struct rb_root root_session; //用于存放调用会话信息
};


//初始化
struct ebus_ctx* ebus_init(const char* service,struct ebus_method* mts,int mts_cnt);


//连接
int ebus_connect(struct ebus_ctx* ctx,const char* path);

//方法调用
int ebus_invoke(struct ebus_ctx* ctx,const char* service,const char* method,void* req,int req_sz,void* resp,int* resp_sz,int timeout);

//退出，资源释放
int ebus_exit(struct ebus_ctx* ctx);



#ifdef __cpluscplus
}
#endif


#endif