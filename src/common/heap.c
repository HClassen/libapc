#include <stddef.h>

#include "heap.h"

void heap_init(heap *heap, node_cmp cmp){
    if(heap == NULL){
        return;
    }
    heap->head = NULL;
    heap->nr_nodes = 0;
    heap->cmp = cmp;
}

heap_node* heap_peek_head(heap *heap){
    return heap->head;
}

static void heap_swap_nodes(heap *heap, heap_node *child, heap_node *parent){
    if(child == NULL || parent == NULL){
        return;
    }
    
    heap_node *sibling, t;

    t = *parent;
    *parent = *child;
    *child = t;

    parent->parent = child;
    if(child->left == child){
        child->left = parent;
        sibling = child->right;
    }else{
        child->right = parent;
        sibling = child->left;
    }

    if(sibling != NULL){
        sibling->parent = child;
    }

    if(parent->left != NULL){
        parent->left->parent = parent;
    }
    if(parent->right != NULL){
        parent->right->parent = parent;
    }

    if(child->parent == NULL){
        heap->head = child;
    }else if(child->parent->left == parent){
        child->parent->left = child;
    }else{
        child->parent->right = child;
    }
}

static void heap_decrease(heap *heap, heap_node *start){
    while(start->parent != NULL && heap->cmp(start, start->parent)){
        heap_swap_nodes(heap, start, start->parent);
    }
}

static void heap_heapify(heap *heap, heap_node *start){
    heap_node *tmp = NULL;
    while(1){
        tmp = start;
        if(start->left != NULL && !heap->cmp(tmp, start->left)){
            tmp = start->left;
        }
        if(start->right != NULL && !heap->cmp(tmp, start->right)){
            tmp = start->right;
        }
        if(tmp == start){
            break;
        }
        heap_swap_nodes(heap, tmp, start);
    }    
}

void heap_insert(heap *heap, heap_node *new){
    if(heap == NULL || new == NULL){
        return;
    }

    new->left = NULL;
    new->right = NULL;
    new->parent = NULL;

    //calculate path through heap
    int path = 0, i, j;
    for(i = 0, j = heap->nr_nodes + 1; j>=2; i += 1, j /= 2){
        path = (path << 1) | (j & 1);
    }

    
    heap_node **child, **parent;
    parent = child = &heap->head;
    for(; i>0; i -= 1){
        parent = child;
        if(path & 1){
            child = &(*child)->right;
        }else{
            child = &(*child)->left;
        }
        path >>= 1;
    }

    new->parent = *parent;
    *child = new;
    heap->nr_nodes += 1;

    heap_decrease(heap, new);
}

void heap_remove(heap *heap, heap_node *node){
    if(heap == NULL || heap->nr_nodes == 0){
        return;
    }

    //calculate path through heap
    int path = 0, i, j;
    for(i = 0, j = heap->nr_nodes; j>=2; i += 1, j /= 2){
        path = (path << 1) | (j & 1);
    }

    heap_node **last = &heap->head;
    for(; i>0; i -= 1){
        if(path & 1){
            last = &(*last)->right;
        }else{
            last = &(*last)->left;
        }
        path >>= 1;
    }

    heap->nr_nodes -= 1;

    heap_node *tmp = *last;
    *last = NULL;

    if(tmp == node){
        if(heap->head == node){
            heap->head = NULL;
        }
        return;
    }

    tmp->left = node->left;
    tmp->right = node->right;
    tmp->parent = node->parent;

    if(tmp->left != NULL){
        tmp->left->parent = tmp;
    }
    if(tmp->right != NULL){
        tmp->right->parent = tmp;
    }

    if(node->parent == NULL){
        heap->head = tmp;
    }else if(node->parent->left == node){
        node->parent->left = tmp;
    }else{
        node->parent->right = tmp;
    }

    heap_heapify(heap, tmp);
    heap_decrease(heap, tmp);
}

heap_node* heap_dequeue_head(heap *heap){
    heap_node *tmp = heap_peek_head(heap);
    heap_remove(heap, tmp);
    return tmp;
}