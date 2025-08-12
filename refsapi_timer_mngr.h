#include "precompiled.h"

#ifndef WITHOUT_TIMER

/*
#ifdef NDEBUG
    #ifdef DEBUG
        #undef DEBUG
    #endif
    #define DEBUG(...)  debug(__VA_ARGS__)    
#endif
*/

typedef enum TIMER_FLAGS_e
{
    TIMER_FLAG_NONE,
    TIMER_FLAG_ONCE     = (1 << 0),
    TIMER_FLAG_REPEAT   = (1 << 1),
    TIMER_FLAG_LOOP     = (1 << 2),
    TIMER_FLAG_START    = (1 << 3),
    TIMER_FLAG_STOP     = (1 << 4),
    TIMER_FLAG_RELATIVE = (1 << 5),
    // Count
    TIMER_FLAG_SIZE   = 7
} TIMER_FLAGS_t;

#ifdef WITHOUT_SQL
typedef struct fwd_prm_s
{
    int id;
    AMX *amx;
} fwd_prms_t;
typedef std::chrono::_V2::system_clock::time_point time_point_t;
typedef std::chrono::duration<double> elapsed_time_t;
#endif

typedef std::chrono::_V2::steady_clock::time_point steady_point_t;

typedef struct tm_s
{
    size_t id;
    fwd_prms_t fwd;
    size_t group_id;
    float delay_sec;
    steady_point_t time;
    cell *data;
    size_t data_size;
    int flags;
    size_t repeat;
} tm_t;

typedef std::list<tm_t> m_timer_t;
typedef m_timer_t::iterator timer_it;
typedef std::map<size_t, m_timer_t> m_timers_t;

struct TimerCompare
{
    bool operator()(const tm_t &left, const tm_t &right) const
    {
        return left.time > right.time;
    }
};

typedef std::priority_queue<tm_t, std::vector<tm_t>, TimerCompare> m_timer_queue_t;
typedef std::map<std::string, int> callback_t;

class timer_mngr
{
    std::thread *main_thread;
    m_timers_t m_timers;
    m_timer_queue_t m_timer_queue;
    callback_t m_callbacks;
    std::condition_variable cv, cv2;
    std::mutex loop_mutex, timer_mutex, frame_mutex, fwd_mutex;//, callback_mutex;
    struct timespec frame_curr, frame_prev;

private:
    void push_to_queue(tm_t &t)
    {
        m_timer_queue.push(t);
        DEBUG("%s(): add to queue: timer_id = %u, group_id = %u, queue_size = %d", __func__, t.id, t.group_id, m_timer_queue.size());
    }
    void build_queue()
    {
        m_timer_queue_t().swap(m_timer_queue);
        if (stop_timer)
            return;
        // Build new queue
        DEBUG("%s(): BUILD", __func__);
        std::lock_guard lock_1(timer_mutex);
        for (auto it : m_timers)
        {
            auto tm = &it.second;
            for (auto t = tm->begin(); t != tm->end(); t++)
                push_to_queue(*t);
        }
        // Clear loop
        //std::lock_guard lock_2(loop_mutex);
        stop_loop = false;
        DEBUG("%s(): BUILD, queue_size = %d", __func__, m_timer_queue.size());
    }
    void stop(bool all_timers = false)
    {
        // Send signal
        {
            //std::lock_guard lock(loop_mutex);
            stop_loop = true;
            if (all_timers)
                stop_timer = true;
        }
        cv.notify_one();
        DEBUG("%s(): stop loop!", __func__);
        //dump(__func__);
    }
    void dump(const char *from)
    {
        DEBUG("%s-%s(): frames_count = %lld, has_frame = %d, frame_exec = %d", from, __func__, frames_count, has_frame.load(), frame_exec.load());
        DEBUG("%s-%s(): stop_loop = %d, stop_timer = %d", from, __func__, stop_loop.load(), stop_timer.load());
        std::lock_guard lock(timer_mutex);
        for (auto it : m_timers)
        {
            auto timers = &it.second;
            if (timers->size() > 0)
            {
                DEBUG("%s-%s(): group_id = %u, count = %d", from, __func__, it.first, timers->size());
            }
        }
    }
    tm_t* get_timer_by_id(size_t id)
    {
        DEBUG("%s(): BEGIN", __func__);
        tm_t *result = nullptr;
        for (auto it : m_timers)
        {
            auto timers = &it.second;
            auto tm = std::find_if(timers->begin(), timers->end(),
                [id](const tm_t &t) {
                    return t.id == id;
                });
            if (tm == timers->end())
            {
                result = nullptr;
                continue;
            }
            result = &(*tm);
            break;
        }
        return result;
    }
    timer_it get_timer_by_id(size_t group_id, size_t id)
    {
        DEBUG("%s(): BEGIN", __func__);
        auto timers = &m_timers[group_id];
        return std::find_if(timers->begin(), timers->end(),
            [id](const tm_t &t) {
                return t.id == id;
            });
    }
    void remove_data_timer(tm_t &t)
    {
        DEBUG("%s(): timer_id = %u, group_id = %u, data = %p, size = %d", __func__, t.id, t.group_id, t.data, t.data_size);
        if (t.data == nullptr)
            return;
        delete t.data;
        t.data = nullptr;
        t.data_size = 0;
    }
    void remove_data_all_timers(m_timer_t *tm)
    {
        DEBUG("%s(): BEGIN", __func__);
        for (auto t = tm->begin(); t != tm->end(); t++)
            remove_data_timer(*t);
    }
    void remove_timer_by_id(size_t group_id, size_t id)
    {
        DEBUG("%s: timer_id = %u, group_id = %u", __func__, id, group_id);
        std::lock_guard lock(timer_mutex);
        m_timers[group_id].remove_if([&](tm_t &t) {
            if (t.id == id)
                remove_data_timer(t);
            return t.id == id;
        });
    }
    void remove_all_timers()
    {
        DEBUG("%s(): remove all timers", __func__);
        // Remove timers
        for (auto it : m_timers)
        {
            auto tm = &it.second;
            remove_data_all_timers(tm);
            tm->clear();
            m_timer_t().swap(*tm);
        }
        m_timers.clear();
        m_timers_t().swap(m_timers);
    }

public:
    uint64_t frames_count;
    double frame_delay;
    size_t frame_rate, frame_rate_max;
    std::atomic<size_t> m_timer_nums;
    std::atomic<bool> stop_main, frame_exec, has_frame;
    std::atomic<bool> stop_loop, stop_timer;
    float total_std;

    bool remove_timers(size_t group_id)
    {
        std::lock_guard lock(timer_mutex);
        auto it = m_timers.find(group_id);
        if (it == m_timers.end())
            return false;
        auto tm = &it->second;
        remove_data_all_timers(tm);
        tm->clear();
        m_timer_t().swap(*tm);
        stop();
        return true;
    }
    bool remove_timer_by_id(size_t id)
    {
        bool result;
        std::lock_guard lock(timer_mutex);
        for (auto it : m_timers)
        {
            auto tm = &it.second;
            for (auto t = tm->begin(); t != tm->end(); t++)
                if (t->id == id)
                {
                    result = true;
                    remove_data_timer(*t);
                    tm->erase(t);
                    if (!tm->size())
                        m_timer_t().swap(*tm);
                }
        }
        stop();
        return result;
    }
    bool timer_exists(size_t group_id)
    {
        std::lock_guard lock(timer_mutex);
        return m_timers.find(group_id) != m_timers.end();
    }
    bool timer_exists_by_id(size_t id)
    {
        bool result;
        {
            std::lock_guard lock(timer_mutex);
            for (auto it : m_timers)
            {
                auto timers = &it.second;
                if ((result = std::find_if(timers->begin(), timers->end(), [&](const tm_t &t) {
                        return t.id == id;
                    }) != timers->end()))
                break;
            }
        }
        stop();
        return result;
    }
    bool change_timer_by_id(size_t id, float delay_sec, int flags, size_t repeat)
    {
        {
            std::lock_guard lock(timer_mutex);
            auto tm = get_timer_by_id(id);
            if (tm == nullptr)
                return false;
            tm->delay_sec = delay_sec;
            if (flags != TIMER_FLAG_NONE)
                tm->flags = flags;
            if (repeat)
                tm->repeat = repeat;
        }
        stop();
        return true;
    }
    bool add_timer(AMX *amx, std::string name, float delay_sec, size_t group_id, cell *data, size_t data_size, int flags, size_t repeat)
    {
        DEBUG("%s(): timer_id = %u, group_id = %u, name = %s, delay = %f", __func__, m_timer_nums.load(), group_id, name.c_str(), delay_sec);
        DEBUG("%s(): data = %p, size = %u, flags = %d, repeat = %u", __func__, data, data_size, flags, repeat);
        auto fwd_id = create_forward(amx, name.c_str());
        if (amx == nullptr || fwd_id == -1)
            return false;
        tm_t t;
        t.id = m_timer_nums++;
        t.fwd.amx = amx;
        t.fwd.id = fwd_id;
        t.group_id = group_id;
        t.delay_sec = std::clamp(delay_sec, 0.005f, 9999.999f);
        t.time = std::chrono::steady_clock::now() + std::chrono::milliseconds((int64_t)(std::round(1000.0f * t.delay_sec)));
        t.data_size = clamp(data_size, 1U, 4096U);
        t.data = new cell[t.data_size]{0};
        if (data != nullptr)
            Q_memcpy(t.data, data, data_size << 2);
        t.flags = flags;
        t.repeat = repeat;
        // Add timer
        {
            std::lock_guard lock(timer_mutex);
            auto it = m_timers.find(group_id);
            if (it != m_timers.end())
            {
                auto timers = &it->second;
                timers->push_back(t);
            }
            else
                m_timers[group_id].push_back(t);
            DEBUG("%s(): group_id = %d, timer_count = %d", __func__, group_id, m_timers[group_id].size());
        }
        stop();
        return t.id;
    }
    bool exist_timer(size_t group_id)
    {
        std::lock_guard lock(timer_mutex);
        auto it = m_timers.find(group_id);
        return it != m_timers.end();
    }
    void on_client_disconnected(edict_t *p)
    {
        auto id = ENTINDEX(p);
        if (is_valid_index(id))
        {
            std::lock_guard lock(timer_mutex);
            for (auto it : m_timers)
            {
                auto group_id = it.first;
                if (group_id < 100'000 && ((group_id & 0xFF) & id))
                {
                    auto it = m_timers.find(group_id);
                    if (it != m_timers.end())
                    {
                        auto tm = &it->second;
                        remove_data_all_timers(tm);
                        tm->clear();
                        m_timer_t().swap(*tm);
                        stop();
                    }
                }
            }
        }
    }
    int create_forward(AMX *amx, std::string callback)
    {
        int fwd_id;
        //std::lock_guard lock(callback_mutex);
        auto callback_hash = callback + "_" + std::to_string(reinterpret_cast<uintptr_t>(amx));
        auto it = m_callbacks.find(callback_hash);
        if (it == m_callbacks.end())
        {
            // public TimerCallback(const group_id, const data[], const data_size, const repeat, const timer_id);
            fwd_id = g_amxxapi.RegisterSPForwardByName(amx, callback.c_str(), FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
            m_callbacks.insert({ callback_hash, fwd_id });
        }
        else
            fwd_id = it->second;
        return fwd_id;
    }
    void start()
    {
        frame_mutex.lock();
        fwd_mutex.unlock();
        // Send signal
        {
            //std::lock_guard lock(loop_mutex);
            stop_timer = false;
            stop_loop = true;
        }
        cv.notify_one();
    }
    void stop_all()
    {
        DEBUG("%s(): stop all", __func__);
        dump(__func__);
        if (!frame_mutex.try_lock() && frame_exec || frame_exec)
            fwd_mutex.lock();
        else 
            fwd_mutex.try_lock();
        dump(__func__);
        stop(true);
        DEBUG("%s(): remove all timers", __func__);
        remove_all_timers();
        // Remove callbacks
        {
            //std::lock_guard lock(callback_mutex);
            m_callbacks.clear();
            callback_t().swap(m_callbacks);
        }
        frame_mutex.unlock();
        DEBUG("%s(): RESTART TIMERS", __func__);
        if (!stop_main)
            start();
    }
    void exec_forward(tm_t &t)
    {
        int ret = 0;
        // Exec frame
        has_frame = true;
        frame_mutex.lock();
        frame_exec = true;
        auto repeat = [&](float &std) {
                if (!m_timer_queue.size())
                    return false;
                t = m_timer_queue.top();
                auto curr_time = std::chrono::steady_clock::now();
                if (t.time > curr_time)
                    return false;
                float std_delay = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - t.time).count() / 1000.0;
                std += std_delay;
                DEBUG("%s(): execute timer_id = %u, group_id = %u, init_delay = %f, std_delay = %f", __func__, t.id, t.group_id, t.delay_sec, std_delay);
                m_timer_queue.pop();
                return true;
            };
        do
        {
            DEBUG("%s(): BEGIN, timer_id = %u, group_id = %u, init_delay = %f", __func__, t.id, t.group_id, t.delay_sec);
            auto f = t.fwd;
            // Prepare array
            cell *data_tmp, data_param;
            size_t total_size = 0;
            // Callback error
            if (amx_allot(f.amx, t.data_size, &data_param, &data_tmp))
            {
                assert(t.data != nullptr);
                Q_memcpy(data_tmp, t.data, t.data_size << 2);
                total_size += t.data_size << 2;
                // public TimerCallback(const group_id, const data[], const data_size, const repeat, const timer_id);
                ret = g_amxxapi.ExecuteForward(f.id, t.group_id, data_param, t.data_size, t.repeat, t.id);
                // Fix heap
                if ((f.amx->hea - data_param) == total_size)
                    f.amx->hea = data_param;
                else
                    AMXX_LogError(f.amx, AMX_ERR_NATIVE, "%s(): can't fix amx->hea, possible memory leak!", __func__);
            }
            DEBUG("%s(): END, timer_id = %u, group_id = %u, init_delay = %f", __func__, t.id, t.group_id, t.delay_sec);
            // Check repeat or loop?
            if (!ret && ((t.flags & TIMER_FLAG_LOOP) || ((t.flags & TIMER_FLAG_REPEAT) && t.repeat > 1)))
            {   
                {
                    std::lock_guard lock(timer_mutex);
                    auto tm = get_timer_by_id(t.group_id, t.id);
                    if ((t.flags & TIMER_FLAG_RELATIVE))
                        tm->time = std::chrono::steady_clock::now() + std::chrono::milliseconds((int64_t)std::round(1000.0f * t.delay_sec));
                    else
                        tm->time += std::chrono::milliseconds((int64_t)std::round(1000.0f * t.delay_sec));
                    if ((t.flags & TIMER_FLAG_REPEAT))
                        tm->repeat--;
                    else
                        tm->repeat++;
                    push_to_queue(*tm);
                }
            }
            else
                remove_timer_by_id(t.group_id, t.id);
        } while (!ret && repeat(total_std));
        fwd_mutex.unlock();
        has_frame = false;
        frame_exec = false;
    }
    void start_frame()
    {
        if (!(frames_count % 1000))
        {
            clock_gettime(CLOCK_MONOTONIC, &frame_curr);
            if (frame_prev.tv_sec >= 0 && frame_prev.tv_nsec > 0)
            {
                auto delay = timespec_diff(&frame_prev, &frame_curr);
                if (frame_delay > 0.0) {
                    auto k1 = delay > frame_delay ? 25.0 * abs(frame_delay - delay) / (frame_delay + delay) : 1.0;
                    auto k2 = frame_delay > 1.0 ? 2.0 * frame_delay : 1.0;
                    frame_rate = (frame_rate + (uint8_t)std::round(1.0 + k1 + k2)) / 2;
                    if (frame_rate > frame_rate_max)
                        frame_rate_max = frame_rate;
                }
                frame_delay = (frame_delay + delay) / 2.0;
            }
            frame_prev = frame_curr;
            //dump(__func__);
        }
        // Check frame
        if (has_frame && !(frames_count % frame_rate) && !frame_mutex.try_lock() && !frame_exec)
        {
            if (fwd_mutex.try_lock())
            {
                frame_mutex.unlock();
                DEBUG("%s(): UNLOCK TIMER FRAME = %lld", __func__, frames_count);
                while (!fwd_mutex.try_lock())
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                //    std::this_thread::yield();
                //DEBUG("%s(): UNLOCK TIMER FORWARD = %lld", __func__, frames_count);
            }
            else
                fwd_mutex.unlock();
        }
        frames_count++;
    }
    void main()
    {
        DEBUG("%s(): start timer main", __func__);
        while (!stop_main)
        {
            build_queue();
            while (m_timer_queue.size())
            {
                DEBUG("%s(): queue_size = %d", __func__, m_timer_queue.size());
                auto t = m_timer_queue.top();
                auto curr_time = std::chrono::steady_clock::now();
                if (t.time > curr_time)
                {
                    auto delay_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t.time - std::chrono::steady_clock::now()).count();
                    DEBUG("%s(): set delay = %lld, timer_id = %u, group_id = %u, init_delay = %f", __func__, delay_ms, t.id, t.group_id, t.delay_sec);
                    std::unique_lock<std::mutex> lock(loop_mutex);
                    cv.wait_for(lock, std::chrono::milliseconds(delay_ms), [&] { return stop_timer || stop_loop; });
                }
                if (stop_timer || stop_loop)
                    break;
                float std_delay = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t.time).count() / 1000.0;
                total_std += std_delay;
                DEBUG("%s(): execute timer_id = %u, group_id = %u, init_delay = %f, std_delay = %f", __func__, t.id, t.group_id, t.delay_sec, std_delay);
                // Timer start
                m_timer_queue.pop();
                exec_forward(t);
            }
            if (!stop_timer && stop_loop)
                continue;
            // Long timer loop
            DEBUG("%s(): timer wait signal => START", __func__);
            std::unique_lock<std::mutex> lock(loop_mutex);
            cv.wait(lock, [&] { return !stop_timer && stop_loop; });
            DEBUG("%s(): timer wait signal => END", __func__);
        }
    }
    timer_mngr()
    {
        total_std = 0.0f;
        stop_timer = 
            stop_loop = 
                stop_main = false;
        frame_delay = 0.0;
        frame_rate =
            frame_rate_max = 1;
        m_timer_nums = 0;
        start();
        main_thread = new std::thread(&timer_mngr::main, this);
    }
    ~timer_mngr()
    {
        stop_main = true;
        stop_all();
        if (main_thread->joinable())
            main_thread->join();
        delete main_thread;
    }
};

/*
#ifdef NDEBUG
    #ifdef DEBUG
        #undef DEBUG
    #endif    
    #define DEBUG(...) ((void) 0)
#endif
*/

extern timer_mngr g_timer_mngr;
#endif