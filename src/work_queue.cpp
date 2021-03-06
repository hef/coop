#include <coop/detail/work_queue.hpp>

#include <coop/detail/tracer.hpp>

#ifdef _WIN32
#    define WIN32_LEAN_AND_MEAN
#    include <Windows.h>
#elif defined(__linux__)
#elif (__APPLE__)
#endif

using namespace coop;
using namespace coop::detail;

work_queue_t::work_queue_t(scheduler_t& scheduler, uint32_t id)
    : scheduler_{scheduler}
    , id_{id}
{
    sprintf_s(label_, sizeof(label_), "work_queue:%i", id);
    event_.init(false, label_);
    active_ = true;
    thread_ = std::thread([this] {
#if defined(_WIN32)
        SetThreadAffinityMask(
            thread_.native_handle(), static_cast<uint32_t>(1ull << id_));
#elif defined(__linux__)
    // TODO: Android/Linux implementation
#elif (__APPLE__)
    // TODO: MacOS/iOS implementation
#endif

        while (true)
        {
            event_.wait();

            for (int i = COOP_PRIORITY_COUNT - 1; i >= 0; --i)
            {
                std::coroutine_handle<> coroutine;
                while (queues_[i].try_dequeue(coroutine))
                {
                    COOP_LOG("Dequeueing coroutine on CPU %i thread %i\n",
                             id_,
                             std::this_thread::get_id());
                    coroutine.resume();
                }
            }

            // TODO: Implement some sort of work stealing here

            if (!active_)
            {
                return;
            }
        }
    });
}

work_queue_t::~work_queue_t() noexcept
{
    active_ = false;
    event_.signal();
    thread_.join();
}

void work_queue_t::enqueue(std::coroutine_handle<> coroutine,
                           uint32_t priority,
                           source_location_t source_location)
{
    COOP_LOG("Enqueueing coroutine on CPU %i (%s:%zu)\n",
             id_,
             source_location.file,
             source_location.line);
    queues_[priority].enqueue(coroutine);
    event_.signal();
}
