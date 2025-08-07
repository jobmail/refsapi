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

CPluginMngr::CPlugin * get_plugin(AMX *amx)
{
    auto plugin = findPluginFast(amx);
    if ((plugin == nullptr || !plugin->isValid()))
    {
        if (g_amxxapi.FindAmxScriptByAmx(amx) != -1)
            AMXX_LogError(amx, AMX_ERR_NATIVE, "%s(): invalid plugin detected!", __func__);
        return nullptr;
    }
    return plugin;
}

/*
bool is_valid_utf8_simd(const uint8_t *data, size_t size)
{
    const uint8_t *end = data + size;
    const uint8_t *aligned_end = data + (size & ~15); // Выравниваем по 16 байт
    __m128i mask_80 = _mm_set1_epi8(0x80); // Маска для проверки старшего бита
    __m128i mask_C0 = _mm_set1_epi8(0xC0); // Маска для проверки начала символа
    __m128i mask_7F = _mm_set1_epi8(0x7F); // Маска для ASCII символов
    int32_t remaining_bytes = 0;
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
        // Обрабатываем байты по одному если есть многобайтовые символы
        for (int i = 0; i < 16; ++i)
        {
            uint8_t byte = _mm_extract_epi8(chunk, i);
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
                    return false;
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
                return false;
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
*/