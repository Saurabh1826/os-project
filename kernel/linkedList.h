#ifndef LINKED_LIST_H
#define LINKED_LIST_H

typedef struct lNode {
    void *payload; // Pointer to the payload of this node
    struct lNode *prev;
    struct lNode *next;
} lNode;

typedef struct linkedList {
    lNode *head;
    lNode *tail;
    int length;
} linkedList;

lNode *addNodeTail(linkedList *list, void *payload);
lNode *addNodeHead(linkedList *list, void *payload);
int removeNode(linkedList *list, lNode *node);
linkedList *createList(void);
lNode *findNode(linkedList *list, void *payload);
void freeList(linkedList *list);

#endif