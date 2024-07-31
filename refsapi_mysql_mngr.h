#include "precompiled.h"

#define MAX_QUERY_PRIORITY              3   // PRI->THREADS: 0 -> 8, 1 -> 4, 2 -> 2, 3 -> 1
#define QUERY_POOLING_INTERVAL          5
#define SQL_FLAGS                       CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS
#define RF_MAX_FIELD_SIZE               255
#define RF_MAX_FIELD_NAME               63
#define check_conn_rc(rc, mysql) \
do {\
if (rc)\
{\
    mysql_close(mysql);\
    return nullptr;\
}\
} while(0)
#define gettid()                        std::this_thread::get_id()
#define TQUERY_CONNECT_FAILED           -2
#define TQUERY_QUERY_FAILED	            -1
#define TQUERY_SUCCESS		            0

typedef enum field_types_e
{
    FT_AUTO,
    FT_INT,
    FT_FLT,
    FT_STR,
    FT_STR_BUFF,
} field_types_t;

typedef struct m_conn_prm_s
{
    int fwd;
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_name;
    std::string db_chrs;    // charterset
    size_t db_port;         // 3306
    size_t timeout;         // ms
} m_conn_prm_t;

typedef std::list<m_conn_prm_t> m_conn_prm_list_t;
typedef m_conn_prm_list_t::iterator m_conn_prm_list_it;

typedef struct m_query_s
{
    uint64 id;
    std::thread::id tid;
    uint8 pri;
    size_t conn_id;
    std::string query;
    cell* data;
    size_t data_size;
    size_t timeout;
    bool started;
    bool finished;
    double time_create;
    double time_start;
    double time_end;
    MYSQL *conn;
    MYSQL_RES* result;
    bool is_buffered;
    MYSQL_ROW row;
    m_conn_prm_list_it prms;
    std::string error;
} m_query_t;

typedef std::map<uint64, m_query_t*> m_query_list_t;
typedef m_query_list_t::iterator m_query_list_it;

class mysql_mngr {
    const size_t max_threads = clamp(std::thread::hardware_concurrency(), 1U, 4U);
    m_conn_prm_list_t m_conn_prms;
    m_query_list_t m_queries[MAX_QUERY_PRIORITY + 1];
    std::thread main_thread;
    std::map<std::thread::id, std::thread> m_threads;
    std::atomic<int> num_finished;
    bool stop_threads;
    std::mutex threads_mutex;
    std::atomic<uint64> m_query_nums;
    std::vector<MYSQL*> conns;
private:
    int wait_for_mysql(MYSQL *conn, int status, int timeout_ms = 0)
    {
        int timeout, res;
        struct pollfd pfd;
        //UTIL_ServerPrint("[DEBUG] wait_for_mysql(): conn = %d\n", conn);
        pfd.fd = mysql_get_socket(conn);
        //UTIL_ServerPrint("[DEBUG] wait_for_mysql(): socket = %d\n", pfd.fd);
        pfd.events =
            (status & MYSQL_WAIT_READ ? POLLIN : 0) |
            (status & MYSQL_WAIT_WRITE ? POLLOUT : 0) |
            (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);
        //UTIL_ServerPrint("[DEBUG] wait_for_mysql(): socket = %d, events = %d\n", pfd.fd, pfd.events);    
        if (status & MYSQL_WAIT_TIMEOUT)
            timeout = timeout_ms ? timeout_ms : mysql_get_timeout_value(conn);
        else
            timeout = -1;
        //UTIL_ServerPrint("[DEBUG] wait_for_mysql(): timeout = %d\n", timeout);
        res = poll(&pfd, 1, timeout);
        //UTIL_ServerPrint("[DEBUG] wait_for_mysql(): res = %d\n", res);
        if (res == 0)
            return MYSQL_WAIT_TIMEOUT;
        else if (res < 0)
            return MYSQL_WAIT_TIMEOUT; //MYSQL_WAIT_EXCEPT;
        else {
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
    void set_default_params(MYSQL *conn, m_conn_prm_list_it prms)
    {
        enum mysql_protocol_type prot_type = MYSQL_PROTOCOL_TCP;
        mysql_optionsv(conn, MYSQL_OPT_PROTOCOL, (void *)&prot_type);
        mysql_optionsv(conn, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&prms->timeout);
        if (!prms->db_chrs.empty())
            mysql_set_character_set(conn, prms->db_chrs.c_str());
    }

public:
    void main()
    {
        UTIL_ServerPrint("[DEBUG] main(): pid = %d, max_thread = %d, interval = %d, START\n", gettid(), 1 << MAX_QUERY_PRIORITY, QUERY_POOLING_INTERVAL);
        while (!stop_threads)
        {
            run_query();
            usleep(QUERY_POOLING_INTERVAL * 1000);
        }
    }
    void run_query()
    {
        int pri;
        std::thread::id key;
        m_query_t* q;
        std::vector<m_query_list_t::iterator> finished;
        for (pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
        {
            for (auto it = m_queries[pri].begin(); it != m_queries[pri].end(); it++)
            {
                q = it->second;
                if (q->finished)
                {
                    UTIL_ServerPrint("[DEBUG] run_query(): cleaner, total finished: %d\n", num_finished.load());
                    key = q->tid;
                    auto &t = m_threads[key];
                    if (t.joinable())
                        t.join();
                    num_finished--;
                    m_threads.erase(key);
                    finished.emplace_back(it);
                    UTIL_ServerPrint("[DEBUG] run_query(): cleaner, threads left = %d\n", m_threads.size());
                    continue;
                }
                if (!q->started && (m_threads.size() - num_finished.load()) < (max_threads << (MAX_QUERY_PRIORITY - q->pri)))
                {
                    UTIL_ServerPrint("[DEBUG] run_query(): pid = %d, threads_num = %d\n", gettid(), m_threads.size());
                    q->started = true;
                    q->time_start = g_RehldsData->GetTime();
                    std::thread t(&mysql_mngr::exec_async_query, this, pri, q);
                    //m_threads.emplace(key = t.get_id(), t);
                    m_threads.insert({ key = t.get_id(), std::move(t) });
                    q->tid = key;
                    UTIL_ServerPrint("[DEBUG] run_query(): thread starting pid = %d\n", key);
                }
            }
        }
        for (auto it : finished)
        {
            std::lock_guard<std::mutex> lock(threads_mutex);
            UTIL_ServerPrint("[DEBUG] run_query(): free query = %d\n", it->second->id);
            m_queries[it->second->pri].erase(it);
            delete it->second;
        }
    }
    void exec_async_query(int pri, m_query_t *q)
    {
        int err, status;
        std::thread::id pid = gettid();
        //auto q = &query_list->second;
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, typle = %d, START\n", pid, q->conn_id);
        int failstate = TQUERY_SUCCESS;
        if (async_connect(q))
        {
            UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, start = %f, query = <%s>\n", pid, q->time_start, q->query.c_str());
            status = mysql_real_query_start(&err, q->conn, q->query.c_str(), q->query.size());
            while (status)
            {
                status = wait_for_mysql(q->conn, status, q->timeout);
                status = mysql_real_query_cont(&err, q->conn, status);
            }
            if (err)
                failstate = TQUERY_QUERY_FAILED;
        }
        else
            failstate = TQUERY_CONNECT_FAILED;
        q->time_end = g_RehldsData->GetTime();
        float queuetime = q->time_end - q->time_start;
        if (q->prms->fwd != -1)
        {
            std::lock_guard<std::mutex> lock(threads_mutex);
            UTIL_ServerPrint("[DEBUG] exec_async_query: EXEC AMXX FORWARD = %d, query = %d, time = %f, error = %s\n", q->prms->fwd, q, queuetime, q->error.c_str());
            //public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
            g_amxxapi.ExecuteForward(q->prms->fwd, failstate, q, g_amxxapi.PrepareCharArrayA(q->error.data(), q->error.size(), false), err, g_amxxapi.PrepareCellArrayA(q->data, q->data_size, false), q->data_size, amx_ftoc(queuetime));
            UTIL_ServerPrint("[DEBUG] exec_async_query: AFTER FORWARD\n");
        }
        UTIL_ServerPrint("[DEBUG] exec_async_query: FREE RESULT\n");
        if (q->result != nullptr)
        {
            mysql_free_result(q->result);
            q->result = nullptr;
        }
        UTIL_ServerPrint("[DEBUG] exec_async_query: CLOSE\n");
        if (q->conn != nullptr)
        {
            mysql_close(q->conn);
            q->conn = nullptr;
        }
        UTIL_ServerPrint("[DEBUG] exec_async_query: FREE DATA\n");
        if (q->data != nullptr)
        {
            delete q->data;
            q->data = nullptr;
        }
        auto a = q->row[0];
        num_finished++;
        q->finished = true;
        //UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, err = %d, end = %f, query_time = %f\n", pid, err, q->time_end, q->time_end - q->time_start);
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, END, num_finished = %d\n", pid, num_finished.load());

    }
    bool push_query(size_t conn_id, const char *query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout_ms = 60000)
    {
        if (*query == 0 || pri > MAX_QUERY_PRIORITY)
            return false;
        std::lock_guard<std::mutex> lock(threads_mutex);
        UTIL_ServerPrint("[DEBUG] push_query(): query = %s\n", query);
        auto m_query = new m_query_t;
        m_query->conn = nullptr;
        m_query->result = nullptr;
        m_query->started = false;
        m_query->finished = false;
        m_query->id = m_query_nums++;
        m_query->pri = pri;
        m_query->conn_id = conn_id;
        m_query->query = query;
        m_query->data = data_size ? new cell[data_size] : nullptr;
        m_query->data_size = data_size;
        m_query->timeout = timeout_ms;
        Q_memcpy(m_query->data, data, data_size << 2);
        m_query->time_create = g_RehldsData->GetTime();
        UTIL_ServerPrint("[DEBUG] push_query(): BEFORE LOCK\n");
        //m_queries[pri][m_query->id] = m_query;
        m_queries[pri].emplace(m_query->id, m_query);
        //m_queries[pri].insert({ m_query->id, m_query });
        UTIL_ServerPrint("[DEBUG] push_query(): pri = %d, count = %d\n", pri, m_queries[pri].size());
        return true;
    }
    MYSQL_RES* exec_query(MYSQL *conn, std::string *query)
    {
        if (conn == nullptr || query->empty())
            return nullptr;
        int rc = mysql_query(conn, query->c_str());
        check_conn_rc(rc, conn);
        return mysql_store_result(conn);
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
            set_amx_string(ret, q->result->fields[offset].name, RF_MAX_FIELD_NAME);
        return 1;
    }
    cell fetch_field(m_query_t *q, size_t offset, int type, cell *ret, cell *buff, size_t buff_len)
    {
        switch (type)
        {
            case FT_AUTO:
                switch (q->result->fields[offset].type)
                {
                    case MYSQL_TYPE_TINY:
                    case MYSQL_TYPE_SHORT:
                    case MYSQL_TYPE_INT24:
                    case MYSQL_TYPE_LONG:
                    case MYSQL_TYPE_BIT:
                        *ret = atoi(q->row[offset]);
                        return 1;
                    case MYSQL_TYPE_FLOAT:
                    case MYSQL_TYPE_DOUBLE:
                    case MYSQL_TYPE_DECIMAL:
                        *ret = (float)atof(q->row[offset]);
                        return 1;
                    default:
                        set_amx_string(ret, q->row[offset], RF_MAX_FIELD_SIZE);
                        return 1;
                }
                
            case FT_INT:
                *ret = atoi(q->row[offset]);
                return 1;
            case FT_FLT:
                *ret = (float)atof(q->row[offset]);
                return 1;
            default:
                if (type == FT_STR)
                    set_amx_string(ret, q->row[offset], RF_MAX_FIELD_SIZE);
                else if (buff_len)
                    *ret = set_amx_string(buff, q->row[offset], buff_len);
                return 1;
        }
    }
    bool fetch_row(m_query_t *q)
    {
        //UTIL_ServerPrint("[DEBUG] fetch_row(): query = %d\n", q);
        if (q->result == nullptr)
            return false;
        /*
        if (!q->is_buffered)
        {
            auto status = mysql_fetch_row_start(&q->row, q->result);
            while (status) {
                status = wait_for_mysql(q->conn, status);
                status = mysql_fetch_row_cont(&q->row, q->result, status);
            }
        }
        else
            q->row = mysql_fetch_row(q->result);
        */
        q->row = mysql_fetch_row(q->result);
        return q->row != 0;
    }
    cell field_count(m_query_t *q)
    {
        return q->result->field_count;
    }
    bool get_result(m_query_t *q, bool is_buffered)
    {
        if (q->result == nullptr)
        {
            q->result = (q->is_buffered = is_buffered) ? mysql_store_result(q->conn) : mysql_use_result(q->conn);
            return true;
        }
        return false;
    }
    size_t num_rows(m_query_t *q)
    {
        return mysql_num_rows(q->result);
    }
    MYSQL* connect(size_t conn_id, cell *err_num, cell *err_str, size_t err_str_size)
    {
        MYSQL *ret, *conn = mysql_init(NULL);
        auto prms = get_connect(conn_id);
        if (!(conn = mysql_init(0)) || check_it_empty(prms))
            return nullptr;
        set_default_params(conn, prms);
        if (ret = mysql_real_connect(conn, prms->db_host.c_str(), prms->db_user.c_str(), prms->db_pass.c_str(), prms->db_name.c_str(), prms->db_port, NULL, SQL_FLAGS))
            conns.push_back(ret);
        *err_num = mysql_errno(conn);
        setAmxString(err_str, mysql_error(conn), err_str_size);
        return ret;
    }
    bool close(MYSQL *conn)
    {
        auto it = std::find(conns.begin(), conns.end(), conn);
        if (it == conns.end())
            return false;
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
    bool async_connect(m_query_t *q)
    {
        MYSQL *ret;
        q->conn = mysql_init(NULL);
        if (!(q->conn = mysql_init(0)) || check_it_empty(q->prms = get_connect(q->conn_id)))
            return false;
        set_default_params(q->conn, q->prms);
        
        mysql_options(q->conn, MYSQL_OPT_NONBLOCK, 0);
        int status = mysql_real_connect_start(&ret, q->conn, q->prms->db_host.c_str(), q->prms->db_user.c_str(), q->prms->db_pass.c_str(), q->prms->db_name.c_str(), q->prms->db_port, NULL, SQL_FLAGS);
        //UTIL_ServerPrint("[DEBUG] connect(): MYSQL_OPT_NONBLOCK, conn = %d, port = %d, status = %d\n", q->conn, q->prms->db_port, status);
        while (status)
        {
            status = wait_for_mysql(q->conn, status);
            status = mysql_real_connect_cont(&ret, q->conn, status);
        }
        //UTIL_ServerPrint("[DEBUG] connect(): ret = %d\n", ret);
        if (!ret)
        {
            q->error = mysql_error(q->conn);
            //UTIL_ServerPrint("[DEBUG] connect(): error = %s\n", q->error.c_str());
            return false;
        }
        return true;
    }
    size_t add_connect(int fwd, std::string &db_host, std::string &db_user, std::string &db_pass, std::string &db_name, std::string &db_chrs, size_t timeout_ms = 250)
    {
        //UTIL_ServerPrint("[DEBUG] add_connect(): pid = %d, fwd = %d, host = <%s>, user = <%s>, pass = <%s>, name = <%s>, timeout = %d\n",
        //    gettid(), fwd, db_host.c_str(), db_user.c_str(), db_pass.c_str(), db_name.c_str(), timeout_ms
        //);
        m_conn_prm_t prms;
        prms.fwd = fwd;
        size_t pos = db_host.find(':');
        prms.db_host = pos == std::string::npos ? db_host : db_host.substr(0, pos++);
        prms.db_port = pos == std::string::npos ? 3306 : atoi(db_host.substr(pos, db_host.size() - pos).c_str());
        prms.db_user = db_user;
        prms.db_pass = db_pass;
        prms.db_name = db_name;
        prms.db_chrs = db_chrs;
        prms.timeout = timeout_ms;
        m_conn_prms.push_back(prms);
        //UTIL_ServerPrint("[DEBUG] add_connect(): conn = %d\n", m_conn_prms.size());
        return m_conn_prms.size();
    }
    m_conn_prm_list_it get_connect(size_t conn_id)
    {
        if (conn_id < 1 || conn_id > m_conn_prms.size())
            return m_conn_prm_list_it();
        return std::next(m_conn_prms.begin(), conn_id - 1);
    }
    void start()
    {
        //UTIL_ServerPrint("[DEBUG] start(): pid = %d, START\n", gettid());
        main_thread = std::thread(&mysql_mngr::main, this);
    }
    mysql_mngr() : main_thread()
    {
        stop_threads = false;
        m_query_nums.store(0);
        num_finished = 0;
    }
    ~mysql_mngr()
    {
        stop_threads = true;
        main_thread.join();
    }
};

extern mysql_mngr g_mysql_mngr;