#include "precompiled.h"

#ifndef WITHOUT_SQL
#define _DEBUG 1
#define MAX_QUERY_PRIORITY 5 // PRI->THREADS: 0 -> 8, 1 -> 4, 2 -> 2, 3 -> 1
#define QUERY_POOLING_INTERVAL 5
#define QUERY_RETRY_COUNT 3
#define SQL_FLAGS CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS
#define RF_MAX_FIELD_SIZE 256
#define RF_MAX_FIELD_NAME 64
#define gettid() std::this_thread::get_id()
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

typedef struct prm_s
{
    int fwd;
    AMX *amx;
    bool is_debug;
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_name;
    std::string db_chrs; // charterset
    size_t db_port;      // 3306
    size_t timeout;      // sec
} prm_t;

typedef std::list<prm_t> prm_list_t;
typedef prm_list_t::iterator prm_list_it;
typedef std::chrono::_V2::system_clock::time_point time_point_t;
typedef std::chrono::duration<double> elapsed_time_t;
typedef std::map<std::thread::id, std::thread *> m_thread_t;
typedef std::map<std::string, size_t> f_names_t;

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
    prm_list_it prms;
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
    std::thread main_thread;
    m_thread_t m_threads;
    std::atomic<int> num_threads;
    std::atomic<int> num_finished;
    std::atomic<bool> stop_main;
    std::atomic<bool> stop_threads;
    std::mutex frame_mutex, threads_mutex, fwd_mutex, prms_mutex, std_mutex;
    std::atomic<uint64> m_query_nums;
    std::vector<MYSQL *> conns;
    time_point_t _time_0;
    size_t m_frames;

private:
    int wait_for_mysql(MYSQL *conn, int status, int timeout_ms = 0)
    {
        int timeout, res;
        struct pollfd pfd = {};
        DEBUG("%s(): conn = %p", __func__, conn);
        if (conn == nullptr)
        {
            DEBUG("%s(): Invalid connection pointer", __func__);
            return MYSQL_WAIT_TIMEOUT;
        }
        pfd.fd = mysql_get_socket(conn);
        if (pfd.fd < 0)
        {
            DEBUG("%s(): Invalid socket (%d)", __func__, pfd.fd);
            return MYSQL_WAIT_TIMEOUT;
        }
        DEBUG("%s(): socket = %d", __func__, pfd.fd);
        pfd.events = 0;
        if (status & MYSQL_WAIT_READ)
            pfd.events |= POLLIN;
        if (status & MYSQL_WAIT_WRITE)
            pfd.events |= POLLOUT;
        if (status & MYSQL_WAIT_EXCEPT)
            pfd.events |= POLLPRI;
        DEBUG("%s(): socket = %d, events = %d", __func__, pfd.fd, pfd.events);
        timeout = (status & MYSQL_WAIT_TIMEOUT) ? (timeout_ms > 0 ? timeout_ms : mysql_get_timeout_value_ms(conn)) : -1;
        DEBUG("%s(): timeout = %d", __func__, timeout);
        do
            res = poll(&pfd, 1, timeout);
        while (!stop_threads && res == -1 && errno == EINTR); //(!stop_threads || timeout_ms < 0)
        DEBUG("%s(): poll = %d", __func__, res);
        if (res <= 0 || stop_threads) //(stop_threads && timeout_ms >= 0)
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

public:
    void main()
    {
        DEBUG("main(): pid = %p, max_thread = %d, interval = %d, START", gettid(), 1 << MAX_QUERY_PRIORITY, QUERY_POOLING_INTERVAL);
        while (!stop_main)
        {
            run_query();
            std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
        }
    }
    void run_query()
    {
        uint8 pri;
        std::thread::id key;
        std::vector<m_query_t *> finished;
        // DEBUG("%s(): RUN QUERY", __func__);
        for (pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
        {
            for (auto it = m_queries[pri].begin(); it != m_queries[pri].end(); it++)
            {
                auto q = it->second;
                if (q->finished || q->aborted)
                {
                    key = q->tid;
                    if (q->async && key != std::thread::id(0))
                    {
                        DEBUG("%s(): cleaner, total finished: %d", __func__, num_finished.load());
                        auto t = m_threads[key];
                        // threads_mutex.unlock();
                        if (t->joinable())
                            t->join();
                        // threads_mutex.lock();
                        delete t;
                        num_threads--;
                        if (q->finished)
                            num_finished--;
                        m_threads.erase(key);
                        // Free memory
                        if (m_threads.empty())
                            m_thread_t().swap(m_threads);
                    }
                    if (!stop_threads && q->async && !q->aborted && !q->successful && ++q->retry_count < QUERY_RETRY_COUNT)
                    {
                        q->started = false;
                        q->finished = false;
                    }
                    else
                    {
                        finished.push_back(q);
                        // DEBUG("run_query(): cleaner, threads left = %d, num_threads = %d", m_threads.size(), num_threads.load());
                    }
                }
                else if (!stop_threads && q->async && !q->started && !q->aborted && (m_threads.size() - num_finished) < (max_threads << (MAX_QUERY_PRIORITY - q->pri)))
                {
                    q->started = true;
                    q->time_start = get_time();
                    std::thread *t = new std::thread(&mysql_mngr::exec_async_query, this, pri, q);
                    m_threads.emplace(key = t->get_id(), t);
                    // m_threads.insert({ key = t->get_id(), std::move(t) });
                    q->tid = key;
                    num_threads++;
                    DEBUG("%s(): thread starting pid = %p, q = %p, threads_num = %d", __func__, key, q, m_threads.size());
                    // threads_mutex.unlock();
                }
            }
        }
        // CLEANER
        std::lock_guard lock(threads_mutex);
        for (auto q : finished)
        {
            DEBUG("%s(): free query = %p", __func__, q);
            free_query(q);
            m_queries[q->pri].erase(q->id);
            // Free memory
            if (m_queries[q->pri].empty())
                m_query_list_t().swap(m_queries[q->pri]);
            // Free query
            delete q;
        }
    }
    void exec_async_query(uint8 pri, m_query_t *q)
    {
        int status;
        std::thread::id pid = gettid();
        DEBUG("%s(): pid = %p, q = %p, START", __func__, pid, q);
        q->failstate = TQUERY_SUCCESS;
        if (async_connect(q))
        {
            DEBUG("%s: pid = %p, q = %p, start = %f, query = <%s>", __func__, pid, q, q->time_start, q->query.c_str());
            status = mysql_real_query_start(&q->err, q->conn, q->query.c_str(), q->query.size() + 1);
            while (status)
            {
                status = wait_for_mysql(q->conn, status, q->timeout);
                status = mysql_real_query_cont(&q->err, q->conn, status);
            }
            if (q->err)
                q->failstate = TQUERY_QUERY_FAILED;
        }
        else
            q->failstate = TQUERY_CONNECT_FAILED;
        if (q->failstate == TQUERY_SUCCESS)
            get_result(q, false);
        q->queuetime = (q->time_end = get_time()) - q->time_start;
        DEBUG("%s(): pid = %p, start = %f, end = %f, time = %f, successful = %d, stop = %d", __func__, pid, q->time_start, q->time_end, q->queuetime, q->successful.load(), stop_threads.load());
        frame_forward(q);
        q->finished = true;
        num_finished++;
        DEBUG("%s(): pid = %p, END, num_finished = %d", __func__, pid, num_finished.load());
    }
    void frame_forward(m_query_t *q)
    {
        bool repeat = false;
        // size_t repeat_count = 0;
        static cell tmpdata[1] = {0};
        // Prepare array
        cell *error_tmp, *data_tmp, error_param, data_param;
        size_t total_size = 0;
        auto data_size = clamp(q->data_size, 1U, 4096U);
        auto data = q->data_size ? q->data : tmpdata;
        // auto true_sleep = [&]() { ++repeat_count; std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL)); return true; };
        frame_mutex.lock();
        do
        {
            if (stop_threads || q->aborted || !(q->successful || q->retry_count >= QUERY_RETRY_COUNT))
                break;
            prms_mutex.lock();
            if (check_it_empty(q->prms))
            {
                prms_mutex.unlock();
                break;
            }
            // COPY PARAMS
            auto amx = q->prms->amx;
            auto fwd = q->prms->fwd;
            auto is_debug = q->prms->is_debug;
            int m_top = -2;
            size_t m_len = -1;
            prms_mutex.unlock();
            // Check FWD
            if (fwd == -1)
                break;
            std::lock_guard lock(fwd_mutex);
            if (is_debug)
            {
                Debugger *pDebugger = (Debugger *)amx->userdata[UD_DEBUGGER];
                m_len = pDebugger->m_pCalls.length();
                if (repeat = (m_top = pDebugger->m_Top) >= 0)
                    AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): Fucking AMXX-debugger in trace! m_top = %d, m_len = %d", __func__, m_top, m_len);
                // std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
                // pDebugger->Reset();
            }
            DEBUG("%s(): EXEC AMXX FORWARD = %d, is_debug = %d (%d / %d), failstate = %d, err = %d, query = %p, time = %f (%f / %f / %f), error = %s",
                  __func__, fwd, is_debug, m_top, m_len, q->failstate, q->err, q, q->queuetime, q->time_create, q->time_start, q->time_end, q->error.c_str());
            // Callback error
            if (!repeat && g_amxxapi.amx_Allot(amx, q->error.size() + 1, &error_param, &error_tmp) == AMX_ERR_NONE)
            {
                total_size += (q->error.size() + 1) << 2;
                setAmxString(error_tmp, q->error.c_str(), q->error.size());
                // Callback data
                if (g_amxxapi.amx_Allot(amx, data_size, &data_param, &data_tmp) == AMX_ERR_NONE)
                {
                    Q_memcpy(data_tmp, data, data_size << 2);
                    total_size += data_size << 2;
                    // public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
                    int ret = g_amxxapi.ExecuteForward(fwd, q->failstate, q, error_param, q->err, data_param, data_size, amx_ftoc(q->queuetime));
                    // int ret = g_amxxapi.ExecuteForward(fwd, failstate, q, q->error.c_str(), err, g_amxxapi.PrepareCellArray(data, data_size), data_size, amx_ftoc(queuetime));
                    DEBUG("%s(): AFTER FORWARD, fix = %d, hea = %p, param = %p, size = %d (%d)", __func__, amx->hea > data_param, amx->hea, data_param, amx->hea - error_param, total_size);
                }
                else
                    q->successful = false;
                // Fix heap
                if ((amx->hea - error_param) == total_size)
                    amx->hea = error_param;
                else
                    AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): can't fix amx->hea, possible memory leak!", __func__);
            }
            else
                q->successful = false;
        } while (0);
        // [&]() { std::this_thread::sleep_for(std::chrono::microseconds(10)); frame_mutex.unlock(); return true; });
        if (stop_threads)
            frame_mutex.unlock();
    }
    m_query_t *prepare_query(MYSQL *conn, const char *query, bool async = false, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout = 0, size_t conn_id = 0, cell *data = nullptr, size_t data_size = 0)
    {
        if (*query == '\0' || pri > MAX_QUERY_PRIORITY)
            return nullptr;
        auto q = new m_query_t{0};
        q->tid = std::thread::id(0);
        q->conn = conn;
        q->data = nullptr;
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
        q->data_size = data_size;
        q->data = data_size ? new cell[data_size] : nullptr;
        if (data != nullptr)
            Q_memcpy(q->data, data, data_size << 2);
        std::lock_guard lock(threads_mutex);
        m_queries[pri].insert({q->id, q});
        return q;
    }
    bool push_query(size_t conn_id, const char *query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout = 60U)
    {
        auto q = prepare_query(nullptr, query, true, pri, 1000 * timeout, conn_id, data, data_size);
        DEBUG("%s(): q = %p, query = %s, pri = %d, count = %d", __func__, q, query, pri, m_queries[pri].size());
        return q != nullptr;
    }
    bool exec_query(m_query_t *q)
    {
        if (q == nullptr || q->conn == nullptr || q->query.empty() || q->started || q->aborted)
            return false;
        q->started = true;
        q->time_start = get_time();
        DEBUG("%s(): conn = %p, start = %f, query = <%s>", __func__, q->conn, q->time_start, q->query.c_str());
        auto ret = mysql_query(q->conn, q->query.c_str());
        if (ret)
        {
            q->error = mysql_error(q->conn);
            q->err = mysql_errno(q->conn);
            // close(q->conn);
            return false;
        }
        DEBUG("%s(): ret = %d", __func__, ret);
        // q->finished = true;
        return get_result(q, true);
    }
    void free_query(m_query_t *q)
    {
        if (q == nullptr)
            return;
        if (q->result != nullptr)
        {
            DEBUG("%s(): FREE RESULT, q = %p, buffered = %d", __func__, q, q->is_buffered);
            if (!q->is_buffered)
            {
                auto status = mysql_free_result_start(q->result);
                while (status)
                {
                    status = wait_for_mysql(q->conn, status);
                    status = mysql_free_result_cont(q->result, status);
                }
            }
            else
                mysql_free_result(q->result);
            q->result = nullptr;
        }
        if (q->conn != nullptr && q->async)
        {
            DEBUG("%s(): CLOSE, q = %p, async = %d", __func__, q, q->async);
            auto status = mysql_close_start(q->conn);
            while (status)
            {
                status = wait_for_mysql(q->conn, status);
                status = mysql_close_cont(q->conn, status);
            }
            q->conn = nullptr;
        }
        if (q->data != nullptr && (stop_threads || q->aborted || q->successful))
        {
            DEBUG("%s(): FREE DATA, q = %p", __func__, q);
            delete q->data;
            q->data = nullptr;
            q->data_size = 0;
        }
        // Free memory
        q->f_names.clear();
        f_names_t().swap(q->f_names);
        q->f_count = 0;
        DEBUG("%s(): done!", __func__);
    }
    cell query_str(m_query_t *q, cell *ret, cell *buff, size_t buff_len)
    {
        if (q == nullptr || q->query.empty() || q->aborted || (stop_threads && q->async))
        {
            if (buff_len)
                *buff = '\0';
            return *ret = 0;
        }
        if (buff_len)
            *ret = set_amx_string(buff, q->query.c_str(), buff_len);
        else
            set_amx_string(ret, q->query.c_str(), RF_MAX_FIELD_SIZE);
        return 1;
    }
    cell affected_rows(m_query_t *q)
    {
        return (q == nullptr || q->conn == nullptr || (stop_threads && q->async)) ? -1 : mysql_affected_rows(q->conn);
    }
    cell insert_id(m_query_t *q)
    {
        return (q == nullptr || q->conn == nullptr || (stop_threads && q->async)) ? -1 : mysql_insert_id(q->conn);
    }
    cell field_name(m_query_t *q, size_t offset, cell *ret, cell *buff, size_t buff_len)
    {
        if (q == nullptr || q->result == nullptr || q->row == nullptr || offset >= q->f_count || q->aborted || (stop_threads && q->async))
        {
            if (buff_len)
                *buff = '\0';
            return *ret = 0;
        }
        if (buff_len)
            *ret = set_amx_string(buff, q->result->fields[offset].name, buff_len);
        else
            set_amx_string(ret, q->result->fields[offset].name, RF_MAX_FIELD_NAME - 1);
        return 1;
    }
    cell fetch_field(m_query_t *q, const char *f_name, int mode, cell *ret, cell *buff, size_t buff_len)
    {
        auto pos = q->f_names.find(f_name);
        return pos == q->f_names.end() ? 0 : fetch_field(q, pos->second, mode, ret, buff, buff_len);
    }
    cell fetch_field(m_query_t *q, size_t offset, int mode, cell *ret, cell *buff, size_t buff_len)
    {
        if (q == nullptr || q->result == nullptr || q->row == nullptr || offset >= q->f_count || q->aborted || (stop_threads && q->async))
        {
            if (buff_len)
                *buff = '\0';
            return *ret = 0;
        }
        DEBUG("%s(): q = %p, async = %d, name = %s, type = %d, stop = %d", __func__, q, q->async, q->result->fields[offset].name, q->result->fields[offset].type, stop_threads.load());
        DEBUG("%s(): value = %s, offset = %d, mode = %d, ret = %p, buff = %p, len = %d", __func__, q->row[offset], offset, mode, ret, buff, buff_len);
        float val;
        switch (mode)
        {
        case FT_AUTO:
            switch (q->result->fields[offset].type)
            {
            case MYSQL_TYPE_TINY:
            case MYSQL_TYPE_SHORT:
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_LONG:
            case MYSQL_TYPE_LONGLONG:
            case MYSQL_TYPE_BIT:
            case MYSQL_TYPE_NULL:
                *ret = q->row[offset] ? atoi(q->row[offset]) : 0;
                break;
            case MYSQL_TYPE_FLOAT:
            case MYSQL_TYPE_DOUBLE:
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_NEWDECIMAL:
                val = q->row[offset] ? (float)atof(q->row[offset]) : 0.0f;
                *ret = amx_ftoc(val);
                break;
            default:
                set_amx_string(ret, q->row[offset], RF_MAX_FIELD_SIZE - 1);
                break;
            }
            break;
        case FT_INT:
            *ret = q->row[offset] ? atoi(q->row[offset]) : 0;
            break;
        case FT_FLT:
            val = q->row[offset] ? (float)atof(q->row[offset]) : 0.0f;
            *ret = amx_ftoc(val);
            break;
        default:
            if (mode == FT_STR)
                set_amx_string(ret, q->row[offset], RF_MAX_FIELD_SIZE - 1);
            else if (buff_len)
                *ret = set_amx_string(buff, q->row[offset], buff_len);
            break;
        }
        DEBUG("%s(): ret = %d", __func__, *ret);
        return 1;
    }
    bool fetch_row(m_query_t *q)
    {
        int status;
        if (q == nullptr || q->result == nullptr || q->aborted || q->f_count <= 0 || (stop_threads && q->async))
            return false;
        DEBUG("%s(): query = %p, is_buffered = %d", __func__, q, q->is_buffered);
        if (!q->is_buffered)
        {
            status = mysql_fetch_row_start(&q->row, q->result);
            while (status)
            {
                status = wait_for_mysql(q->conn, status);
                status = mysql_fetch_row_cont(&q->row, q->result, status);
            }
        }
        else
            q->row = mysql_fetch_row(q->result);
        return q->row != nullptr;
    }
    cell field_count(m_query_t *q)
    {
        return q == nullptr || q->result == nullptr || q->aborted || (stop_threads && q->async) ? -1 : q->f_count;
    }
    bool get_result(m_query_t *q, bool is_buffered)
    {
        if (q == nullptr || q->conn == nullptr || q->result != nullptr || q->aborted || (stop_threads && q->async))
            return false;
        q->result = (q->is_buffered = is_buffered) ? mysql_store_result(q->conn) : mysql_use_result(q->conn);
        DEBUG("%s(): result = %p, query = %p, conn = %p, is_buffered = %d, state = %s", __func__, q->result, q, q->conn, is_buffered, q->conn->net.sqlstate);
        q->f_count = mysql_field_count(q->conn);
        if (q->result != nullptr)
            for (size_t i = 0; i < q->f_count; i++)
                q->f_names.insert({q->result->fields[i].name, i});
        DEBUG("%s(): field count = %d", __func__, q->f_count);
        return q->successful = q->result != nullptr;
    }
    uint32 num_rows(m_query_t *q)
    {
        return (q == nullptr || q->result == nullptr || q->aborted || (stop_threads && q->async)) ? -1 : mysql_num_rows(q->result);
    }
    MYSQL *connect(size_t conn_id, cell *err_num, cell *err_str, size_t err_str_size)
    {
        MYSQL *ret, *conn;
        const char *error;
        prm_list_it prms;
        conn = mysql_init(nullptr);
        prms_mutex.lock();
        if (conn == nullptr || check_it_empty(prms = get_connect(conn_id)))
        {
            prms_mutex.unlock();
            return nullptr;
        }
        // COPY PARAMS
        auto db_host = prms->db_host;
        auto db_user = prms->db_user;
        auto db_pass = prms->db_pass;
        auto db_name = prms->db_name;
        auto db_port = prms->db_port;
        auto db_chrs = prms->db_chrs;
        auto timeout = prms->timeout;
        prms_mutex.unlock();
        set_default_params(conn, timeout, db_chrs.c_str());
        ret = mysql_real_connect(conn, db_host.c_str(), db_user.c_str(), db_pass.c_str(), db_name.c_str(), db_port, NULL, SQL_FLAGS);
        if (ret == nullptr)
        {
            *err_num = mysql_errno(conn);
            error = mysql_error(conn);
        }
        else
        {
            *err_num = 0;
            conns.push_back(ret);
        }
        setAmxString(err_str, error == nullptr ? "" : error, err_str_size);
        return ret;
    }
    bool async_connect(m_query_t *q)
    {
        int status;
        MYSQL *ret;
        if (stop_threads || q == nullptr || q->aborted || !q->async)
            return false;
        q->conn = mysql_init(nullptr);
        prms_mutex.lock();
        if (q->conn == nullptr || check_it_empty(q->prms = get_connect(q->conn_id)))
        {
            prms_mutex.unlock();
            return false;
        }
        // COPY PARAMS
        auto db_host = q->prms->db_host;
        auto db_user = q->prms->db_user;
        auto db_pass = q->prms->db_pass;
        auto db_name = q->prms->db_name;
        auto db_port = q->prms->db_port;
        auto db_chrs = q->prms->db_chrs;
        auto timeout = q->prms->timeout;
        prms_mutex.unlock();
        set_default_params(q->conn, timeout, db_chrs.c_str());
        mysql_options(q->conn, MYSQL_OPT_NONBLOCK, 0);
        status = mysql_real_connect_start(&ret, q->conn, db_host.c_str(), db_user.c_str(), db_pass.c_str(), db_name.c_str(), db_port, NULL, SQL_FLAGS);
        DEBUG("%s(): MYSQL_OPT_NONBLOCK, conn = %p, port = %d, status = %d", __func__, q->conn, db_port, status);
        while (status)
        {
            status = wait_for_mysql(q->conn, status);
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
        auto it = std::find(conns.begin(), conns.end(), conn);
        if (it == conns.end())
            return false;
        DEBUG("%s(): STATIC, conn = %p", __func__, conn);
        conns.erase(it);
        conns.shrink_to_fit();
        mysql_close(conn);
        conn = nullptr;
        return true;
    }
    void close_all()
    {
        for (auto it : conns)
            mysql_close(it);
        conns.clear();
        conns.shrink_to_fit();
    }
    size_t add_connect(AMX *amx, int fwd, bool is_debug, std::string &db_host, std::string &db_user, std::string &db_pass, std::string &db_name, std::string &db_chrs, size_t timeout)
    {
        DEBUG("%s(): pid = %p, fwd = %d, host = <%s>, user = <%s>, pass = <%s>, name = <%s>, timeout = %d",
              __func__, gettid(), fwd, db_host.c_str(), db_user.c_str(), db_pass.c_str(), db_name.c_str(), timeout);
        prm_t prms;
        prms.amx = amx;
        prms.fwd = fwd;
        prms.is_debug = is_debug;
        size_t pos = db_host.find(':');
        prms.db_host = pos == std::string::npos ? db_host : db_host.substr(0, pos++);
        prms.db_port = pos == std::string::npos ? 3306 : atoi(db_host.substr(pos, db_host.size() - pos).c_str());
        prms.db_user = db_user;
        prms.db_pass = db_pass;
        prms.db_name = db_name;
        prms.db_chrs = db_chrs;
        prms.timeout = timeout;
        m_conn_prms.push_back(prms);
        DEBUG("%s(): conn = %d, is_debug = %d", __func__, m_conn_prms.size(), is_debug);
        return m_conn_prms.size();
    }
    prm_list_it get_connect(size_t conn_id)
    {
        auto count = m_conn_prms.size();
        if (!count || !conn_id || conn_id > count)
            return prm_list_it();
        return std::next(m_conn_prms.begin(), conn_id - 1);
    }
    void start_main()
    {
        // DEBUG("start(): pid = %d, START", gettid());
        stop_main = false;
        main_thread = std::thread(&mysql_mngr::main, this);
        /*
        try {
            throw std::runtime_error("Test");
        } catch (...) {
            UTIL_ServerPrint("Exception caught!");
        }
        */
    }
    void start()
    {
        //frame_mutex.lock();
        //fwd_mutex.lock();
        UTIL_ServerPrint("\n *** STATUS: m_frame = %u\n\n", m_frames);
        stop_threads = false;
    }
    void stop()
    {
        stop_threads = true;
        /*
            Пробуем заблокировать frame_mutex, поскольку start_frame() и abort() запускаются rehlds,
            попасть на ситуацию с разблокированным frame_mutex на 1мкс в start_frame мы не должны...
            В теории, он должен быть заблокирован...
        */
        UTIL_ServerPrint("\n *** STATUS: m_frame = %u, frame = %d, fwd = %d *** \n\n", m_frames, frame_mutex.try_lock(), fwd_mutex.try_lock());
        threads_mutex.lock();
        for (uint8 pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
        {
            std::lock_guard lock(prms_mutex);
            for (auto it = m_queries[pri].begin(); it != m_queries[pri].end(); it++)
            {
                auto q = it->second;
                q->aborted = true;
                q->conn_id = 0;
                q->prms = prm_list_it{};
            }
        }
        m_conn_prms.clear();
        prm_list_t().swap(m_conn_prms);
        close_all();
        fwd_mutex.unlock();
        frame_mutex.unlock();
        threads_mutex.unlock();
        // STATUS
        if (m_threads.size())
            UTIL_ServerPrint("[REFSAPI_SQL] m_threads(%d) not empty! num_threads = %d, num_finished = %d\n", m_threads.size(), num_threads.load(), num_finished.load());
        for (uint8 pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
            if (m_queries[pri].size())
                UTIL_ServerPrint("[REFSAPI_SQL] m_query[%d](%d) not empty!\n", pri, m_queries[pri].size());
        /*
        while (m_threads.size() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(QUERY_POOLING_INTERVAL));
        UTIL_ServerPrint("\n *** [REFSAPI_SQL] EMPTY! ***\n\n");
        */
    }
    double get_time()
    {
        elapsed_time_t queuetime = std::chrono::system_clock::now() - _time_0;
        return queuetime.count();
    }
    void start_frame()
    {
        bool has_threads = (num_threads - num_finished) > 0;
        // По окончании работа предыдущего кадра frame_mutex должен быть заблокирован
        if (!stop_threads && has_threads && !frame_mutex.try_lock())
        {
            // DEBUG("frame = %u", m_frames);
            // По окончании работы предыдущего кадра fwd_mutex должен быть разблокирован
            if (fwd_mutex.try_lock())
            {
                fwd_mutex.unlock();
                frame_mutex.unlock();
                std::this_thread::sleep_for(std::chrono::microseconds(1));
                /*
                    Может случиться так, что 1мкс не достаточно на блокировку mutex в других потоках,
                    поэтому нужен проработать вариант с "откатом состояний" mutex-ов, если frame_mutex
                    оказывается свободен на этом шаге!
                */
                fwd_mutex.lock();
                // Если frame_mutex занят - это правильная работа логики, разблокируем fwd_mutex
                if (!frame_mutex.try_lock())
                {
                    fwd_mutex.unlock();
                    m_frames++;
                }
            }
            /*
                В предыдущем кадре процессам не удалось захватить блокировку frame_mutex,
                или это первый кадр после смены карты...
            */
            else
                fwd_mutex.unlock();
        }
    }
    /*
            if (!frame_mutex.try_lock() && fwd_mutex.try_lock())
            {
                frame_mutex.unlock();
                fwd_mutex.unlock();
                //std::this_thread::sleep_for(std::chrono::milliseconds(1));
                //fwd_mutex.lock();
                //fwd_mutex.unlock();
            }
            */
    mysql_mngr() : main_thread()
    {
        stop_main = false;
        stop_threads = true;
        m_query_nums = 1;
        num_threads = 0;
        num_finished = 0;
        m_frames = 0;
        max_threads = clamp(std::thread::hardware_concurrency() >> 1, 1U, 4U);
        _time_0 = std::chrono::system_clock::now();
    }
    ~mysql_mngr()
    {
        stop();
        stop_main = true;
        if (main_thread.joinable())
            main_thread.join();
    }
    void DEBUG(const char *fmt, ...)
    {
#ifdef _DEBUG
        va_list arg_ptr;
        char str[1014], out[1024];
        va_start(arg_ptr, fmt);
        vsnprintf(str, sizeof(str) - 1, fmt, arg_ptr);
        va_end(arg_ptr);
        str[sizeof(str) - 1] = 0;
        snprintf(out, sizeof(out) - 1, "[DEBUG] %s\n", str);
        out[sizeof(out) - 1] = 0;
        std::lock_guard lock(std_mutex);
        SERVER_PRINT(out);
#endif
    }
};

extern mysql_mngr g_mysql_mngr;
#endif