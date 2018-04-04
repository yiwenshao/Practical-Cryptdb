#include <stdlib.h>
#include "adlist.h"
#include "zmalloc.h"



list *listCreate(void){
    struct list *list;
    // 分配内存
    if ((list = (struct list*)zmalloc(sizeof(*list))) == NULL)
        return NULL;
    // 初始化属性
    list->head = list->tail = NULL;
    list->len = 0;
    list->dup = NULL;
    list->free = NULL;
    list->match = NULL;
    return list;
}


list *listAddNodeTail(list *list, void *value){
    listNode *node;
    // 为新节点分配内存
    if ((node = (listNode*)zmalloc(sizeof(*node))) == NULL)
        return NULL;
    // 保存值指针
    node->value = value;
    // 目标链表为空
    if (list->len == 0) {
        list->head = list->tail = node;
        node->prev = node->next = NULL;
    // 目标链表非空
    } else {
        node->prev = list->tail;
        node->next = NULL;
        list->tail->next = node;
        list->tail = node;
    }
    // 更新链表节点数
    list->len++;
    return list;
}

void listDelNode(list *list, listNode *node){
    // 调整前置节点的指针
    if (node->prev)
        node->prev->next = node->next;
    else
        list->head = node->next;
    // 调整后置节点的指针
    if (node->next)
        node->next->prev = node->prev;
    else
        list->tail = node->prev;
    // 释放值
    if (list->free) list->free(node->value);
    // 释放节点
    zfree(node);
    // 链表数减一
    list->len--;
}

void listRelease(list *list){
    unsigned long len;
    listNode *current, *next;
    // 指向头指针
    current = list->head;
    // 遍历整个链表
    len = list->len;
    while(len--) {
        next = current->next;
        // 如果有设置值释放函数，那么调用它
        if (list->free) list->free(current->value);
        // 释放节点结构
        zfree(current);
        current = next;
    }
    // 释放链表结构
    zfree(list);
}


