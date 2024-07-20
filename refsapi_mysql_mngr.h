#include "precompiled.h"


#define MAX_QUERY_PRIORITY              3
#define MAX_QUERY_THREADS               10
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
    void* fwd;
    bool is_started;
    bool is_finished;
    double time_creation;
    double time_start;
    double time_end;
} m_query_t;

typedef std::list<m_query_t> m_query_list_t;

class mysql_mngr {
    const size_t max_query_size = 16384;
    m_conn_prm_list_t m_conn_prms;
    m_query_list_t m_queries[MAX_QUERY_PRIORITY + 1];
    int m_threads_num;
    std::mutex threads_mutex;

private:
    int wait_for_mysql(MYSQL *conn, int status)
    {
        int status = 0, timeout, res;
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
            status = MYSQL_WAIT_TIMEOUT;
        else if (res < 0)
            status = MYSQL_WAIT_EXCEPT;
        else {
            if (pfd.revents & POLLIN)
                status |= MYSQL_WAIT_READ;
            if (pfd.revents & POLLOUT)
                status |= MYSQL_WAIT_WRITE;
            if (pfd.revents & POLLPRI)
                status |= MYSQL_WAIT_EXCEPT;
        }
        return status;
    }

public:
    void run_query()
    {
        int count;
        for (auto &q_list : m_queries)
            for (auto &q : q_list)
            {
                threads_mutex.lock();
                count = MAX_QUERY_THREADS - m_threads_num;
                if (count > 0)
                {
                    std::thread(exec_async_query, &q);
                    m_threads_num++;
                }
                threads_mutex.unlock();
                if (--count <= 0)
                    break;
            }
    }
    void exec_async_query(m_query_t *q)
    {
        MYSQL* conn;
        int err, status;
        auto prms = get_connect(q->conn_id);
        if (!check_it_empty(prms) && (conn = connect(prms)) != nullptr)
        {
            q->is_started = true;
            q->time_start = g_RehldsData->GetTime();
            status = mysql_real_query_start(&err, conn, q->query.c_str(), q->query.size());
            while (status) {
                status = wait_for_mysql(conn, status);
                status = mysql_real_query_cont(&err, conn, status);
            }
            q->result = err ? nullptr : mysql_use_result(conn);
        }
        threads_mutex.lock();
        m_threads_num--;
        threads_mutex.unlock();
    }
    bool push_query(size_t conn_id, void *fwd, std::string &query, uint8 pri = MAX_QUERY_PRIORITY)
    {
        if (fwd == nullptr || query.empty() || pri > MAX_QUERY_PRIORITY)
            return false;
        m_query_t m_query;
        m_query.conn_id = conn_id;
        m_query.fwd = fwd;
        m_query.query = query;
        m_query.result = nullptr;
        m_query.time_creation = g_RehldsData->GetTime();
        m_queries[pri].push_back(m_query);
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
    size_t add_connect(MYSQL *conn, const char *db_user, char *db_pass, const char *db_name, const char *db_port = "3306", size_t timeout = 60, bool is_nonblocking = false)
    {
        m_conn_prm_t prms;
        prms.db_user = db_user;
        prms.db_pass = db_pass;
        prms.db_name = db_name;
        prms.db_port = atoi(db_port);
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
};