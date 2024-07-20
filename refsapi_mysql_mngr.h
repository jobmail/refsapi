#include "precompiled.h"


#define MAX_QUERY_PRIORITY              3
#define MAX_QUERY_THREADS               10
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
    size_t conn_id;
    MYSQL_RES* result;
    std::string query;
    cell* data;
    size_t data_size;
    bool is_started;
    bool is_finished;
    double time_creation;
    double time_start;
    double time_end;
} m_query_t;

typedef std::list<m_query_t> m_query_list_t;
typedef m_query_list_t::iterator m_query_list_it;

class mysql_mngr {
    const size_t max_query_size = 16384;
    m_conn_prm_list_t m_conn_prms;
    m_query_list_t m_queries[MAX_QUERY_PRIORITY + 1];
    int m_threads_num;
    std::mutex threads_mutex;
private:
    static int wait_for_mysql(MYSQL *conn, int status)
    {
        int timeout, res;
        struct pollfd pfd;
        pfd.fd = mysql_get_socket(conn);
        pfd.events =
            (status & MYSQL_WAIT_READ ? POLLIN : 0) |
            (status & MYSQL_WAIT_WRITE ? POLLOUT : 0) |
            (status & MYSQL_WAIT_EXCEPT ? POLLPRI : 0);
        if (status & MYSQL_WAIT_TIMEOUT)
            timeout = 1000 * mysql_get_timeout_value(conn);
        else
            timeout = -1;
        res = poll(&pfd, 1, timeout);
        if (res == 0)
            return MYSQL_WAIT_TIMEOUT;
        else if (res < 0)
            return MYSQL_WAIT_EXCEPT;
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
    static void main(m_query_list_t* query, std::mutex* m, int* num)
    {
        UTIL_ServerPrint("[DEBUG] main(): pid = %s, max_thread = %d, interval = %d, START", getpid(), MAX_QUERY_THREADS, QUERY_POOLING_INTERVAL);
        while (true)
        {
            run_query(query, m, num);
            usleep(QUERY_POOLING_INTERVAL * 1000);
        }
    }
    static void run_query(m_query_list_t* query, std::mutex* m, int* num)
    {
        int pri, count;
        for (pri = 0; pri < MAX_QUERY_PRIORITY + 1; pri++)
            for (auto q = query[pri].begin(); q != query[pri].end(); q++)
            {
                m->lock();
                count = MAX_QUERY_THREADS - *num;
                if (count > 0)
                {
                    std::thread t1(exec_async_query, pri, q, query, m, num);
                    //exec_async_query(pri, q);
                    (*num)++;
                }
                m->unlock();
                if (--count <= 0)
                    break;
            }
    }
    static void exec_async_query(int pri, m_query_list_it q, m_query_list_t* query, std::mutex* m, int* num)
    {
        MYSQL* conn = nullptr;
        q->result = nullptr;
        int pid = getpid();
        int err, status;
        try
        {
            UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, START", pid);
            m_conn_prm_list_it prms;
            //auto prms = get_connect(q->conn_id);
            if (!check_it_empty(prms))
            {
                q->is_started = true;
                q->time_start = g_RehldsData->GetTime();
                UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %s, start = %f, query = <%s>", pid, q->time_start, q->query.c_str());
                if ((conn = connect(prms)) != nullptr)
                {
                    status = mysql_real_query_start(&err, conn, q->query.c_str(), q->query.size());
                    while (status) {
                        status = wait_for_mysql(conn, status);
                        status = mysql_real_query_cont(&err, conn, status);
                    }
                    q->result = err ? nullptr : mysql_use_result(conn);
                }
                q->is_finished = true;
                q->time_end = g_RehldsData->GetTime();
                UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, end = %f, err = %d, query_time = %f", pid, q->time_end, err, q->time_end - q->time_start);
                //g_amxxapi.ExecuteForward(prms->fwd);
            }
        }
        catch (const char* error_message)
        {
            UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, error = <%s>", pid, error_message);
        }
        if (q->result != nullptr)
            mysql_free_result(q->result);
        if (conn != nullptr)
            mysql_close(conn);
        delete q->data;
        m->lock();
        query[pri].erase(q);
        (*num)--;
        m->unlock();
        UTIL_ServerPrint("[DEBUG] exec_async_query(): pid = %d, END", pid);
    }
    bool push_query(size_t conn_id, std::string query, cell *data, size_t data_size, uint8 pri = MAX_QUERY_PRIORITY)
    {
        if (query.empty() || pri > MAX_QUERY_PRIORITY)
            return false;
        m_query_t m_query;
        m_query.conn_id = conn_id;
        m_query.query = query;
        m_query.result = nullptr;
        m_query.data = new cell[data_size];
        m_query.data_size = data_size;
        Q_memcpy(m_query.data, data, data_size << 2);
        m_query.time_creation = g_RehldsData->GetTime();
        threads_mutex.lock();
        m_queries[pri].push_back(m_query);
        threads_mutex.unlock();
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
    static MYSQL* connect(m_conn_prm_list_it prms)
    {
        MYSQL *ret, *conn = mysql_init(NULL);
        if (!(conn = mysql_init(0)))
            return nullptr;
        enum mysql_protocol_type prot_type = MYSQL_PROTOCOL_TCP;
        mysql_optionsv(conn, MYSQL_OPT_PROTOCOL, (void *)&prot_type);
        mysql_optionsv(conn, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&prms->timeout);
        if (prms->is_nonblocking)
        {
            int status;
            mysql_options(conn, MYSQL_OPT_NONBLOCK, 0);
            status = mysql_real_connect_start(&ret, conn, prms->db_host.c_str(), prms->db_user.c_str(), prms->db_pass.c_str(), prms->db_name.c_str(), prms->db_port, NULL, SQL_FLAGS);
            while (status) {
                status = wait_for_mysql(conn, status);
                status = mysql_real_connect_cont(&ret, conn, status);
            }
        } else
            ret = mysql_real_connect(conn,
                prms->db_host.c_str(), prms->db_user.c_str(), prms->db_pass.c_str(), prms->db_name.c_str(), prms->db_port, NULL, SQL_FLAGS) ? conn : nullptr;
        if (ret == nullptr)
        {
            mysql_close(conn);
            return nullptr;
        }
        return conn;
    }
    size_t add_connect(int fwd, const char *db_host, const char *db_user, const char *db_pass, const char *db_name, size_t timeout = 60, bool is_nonblocking = false)
    {
        UTIL_ServerPrint(
            "[DEBUG] add_connect(): pid = %d, fwd = %d, host = <%s>, user = <%s>, pass = <%s>, name = <%s>, timeout = %d, block = %d",
            getpid(), fwd, db_host, db_user, db_pass, db_name, timeout, is_nonblocking
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
        return m_conn_prms.size() - 1;
    }
    m_conn_prm_list_it get_connect(size_t conn_id)
    {
        if (conn_id > m_conn_prms.size() - 1)
            return m_conn_prm_list_it();
        return std::next(m_conn_prms.begin(), conn_id);
    }
    mysql_mngr()
    {
        UTIL_ServerPrint("[DEBUG] mysql_mngr(): pid = %d, CREATED", getpid());
        m_threads_num = 0;
        //std::thread t1(main, (m_query_list_t *)&m_queries, (std::mutex *)&threads_mutex, (int *)&m_threads_num);
    }
};

extern mysql_mngr g_mysql_mngr;