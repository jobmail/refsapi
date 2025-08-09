#include "precompiled.h"

#ifndef WITHOUT_SQL
//#define NON_BLOCKING_FRAME
#define MAX_QUERY_PRIORITY 5 // PRI->THREADS: 0 -> 8, 1 -> 4, 2 -> 2, 3 -> 1
#define QUERY_POOLING_INTERVAL 5
#define QUERY_RETRY_COUNT 3
#define SQL_FLAGS CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS
#define RF_MAX_FIELD_SIZE 256
#define RF_MAX_FIELD_NAME 64
#define TQUERY_CONNECT_TIMEOUT -3
#define TQUERY_CONNECT_FAILED -2
#define TQUERY_QUERY_FAILED -1
#define TQUERY_SUCCESS 0

typedef enum field_types_e
{
    FT_AUTO,
    FT_INT,
    FT_FLT,
    FT_STR,
    FT_STR_BUFF,
} field_types_t;

typedef struct fwd_prm_s
{
    int id;
    AMX *amx;
    bool is_debug;
} fwd_prms_t;

typedef struct sql_prm_s
{
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_name;
    std::string db_chrs; // charterset
    size_t db_port;      // 3306
    size_t timeout;      // sec
} sql_prm_t;

typedef struct prm_s
{
    fwd_prms_t fwd;
    sql_prm_t sql;
} prm_t;

typedef std::list<prm_t> prm_list_t;
//typedef prm_list_t::iterator prm_list_it;
typedef std::chrono::_V2::system_clock::time_point time_point_t;
typedef std::chrono::duration<double> elapsed_time_t;
typedef std::map<std::thread::id, std::thread *> m_thread_t;
typedef std::map<std::string, size_t> f_names_t;
typedef f_names_t::iterator f_names_it;

typedef struct m_query_s
{
    uint64 id;
    std::thread::id tid;
    uint8 pri;
    size_t conn_id;
    std::string query;
    cell *data;
    size_t data_size;
    size_t timeout;
    uint8 retry_count;
    bool async;
    std::atomic<bool> started;
    std::atomic<bool> aborted;
    std::atomic<bool> finished;
    std::atomic<bool> successful;
    float queuetime;
    double time_create;
    double time_start;
    double time_end;
    MYSQL *conn;
    MYSQL_RES *result;
    bool is_buffered;
    MYSQL_ROW row;
    prm_t prms;
    std::string error;
    int err;
    int failstate;
    f_names_t f_names;
    size_t f_count;
} m_query_t;

typedef std::map<uint64, m_query_t *> m_query_list_t;
typedef m_query_list_t::iterator m_query_list_it;

class mysql_mngr
{
    size_t max_threads;
    prm_list_t m_conn_prms;
    m_query_list_t m_queries[MAX_QUERY_PRIORITY + 1];
    std::thread *main_thread;
    m_thread_t m_threads;
    std::atomic<int> num_threads;
    std::atomic<int> num_finished;
    std::atomic<bool> stop_main;
    std::atomic<bool> stop_threads;
    std::mutex frame_mutex, threads_mutex, fwd_mutex, conn_mutex;
    std::mutex prms_mutex;    
    std::vector<MYSQL *> conns;
    time_point_t _time_0;
    struct timespec frame_curr, frame_prev;
    std::atomic<bool> frame_exec;

private:
    bool is_valid_conn(m_query_t *q)
    {
        DEBUG("%s(): q = %p, async = %d", __func__, q, q->async);
        if (q == nullptr || q->conn == nullptr)
            return false;
        if (!q->async && q->started && q->result != nullptr)
        {
            int res = mysql_ping(q->conn);
            DEBUG("%s(): q = %p, PING = %d", __func__, q, res);
            if (res != 0)
            {
                q->err = mysql_errno(q->conn);
                q->error = mysql_error(q->conn);
                DEBUG("%s(): q = %p, connection lost (%d) = %s", __func__, q, q->err, q->error.c_str());
                return false;
            }
        }        
        DEBUG("%s(): q = %p, valid!", __func__, q);
        return true;
    }
    bool is_valid_conn(MYSQL *conn)
    {
        DEBUG("%s(): conn = %p", __func__, conn);
        if (conn == nullptr)
            return false;
        DEBUG("%s(): conn = %p, valid!", __func__, conn);
        return true;
    }
    void dump(const char *from)
    {
        DEBUG("%s-%s(): threads = %d (%d), m_frames = %u, stop = %d", from, __func__, (num_threads - num_finished), m_threads.size(), m_frames, stop_threads.load());
        DEBUG("%s-%s(): frame_lock = %d, fwd_lock = %d, thr_lock = %d, frame_exec = %d", from, __func__, is_mutex_locked(frame_mutex), is_mutex_locked(fwd_mutex), is_mutex_locked(threads_mutex), frame_exec.load());
        DEBUG("%s-%s(): delay = %f, rate = %d (%d)", from, __func__, frame_delay, frame_rate, frame_rate_max);
        for (uint8 pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
            if (m_queries[pri].size())
                DEBUG("%s(): m_query[%d](%d)", __func__, pri, m_queries[pri].size());
    }
    int wait_for_mysql(m_query_t *q, int status)
    {
        int timeout, res;
        struct pollfd pfd = {};
        struct timespec start, now;
        DEBUG("%s(): conn = %p", __func__, q->conn);
        if (!is_valid_conn(q))
            return MYSQL_WAIT_TIMEOUT;
        pfd.fd = mysql_get_socket(q->conn);
        if (pfd.fd < 0)
        {
            DEBUG("%s(): Invalid socket (%d)", __func__, pfd.fd);
            return MYSQL_WAIT_TIMEOUT;
        }
        // DEBUG("%s(): socket = %d", __func__, pfd.fd);
        pfd.events = 0;
        if (status & MYSQL_WAIT_READ)
            pfd.events |= POLLIN;
        if (status & MYSQL_WAIT_WRITE)
            pfd.events |= POLLOUT;
        if (status & MYSQL_WAIT_EXCEPT)
            pfd.events |= POLLPRI;
        // DEBUG("%s(): socket = %d, events = %d", __func__, pfd.fd, pfd.events);
        timeout = (status & MYSQL_WAIT_TIMEOUT) ? (q->timeout > 0 ? q->timeout : mysql_get_timeout_value_ms(q->conn)) : -1;
        if (timeout > 0)
            clock_gettime(CLOCK_MONOTONIC, &start);  // Фиксируем начало ожидания
        // DEBUG("%s(): timeout = %d", __func__, timeout);
        res = safe_poll(&pfd, 1, timeout);
        DEBUG("%s(): res = %d, stop = %d", __func__, res, stop_threads.load());
        if (res <= 0)
            return MYSQL_WAIT_TIMEOUT;
        int result_status = 0;
        if (pfd.revents & POLLIN)
            result_status |= MYSQL_WAIT_READ;
        if (pfd.revents & POLLOUT)
            result_status |= MYSQL_WAIT_WRITE;
        if (pfd.revents & POLLPRI)
            result_status |= MYSQL_WAIT_EXCEPT;
        if (pfd.revents & POLLERR)
        {
            DEBUG("%s(): Socket error occurred", __func__);
            return MYSQL_WAIT_TIMEOUT;
        }
        if (pfd.revents & POLLHUP)
        {
            DEBUG("%s(): Socket hang up", __func__);
            return MYSQL_WAIT_TIMEOUT;
        }
        return result_status;
    }
    void set_default_params(MYSQL *conn, size_t timeout, const char *db_chrs)
    {
        auto prot_type = MYSQL_PROTOCOL_TCP;
        mysql_optionsv(conn, MYSQL_OPT_PROTOCOL, (void *)&prot_type);
        mysql_optionsv(conn, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&timeout);
        my_bool my_true = true;
        mysql_optionsv(conn, MYSQL_OPT_RECONNECT, (const char *)&my_true);
        if (*db_chrs != '\0')
            mysql_set_character_set(conn, db_chrs);
    }
        void frame_forward(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        bool repeat = false;
        // Prepare array
        cell *error_tmp, *data_tmp, error_param, data_param;
        size_t total_size = 0;
        DEBUG("%s(): FRAME_WAITLOCK", __func__);
        dump(__func__);
        frame_mutex.lock();
        frame_exec = true;
        do
        {
            auto f = q->prms.fwd;
            DEBUG("%s(): FRAME_LOCKED !!!, amx = %p, fwd = %d", __func__, f.amx, f.id);
            if (stop_threads || q->finished || q->aborted || (!q->successful && q->retry_count < QUERY_RETRY_COUNT)
                || f.amx == nullptr  || get_plugin(f.amx) == nullptr || f.id == -1)
                break;
            DEBUG("%s(): EXEC AMXX FORWARD = %d, is_debug = %d, failstate = %d, err = %d, query = %p, time = %f (%f / %f / %f), error = %s",
                  __func__, f.id, f.is_debug, q->failstate, q->err, q, q->queuetime, q->time_create, q->time_start, q->time_end, q->error.c_str());
            // Callback error
            if (amx_allot(f.amx, q->error.size() + 1, &error_param, &error_tmp))
            {
                total_size += (q->error.size() + 1) << 2;
                setAmxString(error_tmp, q->error.c_str(), q->error.size());
                // Callback data
                if (amx_allot(f.amx, q->data_size, &data_param, &data_tmp))
                {
                    Q_memcpy(data_tmp, q->data, q->data_size << 2);
                    total_size += q->data_size << 2;
                    // public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
                    int ret = g_amxxapi.ExecuteForward(f.id, q->failstate, q, error_param, q->err, data_param, q->data_size, amx_ftoc(q->queuetime));
                    DEBUG("%s(): AFTER FORWARD, fix = %d, hea = %p, param = %p, size = %d (%d)",
                        __func__, f.amx->hea > data_param, f.amx->hea, data_param, f.amx->hea - error_param, total_size);
                }
                // Fix heap
                if ((f.amx->hea - error_param) == total_size)
                    f.amx->hea = error_param;
                else
                    AMXX_LogError(f.amx, AMX_ERR_NATIVE, "%s(): can't fix amx->hea, possible memory leak!", __func__);
            }
        } while (0);
        fwd_mutex.unlock();
        if (stop_threads)
            frame_mutex.unlock();
        frame_exec = false;
    }

public:
    uint64 m_frames;
    double frame_delay;
    size_t frame_rate, frame_rate_max;
    std::atomic<uint64> m_query_nums;

    std::atomic<bool> block_changelevel;
    void main()
    {
        DEBUG("main(): pid = %p, max_thread = %d, interval = %d, START", gettid(), 1 << MAX_QUERY_PRIORITY, QUERY_POOLING_INTERVAL);
        while (!stop_main)
        {
            poll_query();
            std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
        }
    }
    void poll_query()
    {
        uint8 pri;
        std::thread::id key;
        std::vector<m_query_t *> start_queries, finished_queries;
        std::vector<std::thread *> finished_threads;
        threads_mutex.lock();
        for (pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
        {
            for (auto &it : m_queries[pri])
            {
                auto q = it.second;
                if (q->finished || q->aborted)
                {
                    if (q->async && q->started && q->finished)
                    {
                        key = q->tid;
                        auto it = m_threads.find(key);
                        // Check if the threads have already been cleared
                        if (it != m_threads.end())
                        {
                            finished_threads.push_back(it->second);
                            num_threads--;
                            num_finished--;
                            m_threads.erase(it);
                            // Free memory
                            if (m_threads.empty())
                                m_thread_t().swap(m_threads);
                        }
                    }
                    if (!stop_threads && q->async && !q->aborted && !q->successful && ++q->retry_count < QUERY_RETRY_COUNT)
                    {
                        q->started = false;
                        q->finished = false;
                    }
                    else if (!q->async || q->finished || (!q->started && q->aborted))
                        finished_queries.push_back(q);
                }
                else if (!stop_threads && q->async && !q->started && !q->aborted && threads_count() < (max_threads << (MAX_QUERY_PRIORITY - q->pri)))
                {
                    start_queries.push_back(q);
                    num_threads++;
                }
            }
        }
        // Start queries
        for (auto q : start_queries)
        {
            q->started = true;
            q->time_start = get_time();
            auto t = new std::thread(&mysql_mngr::run_query, this, q);
            assert(t != nullptr);
            auto result = m_threads.emplace((key = t->get_id()), t); // m_threads.insert({ key = t->get_id(), std::move(t) }); // t->native_handle();
            assert(result.second);
            assert(key != std::thread::id(0));
            q->tid = key;
            DEBUG("%s(): starting pid = %p, key = %p, q = %p, num_threads = %d (%d)", __func__, t, key, q, num_threads.load(), m_threads.size());
        }
        threads_mutex.unlock();
        // Cleaner queries
        for (auto q : finished_queries)
        {
            DEBUG("%s(): free query = %p", __func__, q);
            assert(q != nullptr);
            free_query(q);
            threads_mutex.lock();
            m_queries[q->pri].erase(q->id);
            // Free memory
            if (m_queries[q->pri].empty())
                m_query_list_t().swap(m_queries[q->pri]);
            threads_mutex.unlock();
            // Free query
            delete q;
        }
        // Cleaner threads
        for (auto t : finished_threads)
        {
            DEBUG("%s(): free thread = %p", __func__, t);
            assert(t != nullptr);
            if (t->joinable())
                t->join();
            delete t;
        }
    }
    void run_query(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        std::thread::id pid = gettid();
        DEBUG("%s(): pid = %p, q = %p, START", __func__, pid, q);
        if (q->successful = exec_async_query(q, pid))
            get_result(q, false);
        q->queuetime = (q->time_end = get_time()) - q->time_start;
        DEBUG("%s(): pid = %p, start = %f, end = %f, time = %f, successful = %d, stop = %d",
            __func__, pid, q->time_start, q->time_end, q->queuetime, q->successful.load(), stop_threads.load());
        frame_forward(q);
        q->finished = true;
        threads_mutex.lock();
        num_finished++;
        threads_mutex.unlock();
        DEBUG("%s(): pid = %p, END, num_finished = %d", __func__, pid, num_finished.load());
    }
    m_query_t *prepare_query(MYSQL *conn, const char *query, bool async = false, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout = 0, size_t conn_id = 0, cell *data = nullptr, size_t data_size = 0)
    {
        DEBUG("%s(): START", __func__);
        if (*query == '\0' || pri > MAX_QUERY_PRIORITY)
            return nullptr;
        auto q = new m_query_t{0};
        q->tid = std::thread::id(0);
        q->conn = conn;
        q->result = nullptr;
        q->async = async;
        q->is_buffered = true;
        q->started = false;
        q->aborted = false;
        q->finished = false;
        q->successful = false;
        q->id = m_query_nums++;
        q->query = query;
        q->time_create = get_time();
        q->pri = pri;
        q->retry_count = 0;
        q->timeout = timeout;
        q->conn_id = conn_id;
        q->data_size = clamp(data_size, 1U, 4096U);
        q->data = new cell[q->data_size]{0};
        if (data != nullptr)
            Q_memcpy(q->data, data, data_size << 2);
        threads_mutex.lock();
        m_queries[pri].insert({q->id, q});
        threads_mutex.unlock();
        return q;
    }
    bool push_query(size_t conn_id, const char *query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout = 60U)
    {
        DEBUG("%s(): START", __func__);
        auto q = prepare_query(nullptr, query, true, pri, 1000 * timeout, conn_id, data, data_size);
        DEBUG("%s(): q = %p, query = %s, pri = %d, count = %d", __func__, q, query, pri, m_queries[pri].size());
        return q != nullptr;
    }
    bool exec_async_query(m_query_t *q, std::thread::id &pid)
    {
        DEBUG("%s(): START", __func__);
        int status;
        q->failstate = TQUERY_SUCCESS;
        if (async_connect(q))
        {
            DEBUG("%s: pid = %p, mtid = %p, q = %p, start = %f, query = %s", __func__, pid, mysql_thread_id(q->conn), q, q->time_start, q->query.c_str());
            status = mysql_real_query_start(&q->err, q->conn, q->query.c_str(), q->query.size());
            while (status)
            {
                status = wait_for_mysql(q, status);
                status = mysql_real_query_cont(&q->err, q->conn, status);
            }
            if (q->err) {
                q->error = mysql_error(q->conn);
                q->failstate = TQUERY_QUERY_FAILED;
            }
        }
        else
            q->failstate = TQUERY_CONNECT_FAILED;
        return q->failstate == TQUERY_SUCCESS;
    }
    bool exec_query(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        if (!is_valid_conn(q) || q->query.empty() || q->started || q->finished || q->aborted)
            return false;
        q->started = true;
        q->time_start = get_time();
        DEBUG("%s(): conn = %p, start = %f, query = %s", __func__, q->conn, q->time_start, q->query.c_str());
        auto ret = mysql_query(q->conn, q->query.c_str());
        if (ret)
        {
            q->error = mysql_error(q->conn);
            q->err = mysql_errno(q->conn);
            return false;
        }
        DEBUG("%s(): ret = %d", __func__, ret);
        q->successful = true;
        get_result(q, true);
        return true;
    }
    void free_query(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        if (q == nullptr)
            return;
        if (!q->async && !q->finished)
        {
            q->finished = true;
            return;
        }
        if (q->result != nullptr)
        {
            DEBUG("%s(): FREE RESULT, q = %p, buffered = %d", __func__, q, q->is_buffered);
            if (!q->is_buffered)
            {
                auto status = mysql_free_result_start(q->result);
                while (status)
                {
                    status = wait_for_mysql(q, status);
                    status = mysql_free_result_cont(q->result, status);
                }
            }
            else
                mysql_free_result(q->result);
            q->result = nullptr;
        }
        if (q->data != nullptr && (stop_threads || q->finished || q->aborted || q->successful))
        {
            DEBUG("%s(): FREE DATA, q = %p", __func__, q);
            delete q->data;
            q->data = nullptr;
            q->data_size = 0;
        }
        if (q->async)
        {
            if (q->conn != nullptr)
            {
                DEBUG("%s(): CLOSE, q = %p, async = %d", __func__, q, q->async);
                auto status = mysql_close_start(q->conn);
                while (status)
                {
                    status = wait_for_mysql(q, status);
                    status = mysql_close_cont(q->conn, status);
                }
                q->conn = nullptr;
            }
            mysql_thread_end();
        }

        // Free memory
        q->f_names.clear();
        f_names_t().swap(q->f_names);
        q->f_count = 0;
        DEBUG("%s(): done!", __func__);
    }
    cell query_str(m_query_t *q, cell *ret, cell *buff, size_t buff_len)
    {
        DEBUG("%s(): START", __func__);
        if (q == nullptr || q->query.empty() || q->finished || q->aborted || (stop_threads && q->async))
        {
            DEBUG("%s(): ERROR, q = %p, ret = %p, buff = %p, len = %d", __func__, q, ret, buff, buff_len);
            if (buff_len)
                *buff = '\0';
            *ret = 0;
            return 1;
        }
        if (buff_len)
            *ret = set_amx_string(buff, q->query.c_str(), buff_len);
        else
            set_amx_string(ret, q->query.c_str(), RF_MAX_FIELD_SIZE - 1);
        return 1;
    }
    cell affected_rows(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        return (!is_valid_conn(q) || (stop_threads && q->async)) ? -1 : mysql_affected_rows(q->conn);
    }
    cell insert_id(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        return (!is_valid_conn(q) || (stop_threads && q->async)) ? -1 : mysql_insert_id(q->conn);
    }
    cell field_name(m_query_t *q, size_t offset, cell *ret, cell *buff, size_t buff_len)
    {
        DEBUG("%s(): START", __func__);
        if (q == nullptr || q->result == nullptr || q->result->fields == nullptr || q->row == nullptr || q->f_names.empty() || offset >= q->f_count || q->finished || q->aborted || (stop_threads && q->async))
        {
            if (buff_len)
                *buff = '\0';
            *ret = 0;
            return 1;
        }
        auto name = q->result->fields[offset].name;
        if (buff_len)
            *ret = set_amx_string(buff, name, buff_len);
        else
            set_amx_string(ret, name, RF_MAX_FIELD_NAME - 1);
        return 1;
    }
    cell fetch_field(m_query_t *q, const char *f_name, int mode, cell *ret, cell *buff, size_t buff_len)
    {
        DEBUG("%s(): START", __func__);
        f_names_it pos;
        if (q == nullptr || q->result == nullptr || q->result->fields == nullptr || q->row == nullptr || q->f_names.empty() || q->finished || q->aborted || (stop_threads && q->async) || (pos = q->f_names.find(f_name)) == q->f_names.end())
        {
            if (buff_len)
                *buff = '\0';
            *ret = 0;
            return 1;
        }
        return fetch_field(q, pos->second, mode, ret, buff, buff_len);
    }
    cell fetch_field(m_query_t *q, size_t offset, int mode, cell *ret, cell *buff, size_t buff_len)
    {
        DEBUG("%s(): START", __func__);
        if (q == nullptr || q->result == nullptr || q->result->fields == nullptr || q->row == nullptr || offset >= q->f_count || q->finished || q->aborted || (stop_threads && q->async))
        {
            if (buff_len)
                *buff = '\0';
            *ret = 0;
            return 1;
        }
        float val;
        auto name = q->result->fields[offset].name;
        auto type = q->result->fields[offset].type;
        auto f = q->row[offset];
        //if (f && !strcmp(f, "(null)"))
        //    f = nullptr;
        DEBUG("%s(): q = %p, async = %d, name = %s, type = %d, stop = %d", __func__, q, q->async, name, type, stop_threads.load());
        DEBUG("%s(): value = %s, offset = %d, type = %d, mode = %d, ret = %p, buff = %p, len = %d", __func__, f, offset, type, mode, ret, buff, buff_len);
        switch (mode)
        {
        case FT_AUTO:
            switch (type)
            {
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_LONGLONG:
            case MYSQL_TYPE_BIT:
            case MYSQL_TYPE_NULL:
                *ret = f ? atoi(f) : 0; // std::stoi(f)          
                break;
            case MYSQL_TYPE_FLOAT:
            case MYSQL_TYPE_DOUBLE:
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL:
                val = f ? (float) atof(f) : 0.0f; // std::stof(f)
                *ret = amx_ftoc(val);
                break;
            default:
                set_amx_string(ret, f, RF_MAX_FIELD_SIZE - 1);
                break;
            }
            break;
        case FT_INT:
            *ret = f ? atoi(f) : 0; // std::stoi(f) 
            break;
        case FT_FLT:
            val = f ? (float) atof(f) : 0.0f; // std::stof(f)
            *ret = amx_ftoc(val);
            break;
        default:
            if (mode == FT_STR)
                set_amx_string(ret, f, RF_MAX_FIELD_SIZE - 1);
            else if (buff_len)
                *ret = set_amx_string(buff, f, buff_len);
            break;
        }
        DEBUG("%s(): ret = %d", __func__, *ret);
        return 1;
    }
    bool fetch_row(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        int status;
        if (!is_valid_conn(q) || q->result == nullptr || q->finished || q->aborted || q->f_count <= 0 || (stop_threads && q->async))
            return false;
        DEBUG("%s(): query = %p, is_buffered = %d", __func__, q, q->is_buffered);
        if (!q->is_buffered)
        {
            status = mysql_fetch_row_start(&q->row, q->result);
            while (status)
            {
                status = wait_for_mysql(q, status);
                status = mysql_fetch_row_cont(&q->row, q->result, status);
            }
        }
        else
            q->row = mysql_fetch_row(q->result);
        return q->row != nullptr;
    }
    cell field_count(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        return q == nullptr || q->result == nullptr || q->finished || q->aborted || (stop_threads && q->async) ? -1 : q->f_count;
    }
    bool get_result(m_query_t *q, bool is_buffered)
    {
        DEBUG("%s(): START", __func__);
        if (!is_valid_conn(q) || q->result != nullptr || q->finished || q->aborted || (stop_threads && q->async))
            return false;
        q->result = (q->is_buffered = is_buffered) ? mysql_store_result(q->conn) : mysql_use_result(q->conn);
        DEBUG("%s(): result = %p, query = %p, conn = %p, is_buffered = %d, state = %s", __func__, q->result, q, q->conn, is_buffered, q->conn->net.sqlstate);
        q->f_count = mysql_field_count(q->conn);
        if (q->result != nullptr && q->result->fields != nullptr)
            for (size_t i = 0; i < q->f_count; i++)
                q->f_names.insert({q->result->fields[i].name, i});
        DEBUG("%s(): field count = %d", __func__, q->f_count);
        return q->result != nullptr;
    }
    uint32 num_rows(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        return q == nullptr || q->result == nullptr || q->finished || q->aborted || (stop_threads && q->async) ? -1 : mysql_num_rows(q->result);
    }
    MYSQL *connect(size_t conn_id, cell *err, cell *error, size_t error_size)
    {
        DEBUG("%s(): START", __func__);
        MYSQL *ret, *conn;
        prm_t p;
        if (!get_connect(conn_id, p) || (conn = mysql_init(nullptr)) == nullptr)
            return nullptr;
        set_default_params(conn, p.sql.timeout, p.sql.db_chrs.c_str());
        ret = mysql_real_connect(conn, p.sql.db_host.c_str(), p.sql.db_user.c_str(), p.sql.db_pass.c_str(), p.sql.db_name.c_str(), p.sql.db_port, NULL, SQL_FLAGS);
        if (ret != nullptr)
        {
            conn_mutex.lock();
            conns.push_back(ret);
            conn_mutex.unlock();
        }
        *err = mysql_errno(conn);
        setAmxString(error, mysql_error(conn), error_size);
        return ret;
    }
    bool async_connect(m_query_t *q)
    {
        DEBUG("%s(): START", __func__);
        int status;
        MYSQL *ret;
        if (q == nullptr || q->finished || q->aborted || !q->async)
            return false;
        if (!get_connect(q->conn_id, q->prms) || mysql_thread_init() || (q->conn = mysql_init(nullptr)) == nullptr)
            return false;
        auto p = q->prms;
        set_default_params(q->conn, p.sql.timeout, p.sql.db_chrs.c_str());
        mysql_options(q->conn, MYSQL_OPT_NONBLOCK, 0);
        status = mysql_real_connect_start(&ret, q->conn, p.sql.db_host.c_str(), p.sql.db_user.c_str(), p.sql.db_pass.c_str(), p.sql.db_name.c_str(), p.sql.db_port, NULL, SQL_FLAGS);
        DEBUG("%s(): MYSQL_OPT_NONBLOCK, conn = %p, port = %d, status = %d", __func__, q->conn, p.sql.db_port, status);
        while (status)
        {
            status = wait_for_mysql(q, status);
            status = mysql_real_connect_cont(&ret, q->conn, status);
        }
        if (ret == nullptr)
        {
            q->error = mysql_error(q->conn);
            q->err = mysql_errno(q->conn);
        }
        return ret != nullptr;
    }
    bool close(MYSQL *conn)
    {
        bool result;
        DEBUG("%s(): START", __func__);
        conn_mutex.lock();
        auto it = std::find(conns.begin(), conns.end(), conn);
        if ((result = it != conns.end()))
        {
            DEBUG("%s(): STATIC, conn = %p", __func__, conn);
            conns.erase(it);
            conns.shrink_to_fit();
            conn_mutex.unlock();
            mysql_close(conn);
            DEBUG("%s(): STATIC, conn = %p, closed!", __func__, conn);
            conn = nullptr;
        }
        else
            conn_mutex.unlock();
        return result;
    }
    void close_all()
    {
        DEBUG("%s(): START", __func__);
        std::vector<MYSQL *> _conns;
        conn_mutex.lock();
        _conns.swap(conns);
        conn_mutex.unlock();
        for (auto it : _conns)
        {
            assert(it != nullptr);
            mysql_close(it);
        }
    }
    size_t add_connect(AMX *amx, int id, bool is_debug, std::string &db_host, std::string &db_user, std::string &db_pass, std::string &db_name, std::string &db_chrs, size_t timeout)
    {
        DEBUG("%s(): pid = %p, fwd = %d, host = %s, user = %s, pass = %s, name = %s, timeout = %d", __func__, gettid(), id, db_host.c_str(), db_user.c_str(), db_pass.c_str(), db_name.c_str(), timeout);
        prm_t prms;
        prms.fwd.amx = amx;
        prms.fwd.id = id;
        prms.fwd.is_debug = is_debug;
        size_t pos = db_host.find(':');
        prms.sql.db_host = pos == std::string::npos ? db_host : db_host.substr(0, pos++);
        prms.sql.db_port = pos == std::string::npos ? 3306 : atoi(db_host.substr(pos, db_host.size() - pos).c_str());
        prms.sql.db_user = db_user;
        prms.sql.db_pass = db_pass;
        prms.sql.db_name = db_name;
        prms.sql.db_chrs = db_chrs;
        prms.sql.timeout = timeout;
        prms_mutex.lock();
        m_conn_prms.push_back(prms);
        prms_mutex.unlock();
        DEBUG("%s(): conn = %d, is_debug = %d", __func__, m_conn_prms.size(), is_debug);
        return m_conn_prms.size();
    }
    bool get_connect(size_t conn_id, prm_t &p)
    {
        DEBUG("%s(): START", __func__);
        auto count = m_conn_prms.size();
        if (!count || !conn_id || conn_id > count)
            return false;
        std::lock_guard lock(prms_mutex);
        auto it = std::next(m_conn_prms.begin(), conn_id - 1);
        // Copy PARAMS
        p.fwd = it->fwd;
        p.sql = it->sql;
        return true;
    }
    void start_main()
    {
        DEBUG("start(): pid = %p, START", gettid());
        try {
            throw std::runtime_error("Test");
        } catch (...) {
            DEBUG("Exception caught!");
            stop_main = false;
        }
        if (!stop_main)
            main_thread = new std::thread(&mysql_mngr::main, this);
    }
    void start()
    {
        DEBUG("%s(): LOCK FRAME", __func__);
        frame_mutex.lock();
        DEBUG("%s(): UNLOCK FWD", __func__);
        fwd_mutex.unlock();
        _time_0 = std::chrono::system_clock::now();
        DEBUG("*** STATUS: m_frame = %u", m_frames);
        dump(__func__);
        stop_threads = false;
    }
    void stop()
    {
        stop_threads = true;
        m_frames =
            frame_prev.tv_sec =
                frame_prev.tv_nsec = 0;
        frame_delay = 0.0;
        if (!frame_mutex.try_lock() && frame_exec || frame_exec)
            fwd_mutex.lock();
        else 
            fwd_mutex.try_lock();
        DEBUG("%s(): LOCK THREAD", __func__);
        threads_mutex.lock();
        for (uint8 pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
            for (auto &it : m_queries[pri])
            {
                auto q = it.second;
                assert(q != nullptr);
                q->aborted = true;
                q->conn_id = 0;
                q->prms.fwd.id = -1;
            }
        DEBUG("%s(): UNLOCK THREAD", __func__);
        threads_mutex.unlock();
        DEBUG("%s(): UNLOCK FRAME", __func__);
        frame_mutex.unlock();
        DEBUG("%s(): CLOSE ALL", __func__);
        close_all();
        prms_mutex.lock();
        m_conn_prms.clear();
        prm_list_t().swap(m_conn_prms);
        prms_mutex.unlock();
        // STATUS
        if (m_threads.size())
            DEBUG("[REFSAPI_SQL] m_threads(%d) not empty! num_threads = %d, num_finished = %d\n", m_threads.size(), num_threads.load(), num_finished.load());
        for (uint8 pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
            if (m_queries[pri].size())
                DEBUG("[REFSAPI_SQL] m_query[%d](%d) not empty!\n", pri, m_queries[pri].size());
    }
    double get_time()
    {
        elapsed_time_t queuetime = std::chrono::system_clock::now() - _time_0;
        return queuetime.count();
    }
    int threads_count()
    {
        auto num = (num_threads - num_finished);
        assert(num >= 0);
        return num;
    }
    void start_frame()
    {
        if (stop_threads)
            return;
        if (!(m_frames % 1000))
        {
            clock_gettime(CLOCK_MONOTONIC, &frame_curr);
            if (frame_prev.tv_sec >= 0 && frame_prev.tv_nsec > 0)
            {
                auto delay = timespec_diff(&frame_prev, &frame_curr);
                if (frame_delay > 0.0) {
                    auto k1 = delay > frame_delay ? 25.0 * abs(frame_delay - delay) / (frame_delay + delay) : 1.0;
                    auto k2 = frame_delay > 1.0 ? frame_delay : 1.0;
                    frame_rate = std::round(1.0 + k1 * k2);
                    if (frame_rate > frame_rate_max)
                        frame_rate_max = frame_rate;
                }
                frame_delay = (frame_delay + delay) / 2.0;
            }
            frame_prev = frame_curr;
            dump(__func__);
        }
        // Check frame
        if (threads_count() > 0 && !(m_frames % frame_rate) && !frame_mutex.try_lock() && !frame_exec) // && !frame_mutex.try_lock() && fwd_mutex.try_lock())//if (!stop_threads && threads_count() > 0 && fwd_mutex.try_lock() && !frame_mutex.try_lock())
        {
            if (fwd_mutex.try_lock())
            {
                frame_mutex.unlock();
                //fwd_mutex.lock();
            }
            else
                fwd_mutex.unlock();
        }
        m_frames++;
    }
    mysql_mngr()
    {
        stop_main = false;
        stop_threads = true;
        m_frames =
            m_query_nums =
                num_threads =
                    num_finished = 0;
        frame_delay = 0.0;
        frame_rate =
            frame_rate_max = 1;
        max_threads = clamp(std::thread::hardware_concurrency() >> 1, 1U, 4U);
    }
    ~mysql_mngr()
    {
        stop();
        stop_main = true;
        if (main_thread->joinable())
            main_thread->join();
        delete main_thread;
    }
};

extern mysql_mngr g_mysql_mngr;
#endif