#ifndef TASK_SCHEDULER_GLSL
#define TASK_SCHEDULER_GLSL 1

#ifndef SCHEDULER_DATA_ADDRESS
#define SCHEDULER_DATA_ADDRESS pc.schedulerDataAddress
#endif

#ifndef SCHEDULER_GLOBAL_TASK_ID
struct Task {
    uint phase;
    uint id;
};
#else
struct Task {
    uint phase;
    uint id;
    uint idGlobal;
};
#endif

struct Scheduler {
    uint phase;
    uint idStart;
    uint idEnd;
    uint idToAssign;
    uint numFinished;
};

layout (buffer_reference, scalar) buffer SchedRef { Scheduler sched; };
shared uint sharedTaskId;

void allocTasks(in uint numTasks, in uint phase)
{
    SchedRef data = SchedRef(SCHEDULER_DATA_ADDRESS);

    const uint end = data.sched.idEnd;
    data.sched.phase = phase;
    data.sched.idStart = end;
    memoryBarrier(gl_ScopeDevice, gl_StorageSemanticsBuffer,
                  gl_SemanticsAcquireRelease | gl_SemanticsMakeAvailable | gl_SemanticsMakeVisible);
    data.sched.idEnd = end + numTasks;
}

Task beginTask(const uint localId)
{
    SchedRef data = SchedRef(SCHEDULER_DATA_ADDRESS);

    if (localId == 0)
        sharedTaskId = atomicAdd(data.sched.idToAssign, 1);
    barrier();
    const uint myTaskId = sharedTaskId;

    do {
        memoryBarrier(gl_ScopeDevice, gl_StorageSemanticsBuffer,
                      gl_SemanticsAcquireRelease | gl_SemanticsMakeAvailable | gl_SemanticsMakeVisible);
    } while (myTaskId >= data.sched.idEnd);

#ifndef SCHEDULER_GLOBAL_TASK_ID
    return Task(data.sched.phase, myTaskId - data.sched.idStart);
#else
    return Task(data.sched.phase, myTaskId - data.sched.idStart, myTaskId);
#endif
}

bool endTask(const uint localId)
{
    SchedRef data = SchedRef(SCHEDULER_DATA_ADDRESS);

    controlBarrier(gl_ScopeWorkgroup, gl_ScopeDevice, gl_StorageSemanticsBuffer,
                   gl_SemanticsAcquireRelease | gl_SemanticsMakeAvailable | gl_SemanticsMakeVisible);
    if (localId == 0) {
        const uint endPhaseIndex = data.sched.idEnd;
        const uint finishedTaskId = atomicAdd(data.sched.numFinished, 1);
        return (finishedTaskId == (endPhaseIndex - 1));
    }
    return false;
}

uint getTaskCount()
{
    SchedRef data = SchedRef(SCHEDULER_DATA_ADDRESS);
    return data.sched.idEnd - data.sched.idStart;
}

#endif