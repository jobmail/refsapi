#include "precompiled.h"


#define MAX_QUERY_PRIORITY              3
#define check_conn_rc(rc, mysql) \
do {\
if (rc)\
{\
    mysql_close(mysql);\
    return nullptr;\
}\
} while(0)

typedef struct m_query_s
{
    MYSQL* conn;
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
    m_query_list_t m_queries[MAX_QUERY_PRIORITY + 1];

private:


public:
    bool push_query(MYSQL *conn, void *fwd, std::string &query, uint8 pri = MAX_QUERY_PRIORITY)
    {
        if (conn == nullptr || fwd == nullptr || query.empty() || pri > MAX_QUERY_PRIORITY)
            return false;
        m_query_t m_query;
        m_query.conn = conn;
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
    MYSQL* connect(std::string DB_HOST, std::string DB_USER, std::string DB_PASS, std::string DB_NAME, std::string DB_PORT, size_t timeout)
    {
        MYSQL *conn = mysql_init(NULL);
        if (!(conn = mysql_init(0)))
            return nullptr;
        enum mysql_protocol_type prot_type = MYSQL_PROTOCOL_TCP;
        mysql_optionsv(conn, MYSQL_OPT_PROTOCOL, (void *)&prot_type);
        mysql_optionsv(conn, MYSQL_OPT_CONNECT_TIMEOUT, (void *)&timeout);
        if (mysql_real_connect(conn, DB_HOST.c_str(), DB_USER.c_str(), DB_PASS.c_str(), DB_NAME.c_str(), atoi(DB_PORT.c_str()), NULL, CLIENT_COMPRESS | CLIENT_MULTI_STATEMENTS) == NULL)
        {
            mysql_close(conn);
            return nullptr;
        }
        return conn;
    }
};