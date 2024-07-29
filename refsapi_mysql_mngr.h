#include "precompiled.h"

#define MAX_QUERY_PRIORITY              3   // PRI->THREADS: 0 -> 8, 1 -> 4, 2 -> 2, 3 -> 1
#define QUERY_POOLING_INTERVAL          5
#define SQL_FLAGS                       CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS
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
    MYSQL_ROW row;
    m_conn_prm_list_it prms;
    std::string error;
} m_query_t;

typedef std::map<uint64, m_query_t> m_query_list_t;
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
    uint64 m_query_nums;
    std::vector<MYSQL*> conns;
private:
    int wait_for_mysql(MYSQL *conn, int status, size_t timeout_ms = 0)
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
        std::vector<m_query_list_t::iterator> finished;
        for (pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
            for (auto it = m_queries[pri].begin(); it != m_queries[pri].end(); it++)
            {
                auto &q = it->second;
                if (q.finished)
                {
                    UTIL_ServerPrint("[DEBUG] run_query(): cleaner, total finished: %d\n", num_finished.load());
                    key = q.tid;
                    auto &t = m_threads[key];
                    if (t.joinable())
                        t.join();
                    num_finished--;
                    m_threads.erase(key);
                    finished.push_back(it);
                    UTIL_ServerPrint("[DEBUG] run_query(): cleaner, threads left = %d\n", m_threads.size());
                    continue;
                }
                if (!q.started && (m_threads.size() - num_finished.load()) < (max_threads << (MAX_QUERY_PRIORITY - q.pri)))
                {
                    UTIL_ServerPrint("[DEBUG] run_query(): pid = %d, threads_num = %d\n", gettid(), m_threads.size());
                    q.started = true;
                    q.time_start = g_RehldsData->GetTime();
                    std::thread t(&mysql_mngr::exec_async_query, this, pri, it);
                    m_threads.insert({ key = t.get_id(), std::move(t) });
                    q.tid = key;
                    UTIL_ServerPrint("[DEBUG] run_query(): thread starting pid = %d\n", key);
                }
            }
        for (auto it : finished)
            m_queries[it->second.pri].erase(it);
    }
    void exec_async_query(int pri, m_query_list_it query_list)
    {
        std::thread::id pid = gettid();
        auto &q = query_list->second;
        int err, status;
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, typle = %d, START\n", pid, q.conn_id);
        int failstate = TQUERY_SUCCESS;
        if (async_connect(q))
        {
            UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, start = %f, query = <%s>\n", pid, q.time_start, q.query.c_str());
            status = mysql_real_query_start(&err, q.conn, q.query.c_str(), q.query.size());
            while (status)
            {
                status = wait_for_mysql(q.conn, status, q.timeout);
                status = mysql_real_query_cont(&err, q.conn, status);
            }
            if (err)
                failstate = TQUERY_QUERY_FAILED;
            else
                q.result = mysql_use_result(q.conn);
            /*
            int max_row = 10;
            while (max_row--) {
                status = mysql_fetch_row_start(&q.row, q.result);
                while (status) {
                    status = wait_for_mysql(q.conn, status);
                    status = mysql_fetch_row_cont(&q.row, q.result, status);
                }
                if (!q.row)
                    break;
                UTIL_ServerPrint("[DEBUG] DUMP: %s: %s: %s: %s \n", q.row[0], q.row[1], q.row[2], q.row[3]);
            }
            */
        }
        else
            failstate = TQUERY_CONNECT_FAILED;
        q.time_end = g_RehldsData->GetTime();
        float queuetime = q.time_end - q.time_start;
        if (q.prms->fwd != -1)
        {
            UTIL_ServerPrint("[DEBUG] exec_async_query: EXEC AMXX FORWARD = %d, time = %f, error = %s\n", q.prms->fwd, queuetime, q.error.c_str());
            //public QueryHandler(failstate, Handle:query, error[], errnum, data[], size, Float:queuetime);
            g_amxxapi.ExecuteForward(q.prms->fwd, failstate, &q, g_amxxapi.PrepareCharArrayA(q.error.data(), q.error.size() + 1, false), err, g_amxxapi.PrepareCellArrayA(q.data, q.data_size, false), q.data_size, amx_ftoc(queuetime));
        }
        if (q.result != nullptr)
            mysql_free_result(q.result);
        if (q.conn != nullptr)
            mysql_close(q.conn);
        if (q.data != nullptr)
            delete q.data;
        num_finished++;
        q.finished = true;
        //UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, err = %d, end = %f, query_time = %f\n", pid, err, q.time_end, q.time_end - q.time_start);
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, END, num_finished = %d\n", pid, num_finished.load());
    }
    bool push_query(size_t conn_id, std::string query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY, size_t timeout_ms = 60000)
    {
        if (query.empty() || pri > MAX_QUERY_PRIORITY)
            return false;
        UTIL_ServerPrint("[DEBUG] push_query(): query = %s, callback = %s\n", query.c_str(), data);
        m_query_t m_query;
        m_query.conn = nullptr;
        m_query.result = nullptr;
        m_query.started = false;
        m_query.finished = false;
        m_query.id = m_query_nums++;
        m_query.pri = pri;
        m_query.conn_id = conn_id;
        m_query.query = query;
        m_query.data = new cell[data_size];
        m_query.data_size = data_size;
        m_query.timeout = timeout_ms;
        Q_memcpy(m_query.data, data, data_size << 2);
        m_query.time_create = g_RehldsData->GetTime();
        threads_mutex.lock();
        m_queries[pri].insert({ m_query.id, m_query });
        threads_mutex.unlock();
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
    bool async_connect(m_query_t &q)
    {
        MYSQL *ret;
        q.conn = mysql_init(NULL);
        if (!(q.conn = mysql_init(0)) || check_it_empty(q.prms = get_connect(q.conn_id)))
            return false;
        set_default_params(q.conn, q.prms);
        
        mysql_options(q.conn, MYSQL_OPT_NONBLOCK, 0);
        int status = mysql_real_connect_start(&ret, q.conn, q.prms->db_host.c_str(), q.prms->db_user.c_str(), q.prms->db_pass.c_str(), q.prms->db_name.c_str(), q.prms->db_port, NULL, SQL_FLAGS);
        UTIL_ServerPrint("[DEBUG] connect(): MYSQL_OPT_NONBLOCK, conn = %d, port = %d, status = %d\n", q.conn, q.prms->db_port, status);
        while (status)
        {
            status = wait_for_mysql(q.conn, status);
            status = mysql_real_connect_cont(&ret, q.conn, status);
        }
        UTIL_ServerPrint("[DEBUG] connect(): ret = %d\n", ret);
        if (!ret)
        {
            q.error = mysql_error(q.conn);
            UTIL_ServerPrint("[DEBUG] connect(): error = %s\n", q.error.c_str());
            return false;
        }
        return true;
    }
    size_t add_connect(int fwd, std::string &db_host, std::string &db_user, std::string &db_pass, std::string &db_name, std::string &db_chrs, size_t timeout_ms = 250)
    {
        UTIL_ServerPrint("[DEBUG] add_connect(): pid = %d, fwd = %d, host = <%s>, user = <%s>, pass = <%s>, name = <%s>, timeout = %d\n",
            gettid(), fwd, db_host, db_user, db_pass, db_name, timeout_ms
        );
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
        UTIL_ServerPrint("[DEBUG] add_connect(): conn = %d\n", m_conn_prms.size());
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
        UTIL_ServerPrint("[DEBUG] start(): pid = %d, START\n", gettid());
        main_thread = std::thread(&mysql_mngr::main, this);
    }
    mysql_mngr() : main_thread()
    {
        stop_threads = false;
        m_query_nums = 0;
        num_finished = 0;
    }
    ~mysql_mngr()
    {
        stop_threads = true;
        main_thread.join();
    }
};

extern mysql_mngr g_mysql_mngr;