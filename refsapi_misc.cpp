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

size_t set_amx_string(cell *dest, const char *str, size_t max_len)
{
    size_t count = 0;
    if (dest && max_len)
    {
        while (str && *str && ++count < max_len)
            *dest++ = (cell)*str++;
        *dest = 0;
    }
    return count;
}

bool is_valid_utf8(const std::vector<uint8_t> &data)
{
    int remaining_bytes = 0;
    uint32_t code_point = 0;
    uint8_t byte;
    for (size_t i = 0; i < data.size(); ++i)
    {
        byte = data[i];
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
            }
            else if ((byte >> 4) == 0x0E)
            { // 1110xxxx (3 байта)
                remaining_bytes = 2;
                code_point = byte & 0x0F;
            }
            else if ((byte >> 3) == 0x1E)
            { // 11110xxx (4 байта)
                remaining_bytes = 3;
                code_point = byte & 0x07;
            }
            else
                return false; // Недопустимый первый байт
            // Проверка на overlong-encoding (избыточное кодирование)
            if (remaining_bytes == 1 && code_point < 0x02)
                return false;
            if (remaining_bytes == 2 && code_point < 0x04)
                return false;
            if (remaining_bytes == 3 && code_point < 0x08)
                return false;
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
                    return false;
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

bool is_valid_utf8_simd(const uint8_t *data, size_t size)
{
    const uint8_t *end = data + size;
    const uint8_t *aligned_end = data + (size & ~15); // Выравниваем по 16 байт

    __m128i mask_80 = _mm_set1_epi8(0x80); // Маска для проверки старшего бита
    __m128i mask_C0 = _mm_set1_epi8(0xC0); // Маска для проверки начала символа

    int remaining_bytes = 0;
    uint32_t code_point = 0;

    while (data < aligned_end)
    {
        __m128i chunk = _mm_loadu_si128((__m128i *)data);
        data += 16;

        // Проверяем ASCII символы (быстрый путь)
        __m128i ascii_mask = _mm_cmpeq_epi8(_mm_and_si128(chunk, mask_80), _mm_setzero_si128());
        int ascii_bits = _mm_movemask_epi8(ascii_mask);

        if (ascii_bits == 0xFFFF)
        {
            if (remaining_bytes == 0)
                continue;
            return false; // ASCII символы в середине последовательности
        }

        // Обрабатываем байты по одному (без _mm_extract_epi8)
        alignas(16) uint8_t bytes[16];
        _mm_store_si128((__m128i *)bytes, chunk);

        for (int i = 0; i < 16; ++i)
        {
            uint8_t byte = bytes[i]; // Доступ через массив вместо _mm_extract_epi8

            if (remaining_bytes == 0)
            {
                if ((byte & 0x80) == 0)
                    continue; // ASCII

                if ((byte >> 5) == 0x06)
                { // 110xxxxx
                    remaining_bytes = 1;
                    code_point = byte & 0x1F;
                }
                else if ((byte >> 4) == 0x0E)
                { // 1110xxxx
                    remaining_bytes = 2;
                    code_point = byte & 0x0F;
                }
                else if ((byte >> 3) == 0x1E)
                { // 11110xxx
                    remaining_bytes = 3;
                    code_point = byte & 0x07;
                }
                else
                {
                    return false;
                }

                // Проверка на overlong encoding
                if (remaining_bytes == 1 && code_point < 0x02)
                    return false;
                if (remaining_bytes == 2 && code_point < 0x04)
                    return false;
                if (remaining_bytes == 3 && code_point < 0x08)
                    return false;
            }
            else
            {
                if ((byte >> 6) != 0x02)
                    return false;
                code_point = (code_point << 6) | (byte & 0x3F);
                remaining_bytes--;

                if (remaining_bytes == 0)
                {
                    if (code_point >= 0xD800 && code_point <= 0xDFFF)
                        return false;
                    if (code_point > 0x10FFFF)
                        return false;
                }
            }
        }
    }

    // Обрабатываем оставшиеся байты
    while (data < end)
    {
        uint8_t byte = *data++;

        if (remaining_bytes == 0)
        {
            if ((byte & 0x80) == 0)
                continue;

            if ((byte >> 5) == 0x06)
            {
                remaining_bytes = 1;
                code_point = byte & 0x1F;
            }
            else if ((byte >> 4) == 0x0E)
            {
                remaining_bytes = 2;
                code_point = byte & 0x0F;
            }
            else if ((byte >> 3) == 0x1E)
            {
                remaining_bytes = 3;
                code_point = byte & 0x07;
            }
            else
            {
                return false;
            }

            if (remaining_bytes == 1 && code_point < 0x02)
                return false;
            if (remaining_bytes == 2 && code_point < 0x04)
                return false;
            if (remaining_bytes == 3 && code_point < 0x08)
                return false;
        }
        else
        {
            if ((byte >> 6) != 0x02)
                return false;
            code_point = (code_point << 6) | (byte & 0x3F);
            remaining_bytes--;

            if (remaining_bytes == 0)
            {
                if (code_point >= 0xD800 && code_point <= 0xDFFF)
                    return false;
                if (code_point > 0x10FFFF)
                    return false;
            }
        }
    }

    return remaining_bytes == 0;
}

// Обертка для std::vector
bool is_valid_utf8_simd(const std::vector<uint8_t> &data)
{
    if (data.empty())
        return true;
    return is_valid_utf8_simd(data.data(), data.size());
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
            perror("poll() error");
            return -1; // Другие ошибки
        }
    } while (res == -1); // Только для EINTR
    return res; // Количество готовых дескрипторов
}

CPluginMngr::CPlugin *get_plugin(AMX *amx)
{
    auto plugin = findPluginFast(amx);
    if (plugin == nullptr) // || ((void*)amx != (void *)plugin) && !plugin->isValid()))
        AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): invalid plugin detected!", __func__);
    return plugin;
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