# 1.介绍
简单且高效的消息总线实现,用于多进程间通信,非常适合嵌入式Linux下使用:
- 提供ebusd服务,及libebus 用户端操作库
- 所有接口都进行协程化支持
- 依赖libeco协程框架

# 2.编译与安装
- mkdir build

- cd build 

- cmake ..

- make 

- make install

# 3.如何使用
ebus_echosrv实现如下,更多示例可以参考example
```c
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
