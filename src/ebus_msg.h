#ifndef _EBUS_MSG_H_
#define _EBUS_MSG_H_

#include "ebus.h"

struct ebus_ctx;

#define SERVICE_NAME_LEN  64
#define METHOD_NAME_LEN  64
#define MAX_MSG_LEN  1024*1024


enum
{
    MSG_TYPE_REG = 0,  //服务注册
    MSG_TYPE_INVOKE = 1, //调用
    MSG_TYPE_RESP = 2,   //调用返回
};

enum
{
    EBUS_STATUS_OK = 0,  //调用成功
    EBUS_STATUS_ERROR = -1, //出错
};

struct ebus_message
{
    int type;  //消息类型
    int session; //会话标识
    char service[SERVICE_NAME_LEN]; //服务
    char method[METHOD_NAME_LEN]; //方法
    int status; //调用结果
    int len;  //消息体长度
    char msg[];  //消息体
};

int ebus_send_msg(int fd,const char* service,const char* method,int msg_type,int session,int status,void* req,int req_sz);
int ebus_receive_msg(int fd,char* service,char* method,int *msg_type,int *session,int* status,void* msg,int msg_sz);
int ebus_receive_msg_ex(int fd,char* service,char* method,int *msg_type,int *session,int* status,void** msg);


#endif