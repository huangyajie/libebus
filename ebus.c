#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include "eco_socket.h"
#include "ebus.h"



//初始化
struct ebus_ctx* ebus_init(const char* service,struct ebus_method* mts,int mts_cnt)
{
    if((service == NULL) || (mts == NULL) || (mts_cnt <= 0))
    {
        return NULL;
    }

    struct ebus_ctx* ctx = (struct ebus_ctx*)calloc(1,sizeof( struct ebus_ctx));
    if(ctx == NULL)
    {
        return NULL;
    }

    ctx->mts = (struct ebus_method*)calloc(mts_cnt,sizeof(struct ebus_method));
    if(ctx->mts == NULL)
    {
        free(ctx);
        ctx = NULL;
        return NULL;
    }
    ctx->fd = -1;
    memcpy(ctx->mts,mts,sizeof(struct ebus_method)*mts_cnt);
    ctx->mts_cnt = mts_cnt;

    snprintf(ctx->service,sizeof(ctx->service),"%s",service);

    rbtree_init(ctx->root_session);

    return ctx;
    
}


static int _ebus_register(struct ebus_ctx* ctx)
{
    if(ctx == NULL)
    {
        return -1;
    }

    int ret = -1;
    int msg_type = -1;
    int session = -1;
    char method[METHOD_NAME_LEN] = {0};
    char* resp = (char*)calloc(1,MAX_MSG_LEN);
    if(resp == NULL)
    {
        return -1;
    }
 
    ebus_send_msg(ctx,"EBUSD","",MSG_TYPE_REG,++ctx->session,NULL,0);
    ret = ebus_receive_msg(ctx,method,&msg_type,&session,resp,MAX_MSG_LEN);
    if(ret <= 0)
    {
        free(resp);
        resp = NULL;
        return -1;
    }

    if(msg_type != MSG_TYPE_REG)
    {
        free(resp);
        resp = NULL;
        return -1;
    }

    free(resp);
    resp = NULL;

    return 0;
}

static int _ebus_msg_proc(struct ebus_ctx* ctx,const char* method,int msg_type,int session,void* msg,int msg_sz)
{
    if((ctx == NULL) || (method == NULL) || (msg == NULL))
    {
        return -1;
    }

    int i = 0;

    if(msg_type == MSG_TYPE_INVOKE)  //方法被调用
    {
        for(i = 0;i < ctx->mts_cnt;i++)
        {
            if(strcmp(ctx->mts[i].method_name,method) == 0)
            {
                ctx->mts[i].func(ctx,session,msg,msg_sz);
                break;
            }
        }

    }
    else if(msg_type == MSG_TYPE_RESP)  //调用别的服务方法返回
    {
        char key[128] = {0};
        snprintf(key,sizeof(key),"%d",session);
        struct rbtree_node* rn = rbtree_find(&ctx->root_session,key);
        if(rn == NULL)
        {
           return -1;
        }

        int fd = *((int*)rn->value);

        free(rn->value);
        rn->value = NULL;

        rn->value = (char*)calloc(1,msg_sz+4);
        memcpy(rn->value,&msg_sz,sizeof(int));
        memcpy(rn->value+sizeof(int),msg,msg_sz);
        
        eco_write(fd,"R",1);
    }
    else
    {
        //不支持的消息类型  noting to do
    }

}

static void _ebus_msg_func(struct schedule * sch, void *ud)
{
    struct ebus_ctx* ctx = (struct ebus_ctx*)ud;
    int ret = -1;
    int msg_type = -1;
    int session = -1;
    char method[METHOD_NAME_LEN] = {0};

    char* msg = (char*)calloc(1,MAX_MSG_LEN);
    if(msg == NULL)
    {
        return;
    }
    
    for(;;)
    {
        if(ctx->fd < 0)
        {
            if(ebus_connect(ctx,EBUS_UNIX_PATH) < 0)
            {
                eco_sleep(3);
                continue;
            }
        }
        ret = ebus_receive_msg(ctx,method,&msg_type,&session,msg,MAX_MSG_LEN);
        if(ret <= 0)
        {
            fprintf(stderr,"connection is lost \n");
            eco_close(ctx->fd);
            ctx->fd = -1;
            continue;
        }

        _ebus_msg_proc(ctx,method,msg_type,session,msg,ret);  //消息处理

    }

    free(msg);
    msg = NULL;
}

//连接
int ebus_connect(struct ebus_ctx* ctx,const char* path)
{
    if((ctx == NULL) || (path == NULL))
    {
        return -1;
    }
    struct sockaddr_un sun = {.sun_family = AF_UNIX};
    if (strlen(path) >= sizeof(sun.sun_path)) 
    {
		return -1;
	}

    snprintf(sun.sun_path,sizeof(sun.sun_path),"%s",path);

    int fd = eco_socket(PF_UNIX,SOCK_STREAM,0);
    if(fd < 0)
    {
        return -1;
    }

    int ret = eco_connect(fd,(struct sockaddr*)(&sun),sizeof(sun));
    if(ret < 0)
    {
        return ret;
    }

    ctx->fd = fd;
    ctx->session = 0;

    //shake
    //fd + service  binding
    ret = _ebus_register(ctx);
    if(ret < 0)
    {
        eco_close(fd);
        ctx->fd = -1;
    }

    //read write coroutine create
    int co = eco_create(eco_get_cur_schedule(),_ebus_msg_func,ctx);
    if(co < 0)
    {
        eco_close(fd);
        ctx->fd = -1;
        return -1;
    }
    
    eco_resume_later(eco_get_cur_schedule(),co);

    return ret;
    
}

//方法调用  -1:失败 0:超时 1:成功
int ebus_invoke(struct ebus_ctx* ctx,const char* service,const char* method,void* req,int req_sz,void* resp,int* resp_sz,int timeout)
{
    if((ctx == NULL) || (service == NULL) || (method == NULL) || (req == NULL))
    {
        return -1;
    }

    if(ctx->fd < 0)
    {
        return -1;
    }

    int ret = ebus_send_msg(ctx,service,method,MSG_TYPE_INVOKE,++ctx->session,req,req_sz);
    if(ret <= 0)
    {
        return -1;
    }

    if(timeout == 0)  //异步，不再等结果
    {
        return 1;
    }


    //创建socket pair 保存会话信息，等待数据返回
    int fd[2];
    ret = eco_socketpair(AF_UNIX, SOCK_STREAM, 0, fd);
    if(ret < 0)
    {
        return -1;
    }

    struct rbtree_node node;
    memset(&node,0,sizeof(node));
    snprintf(node.key,sizeof(node.key),"%d",ctx->session);
    node.value =(int*)malloc(sizeof(int));
    *((int*)node.value) = fd[1]; 
    rbtree_insert(&ctx->root_session,&node);

    struct  poll_fd pfd_in[1];
    struct  poll_fd pfd_out[1];
    pfd_in[0].fd = fd[0];
    pfd_in[0].events = ELOOP_READ;

    ret = eco_poll(pfd_in,1,pfd_out,1,timeout);
    if(ret <= 0)
    {
        //失败或超时
        rbtree_delete(&ctx->root_session,node.key);
        eco_close(fd[0]);
        eco_close(fd[1]);
        return ret;
    }

    //成功 node->value: LV:len(int)+msg
    struct rbtree_node* rn = rbtree_find(&ctx->root_session,node.key);
    if(rn == NULL)
    {
        eco_close(fd[0]);
        eco_close(fd[1]);
        return -1;
    }

    memcpy(resp_sz,rn->value,sizeof(int));
    memcpy(resp,rn->value+sizeof(int),*resp_sz);
    
    rbtree_delete(&ctx->root_session,node.key);
    eco_close(fd[0]);
    eco_close(fd[1]);

    return 1;

    
}


//退出，资源释放
int ebus_exit(struct ebus_ctx* ctx)
{
    if(ctx == NULL)
    {
        return -1;
    }

    if(ctx->fd > 0)
    {
        eco_close(ctx->fd);
        ctx->fd = -1;
    }

    free(ctx->mts);
    ctx->mts = NULL;

    rbtree_exit(&ctx->root_session);
    
    free(ctx);
    ctx = NULL;

    return 0;
}