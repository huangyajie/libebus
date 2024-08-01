#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include "eco_socket.h"
#include "ebus.h"
#include "ebus_msg.h"

#define INVOKE_COUNT_TRIGGER_CHECK  10
#define INVOKE_SESSION_TIMEOUT  60  

struct ebusd_ctx
{
    int fd;
    int session; //会话标识
    struct rb_root root_fds; //用于客户端fd与service对应关系
    struct rb_root root_session; //用于客户端fd与session对应关系
    int invoke_count; //统计rbtree在存储的发起调用次数,用于触发session清理
};

struct _ebusd_msg_ctx
{
    struct ebusd_ctx* ctx;
    int cfd;
};

struct _ebusd_msg_session
{
    int fd;
    int session;
    int time;
};


static int _ebusd_server_init(const char* path)
{
    if(path == NULL)
    {
        return -1;
    }
    struct sockaddr_un sun = {.sun_family = AF_UNIX};
    if (strlen(path) >= sizeof(sun.sun_path)) 
    {
		return -1;
	}

    snprintf(sun.sun_path,sizeof(sun.sun_path),"%s",path);

    int fd = eco_socket(AF_UNIX,SOCK_STREAM,0);
    if(fd < 0)
    {
        return -1;
    }


    const int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if(eco_bind(fd, (struct sockaddr *)&sun, sizeof(sun)) != 0)
    {
        eco_close(fd);
        return -1;
    }

    if(eco_listen(fd, SOMAXCONN) != 0)
    {
        eco_close(fd);
        return -1;
    }
	
    return fd;
}

//初始化
struct ebusd_ctx* ebusd_init(const char* path)
{
    struct ebusd_ctx* ctx = (struct ebusd_ctx*)calloc(1,sizeof(struct ebusd_ctx));
    if(ctx == NULL)
    {
        return NULL;
    }

    ctx->fd = _ebusd_server_init(path);
    if(ctx->fd < 0)
    {
        return NULL;
    }

    rbtree_init(ctx->root_fds);
    rbtree_init(ctx->root_session);


    return ctx;
}

//退出，清理资源
int ebusd_exit(struct ebusd_ctx* ctx)
{
    if(ctx == NULL)
    {
        return -1;
    }

    eco_close(ctx->fd);
    ctx->fd = -1;
    rbtree_exit(&ctx->root_fds);
    rbtree_exit(&ctx->root_session);
    free(ctx);
    ctx = NULL;

    return 0;
}

//获取开机到当前的秒数
int get_monotonic_time()
{
    struct timespec time = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &time);
    
    return time.tv_sec;
}


static int _ebusd_session_clean(struct ebusd_ctx* ctx)
{
    struct rbtree_node* rbn = NULL; 
    struct rb_node *rnode;
    int i = 0;
    int count = 0;
    const char* keys[INVOKE_COUNT_TRIGGER_CHECK] = {NULL}; 
    struct _ebusd_msg_session* ems = NULL;
    int cur_time  = get_monotonic_time();  
    rbtree_for_each(ctx->root_session,rnode)
    {
        rbn = rb_entry(rnode, struct rbtree_node, node);
        ems = (struct _ebusd_msg_session*)(rbn->value);
        if(cur_time - ems->time > INVOKE_SESSION_TIMEOUT)
        {
            keys[i++] = rbn->key;
            if(i > sizeof(keys)/sizeof(keys[0]) - 1)
            {
                break;
            }
        }
    }
    count = i;

    for (i = 0;i < count;i++)
    {
        rbtree_delete(&ctx->root_session,keys[i]);
        ctx->invoke_count--;
    }

    return 0;
}

static int _ebusd_msg_proc(struct ebusd_ctx* ctx,int fd,const char* service,const char* method,int msg_type,int session,void* msg,int msg_sz)
{
    if((ctx == NULL) || (service == NULL) || (method == NULL) || (msg == NULL))
    {
        return -1;
    }

    int ret = -1;
    struct rbtree_node* rbn = NULL;

    if(msg_type == MSG_TYPE_REG)  //注册
    {
        rbn = rbtree_find(&ctx->root_fds,service);
        if(rbn != NULL)
        {
            //已注册过
            free(msg);
            msg = NULL;
            return -1;
        }

        struct rbtree_node node;
        memset(&node,0,sizeof(node));
        snprintf(node.key,sizeof(node.key),"%s",service);
        node.value =(int*)malloc(sizeof(int));
        *((int*)node.value) = fd; 
        rbtree_insert(&ctx->root_fds,&node);

        ebus_send_msg(fd,service,method,msg_type,session,EBUS_STATUS_OK,NULL,0);  //ack
    }
    else if(msg_type == MSG_TYPE_INVOKE) //服务调用
    {
        rbn = rbtree_find(&ctx->root_fds,service);
        if(rbn == NULL)
        {
            //未找到
            ebus_send_msg(fd,service,method,msg_type,session,EBUS_STATUS_ERROR,NULL,0);  //ack
            free(msg);
            msg = NULL;
            return -1;
        }

        int dst_fd = *((int*)(rbn->value));
        int ebusd_session = ++ctx->session;
        ret = ebus_send_msg(dst_fd,service,method,msg_type,ebusd_session,EBUS_STATUS_OK,msg,msg_sz); 
        if(ret <= 0)
        {
            ebus_send_msg(fd,service,method,msg_type,session,EBUS_STATUS_ERROR,NULL,0);  //ack
            free(msg);
            msg = NULL;
            return -1;
        }

        struct rbtree_node node;
        memset(&node,0,sizeof(node));
        snprintf(node.key,sizeof(node.key),"%d",ebusd_session);
        node.value =(struct _ebusd_msg_session*)calloc(1,sizeof(struct _ebusd_msg_session));
        struct _ebusd_msg_session* ems = (struct _ebusd_msg_session*)(node.value);
        ems->fd = fd;
        ems->session = session;
        ems->time = get_monotonic_time(); 
        rbtree_insert(&ctx->root_session,&node);
        ctx->invoke_count++;
        
    }
    else if(msg_type == MSG_TYPE_RESP)  //调用返回
    {
        char key[128] = {0};
        snprintf(key,sizeof(key),"%d",session);
        rbn = rbtree_find(&ctx->root_session,key);
        if(rbn == NULL)
        {
            free(msg);
            msg = NULL;
            return -1;
        }
        struct _ebusd_msg_session* ems = (struct _ebusd_msg_session*)(rbn->value);
        ebus_send_msg(ems->fd,service,method,msg_type,ems->session,EBUS_STATUS_OK,msg,msg_sz);
        rbtree_delete(&ctx->root_session,key);
        ctx->invoke_count--;

        if(ctx->invoke_count > INVOKE_COUNT_TRIGGER_CHECK)
        {
            //清理未返回session
            _ebusd_session_clean(ctx);
        }

    }
    else  //不支持的消息类型
    {
        //nothing to do 
    }

    free(msg);
    msg = NULL;

    return 0;
}

static int _ebus_del_fds(struct ebusd_ctx* ctx,int fd)
{
    int dst_fd = -1;
    struct rbtree_node* rbn = NULL; 
    struct rb_node *rnode;
    
    rbtree_for_each(ctx->root_fds,rnode)
    {
        rbn = rb_entry(rnode, struct rbtree_node, node);
        dst_fd = *((int*)(rbn->value));

        if(dst_fd == fd)
        {
            break;
        }
    }

    rbtree_delete(&ctx->root_fds,rbn->key);

    return 0;
}

static void _ebusd_msg_func(struct schedule * sch, void *ud) 
{
    int ret = 0;
    struct _ebusd_msg_ctx* mctx = (struct _ebusd_msg_ctx*)(ud);
    struct ebusd_ctx* ctx = mctx->ctx;
    int fd = mctx->cfd;
    free(mctx);
    mctx = NULL;
    

    int msg_type = -1;
    int session = -1;
    int status = -1;
    char method[METHOD_NAME_LEN] = {0};
    char service[SERVICE_NAME_LEN] = {0};
    void* msg = NULL;
    for(;;)
    {
        ret = ebus_receive_msg_ex(fd,service,method,&msg_type,&session,&status,&msg);
        if(ret <= 0)
        {
            // fprintf(stderr,"connection is lost \n");
            _ebus_del_fds(ctx,fd);
            eco_close(fd);
            break;
        }

        _ebusd_msg_proc(ctx,fd,service,method,msg_type,session,msg,ret);
    }

    
    
}


static void _ebusd_accept_func(struct schedule * sch, void *ud) 
{
    struct ebusd_ctx* ctx = (struct ebusd_ctx*)(ud);
    int listen_fd = ctx->fd;
    int cfd = -1;
    int co_rw = -1;

    struct sockaddr_un cliun;  
    socklen_t cliun_len = sizeof(cliun); 


    for(;;)
    {   
        cfd = eco_accept(listen_fd, (struct sockaddr *) &cliun, &cliun_len);
        if (cfd < 0) 
        {
            //errno  todo:EMFILE 未处理,其它情况需要再次accept
            continue;
        }

        // fprintf(stderr,"new connection cfd = %d \n",cfd);

        struct _ebusd_msg_ctx* mctx = (struct _ebusd_msg_ctx*)calloc(1,sizeof(struct _ebusd_msg_ctx));
        if(mctx == NULL)
        {
            eco_close(cfd);
            continue;
        }
        mctx->ctx = ctx;
        mctx->cfd = cfd;

        co_rw = eco_create(eco_get_cur_schedule(),_ebusd_msg_func,mctx);
        if(co_rw < 0)
        {
            free(mctx);
            mctx = NULL;
            eco_close(cfd);
            continue;
        }
        eco_resume_later(eco_get_cur_schedule(),co_rw);
        
    }
    
    eco_close(listen_fd);
}


int main()
{

    signal(SIGPIPE, SIG_IGN);
    eco_loop_init();
    
    unlink(EBUS_UNIX_PATH);
    umask(0111);
    struct ebusd_ctx* ctx = ebusd_init(EBUS_UNIX_PATH);
    if(ctx == NULL)
    {
        return -1;
    }

    int co_ac = eco_create(eco_get_cur_schedule(),_ebusd_accept_func,ctx);
    if(co_ac < 0)
    {
        ebusd_exit(ctx);
        return -1;
    }
    eco_resume(eco_get_cur_schedule(),co_ac);
    
    eco_loop_run();

    ebusd_exit(ctx);

    eco_loop_exit();
     
    
    return 0;
}