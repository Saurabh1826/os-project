// A generic doubly linked list that can be used as a queue/stack for any type 
// of object

#include <stdio.h>
#include <stdlib.h>
#include "linkedList.h"

// Function to add a node to the tail of the linked list. The function creates 
// a new node with the specified payload and add it to the list. NOTE: the pointer
// payload that is passed to the function should be a pointer returned by 
// malloc. In other words, the location pointed to by payload should have been 
// malloc'd so that it doesn't get overwritten later. Also note that the payload
// for each node should be unique (no two nodes should have the same payload
// pointer). Also, if a payload is being used in multiple different lists, it
// should be malloc'd for each of those lists--ie: each list's payload should be a 
// different location in memory
// Arguments: 
//     list: The linked list to add the node to
//     payload: The payload of the node to be added 
// Returns: 
//     A pointer to the newly created node
lNode *addNodeTail(linkedList *list, void *payload) {
    lNode *node = malloc(sizeof(lNode));
    node -> payload = payload;
    if ((list -> length) == 0) {
        list -> head = list -> tail = node;
        node -> prev = node -> next = NULL;
    } else {
        node -> prev = list -> tail;
        node -> next = NULL;
        list -> tail -> next = node;
        list -> tail = node;
    }

    // Increment length of the list 
    list -> length += 1;

    return node;
}

// Function to add a node to the head of the linked list. The function creates 
// a new node with the specified payload and add it to the list. NOTE: the pointer
// payload that is passed to the function should be a pointer returned by 
// malloc. In other words, the location pointed to by payload should have been 
// malloc'd so that it doesn't get overwritten later. Also note that the payload
// for each node should be unique (no two nodes should have the same payload
// pointer). Also, if a payload is being used in multiple different lists, it
// should be malloc'd for each of those lists--ie: each list's payload should be a 
// different location in memory
// Arguments: 
//     list: The linked list to add the node to
//     payload: The payload of the node to be added 
// Returns: 
//     A pointer to the newly created node
lNode *addNodeHead(linkedList *list, void *payload) {
    lNode *node = malloc(sizeof(lNode));
    node -> payload = payload;
    if ((list -> length) == 0) {
        list -> head = list -> tail = node;
        node -> prev = node -> next = NULL;
    } else {
        node -> prev = NULL;
        node -> next = list -> head;
        list -> head -> prev = node;
        list -> head = node;
    }

    // Increment length of the list 
    list -> length += 1;

    return node;
}


// Function to remove the specified node from the linked list. The specified node
// is compared to the nodes in the linked list by reference equality to find
// the node to be deleted
// Arguments: 
//     list: The linked list from which to remove the specified node
//     node: The node to remove 
// Returns: 
//     -1 if the specified node is not present in the linked list, 0 otherwise
int removeNode(linkedList *list, lNode *node) {
    if ((list -> length) == 0) return -1;
    if ((list -> length) == 1) {
        if ((list -> head) == node) {
            list -> head = list -> tail = NULL;
            free(node -> payload);
            free(node);
        } else return -1;
    } else { // If length of list is more than 1, iterate through the list to check
             // if node is present in the list
        lNode *currNode = list -> head;
        while (currNode != node) {
            if (currNode == NULL) return -1;
            currNode = currNode -> next;
        }
        // If we get to here, we know node exists in the list
        if ((node -> next) == NULL) { // node is the tail of the list
            node -> prev -> next = NULL;
            list -> tail = node -> prev;
            free(node -> payload);
            free(node);
        } else if ((node -> prev) == NULL) { // node is the head of the list
            node -> next -> prev = NULL;
            list -> head = node -> next;
            free(node -> payload);
            free(node);
        } else { // node is in the middle of the list
            node -> prev -> next = node -> next;
            node -> next -> prev = node -> prev;
            free(node -> payload);
            free(node);
        }
    }

    // Decrement the length of list
    list -> length -= 1;

    return 0;
}

// Function to create a linked list
// Arguments: 
//     None
// Returns: 
//     Pointer to the newly created list
linkedList *createList(void) {
    linkedList *list = malloc(sizeof(linkedList));
    list -> length = 0;
    list -> head = list -> tail = NULL;
    return list;
}

// Function to find a node in the specified list given the node's payload
// Arguments: 
//     list: The list in which to search for the node
//     payload: A pointer to the payload of the node to find
// Returns: 
//     NULL if no matching node is found, otherwise a pointer to the found node 
lNode *findNode(linkedList *list, void *payload) {
    lNode *currNode = list -> head;
    while (currNode != NULL) {
        if (currNode -> payload == payload) return currNode;
        currNode = currNode -> next;
    }

    return NULL;
}


// Function to free memory of the specified list. The list may be nonempty
// Arguments: 
//     list: The list whose memory to free
// Returns: 
//     None
void freeList(linkedList *list) {
    lNode *currNode = list -> head;
    while (currNode != NULL) {
        lNode *nextNode = currNode -> next;
        removeNode(list, currNode);
        currNode = nextNode;
    }

    free(list);
}

