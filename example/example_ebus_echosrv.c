#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "eco_socket.h"
#include "ebus.h"

static void hello_func(struct ebus_ctx* ctx,int session,void* msg,int msg_sz);

struct ebus_method mts[] = {
    {
        .method_name = "hello",
        .func = hello_func
    }
};

static void hello_func(struct ebus_ctx* ctx,int session,void* msg,int msg_sz)
{
    fprintf(stderr,"session = %d, msg = %s \n",session,(char*)msg);
    ebus_response(ctx,session,msg,msg_sz);
    
}


int main()
{
    eco_loop_init();

    struct ebus_ctx* ctx = ebus_init("echosrv",mts,sizeof(mts)/sizeof(mts[0]));
    if(ctx == NULL)
    {
        ebus_exit(ctx);
        return -1;
    }


    if(ebus_connect(ctx,EBUS_UNIX_PATH) < 0)
    {
        return -1;
    }


    
    eco_loop_run();
    
    ebus_exit(ctx);
    eco_loop_exit();

    return 0;
}