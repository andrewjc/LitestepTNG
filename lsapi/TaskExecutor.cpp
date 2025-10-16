#include "TaskExecutor.h"

#include "lsapidefines.h"

#include <chrono>

namespace {
constexpr size_t kMinThreads = 2;
constexpr size_t kMaxThreads = 4;
}

struct TaskExecutor::TaskRecord {
    LSTASKHANDLE id = 0;
    LSTASKEXECUTEPROC executeProc = nullptr;
    LPVOID executeContext = nullptr;
    LSTASKCOMPLETIONPROC completionProc = nullptr;
    LPVOID completionContext = nullptr;
    std::atomic<bool> cancelled{ false };
    std::mutex stateMutex;
    std::condition_variable stateCv;
    bool finished = false;
};

struct TaskExecutor::CompletionPayload {
    std::shared_ptr<TaskRecord> task;
    BOOL cancelled = FALSE;
};

TaskExecutor::TaskExecutor()
    : m_stopping(false)
    , m_nextId(1) {
    size_t hardware = std::thread::hardware_concurrency();
    size_t workerCount = 3;
    if (hardware > 0) {
        size_t suggested = hardware / 2;
        if (suggested < kMinThreads) {
            suggested = kMinThreads;
        }
        if (suggested > kMaxThreads) {
            suggested = kMaxThreads;
        }
        workerCount = suggested;
    }

    if (workerCount == 0) {
        workerCount = kMinThreads;
    }

    m_workers.reserve(workerCount);
    for (size_t i = 0; i < workerCount; ++i) {
        m_workers.emplace_back(&TaskExecutor::WorkerLoop, this);
    }
}

TaskExecutor::~TaskExecutor() {
    Shutdown();
}

LSTASKHANDLE TaskExecutor::Submit(LSTASKEXECUTEPROC executeProc,
                                  LPVOID executeContext,
                                  LSTASKCOMPLETIONPROC completionProc,
                                  LPVOID completionContext) {
    if (m_stopping.load(std::memory_order_acquire)) {
        return 0;
    }
    if (!executeProc) {
        return 0;
    }

    auto task = std::make_shared<TaskRecord>();
    task->id = m_nextId.fetch_add(1, std::memory_order_relaxed);
    task->executeProc = executeProc;
    task->executeContext = executeContext;
    task->completionProc = completionProc;
    task->completionContext = completionContext;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping.load(std::memory_order_acquire)) {
            return 0;
        }
        m_queue.push_back(task);
        m_tasks.emplace(task->id, task);
    }

    m_cv.notify_one();
    return task->id;
}

bool TaskExecutor::Cancel(LSTASKHANDLE handle) {
    std::shared_ptr<TaskRecord> task;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(handle);
        if (it == m_tasks.end()) {
            return false;
        }
        task = it->second;
    }

    task->cancelled.store(true, std::memory_order_release);
    return true;
}

bool TaskExecutor::Wait(LSTASKHANDLE handle, DWORD timeoutMs) {
    std::shared_ptr<TaskRecord> task;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(handle);
        if (it == m_tasks.end()) {
            return true;
        }
        task = it->second;
    }

    std::unique_lock<std::mutex> lock(task->stateMutex);
    if (timeoutMs == INFINITE) {
        task->stateCv.wait(lock, [&] { return task->finished; });
        return true;
    }

    return task->stateCv.wait_for(
        lock,
        std::chrono::milliseconds(timeoutMs),
        [&] { return task->finished; });
}

void TaskExecutor::ProcessCompletionPayload(void* payload) {
    if (!payload) {
        return;
    }

    std::unique_ptr<CompletionPayload> holder(static_cast<CompletionPayload*>(payload));
    auto task = holder->task;
    if (task && task->completionProc) {
        task->completionProc(task->completionContext, holder->cancelled);
    }
    if (task) {
        FinalizeTask(task);
    }
}

void TaskExecutor::Shutdown() {
    bool expected = false;
    if (!m_stopping.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    m_cv.notify_all();
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();

    std::vector<std::shared_ptr<TaskRecord>> remaining;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& pair : m_tasks) {
            remaining.push_back(pair.second);
        }
        m_tasks.clear();
        m_queue.clear();
    }

    for (auto& task : remaining) {
        if (task->completionProc) {
            task->completionProc(task->completionContext, TRUE);
        }
        {
            std::lock_guard<std::mutex> lock(task->stateMutex);
            task->finished = true;
        }
        task->stateCv.notify_all();
    }
}

void TaskExecutor::WorkerLoop() {
    for (;;) {
        std::shared_ptr<TaskRecord> task;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [&] { return m_stopping.load(std::memory_order_acquire) || !m_queue.empty(); });

            if (m_stopping.load(std::memory_order_acquire) && m_queue.empty()) {
                return;
            }

            if (!m_queue.empty()) {
                task = std::move(m_queue.front());
                m_queue.pop_front();
            }
        }

        if (!task) {
            continue;
        }

        if (!task->cancelled.load(std::memory_order_acquire)) {
            task->executeProc(task->executeContext);
        }

        if (task->completionProc) {
            EnqueueCompletion(task, task->cancelled.load(std::memory_order_acquire));
        } else {
            FinalizeTask(task);
        }
    }
}

void TaskExecutor::EnqueueCompletion(const std::shared_ptr<TaskRecord>& task, BOOL cancelled) {
    auto payload = new (std::nothrow) CompletionPayload();
    if (!payload) {
        if (task->completionProc) {
            task->completionProc(task->completionContext, cancelled);
        }
        FinalizeTask(task);
        return;
    }

    payload->task = task;
    payload->cancelled = cancelled;

    HWND target = GetLitestepWnd();
    if (!target || !PostMessage(target, LM_ASYNCTASKCOMPLETE, reinterpret_cast<WPARAM>(payload), 0)) {
        ProcessCompletionPayload(payload);
    }
}

void TaskExecutor::FinalizeTask(const std::shared_ptr<TaskRecord>& task) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.erase(task->id);
    }

    {
        std::lock_guard<std::mutex> lock(task->stateMutex);
        task->finished = true;
    }
    task->stateCv.notify_all();
}
