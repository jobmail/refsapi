#include "precompiled.h"

bool is_number(std::string &s)
{
    trim(s);
    if (s.empty())
        return false;
    auto it = s.begin();
    if (*it == '+' || *it == '-')
        if (++it == s.end())
            return false;
    bool dp_found = false;
    bool dp_not_last = false;
    while (it != s.end())
    {
        if (!std::isdigit(*it))
        {
            // Check for decimal point and need to have one more digit after
            if (!dp_found && *it == DECIMAL_POINT && (it + 1) != s.end())
                dp_found = true;
            else
                break;
        }
        it++;
    }
    // UTIL_ServerPrint("[DEBUG] is_number(): s = <%s>, result = %d\n", s.c_str(), it == s.end());
    return it == s.end();
}

size_t set_amx_string(cell *dest, const char *str, size_t max_size)
{
    size_t count = 0;
    if (dest && max_size)
    {
        while (str && *str && ++count < max_size)
            *dest++ = (cell)*str++;
        *dest = 0;
    }
    return count;
}

bool is_valid_utf8(const std::vector<uint8_t> &data)
{
    int remaining_bytes = 0;
    uint32_t code_point = 0;

    for (size_t i = 0; i < data.size(); ++i)
    {
        uint8_t byte = data[i];

        if (remaining_bytes == 0)
        {
            // Определяем длину символа по первому байту
            if ((byte >> 7) == 0x00)
            { // 0xxxxxxx (ASCII)
                continue;
            }
            else if ((byte >> 5) == 0x06)
            { // 110xxxxx (2 байта)
                remaining_bytes = 1;
                code_point = byte & 0x1F;
                // Минимальное значение для 2-байтовой последовательности: U+0080
                if (code_point < 0x02)
                    return false;
            }
            else if ((byte >> 4) == 0x0E)
            { // 1110xxxx (3 байта)
                remaining_bytes = 2;
                code_point = byte & 0x0F;
                // Минимальное значение для 3-байтовой последовательности: U+0800
                if (code_point == 0 && (data[i + 1] & 0x20) == 0)
                    return false;
            }
            else if ((byte >> 3) == 0x1E)
            { // 11110xxx (4 байта)
                remaining_bytes = 3;
                code_point = byte & 0x07;
                // Минимальное значение для 4-байтовой последовательности: U+10000
                if (code_point == 0 && (data[i + 1] & 0x30) == 0)
                    return false;
            }
            else
            {
                return false; // Недопустимый первый байт
            }
        }
        else
        {
            // Проверяем continuation byte (10xxxxxx)
            if ((byte >> 6) != 0x02)
                return false;

            code_point = (code_point << 6) | (byte & 0x3F);
            remaining_bytes--;

            // Проверяем завершенные символы на валидность диапазона
            if (remaining_bytes == 0)
            {
                // Проверка на surrogate pairs (U+D800..U+DFFF)
                if (code_point >= 0xD800 && code_point <= 0xDFFF)
                {
                    return false;
                }
                // Проверка на максимальное значение Unicode (0x10FFFF)
                if (code_point > 0x10FFFF)
                {
                    return false;
                }
                // Проверка на overlong-encoding для ASCII (0xxxxxxx)
                if (code_point <= 0x7F)
                {
                    return false;
                }
            }
        }
    }

    return remaining_bytes == 0;
}

bool is_valid_utf8(const char *str)
{
    if (!str)
        return false;
    std::vector<uint8_t> data(str, str + strlen(str));
    return is_valid_utf8(data);
}

bool is_valid_utf8(const std::string &str)
{
    return is_valid_utf8(
        reinterpret_cast<const uint8_t *>(str.data()),
        str.size());
}

bool is_valid_utf8(const uint8_t *data, size_t size)
{
    if (!data && size != 0)
        return false;
    std::vector<uint8_t> vec(data, data + size);
    return is_valid_utf8(vec);
}

bool is_valid_utf8(char *str, size_t length)
{
    if (!str && length != 0)
        return false;
    return is_valid_utf8(
        reinterpret_cast<const uint8_t *>(str),
        length);
}

bool is_valid_utf8(const char *str, size_t length)
{
    if (!str && length != 0)
        return false;
    return is_valid_utf8(
        reinterpret_cast<const uint8_t *>(str),
        length);
}

#ifdef __SSE4_2__
bool is_valid_utf8_simd(const std::vector<uint8_t> &data)
{
    size_t i = 0;
    const size_t len = data.size();
    const uint8_t *ptr = data.data();

    // Маски для проверки первого байта
    const __m128i first_byte_mask = _mm_set1_epi8(0xC0);
    const __m128i first_byte_val = _mm_set1_epi8(0x80);

    while (i + 16 <= len)
    {
        // Загружаем 16 байт
        __m128i chunk = _mm_loadu_si128((const __m128i *)(ptr + i));

        // Проверяем на ASCII символы (быстрый путь)
        if (_mm_movemask_epi8(_mm_cmplt_epi8(chunk, _mm_set1_epi8(0x80))))
        {
            i += 16;
            continue;
        }

        // Проверяем первый байт последовательности
        __m128i is_continuation = _mm_cmpeq_epi8(_mm_and_si128(chunk, first_byte_mask), first_byte_val);
        int continuation_mask = _mm_movemask_epi8(is_continuation);

        if (continuation_mask)
        {
            // Нашли continuation byte в позиции первого байта - ошибка
            return false;
        }

        // Определяем длину последовательностей
        __m128i len1 = _mm_and_si128(_mm_srli_epi16(chunk, 4), _mm_set1_epi8(0x0F));
        __m128i is_len2 = _mm_cmpeq_epi8(len1, _mm_set1_epi8(0x0C));
        __m128i is_len3 = _mm_cmpeq_epi8(len1, _mm_set1_epi8(0x0E));
        __m128i is_len4 = _mm_cmpeq_epi8(_mm_and_si128(chunk, _mm_set1_epi8(0xF8)), _mm_set1_epi8(0xF0));

        int len2_mask = _mm_movemask_epi8(is_len2);
        int len3_mask = _mm_movemask_epi8(is_len3);
        int len4_mask = _mm_movemask_epi8(is_len4);

        // Обрабатываем последовательности разной длины
        for (int j = 0; j < 16; j++)
        {
            if (len2_mask & (1 << j))
            {
                if (i + j + 1 >= len)
                    return false;
                if ((ptr[i + j + 1] >> 6) != 0x02)
                    return false;
                j += 1;
            }
            else if (len3_mask & (1 << j))
            {
                if (i + j + 2 >= len)
                    return false;
                if ((ptr[i + j + 1] >> 6) != 0x02 || (ptr[i + j + 2] >> 6) != 0x02)
                    return false;
                // Проверка на overlong encoding
                if ((ptr[i + j] & 0x0F) == 0 && (ptr[i + j + 1] & 0x20) == 0)
                    return false;
                j += 2;
            }
            else if (len4_mask & (1 << j))
            {
                if (i + j + 3 >= len)
                    return false;
                if ((ptr[i + j + 1] >> 6) != 0x02 || (ptr[i + j + 2] >> 6) != 0x02 ||
                    (ptr[i + j + 3] >> 6) != 0x02)
                    return false;
                // Проверка на overlong encoding
                if ((ptr[i + j] & 0x07) == 0 && (ptr[i + j + 1] & 0x30) == 0)
                    return false;
                j += 3;
            }
        }

        i += 16;
    }

    // Обрабатываем оставшиеся байты скалярно
    for (; i < len;)
    {
        uint8_t byte = ptr[i];

        if (byte < 0x80)
        {
            i++;
            continue;
        }

        int remaining_bytes = 0;
        uint32_t code_point = 0;

        if ((byte >> 5) == 0x06)
        { // 2 байта
            if (i + 1 >= len)
                return false;
            if ((ptr[i + 1] >> 6) != 0x02)
                return false;
            code_point = ((byte & 0x1F) << 6) | (ptr[i + 1] & 0x3F);
            if (code_point < 0x80)
                return false;
            i += 2;
        }
        else if ((byte >> 4) == 0x0E)
        { // 3 байта
            if (i + 2 >= len)
                return false;
            if ((ptr[i + 1] >> 6) != 0x02 || (ptr[i + 2] >> 6) != 0x02)
                return false;
            code_point = ((byte & 0x0F) << 12) | ((ptr[i + 1] & 0x3F) << 6) | (ptr[i + 2] & 0x3F);
            if (code_point < 0x800)
                return false;
            if (code_point >= 0xD800 && code_point <= 0xDFFF)
                return false;
            i += 3;
        }
        else if ((byte >> 3) == 0x1E)
        { // 4 байта
            if (i + 3 >= len)
                return false;
            if ((ptr[i + 1] >> 6) != 0x02 || (ptr[i + 2] >> 6) != 0x02 || (ptr[i + 3] >> 6) != 0x02)
                return false;
            code_point = ((byte & 0x07) << 18) | ((ptr[i + 1] & 0x3F) << 12) | ((ptr[i + 2] & 0x3F) << 6) | (ptr[i + 3] & 0x3F);
            if (code_point < 0x10000 || code_point > 0x10FFFF)
                return false;
            i += 4;
        }
        else
        {
            return false;
        }
    }

    return true;
}

// Для const char*
bool is_valid_utf8_simd(const char *str, size_t length)
{
    return is_valid_utf8_simd(reinterpret_cast<const uint8_t *>(str), length);
}

// Для char* (неконстантный вариант)
bool is_valid_utf8_simd(char *str, size_t length)
{
    return is_valid_utf8_simd(reinterpret_cast<const uint8_t *>(str), length);
}

bool is_valid_utf8_simd(const uint8_t *data, size_t size)
{
    if (data == nullptr && size != 0)
        return false;
    return is_valid_utf8_simd(std::vector<uint8_t>(data, data + size));
}

// Для строк с известной длиной
bool is_valid_utf8_simd(const char *str)
{
    return is_valid_utf8_simd(str, strlen(str));
}

bool is_valid_utf8_simd(const std::string &str)
{
    return is_valid_utf8_simd(
        reinterpret_cast<const uint8_t *>(str.data()),
        str.size());
}
#endif

std::wstring format_time(const std::chrono::system_clock::time_point &tp, const std::wstring format)
{
    auto in_time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_buf = *std::localtime(&in_time_t);
    std::wstringstream wss;
    wss << std::put_time(&tm_buf, format.c_str());
    return wss.str();
}

std::wstring cc(const std::wstring &str, bool with_slashes, wchar_t cc)
{
    // Таблица замены управляющих символов
    static const std::unordered_map<wchar_t, std::wstring> map = {
        {L'\0', L"0"},     // Null
        {L'\a', L"a"},     // Bell (alert)
        {L'\b', L"b"},     // Backspace
        {L'\t', L"t"},     // Horizontal tab
        {L'\n', L"n"},     // New line (LF)
        {L'\v', L"v"},     // Vertical tab
        {L'\f', L"f"},     // Form feed
        {L'\r', L"r"},     // Carriage return (CR)
        {L'\x1A', L"x1A"}, // Ctrl+Z (SUB)
        {L'\x1B', L"x1B"}, // Escape (ESC)
        {L'\\', L"\\"},    // Обратный слеш (экранируем его тоже)
    };
    wchar_t buf[8];
    std::wstring result;
    result.reserve(str.length() * 2); // Резервируем память для экранированных символов
    for (auto c : str)
    {
        // DEBUG("%s(), CTRL = %u", __func__, c);
        if (c != L'\\' || with_slashes)
        {
            auto it = map.find(c);
            if (it != map.end())
            {
                result += cc + it->second; // Заменяем на экранированную версию
                continue;
            }
            else if (c < 32 || c == 127)
            {
                // Остальные управляющие символы (0x00-0x1F, 0x7F) заменяем на \xXX
                swprintf(buf, sizeof(buf) / sizeof(buf[0]), L"%cx%02X", cc, static_cast<unsigned int>(c));
                // buf[sizeof(buf) - sizeof(buf[0])] = 0;
                result += buf;
                continue;
            }
        }
        result += c; // Оставляем обычные символы без изменений
    }
    return result;
}

bool dir_exists(const std::wstring &path)
{
    struct stat info;
    return (stat(wstos(path).c_str(), &info) == 0 && S_ISDIR(info.st_mode));
}

bool is_mutex_locked(std::mutex &mutex)
{
    // Пытаемся захватить мьютекс без блокировки
    if (mutex.try_lock())
    {
        mutex.unlock(); // Сразу освобождаем
        return false;   // Мьютекс был свободен
    }
    return true; // Мьютекс занят
}

/*
int safe_poll(struct pollfd *fds, nfds_t nfds, int timeout_ms)
{
    int res;
    struct timespec start, now;
    if (timeout_ms > 0)
        clock_gettime(CLOCK_MONOTONIC, &start);
    do
    {
        res = poll(fds, nfds, timeout_ms);
        if (res == -1)
        {
            if (errno == EINTR)
            {
                if (timeout_ms > 0)
                {
                    clock_gettime(CLOCK_MONOTONIC, &now);
                    int elapsed = (now.tv_sec - start.tv_sec) * 1000 +
                                  (now.tv_nsec - start.tv_nsec) / 1000000;
                    timeout_ms = (timeout_ms > elapsed) ? (timeout_ms - elapsed) : 0;
                }
                continue; // Повторяем poll()
            }
            perror("poll() error:");
            return -1; // Другие ошибки
        }
    } while (res == -1); // Только для EINTR
    return res; // Количество готовых дескрипторов
}
*/

CPluginMngr::CPlugin *get_plugin(AMX *amx)
{
    CPluginMngr::CPlugin *p = nullptr;
    if (amx != nullptr && g_amxxapi.FindAmxScriptByAmx(amx) != -1)
    {
        p = findPluginFast(amx);
        if (p == nullptr) // || ((void*)amx != (void *)plugin) && !plugin->isValid()))
            AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): invalid plugin detected!", __func__);
    }
    return p;
}

bool amx_allot(AMX *amx, int cells, cell *amx_addr, cell **phys_addr)
{
    bool result;
    if (!(result = g_amxxapi.amx_Allot(amx, cells, amx_addr, phys_addr) == AMX_ERR_NONE))
        AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): heap low!", __func__);
    return result;
}

double timespec_diff(const struct timespec *start, const struct timespec *end)
{
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    if (nanoseconds < 0)
    {
        seconds -= 1;
        nanoseconds += 1000000000;
    }
    return (double)seconds + (double)nanoseconds / 1e9;
}

// Флаги CPUID (EDX и ECX при вызове с EAX=1)
const uint32_t FLAG_MMX = (1 << 23);
const uint32_t FLAG_SSE = (1 << 25);
const uint32_t FLAG_SSE2 = (1 << 26);
const uint32_t FLAG_SSE3 = (1 << 0);
const uint32_t FLAG_SSSE3 = (1 << 9);
const uint32_t FLAG_SSE4_1 = (1 << 19);
const uint32_t FLAG_SSE4_2 = (1 << 20);
const uint32_t FLAG_AVX = (1 << 28);

// Флаги CPUID (EBX при вызове с EAX=7, ECX=0)
const uint32_t FLAG_AVX2 = (1 << 5);
const uint32_t FLAG_AVX512F = (1 << 16);

void printFeature(const char *name, bool supported)
{
    UTIL_ServerPrint("%s: %s\n", name, supported ? "Yes" : "No");
}

void checkCPUFeatures()
{
    unsigned int eax, ebx, ecx, edx;
    // Проверка основных функций (EAX=1)
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
    {
        printFeature("MMX", edx & FLAG_MMX);
        printFeature("SSE", edx & FLAG_SSE);
        printFeature("SSE2", edx & FLAG_SSE2);
        printFeature("SSE3", ecx & FLAG_SSE3);
        printFeature("SSSE3", ecx & FLAG_SSSE3);
        printFeature("SSE4.1", ecx & FLAG_SSE4_1);
        printFeature("SSE4.2", ecx & FLAG_SSE4_2);
        printFeature("AVX", ecx & FLAG_AVX);
        // Set compatible for UTF-8 check
        g_SSE4_ENABLE = (ecx & FLAG_SSE4_1) & (ecx & FLAG_SSE4_2);
    }
    // Проверка AVX2 и AVX512 (EAX=7, ECX=0)
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
    {
        printFeature("AVX2", ebx & FLAG_AVX2);
        printFeature("AVX512F", ebx & FLAG_AVX512F);
    }
}

int64_t cpu_benchmark()
{
    const int64_t ITERATIONS = 10'000'000;
    volatile double result = 0; // volatile чтобы компилятор не оптимизировал вычисления
    auto start = std::chrono::high_resolution_clock::now();
    // Вычислительно интенсивная операция
    for (int64_t i = 1; i <= ITERATIONS; ++i)
    {
        // ~435 Тактов
        result += std::sin(i) * std::log(i) + std::sqrt(i);
        result += (i * 0.5) / (std::pow(i, 1.1) + 1.0);
    }
    auto end = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

std::string get_cpu_name()
{
    char brand[0x40] = {0}; // Буфер для названия (64 байта)
    uint32_t eax, ebx, ecx, edx;
    // Получаем брендовую строку процессора (3 вызова CPUID)
    for (uint32_t i = 0; i < 3; ++i)
    {
        __get_cpuid(0x80000002 + i, &eax, &ebx, &ecx, &edx);

        // Копируем 16 байт за раз (4 байта × 4 регистра)
        memcpy(brand + i * 16, &eax, sizeof(uint32_t));
        memcpy(brand + i * 16 + 4, &ebx, sizeof(uint32_t));
        memcpy(brand + i * 16 + 8, &ecx, sizeof(uint32_t));
        memcpy(brand + i * 16 + 12, &edx, sizeof(uint32_t));
    }
    return std::regex_replace(std::string(brand), std::regex("\\s+"), " ");
}

void cpu_test()
{
    UTIL_ServerPrint("\n[REFSAPI] CPU-Test\n");
    auto duration = cpu_benchmark();
    auto perfomance = (10'000'000.0 / duration / 1000);
    auto cpu_frec = perfomance * 435 / 1000;
    UTIL_ServerPrint("CPU: %s\n", get_cpu_name().c_str());
    UTIL_ServerPrint("Perfomance: %.3f REFS\n", perfomance);
    UTIL_ServerPrint("Duration: %" PRIu64 " ms\n", duration);
    UTIL_ServerPrint("Frec: %.3f GHz\n", cpu_frec);
    checkCPUFeatures();
    UTIL_ServerPrint("\n");
}

uint64_t get_next_id(std::atomic<uint64_t> &counter)
{
    uint64_t id = counter.fetch_add(1, std::memory_order_relaxed);

    // Проверяем два случая:
    // 1. Если предыдущее значение было 0 (первый вызов)
    // 2. Если было достигнуто максимальное значение (переполнение)
    if (id == 0 || id == std::numeric_limits<uint64_t>::max())
    {
        counter.store(1, std::memory_order_relaxed);
        return 1;
    }

    return id + 1; // Возвращаем увеличенное значение
}

bool set_thread_priority(std::thread *t, int pri)
{
    int policy;
    sched_param param;

    auto this_thread = t->native_handle();

    if (pthread_getschedparam(this_thread, &policy, &param) != 0)
        return false;

    param.sched_priority = pri;

    if (pthread_setschedparam(this_thread, policy, &param) != 0)
        return false;

    return true;
}

void get_thread_info(bool boost)
{
    pthread_t this_thread = pthread_self();

    int policy;
    sched_param param;

    UTIL_ServerPrint("\n[REFSAPI] Thread info\n");

    if (pthread_getschedparam(this_thread, &policy, &param) != 0)
    {
        UTIL_ServerPrint("Failed to get thread priority\n");
        return;
    }

    UTIL_ServerPrint("Current thread priority: %d\n", param.sched_priority);
    UTIL_ServerPrint("Scheduling policy: ");

    switch (policy)
    {
    case SCHED_OTHER:
        UTIL_ServerPrint("SCHED_OTHER (normal)\n");
        break;
    case SCHED_FIFO:
        UTIL_ServerPrint("SCHED_FIFO (real-time)\n");
        break;
    case SCHED_RR:
        UTIL_ServerPrint("SCHED_RR (round-robin)\n");
        break;
    default:
        UTIL_ServerPrint("Unknown\n");
    }

    if (boost)
    {
        param.sched_priority = -19; // Высокий приоритет

        if (pthread_setschedparam(this_thread, SCHED_OTHER, &param) != 0)
            UTIL_ServerPrint("Boost: failed to set real-time priority...\n");
        else
            UTIL_ServerPrint("Boost: done!\n");
    }

    UTIL_ServerPrint("\n");
}

std::wstring transliterate_ru_to_lat(const std::wstring &input)
{
    std::wstring output;
    output.reserve(input.size() * 2); // Резервируем память с запасом
    for (auto c : input)
    {
        // Ищем замену
        auto it = RU_TO_LAT_MAP.find(c);
        if (it != RU_TO_LAT_MAP.end())
            output += it->second; // Добавляем транслитерацию
        else
            output += c; // Оставляем как есть
    }
    return output;
}

char find_replacement(char32_t symbol)
{
    for (const auto &[ascii_char, similar_chars] : SIMILAR_CHARS_MAP)
    {
        if (similar_chars.find(symbol) != std::string::npos)
        {
            return ascii_char;
        }
    }
    return '\0'; // Если замена не найдена
}

std::wstring replace_similar(const std::wstring &input)
{
    std::wstring output;
    output.reserve(input.size()); // Резервируем память для эффективности

    for (auto c : input)
    {
        auto replacement = find_replacement(c);
        output += replacement ? replacement : c;
    }
    return output;
}

std::wstring remove_duplicates(const std::wstring &s)
{
    if (s.empty())
        return s;

    std::wstring result;
    size_t i = 0;

    while (i < s.size())
    {
        result += s[i++];
        // Пропускаем дубликаты
        while (i < s.size() && s[i] == s[i - 1])
            i++;
    }
    return result;
}

std::wstring normalization_str(std::wstring s)
{
    ws_convert_tolower(s);
    // DEBUG("%s(): tolower = %s", __func__, wstos(s).c_str());
    s = remove_duplicates(s);
    // DEBUG("%s(): remove_dup = %s", __func__, wstos(s).c_str());
    s = transliterate_ru_to_lat(s);
    // DEBUG("%s(): translit = %s", __func__, wstos(s).c_str());
    s = replace_similar(s);
    // DEBUG("%s(): replace_sim = %s", __func__, wstos(s).c_str());
    trim(s);
    // DEBUG("%s(): trim = %s", __func__, wstos(s).c_str());
    s.erase(std::remove_if(s.begin(), s.end(),
                           [](wchar_t c)
                           { return !std::isalnum(c); }),
            s.end());
    // DEBUG("%s(): alfanum = %s", __func__, wstos(s).c_str());
    return s;
}

int fast_levenshtein(const std::wstring &s1, const std::wstring &s2)
{
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    if (len1 == 0)
        return len2;
    if (len2 == 0)
        return len1;

    // Используем стековый массив для скорости
    std::array<int, 32> prev, curr;
    for (size_t j = 0; j <= len2; ++j)
        prev[j] = j;

    for (size_t i = 1; i <= len1; ++i)
    {
        curr[0] = i;
        for (size_t j = 1; j <= len2; ++j)
        {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
            curr[j] = std::min({curr[j - 1] + 1, prev[j] + 1, prev[j - 1] + cost});
        }
        prev.swap(curr);
    }
    return prev[len2];
}

int longest_common_substring(const std::wstring &s1, const std::wstring &s2)
{
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();
    std::vector<std::vector<int>> dp(len1 + 1, std::vector<int>(len2 + 1, 0));
    int max_len = 0;

    for (size_t i = 1; i <= len1; ++i)
    {
        for (size_t j = 1; j <= len2; ++j)
        {
            if (s1[i - 1] == s2[j - 1])
            {
                dp[i][j] = dp[i - 1][j - 1] + 1;
                max_len = std::max(max_len, dp[i][j]);
            }
        }
    }
    return max_len;
}

std::unordered_set<std::wstring> get_cached_ngrams(const std::wstring &s, int n)
{
    if (g_cache_ngrams.count(s))
        return g_cache_ngrams[s];

    std::unordered_set<std::wstring> ngrams;
    for (size_t i = 0; i <= s.size() - n; ++i)
    {
        ngrams.insert(s.substr(i, n));
    }
    g_cache_ngrams[s] = ngrams;
    return ngrams;
}

float similarity_score(const std::wstring &nick1, const std::wstring &nick2, const float lcs_threshold, const float k_lev, const float k_tan, const float k_lcs)
{
    // Нормализация
    std::wstring n1 = normalization_str(nick1);
    std::wstring n2 = normalization_str(nick2);

    // Предварительная фильтрация по длине
    if (n1.size() < 2 || n2.size() < 2 || std::abs((int)n1.size() - (int)n2.size()) > 5)
        return 0.0;

    // Быстрый выход при полном совпадении
    if (n1 == n2)
        return 1.0;

    // LCS (минимум 3 символа)
    auto lcs_len = longest_common_substring(n1, n2);
    if (lcs_len >= 3)
    {
        auto lcs_score = static_cast<float>(lcs_len) / std::max(n1.size(), n2.size());
        if (lcs_score > lcs_threshold)
            return lcs_score; // Если LCS очень похож, пропускаем остальное
    }
    else
        lcs_len = 0;

    // Левенштейн (нормализованный)
    int lev_dist = fast_levenshtein(n1, n2);
    auto lev_score = 1.0f - static_cast<float>(lev_dist) / std::max(n1.size(), n2.size());

    // Танимото на 2-граммах (с кешированием)
    auto ngrams1 = get_cached_ngrams(n1, 2);
    auto ngrams2 = get_cached_ngrams(n2, 2);
    size_t intersection = 0;
    for (const auto &item : ngrams1)
    {
        if (ngrams2.count(item))
            intersection++;
    }
    auto tan_score = static_cast<float>(intersection) / (ngrams1.size() + ngrams2.size() - intersection);

    // Средневзвешенная оценка
    return k_lev * lev_score + k_tan * tan_score + k_lcs * (lcs_len / static_cast<double>(std::max(n1.size(), n2.size())));
}

void calc_frame_delay(const size_t interval, const uint64_t frames_count, timespec &frame_prev, double &frame_delay, size_t &frame_rate, size_t &frame_rate_max, float k1_max, float k2_max)
{
    timespec frame_curr;
    if (!(frames_count % interval))
    {
        clock_gettime(CLOCK_MONOTONIC, &frame_curr);
        if (frame_prev.tv_sec >= 0 && frame_prev.tv_nsec > 0)
        {
            auto delay = timespec_diff(&frame_prev, &frame_curr);
            if (frame_delay > 0.0)
            {
                auto k0 = interval / 1000.0;
                auto k1 = delay > frame_delay ? k1_max * (delay - frame_delay) / (frame_delay + delay) : k0;
                auto k2 = frame_delay > k0 ? k2_max * frame_delay : k0;
                frame_rate = std::max(1U, (frame_rate + (size_t)std::round((k1 + k2) / k0)) >> 1);
                if (frame_rate > frame_rate_max)
                    frame_rate_max = frame_rate;
            }
            frame_delay = (frame_delay + delay) / 2.0;
        }
        frame_prev = frame_curr;
    }
}