#include "eco_socket.h"
#include "ebus.h"
#include "ebus_msg.h"




//返回写入数据长度
int ebus_send_msg(int fd,const char* service,const char* method,int msg_type,int session,int status,void* req,int req_sz)
{
    if((service == NULL) || (method == NULL))
    {
        return -1;
    }


    struct ebus_message *emsg = (struct ebus_message *)calloc(1,sizeof(struct ebus_message)+req_sz);
    if(emsg == NULL)
    {
        return -1;
    }
    snprintf(emsg->service,sizeof(emsg->service),"%s",service);
    snprintf(emsg->method,sizeof(emsg->method),"%s",method);
    emsg->type = msg_type;
    emsg->session = session;
    emsg->status = status;
    emsg->len = req_sz;
    if(req_sz > 0)
    {
        memcpy(emsg->msg,req,req_sz);
    }
    
    int ret = eco_write(fd,emsg,sizeof(struct ebus_message) + emsg->len);
    free(emsg);
    emsg = NULL;
    
    return ret;
}



//循环读取数据
static ssize_t _ebus_receive(int fd, void *buf, size_t nbyte)
{
    int ret = -1;
    int index = 0;
    while (nbyte - index > 0)
    {
        ret = _eco_poll(fd,ELOOP_READ,ECO_SOCKET_READ_TIMEOUT);
        if(ret <= 0)
        {
            continue;
        }
        ret = read(fd,buf+index,nbyte-index);
        if(ret < 0)
        {
            if((errno == EINTR) || (errno == EAGAIN))
            {
                continue;
            }

            return -1;  //链接异常断开
        }
        else if(ret == 0)
        {
            return 0;  //对方主动断开链接
        }
        else
        {
            index += ret;
        }
    }
  
    return index;
}

//返回读取数据长度  <=0 连接断开
int ebus_receive_msg(int fd,char* service,char* method,int *msg_type,int *session,int* status,void* msg,int msg_sz)
{
    if((service == NULL) || (method == NULL) || (msg_type == NULL) || (session == NULL) || (status == NULL) || (msg == NULL))
    {
        return -1;
    }

    struct ebus_message emsg;
    memset(&emsg,0,sizeof(emsg));
    int ret = _ebus_receive(fd,&emsg,sizeof(emsg));

    if(ret <= 0)
    {
        //连接断开
        return ret;
    }

    snprintf(method,METHOD_NAME_LEN,"%s",emsg.method);
    snprintf(service,SERVICE_NAME_LEN,"%s",emsg.service);
    *msg_type = emsg.type;
    *session = emsg.session;
    *status = emsg.status;


    if(emsg.len > 0)
    {
        ret = _ebus_receive(fd,msg,emsg.len > msg_sz ? msg_sz:emsg.len);

        if(ret <= 0)
        {
            return ret;
        }
    }

    return sizeof(emsg)+emsg.len;    
}


//返回读取数据长度  <=0 连接断开
int ebus_receive_msg_ex(int fd,char* service,char* method,int *msg_type,int *session,int* status,void** msg)
{
    if((service == NULL) || (method == NULL) || (msg_type == NULL) || (session == NULL) ||  (status == NULL) || (msg == NULL))
    {
        return -1;
    }

    struct ebus_message emsg;
    int ret = _ebus_receive(fd,&emsg,sizeof(emsg));

    if(ret <= 0)
    {
        //连接断开
        return ret;
    }

    snprintf(method,METHOD_NAME_LEN,"%s",emsg.method);
    snprintf(service,SERVICE_NAME_LEN,"%s",emsg.service);
    *msg_type = emsg.type;
    *session = emsg.session;
    *status = emsg.status;

    int len = (emsg.len > MAX_MSG_LEN ? MAX_MSG_LEN:emsg.len);

    *msg = (char*)calloc(1,len);
    if(*msg == NULL)
    {
        return -1;
    }
    

    if(emsg.len > 0)
    {
        ret = _ebus_receive(fd,*msg,len);

        if(ret <= 0)
        {
            return ret;
        }
    }

    return sizeof(emsg)+len;    
}




