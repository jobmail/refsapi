#include "precompiled.h"

#ifndef WITHOUT_SQL
//#define _DEBUG 1
#define MAX_QUERY_PRIORITY 5 // PRI->THREADS: 0 -> 8, 1 -> 4, 2 -> 2, 3 -> 1
#define QUERY_POOLING_INTERVAL 5
#define QUERY_RETRY_COUNT 3
#define SQL_FLAGS CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS
#define RF_MAX_FIELD_SIZE 256
#define RF_MAX_FIELD_NAME 64
#define gettid() std::this_thread::get_id()
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
    size_t timeout;      // ms
} prm_t;

typedef std::list<prm_t> prm_list_t;
typedef prm_list_t::iterator prm_list_it;
typedef std::chrono::_V2::system_clock::time_point time_point_t;
typedef std::chrono::duration<double> elapsed_time_t;

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
    double time_create;
    double time_start;
    double time_end;
    MYSQL *conn;
    MYSQL_RES *result;
    bool is_buffered;
    MYSQL_ROW row;
    prm_list_it prms;
    std::string error;
    std::map<std::string, size_t> f_names;
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
    std::map<std::thread::id, std::thread> m_threads;
    std::atomic<int> num_threads;
    std::atomic<int> num_finished;
    std::atomic<bool> stop_main;
    std::atomic<bool> stop_threads;
    std::mutex frame_mutex, threads_mutex, fwd_mutex, std_mutex;
    std::atomic<uint64> m_query_nums;
    std::vector<MYSQL *> conns;
    time_point_t _time_0;
    size_t m_frames;

private:
    int wait_for_mysql(MYSQL *conn, int status, int timeout_ms = 0)
    {
        int timeout, res;
        struct pollfd pfd;
        // DEBUG("wait_for_mysql(): conn = %d", conn);
        pfd.fd = mysql_get_socket(conn);
        // DEBUG("wait_for_mysql(): socket = %d", pfd.fd);
        pfd.events =
            (status & MYSQL_WAIT_READ ? POLLIN : 0) |
            (status & MYSQL_WAIT_WRITE ? POLLOUT : 0) |
            (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);
        // DEBUG("wait_for_mysql(): socket = %d, events = %d", pfd.fd, pfd.events);
        if (status & MYSQL_WAIT_TIMEOUT)
            timeout = timeout_ms ? timeout_ms : mysql_get_timeout_value_ms(conn);
        else
            timeout = -1;
        // DEBUG("wait_for_mysql(): timeout = %d", timeout);
        do
            res = poll(&pfd, 1, timeout);
        while (!stop_threads && res == -1 && errno == EINTR);
        // DEBUG("wait_for_mysql(): res = %d", res);
        if (res <= 0 || stop_threads)
            return MYSQL_WAIT_TIMEOUT;
        else
        {
            int status = 0;
            if (pfd.revents & POLLIN)
                status |= MYSQL_WAIT_READ;
            if (pfd.revents & POLLOUT)
                status |= MYSQL_WAIT_WRITE;
            if (pfd.revents & POLLPRI)
                status |= MYSQL_WAIT_EXCEPT;
            return status;
        }
    }
    void set_default_params(MYSQL *conn, prm_list_it prms)
    {
        auto prot_type = MYSQL_PROTOCOL_TCP;
        mysql_optionsv(conn, MYSQL_OPT_PROTOCOL, (void *)&prot_type);
        mysql_optionsv(conn, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&prms->timeout);
        my_bool my_true = true;
        mysql_optionsv(conn, MYSQL_OPT_RECONNECT, (const char *)&my_true);
        if (!prms->db_chrs.empty())
            mysql_set_character_set(conn, prms->db_chrs.c_str());
    }

public:
    void main()
    {
        DEBUG("main(): pid = %d, max_thread = %d, interval = %d, START", gettid(), 1 << MAX_QUERY_PRIORITY, QUERY_POOLING_INTERVAL);
        while (!stop_main)
        {
            run_query();
            usleep(QUERY_POOLING_INTERVAL * 1000);
        }
    }
    void run_query()
    {
        int pri;
        std::thread::id key;
        m_query_t *q;
        std::vector<m_query_list_t::iterator> finished;
        for (pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
        {
            for (auto it = m_queries[pri].begin(); it != m_queries[pri].end(); it++)
            {
                q = it->second;
                if (q->finished || q->aborted)
                {
                    key = q->tid;
                    if (key != std::thread::id())
                    {
                        DEBUG("run_query(): cleaner, total finished: %d", num_finished.load());
                        auto &t = m_threads[key];
                        if (t.joinable())
                            t.join();
                        num_threads--;
                        num_finished--;
                        m_threads.erase(key);
                    }
                    if (!stop_threads && !q->aborted && !q->successful && ++q->retry_count < QUERY_RETRY_COUNT)
                    {
                        q->started = false;
                        q->finished = false;
                    }
                    else
                    {
                        if (q->aborted)
                        {
                            DEBUG("run_query(): ABORTED, q = %p", q);
                            free_query(q);
                        }
                        finished.push_back(it);
                        // DEBUG("run_query(): cleaner, threads left = %d, num_threads = %d", m_threads.size(), num_threads.load());
                        continue;
                    }
                }
                if (!stop_threads && !q->started && !q->aborted && (m_threads.size() - num_finished) < (max_threads << (MAX_QUERY_PRIORITY - q->pri)))
                {
                    q->started = true;
                    q->time_start = get_time();
                    std::thread t(&mysql_mngr::exec_async_query, this, pri, q);
                    // m_threads.emplace(key = t.get_id(), t);
                    m_threads.insert({key = t.get_id(), std::move(t)});
                    q->tid = key;
                    num_threads++;
                    DEBUG("run_query(): thread starting pid = %p, q = %p, threads_num = %d", key, q, m_threads.size());
                }
            }
        }
        std::lock_guard<std::mutex> lock(threads_mutex);
        for (auto it : finished)
        {
            auto q = it->second;
            DEBUG("run_query(): free query = %p", q);
            m_queries[q->pri].erase(q->id);
            delete q;
        }
    }
    void exec_async_query(int pri, m_query_t *q)
    {
        int err, status;
        static cell tmpdata[1] = {0};
        std::thread::id pid = gettid();
        DEBUG("exec_async_query(): pid = %p, q = %p, typle = %d, START", pid, q, q->conn_id);
        int failstate = TQUERY_SUCCESS;
        if (async_connect(q))
        {
            DEBUG("exec_async_query(): pid = %p, start = %f, query = <%s>", pid, q->time_start, q->query.c_str());
            status = mysql_real_query_start(&err, q->conn, q->query.c_str(), q->query.size() + 1);
            while (status)
            {
                status = wait_for_mysql(q->conn, status, 1000 * q->timeout);
                status = mysql_real_query_cont(&err, q->conn, status);
            }
            if (err)
                failstate = TQUERY_QUERY_FAILED;
        }
        else
            failstate = TQUERY_CONNECT_FAILED;
        q->successful = failstate == TQUERY_SUCCESS && get_result(q, false);
        float queuetime = q->time_end - q->time_start;
        frame_mutex.lock();
        if (!stop_threads && !q->aborted && q->prms->fwd != -1)
        {
            std::lock_guard lock(fwd_mutex);
            // public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
            DEBUG("exec_async_query: EXEC AMXX FORWARD = %d, is_debug = %d, failstate = %d, err = %d, query = %p, time = %f (%f / %f / %f), error = %s", q->prms->fwd, q->prms->is_debug, failstate, err, q, queuetime, q->time_create, q->time_start, q->time_end, q->error.c_str());
            auto data_size = clamp(q->data_size, 1U, 4096U);
            auto data = q->data_size ? q->data : tmpdata;
            // Prepare array
            cell *tmp, error_param, data_param;
            auto amx = q->prms->amx;
            // Callback error
            g_amxxapi.amx_Allot(amx, q->error.size() + 1, &error_param, &tmp);
            setAmxString(tmp, q->error.c_str(), q->error.size());
            // Callback data
            g_amxxapi.amx_Allot(amx, data_size, &data_param, &tmp);
            Q_memcpy(tmp, data, data_size << 2);
            auto total_size = (q->error.size() + 1 + data_size) << 2;
            int ret = g_amxxapi.ExecuteForward(q->prms->fwd, failstate, q, error_param, err, data_param, data_size, amx_ftoc(queuetime));
            //int ret = g_amxxapi.ExecuteForward(q->prms->fwd, failstate, q, q->error.c_str(), err, g_amxxapi.PrepareCellArray(data, data_size), data_size, amx_ftoc(queuetime));
            DEBUG("exec_async_query: AFTER FORWARD, fix = %d, hea = %p, param = %p, size = %d (%d)", amx->hea > data_param, amx->hea, data_param, amx->hea - error_param, total_size);
            // Fix heap
            if ((amx->hea - error_param) == total_size)
                amx->hea = error_param;
            else
                AMXX_LogError(amx, AMX_ERR_NATIVE, "%s: can't fix amx->hea, possible memory leak!", __FUNCTION__);
        } else if (stop_threads)
            frame_mutex.unlock();
        free_query(q);
        num_finished++;
        q->finished = true;
        DEBUG("exec_async_query(): pid = %p, END, num_finished = %d", pid, num_finished.load());
    }
    m_query_t *prepare_query(MYSQL *conn, const char *query, bool async = false)
    {
        auto q = new m_query_t;
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
        return q;
    }
    bool push_query(size_t conn_id, const char *query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout = 60U)
    {
        if (*query == 0 || pri > MAX_QUERY_PRIORITY)
            return false;
        auto q = prepare_query(nullptr, query, true);
        q->pri = pri;
        q->retry_count = 0;
        q->timeout = timeout;
        q->conn_id = conn_id;
        q->data_size = data_size;
        q->data = data_size ? new cell[data_size] : nullptr;
        Q_memcpy(q->data, data, data_size << 2);
        std::lock_guard lock(threads_mutex);
        m_queries[pri].insert({q->id, q});
        DEBUG("push_query(): q = %p, query = %s, pri = %d, count = %d", q, query, pri, m_queries[pri].size());
        return true;
    }
    bool exec_query(m_query_t *q)
    {
        if (q->conn == nullptr || q->query.empty() || q->started || q->aborted)
            return false;
        q->started = true;
        q->time_start = get_time();
        if (mysql_query(q->conn, q->query.c_str()))
        {
            q->error = mysql_error(q->conn);
            close(q->conn);
            q->conn = nullptr;
            return false;
        }
        return q->successful = get_result(q, true);
    }
    void free_query(m_query_t *q)
    {
        if (q->result != nullptr)
        {
            DEBUG("free_query: FREE RESULT, q = %p", q);
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
            DEBUG("free_query: CLOSE, q = %p, async = %d", q, q->async);
            auto status = mysql_close_start(q->conn);
            while (status)
            {
                status = wait_for_mysql(q->conn, status);
                status = mysql_close_cont(q->conn, status);
            }
            q->conn = nullptr;
        }
        if (q->data != nullptr && (q->successful || q->aborted || stop_threads))
        {
            DEBUG("free_query: FREE DATA, q = %p", q);
            delete q->data;
            q->data = nullptr;
        }
    }
    cell query_str(m_query_t *q, cell *ret, cell *buff, size_t buff_len)
    {
        if (buff_len)
            *ret = set_amx_string(buff, q->query.c_str(), buff_len);
        else
            set_amx_string(ret, q->query.c_str(), RF_MAX_FIELD_SIZE);
        return 1;
    }
    cell affected_rows(m_query_t *q)
    {
        return mysql_affected_rows(q->conn);
    }
    cell insert_id(m_query_t *q)
    {
        return mysql_insert_id(q->conn);
    }
    cell field_name(m_query_t *q, size_t offset, cell *ret, cell *buff, size_t buff_len)
    {
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
        if (q->result == nullptr || q->aborted || !q->row)
            return 0;
        // DEBUG("fetch_field(): name = %s, type = %d", q->result->fields[offset].name, q->result->fields[offset].type);
        // DEBUG("fetch_field(): field = %s, type = %d, q = %p, offset = %d, mode = %d, ret = %p, buff = %p, len = %d", q->row[offset], q->result->fields[offset].type, q, offset, mode, ret, buff, buff_len);
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
        // DEBUG("fetch_field(): ret = %d", *ret);
        return 1;
    }
    bool fetch_row(m_query_t *q)
    {
        // DEBUG("fetch_row(): query = %p, is_buffered = %d", q, q->is_buffered);
        if (q->result == nullptr || q->aborted || !q->f_count)
            return false;
        if (!q->is_buffered)
        {
            auto status = mysql_fetch_row_start(&q->row, q->result);
            while (status)
            {
                status = wait_for_mysql(q->conn, status);
                status = mysql_fetch_row_cont(&q->row, q->result, status);
            }
        }
        else
            q->row = mysql_fetch_row(q->result);
        // if (q->time_start == q->time_end)
        // DEBUG("fetch_row(): status = %d, %s", status, q->row != NULL ? "done" : "end +++");
        return q->row != NULL;
    }
    cell field_count(m_query_t *q)
    {
        return q->result == nullptr || q->aborted ? -1 : q->f_count;
    }
    bool get_result(m_query_t *q, bool is_buffered)
    {
        if (q->result == nullptr && !q->aborted)
        {
            q->result = (q->is_buffered = is_buffered) ? mysql_store_result(q->conn) : mysql_use_result(q->conn);
            q->time_end = get_time();
            DEBUG("get_result(): result = %p, query = %p, conn = %p, is_buffered = %d, state = %s", q->result, q, q->conn, is_buffered, q->conn->net.sqlstate);
            if ((q->f_count = mysql_field_count(q->conn)) && q->result)
                for (int i = 0; i < q->f_count; i++)
                    q->f_names.insert({q->result->fields[i].name, i});
            DEBUG("get_result(): done, field count = %d", q->f_count);
            return true;
        }
        return false;
    }
    size_t num_rows(m_query_t *q)
    {
        return q->result == nullptr || q->aborted ? -1 : mysql_num_rows(q->result);
    }
    MYSQL *connect(size_t conn_id, cell *err_num, cell *err_str, size_t err_str_size)
    {
        MYSQL *ret, *conn = mysql_init(NULL);
        auto prms = get_connect(conn_id);
        if (!(conn = mysql_init(0)) || check_it_empty(prms))
            return nullptr;
        set_default_params(conn, prms);
        if (ret = mysql_real_connect(conn, prms->db_host.c_str(), prms->db_user.c_str(), prms->db_pass.c_str(), prms->db_name.c_str(), prms->db_port, NULL, SQL_FLAGS))
        {
            conns.push_back(ret);
        }
        *err_num = mysql_errno(conn);
        setAmxString(err_str, mysql_error(conn), err_str_size);
        return ret;
    }
    bool async_connect(m_query_t *q)
    {
        if (stop_threads || q->aborted)
            return false;
        MYSQL *ret;
        q->conn = mysql_init(NULL);
        if (!(q->conn = mysql_init(0)) || check_it_empty(q->prms = get_connect(q->conn_id)))
            return false;
        set_default_params(q->conn, q->prms);
        mysql_options(q->conn, MYSQL_OPT_NONBLOCK, 0);
        auto status = mysql_real_connect_start(&ret, q->conn, q->prms->db_host.c_str(), q->prms->db_user.c_str(), q->prms->db_pass.c_str(), q->prms->db_name.c_str(), q->prms->db_port, NULL, SQL_FLAGS);
        DEBUG("async_connect(): MYSQL_OPT_NONBLOCK, conn = %p, port = %d, status = %d", q->conn, q->prms->db_port, status);
        while (status)
        {
            status = wait_for_mysql(q->conn, status);
            status = mysql_real_connect_cont(&ret, q->conn, status);
        }
        if (!ret)
        {
            q->error = mysql_error(q->conn);
            DEBUG("async_connect(): error = %s", q->error.c_str());
            return false;
        }
        return true;
    }
    bool close(MYSQL *conn)
    {
        auto it = std::find(conns.begin(), conns.end(), conn);
        if (it == conns.end())
            return false;
        DEBUG("close(): STATIC, conn = %p", conn);
        conns.erase(it);
        mysql_close(conn);
        return true;
    }
    void close_all()
    {
        for (auto it : conns)
            mysql_close(it);
        conns.clear();
    }
    size_t add_connect(AMX *amx, int fwd, bool is_debug, std::string &db_host, std::string &db_user, std::string &db_pass, std::string &db_name, std::string &db_chrs, size_t timeout)
    {
        // DEBUG("add_connect(): pid = %d, fwd = %d, host = <%s>, user = <%s>, pass = <%s>, name = <%s>, timeout = %d",
        //     gettid(), fwd, db_host.c_str(), db_user.c_str(), db_pass.c_str(), db_name.c_str(), timeout_ms
        //);
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
        DEBUG("add_connect(): conn = %d, is_debug = %d", m_conn_prms.size(), is_debug);
        return m_conn_prms.size();
    }
    prm_list_it get_connect(size_t conn_id)
    {
        if (conn_id < 1 || conn_id > m_conn_prms.size())
            return prm_list_it();
        return std::next(m_conn_prms.begin(), conn_id - 1);
    }
    void start_main()
    {
        // DEBUG("start(): pid = %d, START", gettid());
        stop_main = false;
        main_thread = std::thread(&mysql_mngr::main, this);
    }
    void start()
    {
        _time_0 = std::chrono::system_clock::now();
        frame_mutex.lock();
        stop_threads = false;
    }
    void stop()
    {
        stop_threads = true;
        abort();
        m_conn_prms.clear();
    }
    void abort()
    {
        threads_mutex.lock();
        for (int pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
        {
            for (auto it = m_queries[pri].begin(); it != m_queries[pri].end(); it++)
            {
                auto q = it->second;
                q->aborted = true;
            }
        }
        threads_mutex.unlock();
        frame_mutex.unlock();
        // Wait before
        while (num_threads > 0)
            usleep(QUERY_POOLING_INTERVAL * 1000);
    }
    double get_time()
    {
        elapsed_time_t queuetime = std::chrono::system_clock::now() - _time_0;
        return queuetime.count();
    }
    void start_frame()
    {
        bool has_threads = (num_threads - num_finished) > 0;
#ifdef _DEBUG
        if (has_threads)
            UTIL_ServerPrint("[ START ] ==>: frame = %u, num_threads = %d, stop_threads = %d\n", m_frames, num_threads.load(), stop_threads.load());
#endif
        if (!stop_threads && has_threads)    //!stop_threads && 
        {
            frame_mutex.unlock();
            usleep(1);
            fwd_mutex.lock();
            frame_mutex.try_lock();
            fwd_mutex.unlock();
        }
#ifdef _DEBUG
        if (has_threads)
            UTIL_ServerPrint("[ END ] ==>: frame = %u, num_threads = %d, stop_threads = %d\n", m_frames, num_threads.load(), stop_threads.load());
#endif
        m_frames++;
    }
    mysql_mngr() : main_thread()
    {
        stop_main = false;
        stop_threads = true;
        m_query_nums = 1;
        num_threads = 0;
        num_finished = 0;
        m_frames = 0;
        max_threads = clamp(std::thread::hardware_concurrency() >> 1, 1U, 4U);
    }
    ~mysql_mngr()
    {
        stop();
        while (num_threads)
            usleep(QUERY_POOLING_INTERVAL * 1000);
        stop_main = true;
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