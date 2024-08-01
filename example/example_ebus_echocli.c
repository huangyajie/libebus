#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "eco_socket.h"
#include "ebus.h"



static void _echocli_func(struct schedule * sch, void *ud)
{
    struct ebus_ctx* ctx = (struct ebus_ctx*)ud;

    char resp[256] = {0};
    int ret = -1;
    int resp_len = -1;

    for(;;)
    {
        memset(resp,0,sizeof(resp));
        ret = ebus_invoke(ctx,"echosrv","hello","world",strlen("world"),resp,&resp_len,100);
        fprintf(stderr,"ret = %d , resp = %s \n",ret,resp);
        eco_sleep(1);
    }
}


int main()
{
    eco_loop_init();

    struct ebus_ctx* ctx = ebus_init("echocli",NULL,0);
    if(ctx == NULL)
    {
        return -1;
    }


    if(ebus_connect(ctx,EBUS_UNIX_PATH) < 0)
    {
        return -1;
    }


    int co = eco_create(eco_get_cur_schedule(),_echocli_func,ctx);
    if(co < 0)
    {
        ebus_exit(ctx);
        return -1;
    }
    
    eco_resume(eco_get_cur_schedule(),co);

    
    eco_loop_run();
    
    ebus_exit(ctx);
    eco_loop_exit();

    return 0;
}