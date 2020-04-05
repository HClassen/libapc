#ifndef APC_HEAP_HEADER
#define APC_HEAP_HEADER

typedef struct heap_node_{
    struct heap_node_ *left;
    struct heap_node_ *right;
    struct heap_node_ *parent;
}heap_node;

typedef int (*node_cmp)(heap_node *a, heap_node *b); /* return 1 if a comes before b, 0 else */

typedef struct heap_{
    heap_node *head;
    int nr_nodes;
    node_cmp cmp;
}heap;


/* inits a heap
 * @param heap - pointer to preallocated memory for a heap struct
 * @param cmp - function pointer to a compare function
 * @return void
 */
void heap_init(heap *heap, node_cmp cmp);

/* peek first element in heap
 * @param heap - pointer to heap
 * @return pointer to first element in heap
 */
heap_node* heap_peek_head(heap *heap);

/* remove first element in heap
 * @param heap - pointer to heap
 * @return pointer to first element in heap
 */
heap_node* heap_dequeue_head(heap *heap);

/* inesrt element into heap
 * @param heap - pointer to heap
 * @param new - pointer to new heap_node
 * @return void
 */
void heap_insert(heap *heap, heap_node *node);

/* removes element from heap
 * @param heap - pointer to heap
 * @param new - pointer to heap_node to remove
 * @return void
 */
void heap_remove(heap *heap, heap_node *node);

#endif