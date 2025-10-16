#pragma once

#include "lsapi.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

class TaskExecutor {
public:
    TaskExecutor();
    ~TaskExecutor();

    LSTASKHANDLE Submit(LSTASKEXECUTEPROC executeProc,
                        LPVOID executeContext,
                        LSTASKCOMPLETIONPROC completionProc,
                        LPVOID completionContext);

    bool Cancel(LSTASKHANDLE handle);
    bool Wait(LSTASKHANDLE handle, DWORD timeoutMs);
    void ProcessCompletionPayload(void* payload);
    void Shutdown();

private:
    struct TaskRecord;
    struct CompletionPayload;

    void WorkerLoop();
    void EnqueueCompletion(const std::shared_ptr<TaskRecord>& task, BOOL cancelled);
    void FinalizeTask(const std::shared_ptr<TaskRecord>& task);

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::shared_ptr<TaskRecord>> m_queue;
    std::unordered_map<LSTASKHANDLE, std::shared_ptr<TaskRecord>> m_tasks;
    std::vector<std::thread> m_workers;
    std::atomic<bool> m_stopping;
    std::atomic_uint64_t m_nextId;
};
