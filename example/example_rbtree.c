#include <stdio.h>
#include <stdlib.h>
#include "rbtree.h"



int main()
{
    struct rb_root root;
    
    rbtree_init(root);

    struct rbtree_node node;  //栈上变量，添加时会自动堆上创建，复制此变量内容，在删除里释放

    snprintf(node.key,sizeof(node.key),"aa");
    node.value = (char*)calloc(1,128);  //必须堆上申请，在删除里会释放
    snprintf(node.value,128,"123");
    rbtree_insert(&root,&node);

    snprintf(node.key,sizeof(node.key),"bb");
    node.value = (char*)calloc(1,128);
    snprintf(node.value,128,"456");
    rbtree_insert(&root,&node);


    snprintf(node.key,sizeof(node.key),"cc");
    node.value = (char*)calloc(1,128);
    snprintf(node.value,128,"789");
    rbtree_insert(&root,&node);
    

    struct rbtree_node * rbn = rbtree_find(&root,"aa");
    if(rbn != NULL)
    {
        fprintf(stderr,"%s\n",(char*)rbn->value);
    }


    rbn = rbtree_find(&root,"cc");
    if(rbn != NULL)
    {
        fprintf(stderr,"%s\n",(char*)rbn->value);
    }

    rbtree_delete(&root,"cc");
 

    struct rb_node *rnode;
    rbtree_for_each(root,rnode)
    {
        fprintf(stderr,"key = %s, value = %s \n",rb_entry(rnode, struct rbtree_node, node)->key,(char*)rb_entry(rnode, struct rbtree_node, node)->value);
 
    }

    rbtree_exit(&root);

}