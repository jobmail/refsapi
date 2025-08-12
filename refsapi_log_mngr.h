#include "precompiled.h"

#ifndef WITHOUT_LOG

#define MAX_PRODUCTIVITY            (max_writer_threads << 1)

typedef struct file_s
{
    AMX *amx;
    std::mutex m;
    std::wfstream *s;
    std::wstring name;
    int log_level;
    uint8 prefix_mode;
    std::atomic<bool> stop_write;
} file_t;

typedef std::chrono::_V2::system_clock::time_point time_point_t;

typedef struct data_s
{
    AMX *amx;
    uint64 seq;
    time_point_t time;
    std::wstring str;
} data_t;

typedef struct plugin_prm_s
{
    AMX *amx;
    CPluginMngr::CPlugin *plugin;
    std::wstring plugin_name;
    std::wstring path;
    std::wstring name;
    int log_level;
    uint8 prefix_mode;
} plugin_prm_t;

typedef std::vector<data_t> buffer_t;
typedef std::vector<file_t *> files_t;
typedef std::map<AMX *, plugin_prm_t> plugin_prms_t;
typedef plugin_prms_t::iterator plugin_prms_it;
typedef std::map<AMX *, file_t *> plugins_t;
typedef plugins_t::iterator plugin_it;
typedef std::map<std::thread::id, std::thread *> m_thread_t;

class log_mngr
{
private:
    buffer_t buffer;
    std::thread main_thread;
    std::atomic<bool> stop_main;
    std::mutex buffer_mutex, data_mutex, thread_mutex, prms_mutex;
    m_thread_t m_threads;
    std::vector<std::thread::id> m_finished;    
    uint8 num_productivity;
    uint8 max_writer_threads;
    std::atomic<int> num_threads;
    std::atomic<int> num_finished;
    std::atomic<bool> stop_threads;
    size_t batch_size;
    std::atomic<size_t> write_interval;
    files_t files;
    plugins_t plugins;
    plugin_prms_t plugin_prms;
    std::wstring log_root;
    std::wstring log_game;
    std::wstring log_path;
    std::atomic<uint64> m_sequence;
public:
    void writer_thread(buffer_t *buff, const size_t local_batch_size)
    {
        buffer_t local_batch;
        AMX *amx_prev = nullptr;
        file_t *file;
        data_t it;
        DEBUG("%s(): START", __func__);
        local_batch.reserve(local_batch_size);
        std::sort(buff->begin(), buff->end(), [](const data_t &a, const data_t &b) {
            return a.amx >= b.amx && a.time > b.time; 
        });
        size_t skip_count = 0;
        while (buff->size())
        {
            if (amx_prev == nullptr || it.amx != amx_prev)
            {
                DEBUG("%s(): 1", __func__);
                it = buff->back();
                DEBUG("%s(): 2", __func__);
                file = open_file(it.amx);
                DEBUG("%s(): 3", __func__);
                if (file == nullptr)
                {
                    buff->pop_back();
                    skip_count++;
                    continue;
                }
                amx_prev = it.amx;
            }
            while (local_batch.size() < local_batch.capacity() && it.amx == amx_prev) {
                local_batch.push_back(std::move(it));
                buff->pop_back();
                if (!buff->size())
                    break;
                it = buff->back();
            }
            DEBUG("%s(): batch = %p, size = %d", __func__, local_batch, local_batch.size());
            write(file, local_batch);
            if (!buff->size() || it.amx != amx_prev)
                file->m.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds((uint64_t)std::round(50.0 * batch_size / local_batch_size)));
        }
        assert(skip_count == 0);
        DEBUG("%s(): DONE", __func__);
        delete buff;
        std::lock_guard lock(thread_mutex);
        m_finished.push_back(gettid());
        num_finished++;
    }
    void write(file_t *file, buffer_t &batch)
    {
        int64_t ms;
        DEBUG("%s(): START, batch_size = %d", __func__, batch.size());
        if (file && file->s && file->s->is_open())
        {
            for (auto &line : batch)
                switch (file->prefix_mode)
                {
                    case 1:
                        *file->s << L"L " << format_time(line.time) << L": " << cc(line.str) << L'\n';
                        break;
                    case 2:
                        ms = std::chrono::duration_cast<std::chrono::microseconds>(line.time.time_since_epoch()).count() % 1000000;
                        *file->s << L"L " << format_time(line.time) << L'.' << std::setfill(L'0') << std::setw(3) << ms / 1000 << L": " << cc(line.str) << L'\n';
                        break;
                    default:
                        *file->s << cc(line.str) << L'\n';
                }
            DEBUG("%s(): FLUSH", __func__);
            file->s->flush();
        }
        batch.clear();
    }
    int get_num_threads()
    {
        auto num = (num_threads - num_finished);
        assert(num >= 0);
        return num;
    }
    bool write_to_disk()
    {
        if (!buffer.empty())
        {
            DEBUG("%s(): START", __func__);
            DEBUG("%s(): buffer_size = %d, num_threads = %d, num_finished = %d, max_threads = %d, num_productivity = %d", __func__, buffer.size(), num_threads.load(), num_finished.load(), max_writer_threads, num_productivity);
            std::lock_guard lock(thread_mutex);
            if (get_num_threads() >= max_writer_threads)
            {
                if (num_productivity > 0)
                    num_productivity--;
                else
                    return false;
                DEBUG("%s(): UPDATE! batch_size = %u, new write_interval = %u ***", __func__, batch_size, write_interval.load());
            }
            else if (num_productivity < MAX_PRODUCTIVITY)
                num_productivity++;
            buffer_t* buff = new buffer_t{0};
            buff->swap(buffer);
            buffer.reserve(MAX_LOG_BUFFER_SIZE);
            DEBUG("%s(): local_buffer = %p, size = %d, str = %s", __func__, buff, buff->size(), wstos(buff->front().str).c_str());
            std::thread *t = new std::thread(&log_mngr::writer_thread, this, buff, batch_size << (MAX_PRODUCTIVITY - num_productivity));
            assert(t != nullptr);
            m_threads.emplace(t->get_id(), t);
            num_threads++;
            DEBUG("%s(): END", __func__);
        }
        return true;
    }
    void write_file(AMX *amx, char *str)
    {
        std::wstring s = stows(str);
        write_file(amx, s);
    }
    void write_file(AMX *amx, std::wstring &str)
    {
        DEBUG("%s(): START", __func__);
        plugin_prm_t prm;
        if (amx == nullptr || !get_config(amx, prm))
            return;
        std::lock_guard lock(buffer_mutex);
        if (buffer.size() >= MAX_LOG_BUFFER_SIZE)
            if (!write_to_disk())
            {
                AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): plugin = %s, exceeding productivity!", __func__, findPluginFast(amx)->getName());
                return;
            }
        data_t data;
        data.amx = amx;
        data.seq = m_sequence++;
        data.time = std::chrono::system_clock::now();
        data.str.swap(str);
        buffer.push_back(data);
        DEBUG("%s(): push to buffer: amx = %p, plugin = %p, str = %s", __func__, amx, findPluginFast(amx), wstos(data.str).c_str());
    }
    int get_log_level(AMX *amx)
    {
        DEBUG("%s(): START", __func__);
        plugin_prm_t prm;
        return get_config(amx, prm) ? prm.log_level : -1;
    }
    bool set_log_level(AMX *amx, int log_level)
    {
        DEBUG("%s(): START", __func__);
        std::lock_guard lock(prms_mutex);
        auto prm = get_config(amx);
        if (prm == nullptr)
            return false;
        prm->log_level = log_level;
        return true;
    }
    bool add_config(AMX *amx, std::wstring path = L"", std::wstring name = L"", int log_level = -1, uint8 prefix_mode = 2)
    {
        if (amx == nullptr)
            return false;
        auto plugin = get_plugin(amx);
        if (plugin == nullptr)
            return false;
        path = log_root + L'/' + log_game + L'/' + (path.empty() ? log_path : path);
        remove_chars(path, _BAD_PATH_CHARS_L);
        rtrim(path, L" /");
        if (!dir_exists(path) && std::filesystem::create_directories(path))
            DEBUG("%s(): plugin = %s, created path = %s", __func__, plugin->getName(), wstos(path).c_str());
        else
            DEBUG("%s(): path = %s", __func__, wstos(path).c_str());
        if (name.empty())
        {
            name = stows(plugin->getName());
            DEBUG("%s(): plugin = %s", __func__, wstos(name).c_str());
            auto pos = name.find(L".amxx");
            if (pos != std::wstring::npos)
                name.replace(pos, sizeof(L".amxx") - 1, L"");
            name += L".log";
        }
        DEBUG("%s(): name = %s", __func__, wstos(name).c_str());
        remove_chars(name, _BAD_PATH_CHARS_L);
        plugin_prm_t prm;
        prm.amx = amx;
        prm.plugin = plugin;
        prm.plugin_name = stows(plugin->getName());
        prm.path = path;
        prm.name = name;
        prm.log_level = log_level;
        prm.prefix_mode = prefix_mode;
        std::lock_guard lock(prms_mutex);
        plugin_prms.insert({ amx, prm });
        return true;
    }
    // NEED TO USE ===> std::lock_guard lock(prms_mutex);
    plugin_prm_t* get_config(AMX *amx)
    {
        plugin_prms_it it;
        return (amx == nullptr || !plugin_prms.size() || (it = plugin_prms.find(amx)) == plugin_prms.end()) ? nullptr : &it->second;
    }
    bool get_config(AMX *amx, plugin_prm_t &prm, std::wstring path = L"", std::wstring name = L"", int log_level = -1, uint8 prefix_mode = 2)
    {
        DEBUG("%s(): START", __func__);
        plugin_prms_it it;
        if (amx == nullptr)
            return false;
        // Copy PARAMS
        prms_mutex.lock();
        if (!plugin_prms.size() || (it = plugin_prms.find(amx)) == plugin_prms.end())
        {
            prms_mutex.unlock();
            return add_config(amx, path, name, log_level, prefix_mode);
        }
        prm = it->second;
        prms_mutex.unlock();
        return true;
    }
    file_t* create_file(AMX *amx, std::wstring path = L"", std::wstring name = L"", std::ios::openmode flags = std::ios::openmode(0), int log_level = -1, uint8 prefix_mode = 2)
    {
        DEBUG("%s(): START, amx = %p", __func__, amx);
        file_t *file = nullptr;
        CPluginMngr::CPlugin *plugin;
        if (amx == nullptr || (plugin = get_plugin(amx)) == nullptr || path.empty() || name.empty() || flags == std::ios::openmode(0))
            return nullptr;
        DEBUG("%s(): START, plugin = %p", __func__, plugin);
        // Create FILE
        std::lock_guard lock(data_mutex);
        file = new file_t{0};
        file->m.lock();
        file->name = path + L'/' + name;
        file->log_level = log_level;
        file->prefix_mode = prefix_mode;
        DEBUG("%s(): plugin = %p, filename(%x) = %s", __func__, plugin, flags, wstos(file->name).c_str());
        try
        {
            file->s = new std::wfstream(wstos(file->name).c_str(), flags);
        }
        catch (...)
        {
            delete file;
            file = nullptr;
        }
        if (file == nullptr || file->s == nullptr || !file->s->is_open())
        {
            AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): plugin = %p, can't open file %s", __func__, plugin, wstos(file->name).c_str());
            return nullptr;
        }
        file->s->imbue(_LOCALE);
        files.push_back(file);
        plugins.insert({ amx, file });
        DEBUG("%s(): DONE! is_open = %d", __func__, file->s->is_open());
        return file;
    }
    file_t* open_file(AMX *amx, std::wstring path = L"", std::wstring name = L"", int log_level = -1, uint8 prefix_mode = 2)
    {
        DEBUG("%s(): START, amx = %p", __func__, amx);
        file_t *file = nullptr;
        if (amx == nullptr)
            return nullptr;
        auto plugin = get_plugin(amx);
        DEBUG("%s(): START, plugin = %p", __func__, plugin);
        if ((file = get_file(amx)) == nullptr)
        {
            /*
            if (plugin == nullptr)
                return nullptr;
            */
            plugin_prm_t prm;
            if (!get_config(amx, prm, path, name, log_level, prefix_mode))
                return nullptr;
            auto filename = prm.path + L'/' + prm.name;
            auto flags = file_exists(filename) ? std::ios::in | std::ios::out | std::ios::binary | std::ios::app : std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc;
            DEBUG("%s(): create file(%x) = %s", __func__, flags, wstos(filename).c_str());
            file = create_file(amx, prm.path, prm.name, flags, prm.log_level, prm.prefix_mode);
            DEBUG("%s(): plugin = %p, file = %s, is_exist = %d, is_open = %d", __func__, plugin, wstos(file->name).c_str(), file_exists(file->name), file->s->is_open());
        } else
            file->m.lock();
        return file;
    }
    void close(file_t *file)
    {
        DEBUG("%s(): START, file = %p", __func__, file);
        if (file == nullptr)
            return;
        if (file->s)
        {
            if (file->s->is_open())
                file->s->close();
            delete file->s;
            file->s = nullptr;
        }
        delete file;
    }
    void close_file(AMX *amx)
    {
        DEBUG("%s(): START", __func__);
        file_t *file;
        if (amx == nullptr || (file = get_file(amx)) == nullptr)
            return;
        close_file(amx, file);
    }
    void close_file(AMX *amx, file_t *file)
    {
        DEBUG("%s(): START", __func__);
        DEBUG("%s(): amx = %p, plugin = %p, file = %s", __func__, amx, findPluginFast(amx), wstos(file->name).c_str());
        file->m.lock();
        std::lock_guard lock(data_mutex);
        auto it = std::find(files.begin(), files.end(), file);
        if (it != files.end())
        {
            files.erase(it);
            files.shrink_to_fit();
        }
        plugins.erase(amx);
        if (!plugins.size())
            plugins_t().swap(plugins);
        close(file);
    }
    void close_all()
    {
        DEBUG("%s(): START", __func__);
        std::lock_guard lock_1(buffer_mutex);

        write_to_disk();
        // Wait for writing...
        while (get_num_threads() > 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        std::lock_guard lock_2(data_mutex);
        for (auto p : plugins)
        {
            plugin_prm_t prm;
            auto amx = p.first;
            auto file = p.second;
            DEBUG("%s(): amx = %p, file = %p", __func__, amx, file);
            if (get_config(amx, prm))
            {
                DEBUG("%s(): plugin = %p, filename = %s", __func__, prm.plugin,  wstos(prm.plugin_name).c_str(), wstos(prm.path + L'/' + prm.name).c_str());
                file->m.lock();
                close(file);
            }
            else
                assert(false);
        }
        files.clear();
        files.shrink_to_fit();
        plugins.clear();
        plugins_t().swap(plugins);
        std::lock_guard lock_3(prms_mutex);
        plugin_prms.clear();
        plugin_prms_t().swap(plugin_prms);
        DEBUG("%s(): END", __func__);
    }
    file_t* get_file(AMX *amx)
    {
        DEBUG("%s(): START", __func__);
        file_t *file = nullptr;
        data_mutex.lock();
        auto it = plugins.find(amx);
        if (it != plugins.end())
        {
            file = it->second;
            assert(file != nullptr);
            if (file->s && file->s->is_open() && !file_exists(file->name)) {
                data_mutex.unlock();
                close_file(amx, file);
                file = nullptr;
                DEBUG("%s(): REOPEN", __func__);
            }
        }
        data_mutex.unlock();
        DEBUG("%s(): END, file = %p", __func__, file);
        return file;
    }
    void start_main()
    {
        log_root = std::filesystem::current_path().wstring();
        trim(log_root);
        log_path = stows(LOCALINFO("amxx_logs"));
        if (log_path.empty())
            log_path = L"addons/amxmodx/logs";
        trim(log_path);
        rtrim(log_path, L" /");
        DEBUG("%s(): dir = %s, g_amxxapi = %p", __func__, wstos(log_path).c_str(), g_amxxapi);
        log_game = stows(g_amxxapi.GetModname());
        if (log_game.empty())
            log_game = L"cstrike";
        DEBUG("%s() log_root = %s, game = %s, log_path = %s", __func__, wstos(log_root).c_str(), wstos(log_game).c_str(), wstos(log_path).c_str());
        stop_main = false;
        main_thread = std::thread(&log_mngr::main, this);
    }
    void main()
    {   
        DEBUG("%s(): pid = %p, START", __func__, gettid());
        while (!stop_main)
        {
            buffer_mutex.lock();
            write_to_disk();
            buffer_mutex.unlock();
            auto ms = clamp(write_interval >> (MAX_PRODUCTIVITY - num_productivity), MAX_LOG_MIN_INTERVAL, MAX_LOG_MAX_INTERVAL);
            std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            // Wait threads
            std::lock_guard lock(thread_mutex);
            if (m_finished.size())
            {
                for (auto key : m_finished)
                {
                    auto it = m_threads.find(key);
                    if (it != m_threads.end())
                    {
                        auto t = it->second;
                        assert(t != nullptr);
                        if (t->joinable())
                            t->join();
                        delete t;
                        m_threads.erase(it);
                        num_threads--;
                        num_finished--;
                    }
                }
                m_finished.clear();
                m_finished.shrink_to_fit();
                DEBUG("%s(): cleaner m_threads = %d, num_threads = %d, num_finished = %d", __func__, m_threads.size(), num_threads.load(), num_finished.load());
            }
            if (!m_threads.size())
                m_thread_t().swap(m_threads);
        }
        DEBUG("%s(): pid = %p, END", __func__, gettid());
    }
    log_mngr(const size_t interval = 5000, const size_t batch = 100, const uint8 threads = 0)
    {
        num_threads =
            num_finished = 0;
        m_sequence = 0;
        write_interval = clamp(interval, MAX_LOG_MIN_INTERVAL, MAX_LOG_MAX_INTERVAL);
        batch_size = batch;
        max_writer_threads = threads ? threads : clamp(std::thread::hardware_concurrency() << 1, 1U, 4U);
        num_productivity = MAX_PRODUCTIVITY;
        buffer.reserve(MAX_LOG_BUFFER_SIZE);
    }
    ~log_mngr()
    {
        stop_main = true;
        if (main_thread.joinable())
            main_thread.join();
    }
};

extern log_mngr g_log_mngr;
#endif