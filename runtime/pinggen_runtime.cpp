#include <windows.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

namespace {

typedef void (*TaskFn)(void*);

struct ConGroup {
    volatile LONG pending;
    SRWLOCK lock;
    CONDITION_VARIABLE cv;
};

struct Task {
    ConGroup* group;
    TaskFn fn;
    void* ctx;
};

struct TaskDeque {
    SRWLOCK lock;
    Task* buffer;
    size_t capacity;
    size_t head;
    size_t tail;
};

struct Scheduler;

struct Worker {
    Scheduler* scheduler;
    size_t index;
    HANDLE thread;
    TaskDeque deque;
};

struct Scheduler {
    Worker* workers;
    size_t worker_count;
    volatile LONG shutdown;
    volatile LONG next_worker;
    SRWLOCK wake_lock;
    CONDITION_VARIABLE wake_cv;
};

struct VecHandle {
    void* data;
    int64_t length;
    int64_t capacity;
    int64_t element_size;
};

INIT_ONCE g_scheduler_once = INIT_ONCE_STATIC_INIT;
Scheduler g_scheduler = {};

void init_deque(TaskDeque* deque) {
    InitializeSRWLock(&deque->lock);
    deque->capacity = 32;
    deque->buffer = static_cast<Task*>(malloc(sizeof(Task) * deque->capacity));
    deque->head = 0;
    deque->tail = 0;
}

size_t deque_size(const TaskDeque* deque) { return deque->tail - deque->head; }

void grow_deque(TaskDeque* deque) {
    const size_t old_size = deque_size(deque);
    const size_t new_capacity = deque->capacity * 2;
    Task* next = static_cast<Task*>(malloc(sizeof(Task) * new_capacity));
    for (size_t i = 0; i < old_size; ++i) {
        next[i] = deque->buffer[(deque->head + i) % deque->capacity];
    }
    free(deque->buffer);
    deque->buffer = next;
    deque->capacity = new_capacity;
    deque->head = 0;
    deque->tail = old_size;
}

void push_bottom(TaskDeque* deque, Task task) {
    AcquireSRWLockExclusive(&deque->lock);
    if (deque_size(deque) + 1 >= deque->capacity) {
        grow_deque(deque);
    }
    deque->buffer[deque->tail % deque->capacity] = task;
    ++deque->tail;
    ReleaseSRWLockExclusive(&deque->lock);
}

BOOL pop_bottom(TaskDeque* deque, Task* out) {
    AcquireSRWLockExclusive(&deque->lock);
    if (deque->head == deque->tail) {
        ReleaseSRWLockExclusive(&deque->lock);
        return FALSE;
    }
    --deque->tail;
    *out = deque->buffer[deque->tail % deque->capacity];
    ReleaseSRWLockExclusive(&deque->lock);
    return TRUE;
}

BOOL steal_top(TaskDeque* deque, Task* out) {
    AcquireSRWLockExclusive(&deque->lock);
    if (deque->head == deque->tail) {
        ReleaseSRWLockExclusive(&deque->lock);
        return FALSE;
    }
    *out = deque->buffer[deque->head % deque->capacity];
    ++deque->head;
    ReleaseSRWLockExclusive(&deque->lock);
    return TRUE;
}

void finish_task(Task task) {
    task.fn(task.ctx);
    if (InterlockedDecrement(&task.group->pending) == 0) {
        AcquireSRWLockExclusive(&task.group->lock);
        WakeAllConditionVariable(&task.group->cv);
        ReleaseSRWLockExclusive(&task.group->lock);
    }
}

BOOL steal_from_other_worker(Scheduler* scheduler, size_t self_index, Task* out) {
    for (size_t offset = 1; offset < scheduler->worker_count; ++offset) {
        const size_t victim = (self_index + offset) % scheduler->worker_count;
        if (steal_top(&scheduler->workers[victim].deque, out)) {
            return TRUE;
        }
    }
    return FALSE;
}

DWORD WINAPI worker_main(LPVOID raw_worker) {
    Worker* worker = static_cast<Worker*>(raw_worker);
    Scheduler* scheduler = worker->scheduler;
    for (;;) {
        if (InterlockedCompareExchange(&scheduler->shutdown, 0, 0) != 0) {
            return 0;
        }

        Task task;
        if (pop_bottom(&worker->deque, &task) || steal_from_other_worker(scheduler, worker->index, &task)) {
            finish_task(task);
            continue;
        }

        AcquireSRWLockExclusive(&scheduler->wake_lock);
        SleepConditionVariableSRW(&scheduler->wake_cv, &scheduler->wake_lock, 1, 0);
        ReleaseSRWLockExclusive(&scheduler->wake_lock);
    }
}

BOOL CALLBACK init_scheduler(PINIT_ONCE, PVOID, PVOID*) {
    DWORD worker_count = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (worker_count == 0) {
        worker_count = 4;
    }
    g_scheduler.worker_count = worker_count;
    g_scheduler.workers = static_cast<Worker*>(malloc(sizeof(Worker) * g_scheduler.worker_count));
    g_scheduler.shutdown = 0;
    g_scheduler.next_worker = 0;
    InitializeSRWLock(&g_scheduler.wake_lock);
    InitializeConditionVariable(&g_scheduler.wake_cv);

    for (size_t i = 0; i < g_scheduler.worker_count; ++i) {
        Worker* worker = &g_scheduler.workers[i];
        worker->scheduler = &g_scheduler;
        worker->index = i;
        init_deque(&worker->deque);
        worker->thread = CreateThread(nullptr, 0, worker_main, worker, 0, nullptr);
    }

    return TRUE;
}

Scheduler* scheduler_instance() {
    InitOnceExecuteOnce(&g_scheduler_once, init_scheduler, nullptr, nullptr);
    return &g_scheduler;
}

}  // namespace

extern "C" {

void* pinggen_con_group_create(int64_t task_count) {
    ConGroup* group = static_cast<ConGroup*>(malloc(sizeof(ConGroup)));
    group->pending = static_cast<LONG>(task_count);
    InitializeSRWLock(&group->lock);
    InitializeConditionVariable(&group->cv);
    return group;
}

void* pinggen_vec_create(int64_t element_size, int64_t initial_capacity) {
    VecHandle* vec = static_cast<VecHandle*>(malloc(sizeof(VecHandle)));
    vec->length = 0;
    vec->capacity = initial_capacity > 0 ? initial_capacity : 0;
    vec->element_size = element_size;
    vec->data = vec->capacity > 0 ? malloc(static_cast<size_t>(vec->capacity * vec->element_size)) : nullptr;
    return vec;
}

int64_t pinggen_vec_len(void* raw_vec) {
    VecHandle* vec = static_cast<VecHandle*>(raw_vec);
    return vec->length;
}

void* pinggen_vec_data(void* raw_vec) {
    VecHandle* vec = static_cast<VecHandle*>(raw_vec);
    return vec->data;
}

void pinggen_vec_push(void* raw_vec, void* element) {
    VecHandle* vec = static_cast<VecHandle*>(raw_vec);
    if (vec->length == vec->capacity) {
        const int64_t next_capacity = vec->capacity == 0 ? 4 : vec->capacity * 2;
        vec->data = realloc(vec->data, static_cast<size_t>(next_capacity * vec->element_size));
        vec->capacity = next_capacity;
    }
    void* destination = static_cast<unsigned char*>(vec->data) + (vec->length * vec->element_size);
    memcpy(destination, element, static_cast<size_t>(vec->element_size));
    ++vec->length;
}

void pinggen_con_spawn(void* raw_group, void* raw_fn, void* ctx) {
    Scheduler* scheduler = scheduler_instance();
    const size_t index =
        static_cast<size_t>(InterlockedIncrement(&scheduler->next_worker) - 1) % scheduler->worker_count;
    Task task;
    task.group = static_cast<ConGroup*>(raw_group);
    task.fn = reinterpret_cast<TaskFn>(raw_fn);
    task.ctx = ctx;
    push_bottom(&scheduler->workers[index].deque, task);
    WakeConditionVariable(&scheduler->wake_cv);
}

void pinggen_con_wait(void* raw_group) {
    ConGroup* group = static_cast<ConGroup*>(raw_group);
    AcquireSRWLockExclusive(&group->lock);
    while (InterlockedCompareExchange(&group->pending, 0, 0) != 0) {
        SleepConditionVariableSRW(&group->cv, &group->lock, INFINITE, 0);
    }
    ReleaseSRWLockExclusive(&group->lock);
    free(group);
}

}
