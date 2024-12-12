#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdint.h>

#include <sim.h>
#include <barrier.h>
#include <bench_args.h>
#include <fam.h>
#include <fastrand.h>
#include <threadtools.h>

SimStruct *sim_struct CACHE_ALIGN;
int64_t d1 CACHE_ALIGN, d2;
SynchBarrier bar CACHE_ALIGN;
SynchBenchArgs bench_args CACHE_ALIGN;
int MAX_BACK CACHE_ALIGN;

static void *Execute(void *Arg) {
    SimThreadState th_state;
    long i, rnum;
    int id = synchGetThreadId();
    volatile long j;

    SimThreadStateInit(&th_state, bench_args.nthreads, id);
    synchFastRandomSetSeed((unsigned long)id + 1);
    synchBarrierWait(&bar);
    if (id == 0) d1 = synchGetTimeMillis();

    for (i = 0; i < bench_args.runs; i++) {
        SimApplyOp(sim_struct, &th_state, fetchAndMultiply, (Object)(id + 1), id);
        rnum = synchFastRandomRange(1, bench_args.max_work);
        for (j = 0; j < rnum; j++)
            ;
    }
    synchBarrierWait(&bar);
    if (id == 0) d2 = synchGetTimeMillis();

    return NULL;
}

int main(int argc, char *argv[]) {
    synchParseArguments(&bench_args, argc, argv);
    sim_struct = synchGetAlignedMemory(CACHE_LINE_SIZE, sizeof(SimStruct));
    synchSimStructInit(sim_struct, bench_args.nthreads, bench_args.backoff_high);
    synchBarrierSet(&bar, bench_args.nthreads);
    synchStartThreadsN(bench_args.nthreads, Execute, bench_args.fibers_per_thread);
    synchJoinThreadsN(bench_args.nthreads);

    printf("time: %d (ms)\tthroughput: %.2f (millions ops/sec)\t", (int)(d2 - d1), bench_args.runs * bench_args.nthreads / (1000.0 * (d2 - d1)));
    synchPrintStats(bench_args.nthreads, bench_args.total_runs);

#ifdef DEBUG
    SimObjectState *l = (SimObjectState *)sim_struct->pool[((pointer_t *)&sim_struct->sp)->struct_data.index];
    fprintf(stderr, "DEBUG: Object state: %d\n", l->counter);
    fprintf(stderr, "DEBUG: rounds: %d\n", l->rounds);
    fprintf(stderr, "DEBUG: Average helping: %f\n", (float)l->counter / l->rounds);
#endif

    return 0;
}
