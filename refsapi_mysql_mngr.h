#include "precompiled.h"

#define MAX_QUERY_PRIORITY              7
#define MAX_QUERY_THREADS               1
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

typedef struct m_conn_prm_s
{
    int fwd;
    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_name;
    size_t db_port;         // 3306
    size_t timeout;         // 60
    bool is_nonblocking;    // false
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
    bool started;
    bool finished;
    double time_create;
    double time_start;
    double time_end;
    MYSQL_RES* result;
} m_query_t;

typedef std::map<uint64, m_query_t> m_query_list_t;
typedef m_query_list_t::iterator m_query_list_it;

class mysql_mngr {
    const size_t max_query_size = 16384;
    m_conn_prm_list_t m_conn_prms;
    m_query_list_t m_queries[MAX_QUERY_PRIORITY + 1];
    std::thread main_thread;
    std::map<std::thread::id, std::thread> m_threads;
    std::atomic<int> num_finished;
    bool stop_threads;
    std::mutex threads_mutex;
    uint64 m_query_nums;
private:
    int wait_for_mysql(MYSQL *conn, int status)
    {
        int timeout, res;
        struct pollfd pfd;
        pfd.fd = mysql_get_socket(conn);
        pfd.events =
            (status & MYSQL_WAIT_READ ? POLLIN : 0) |
            (status & MYSQL_WAIT_WRITE ? POLLOUT : 0) |
            (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);
        UTIL_ServerPrint("[DEBUG] wait_for_mysql(): socket = %d, events = %d\n", pfd.fd, pfd.events);        
        if (status & MYSQL_WAIT_TIMEOUT)
            timeout = 1000 * mysql_get_timeout_value(conn);
        else
            timeout = -1;
        UTIL_ServerPrint("[DEBUG] wait_for_mysql(): timeout = %d\n", timeout);
        res = poll(&pfd, 1, timeout);
        UTIL_ServerPrint("[DEBUG] wait_for_mysql(): res = %d\n", res);
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

public:
    void main()
    {
        UTIL_ServerPrint("[DEBUG] main(): pid = %d, max_thread = %d, interval = %d, START\n", gettid(), MAX_QUERY_THREADS, QUERY_POOLING_INTERVAL);
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
            for (auto q = m_queries[pri].begin(); q != m_queries[pri].end(); q++)
            {
                /*
                if (q->second.finished)
                {
                    //UTIL_ServerPrint("[DEBUG] run_query(): cleaner, total finished: %d\n", num_finished);
                    key = q->second.tid;
                    auto &t = m_threads[key];
                    if (t.joinable())
                        t.join();
                    num_finished--;
                    m_threads.erase(key);
                    finished.push_back(q);
                    UTIL_ServerPrint("[DEBUG] run_query(): cleaner, threads left = %d\n", m_threads.size());
                }
                else
                */
                if (!q->second.started && (m_threads.size() - num_finished) < MAX_QUERY_THREADS)
                {
                    UTIL_ServerPrint("[DEBUG] run_query(): pid = %d, threads_num = %d\n", gettid(), m_threads.size());
                    q->second.started = true;
                    q->second.time_start = g_RehldsData->GetTime();
                    std::thread t(&mysql_mngr::exec_async_query, this, pri, q);
                    m_threads.insert({ key = t.get_id(), std::move(t) });
                    q->second.tid = key;
                    UTIL_ServerPrint("[DEBUG] run_query(): thread starting pid = %d\n", key);
                }
            }
        for (auto q : finished)
            m_queries[q->second.pri].erase(q);
    }
    void exec_async_query(int pri, m_query_list_it ql)
    {
        std::thread::id pid = gettid();
        auto &q = ql->second;
        MYSQL_ROW row;
        //usleep(1000 * 1000);
        MYSQL* conn = nullptr;
        q.result = nullptr;
        int err, status;
        //try
        //{
            UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, typle = %d, START\n", pid, q.conn_id);
            //m_conn_prm_list_it prms;
            auto prms = get_connect(q.conn_id);
            if (!check_it_empty(prms))
            {
                conn = connect(prms);
                UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, start = %f, query = <%s>\n", pid, q.time_start, q.query.c_str());
                if (conn != nullptr)
                {
                    status = mysql_real_query_start(&err, conn, q.query.c_str(), q.query.size());
                    while (status) {
                        status = wait_for_mysql(conn, status);
                        status = mysql_real_query_cont(&err, conn, status);
                    }
                    q.result = err ? nullptr : mysql_use_result(conn);

                    UTIL_ServerPrint("[DEBUG] RESULT = %d, err = %d, error = %s\n", q.result, err, mysql_error(conn)); ////////////////////////

                    int max_row = 10;

                    for (;;) {
                        status = mysql_fetch_row_start(&row, q.result);
                        while (status) {
                            status = wait_for_mysql(conn, status);
                            status = mysql_fetch_row_cont(&row, q.result, status);
                        }
                        if (!row || max_row-- <= 0)
                            break;
                        UTIL_ServerPrint("[DEBUG] DUMP: %s: %s: %s: %s \n", row[0], row[1], row[2], row[3]);
                        //printf("%s: %s\n", row[0], row[1]);
                    }
                }
                //g_amxxapi.ExecuteForward(prms->fwd);
            }
        //}
        //catch (const char* error_message)
        //{
        //    UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, error = <%s>\n", pid, error_message);
        //}
        if (q.result != nullptr)
            mysql_free_result(q.result);
        if (conn != nullptr)
            mysql_close(conn);
        delete q.data;
        q.finished = true;
        q.time_end = g_RehldsData->GetTime();
        num_finished++;
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, err = %d, end = %f, query_time = %f\n", pid, err, q.time_end, q.time_end - q.time_start);
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, END, num_finished = %d\n", pid, num_finished.load());
    }
    bool push_query(size_t conn_id, std::string query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY)
    {
        if (query.empty() || pri > MAX_QUERY_PRIORITY)
            return false;
        UTIL_ServerPrint("[DEBUG] push_query(): query = %s\n", query.c_str());
        m_query_t m_query;
        m_query.started = false;
        m_query.finished = false;
        m_query.id = m_query_nums++;
        m_query.pri = pri;
        m_query.conn_id = conn_id;
        m_query.query = query;
        m_query.result = nullptr;
        m_query.data = new cell[data_size];
        m_query.data_size = data_size;
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
    MYSQL* connect(m_conn_prm_list_it prms)
    {
        MYSQL *ret, *conn = mysql_init(NULL);
        if (!(conn = mysql_init(0)))
            return nullptr;
        enum mysql_protocol_type prot_type = MYSQL_PROTOCOL_TCP;
        mysql_optionsv(conn, MYSQL_OPT_PROTOCOL, (void *)&prot_type);
        mysql_optionsv(conn, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&prms->timeout);
        //my_bool enforce_tls = 0;
        //mysql_optionsv(conn, MYSQL_OPT_SSL_ENFORCE, (void *)&enforce_tls);
        //conn->options.use_ssl = FALSE;
        //conn->options.secure_auth = FALSE;
        if (prms->is_nonblocking)
        {
            int status;
            mysql_options(conn, MYSQL_OPT_NONBLOCK, 0);
            //try {
                status = mysql_real_connect_start(&ret, conn, prms->db_host.c_str(), prms->db_user.c_str(), prms->db_pass.c_str(), prms->db_name.c_str(), prms->db_port, NULL, 0); //SQL_FLAGS
            //}
            //catch(...)
            //{
            //    UTIL_ServerPrint("[DEBUG] connect(): ERROR: %s\n", mysql_error(conn));
            //};
            UTIL_ServerPrint("[DEBUG] connect(): MYSQL_OPT_NONBLOCK, conn = %d, port = %d, use_ssl = %d, status = %d\n", conn, prms->db_port, conn->options.use_ssl, status);
            while (status) {
                status = wait_for_mysql(conn, status);
                status = mysql_real_connect_cont(&ret, conn, status);
                usleep(1000 * 1000);
            }
            UTIL_ServerPrint("[DEBUG] connect(): RET = %d\n", ret);
        } else
            ret = mysql_real_connect(conn,
                prms->db_host.c_str(), prms->db_user.c_str(), prms->db_pass.c_str(), prms->db_name.c_str(), prms->db_port, NULL, SQL_FLAGS) ? conn : nullptr;
        if (!ret) // 
        {
            mysql_close(conn);
            return nullptr;
        }
        return conn;
    }
    size_t add_connect(int fwd, const char *db_host, const char *db_user, const char *db_pass, const char *db_name, size_t timeout = 60, bool is_nonblocking = false)
    {
        UTIL_ServerPrint(
            "[DEBUG] add_connect(): pid = %d, fwd = %d, host = <%s>, user = <%s>, pass = <%s>, name = <%s>, timeout = %d, nonblock = %d\n",
            gettid(), fwd, db_host, db_user, db_pass, db_name, timeout, is_nonblocking
        );
        m_conn_prm_t prms;
        prms.fwd = fwd;
        prms.db_host = db_host;     //std::strtok((char*)db_host, ":");
        prms.db_port = 3306;        //atoi(std::strtok(nullptr, ":"));
        prms.db_user = db_user;
        prms.db_pass = db_pass;
        prms.db_name = db_name;
        prms.timeout = timeout;
        prms.is_nonblocking = is_nonblocking;
        m_conn_prms.push_back(prms);
        UTIL_ServerPrint("[DEBUG] add_connect(): conn = %d\n", m_conn_prms.size() - 1);
        return m_conn_prms.size() - 1;
    }
    m_conn_prm_list_it get_connect(size_t conn_id)
    {
        if (conn_id > m_conn_prms.size() - 1)
            return m_conn_prm_list_it();
        return std::next(m_conn_prms.begin(), conn_id);
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