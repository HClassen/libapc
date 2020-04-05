#ifndef QUEUE_HEADER
#define QUEUE_HEADER

typedef struct _queue{
    struct _queue *next;
    struct _queue *prev;
}queue;

#define QUEUE_NEXT(q) ((q)->next)
#define QUEUE_PREV(q) ((q)->prev)
#define QUEUE_PREV_NEXT(q) QUEUE_PREV(QUEUE_NEXT(q))
#define QUEUE_NEXT_PREV(q) QUEUE_NEXT(QUEUE_PREV(q))

#define QUEUE_INIT(q)                       \
    do{                                     \
        (q)->prev = q;                      \
        (q)->next = q;                      \
    }while(0)

#define QUEUE_EMPTY(q)                      \
    ((q) == QUEUE_NEXT(q) && (q) == QUEUE_PREV(q))
   
#define QUEUE_ADD(q, n)                     \
    do{                                     \
        QUEUE_PREV(QUEUE_NEXT(q)) = (n);    \
        QUEUE_NEXT(n) = QUEUE_NEXT(q);      \
        QUEUE_NEXT(q) = (n);                \
        QUEUE_PREV(n) = (q);                \
    }while(0)

#define QUEUE_ADD_HEAD(q, n)                \
    do{                                     \
        QUEUE_NEXT(n) = QUEUE_NEXT(q);      \
        QUEUE_PREV_NEXT(q) = (n);           \
        QUEUE_PREV(n) = (q);                \
        QUEUE_NEXT(q) = (n);                \
    }while(0)

#define QUEUE_ADD_HEAD_GET(q, n, h)         \
    do{                                     \
        QUEUE_ADD_HEAD(q, n);               \
        (h) = (n);                          \
    }while(0)

#define QUEUE_ADD_TAIL(q, n)                \
    do{                                     \
        QUEUE_PREV(n) = QUEUE_PREV(q);      \
        QUEUE_NEXT_PREV(q) = (n);           \
        QUEUE_NEXT(n) = (q);                \
        QUEUE_PREV(q) = (n);                \
    }while(0)

#define QUEUE_ADD_TAIL_GET(q, n, t)         \
    do{                                     \
        QUEUE_ADD_TAIL(q, n);               \
        (t) = (n);                          \
    }while(0)

#define QUEUE_REMOVE(q)                     \
    do{                                     \
        QUEUE_NEXT_PREV(q) = QUEUE_NEXT(q); \
        QUEUE_PREV_NEXT(q) = QUEUE_PREV(q); \
    }while(0)

#define QUEUE_MOVE(q, n)                    \
    do{                                     \
        if(QUEUE_EMPTY(q)){                 \
            QUEUE_INIT(n);                  \
        }else{                              \
            QUEUE_NEXT(n) = QUEUE_NEXT(q);  \
            QUEUE_PREV(n) = QUEUE_PREV(q);  \
            QUEUE_NEXT_PREV(n) = (n);       \
            QUEUE_PREV_NEXT(n) = (n);       \
            QUEUE_INIT(q);                  \
        }                                   \
    }while(0)

#endif