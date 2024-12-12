#include <hqueue.h>
#include <pool.h>

static RetVal serialEnqueue(void *state, ArgVal arg, int pid);
static RetVal serialDequeue(void *state, ArgVal arg, int pid);

static _Thread_local SynchPoolStruct pool_node CACHE_ALIGN;

void HQueueInit(HQueueStruct *queue_object_struct, uint32_t nthreads, uint32_t numa_nodes) {
    queue_object_struct->enqueue_struct = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(HSynchStruct));
    queue_object_struct->dequeue_struct = synchGetAlignedMemory(S_CACHE_LINE_SIZE, sizeof(HSynchStruct));
    HSynchStructInit(queue_object_struct->enqueue_struct, nthreads, numa_nodes);
    HSynchStructInit(queue_object_struct->dequeue_struct, nthreads, numa_nodes);
    queue_object_struct->guard.val = GUARD_VALUE;
    queue_object_struct->guard.next = NULL;
    queue_object_struct->first = &queue_object_struct->guard;
    queue_object_struct->last = &queue_object_struct->guard;
}

void HQueueThreadStateInit(HQueueStruct *object_struct, HQueueThreadState *lobject_struct, int pid) {
    HSynchThreadStateInit(object_struct->enqueue_struct, &lobject_struct->enqueue_thread_state, (int)pid);
    HSynchThreadStateInit(object_struct->dequeue_struct, &lobject_struct->dequeue_thread_state, (int)pid);
    synchInitPool(&pool_node, sizeof(Node));
}

static RetVal serialEnqueue(void *state, ArgVal arg, int pid) {
    HQueueStruct *st = (HQueueStruct *)state;
    Node *node;

    node = synchAllocObj(&pool_node);
    node->next = NULL;
    node->val = arg;
    st->last->next = node;
    st->last = node;
    synchNonTSOFence();
    return ENQUEUE_SUCCESS;
}

static RetVal serialDequeue(void *state, ArgVal arg, int pid) {
    HQueueStruct *st = (HQueueStruct *)state;
    volatile Node *node, *prev;

    if (st->first->next != NULL){
        prev = st->first;
        st->first = st->first->next;
        node = st->first;
        if (node->val == GUARD_VALUE)
            return serialDequeue(state, arg, pid);
        synchNonTSOFence();
        synchRecycleObj(&pool_node, (Node *)prev);
        return node->val;
    } else {
        return EMPTY_QUEUE;
    }
}

void HQueueApplyEnqueue(HQueueStruct *object_struct, HQueueThreadState *lobject_struct, ArgVal arg, int pid) {
    HSynchApplyOp(object_struct->enqueue_struct, &lobject_struct->enqueue_thread_state, serialEnqueue, object_struct, (ArgVal)arg, pid);
}

RetVal HQueueApplyDequeue(HQueueStruct *object_struct, HQueueThreadState *lobject_struct, int pid) {
    return HSynchApplyOp(object_struct->dequeue_struct, &lobject_struct->dequeue_thread_state, serialDequeue, object_struct, (ArgVal)pid, pid);
}
