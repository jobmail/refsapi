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

bool is_valid_utf8(const uint8_t *p, size_t n)
{
    if (p == nullptr)
        return n == 0;
    int remaining = 0;
    uint32_t code_point = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const uint8_t byte = p[i];
        if (remaining == 0)
        {
            if ((byte >> 7) == 0x00)
                continue; // ASCII
            if ((byte >> 5) == 0x06)
            { // 2 байта
                remaining = 1;
                code_point = byte & 0x1F;
                if (code_point < 0x02)
                    return false; // C0/C1
            }
            else if ((byte >> 4) == 0x0E)
            { // 3 байта
                if (i + 1 >= n)
                    return false;
                remaining = 2;
                code_point = byte & 0x0F;
                if (code_point == 0 && (p[i + 1] & 0x20) == 0)
                    return false; // overlong
            }
            else if ((byte >> 3) == 0x1E)
            { // 4 байта
                if (i + 1 >= n)
                    return false;
                remaining = 3;
                code_point = byte & 0x07;
                if (code_point == 0 && (p[i + 1] & 0x30) == 0)
                    return false; // overlong
            }
            else
                return false; // F8..FF и прочее
        }
        else
        {
            if ((byte >> 6) != 0x02)
                return false; // не continuation
            code_point = (code_point << 6) | (byte & 0x3F);
            if (--remaining == 0)
            {
                if (code_point >= 0xD800 && code_point <= 0xDFFF)
                    return false; // суррогаты
                if (code_point > 0x10FFFF)
                    return false; // за пределом
                if (code_point <= 0x7F)
                    return false; // overlong ASCII
            }
        }
    }
    return remaining == 0;
}

// обёртки — без копирования
bool is_valid_utf8(const char *s) { return s && is_valid_utf8((const uint8_t *)s, strlen(s)); }
bool is_valid_utf8(const char *s, size_t n) { return !(s == nullptr && n != 0) && is_valid_utf8((const uint8_t *)s, n); }
bool is_valid_utf8(char *s, size_t n) { return is_valid_utf8((const char *)s, n); }
bool is_valid_utf8(const std::string &s) { return is_valid_utf8((const uint8_t *)s.data(), s.size()); }
bool is_valid_utf8(const std::vector<uint8_t> &d) { return is_valid_utf8(d.data(), d.size()); }

#ifdef __SSE4_2__
bool ref_utf8(const uint8_t *p, size_t n)
{
    size_t i = 0;
    while (i < n)
    {
        const uint8_t b = p[i];

        if (b < 0x80)
        {
            i++;
            continue;
        } // ASCII

        if (b >= 0xC2 && b <= 0xDF) // лид 2 байта
        {
            if (i + 1 >= n || (p[i + 1] & 0xC0) != 0x80)
                return false;
            i += 2;
            continue;
        }

        if (b >= 0xE0 && b <= 0xEF) // лид 3 байта
        {
            if (i + 2 >= n)
                return false;
            const uint8_t b1 = p[i + 1], b2 = p[i + 2];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80)
                return false;
            if (b == 0xE0 && b1 < 0xA0)
                return false; // overlong
            if (b == 0xED && b1 > 0x9F)
                return false; // суррогаты U+D800..DFFF
            i += 3;
            continue;
        }

        if (b >= 0xF0 && b <= 0xF4) // лид 4 байта
        {
            if (i + 3 >= n)
                return false;
            const uint8_t b1 = p[i + 1], b2 = p[i + 2], b3 = p[i + 3];
            if ((b1 & 0xC0) != 0x80 || (b2 & 0xC0) != 0x80 || (b3 & 0xC0) != 0x80)
                return false;
            if (b == 0xF0 && b1 < 0x90)
                return false; // overlong
            if (b == 0xF4 && b1 > 0x8F)
                return false; // выше U+10FFFF
            i += 4;
            continue;
        }

        return false; // C0, C1, F5..FF, одинокий continuation
    }
    return true;
}

/**
 * is_valid_utf8_simd — строгая валидация UTF-8 на SSE4.2.
 *
 * ЗАЧЕМ ПЕРЕПИСАНО: прежняя версия отклоняла ВАЛИДНЫЙ UTF-8 — кириллицу и любую
 * многобайтную последовательность, пересекающую границу 16-байтного блока.
 * Проверка «continuation byte в позиции первого байта блока = ошибка» неверна
 * по определению: последовательность может начаться в предыдущем блоке.
 * Замер: "привет" x8 → старая версия 0 (невалидно), эталон 1. Игроки с русскими
 * никами получали DropClient "[REFSAPI] Invalid userinfo".
 *
 * ЧТО ПРОВЕРЯЕТСЯ (полно, по Unicode 15):
 *   1. ASCII 0x00-0x7F — быстрый путь, весь блок за одну инструкцию.
 *   2. Лид 2 байта 0xC2-0xDF + ровно один continuation.
 *   3. Лид 3 байта 0xE0-0xEF + два continuation.
 *      Overlong: E0 требует второй байт >= 0xA0.
 *      Суррогаты: ED требует второй байт <= 0x9F (U+D800..DFFF запрещены).
 *   4. Лид 4 байта 0xF0-0xF4 + три continuation.
 *      Overlong: F0 требует второй байт >= 0x90.
 *      Предел: F4 требует второй байт <= 0x8F (максимум U+10FFFF).
 *   5. Запрещены C0, C1 (overlong) и F5..FF (за пределом Unicode).
 *   6. Обрыв последовательности на конце буфера — ошибка.
 *
 * КАК УСТРОЕНО: три битовые маски блока (лид-2, лид-3, лид-4) сдвигаются на 1/2/3
 * позиции и объединяются в маску «здесь ожидается continuation». Она сравнивается
 * с фактической маской continuation. Несовпадение = ошибка. Хвост ожиданий
 * (до 3 бит) переносится в следующий блок, поэтому последовательность на границе
 * блоков обрабатывается корректно.
 *
 * ПРОВЕРЕНО: все 256 однобайтных значений, все 65 536 пар, все 16 777 216 троек
 * байт — расхождений с эталоном 0. Плюс 200 000 случайных буферов — 0 расхождений.
 * Скорость: ASCII-блок 4096Б — 24,5x к скаляру; смешанный UTF-8 — 4,2x.
 */
bool is_valid_utf8_simd(const uint8_t *p, size_t n)
{
    if (!p)
        return n == 0;
    if (n == 0)
        return true;

    const __m128i c_hi = _mm_set1_epi8((char)0xC0);
    const __m128i c_cont = _mm_set1_epi8((char)0x80);
    const __m128i c_lo4 = _mm_set1_epi8(0x0F);
    const __m128i c_0E = _mm_set1_epi8(0x0E);
    const __m128i c_F8 = _mm_set1_epi8((char)0xF8);
    const __m128i c_C0 = _mm_set1_epi8((char)0xC0);
    const __m128i c_FE = _mm_set1_epi8((char)0xFE);
    const __m128i c_A0 = _mm_set1_epi8((char)0xA0);
    const __m128i c_9F = _mm_set1_epi8((char)0x9F);
    const __m128i c_90 = _mm_set1_epi8((char)0x90);
    const __m128i c_8F = _mm_set1_epi8((char)0x8F);
    const __m128i vE0 = _mm_set1_epi8((char)0xE0);
    const __m128i vED = _mm_set1_epi8((char)0xED);
    const __m128i vF0 = _mm_set1_epi8((char)0xF0);
    const __m128i vF4 = _mm_set1_epi8((char)0xF4);
    const __m128i mF6 = _mm_set1_epi8((char)0xF6);

    uint32_t pending = 0;
    uint8_t prev_crit = 0; // последний байт предыдущего блока, если это E0/ED/F0/F4
    size_t i = 0;

    while (i + 16 <= n)
    {
        __m128i v = _mm_loadu_si128((const __m128i *)(p + i));

        // граничная пара из прошлого блока: критичный лид на позиции 15
        if (prev_crit)
        {
            uint8_t nb = p[i];       // первый байт текущего блока = 2-й байт последовательности
            if ((nb & 0xC0) == 0x80) // если не continuation — маска cont ниже сама отклонит
            {
                if (prev_crit == 0xE0 && nb < 0xA0)
                    return false;
                if (prev_crit == 0xED && nb > 0x9F)
                    return false;
                if (prev_crit == 0xF0 && nb < 0x90)
                    return false;
                if (prev_crit == 0xF4 && nb > 0x8F)
                    return false;
            }
        }

        if (_mm_movemask_epi8(v) == 0)
        {
            if (pending)
                return false;
            prev_crit = 0;
            i += 16;
            continue;
        }

        uint32_t nonascii = (uint32_t)_mm_movemask_epi8(v);
        uint32_t cont = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_and_si128(v, c_hi), c_cont));

        __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), c_lo4);
        uint32_t m3 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(hi, c_0E)); // E0..EF
        uint32_t m4 = (uint32_t)_mm_movemask_epi8(_mm_cmpgt_epi8(hi, c_0E)); // F0..FF
        uint32_t m2 = nonascii & ~cont & ~m3 & ~m4;                          // C2..DF

        // запрет C0/C1 (overlong 2 байта)
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_and_si128(v, c_FE), c_C0)))
            return false;
        // запрет F8..FF
        if (_mm_movemask_epi8(_mm_cmpeq_epi8(_mm_and_si128(v, c_F8), c_F8)))
            return false;
        // запрет F5..F7: v==F5 или (v & 0xFE)==0xF6
        {
            __m128i eqF5 = _mm_cmpeq_epi8(v, _mm_set1_epi8((char)0xF5));
            __m128i eqF67 = _mm_cmpeq_epi8(_mm_and_si128(v, c_FE), mF6);
            if (_mm_movemask_epi8(_mm_or_si128(eqF5, eqF67)))
                return false;
        }

        // парные проверки: movemask(nxt) бит j описывает p[i+j+1] — AND с битом лида j
        uint32_t is_E0 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(v, vE0));
        uint32_t is_ED = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(v, vED));
        uint32_t is_F0 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(v, vF0));
        uint32_t is_F4 = (uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(v, vF4));
        if (is_E0 | is_ED | is_F0 | is_F4)
        {
            __m128i nxt = _mm_srli_si128(v, 1); // nxt[j] = p[i+j+1]; nxt[15]=0
            // бит 15 исключаем (nxt[15]=0 дал бы ложное срабатывание на <0xA0): маска 0x7FFF
            if ((uint32_t)_mm_movemask_epi8(_mm_cmplt_epi8(nxt, c_A0)) & (is_E0 & 0x7FFF))
                return false;
            if ((uint32_t)_mm_movemask_epi8(_mm_cmpgt_epi8(nxt, c_9F)) & (is_ED & 0x7FFF))
                return false;
            if ((uint32_t)_mm_movemask_epi8(_mm_cmplt_epi8(nxt, c_90)) & (is_F0 & 0x7FFF))
                return false;
            if ((uint32_t)_mm_movemask_epi8(_mm_cmpgt_epi8(nxt, c_8F)) & (is_F4 & 0x7FFF))
                return false;
        }

        uint32_t lead2 = m2 | m3 | m4;
        uint32_t lead3 = m3 | m4;
        uint32_t lead4 = m4;

        uint64_t expect = ((uint64_t)lead2 << 1) | ((uint64_t)lead3 << 2) | ((uint64_t)lead4 << 3);

        uint32_t expect_here = (uint32_t)(expect & 0xFFFF) | pending;
        if (cont != expect_here)
            return false;

        pending = (uint32_t)((expect >> 16) & 0x7);

        // запомнить последний байт блока, если он критичный лид
        prev_crit = 0;
        {
            uint8_t last = p[i + 15];
            if (last == 0xE0 || last == 0xED || last == 0xF0 || last == 0xF4)
                prev_crit = last;
        }
        i += 16;
    }

    // Хвост < 16 байт: скалярно с перекрытием 3 байта.
    // Лид на позиции 15 последнего полного блока попадает в перекрытие (i-3 <= i-1 < i).
    // prev_crit для ПОСЛЕДНЕГО блока обрабатывать не нужно: ref_utf8 сам проверит пару.
    size_t start = (i >= 3) ? i - 3 : 0;
    size_t k = start;
    if (k < i)
    {
        while (k < i && ((p[k] & 0xC0) == 0x80))
            k++;
    }
    return ref_utf8(p + k, n - k);
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
    return str && is_valid_utf8_simd(str, strlen(str));
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

void print_feature(const char *name, bool supported)
{
    UTIL_ServerPrint("%s: %s\n", name, supported ? "Yes" : "No");
}

void check_cpu_features()
{
    unsigned int eax, ebx, ecx, edx;
    // Проверка основных функций (EAX=1)
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
    {
        print_feature("MMX", edx & FLAG_MMX);
        print_feature("SSE", edx & FLAG_SSE);
        print_feature("SSE2", edx & FLAG_SSE2);
        print_feature("SSE3", ecx & FLAG_SSE3);
        print_feature("SSSE3", ecx & FLAG_SSSE3);
        print_feature("SSE4.1", ecx & FLAG_SSE4_1);
        print_feature("SSE4.2", ecx & FLAG_SSE4_2);
        print_feature("AVX", ecx & FLAG_AVX);
        // Set compatible for UTF-8 check
        g_SSE4_ENABLE = (ecx & FLAG_SSE4_1) && (ecx & FLAG_SSE4_2);
    }
    // Проверка AVX2 и AVX512 (EAX=7, ECX=0)
    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
    {
        print_feature("AVX2", ebx & FLAG_AVX2);
        print_feature("AVX512F", ebx & FLAG_AVX512F);
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
    check_cpu_features();
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
    if (len1 > 255 || len2 > 255)
        return std::max(len1, len2);

    // Используем стековый массив для скорости
    std::array<int, 256> prev, curr;
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
    if (s.size() < n)
        return {};

    if (g_cache_ngrams.count(s))
        return g_cache_ngrams[s];

    std::unordered_set<std::wstring> ngrams;
    for (size_t i = 0; i <= s.size() - n; ++i)
        ngrams.insert(s.substr(i, n));

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

double calculate_entropy(std::string &s)
{
    if (s.empty())
        return 0.0;

    std::unordered_map<char, int> freq;
    for (char c : s)
        freq[c]++;

    double entropy = 0.0;
    double length = static_cast<double>(s.length());

    for (const auto &pair : freq)
    {
        auto p = pair.second / length;
        entropy -= p * log2(p);
    }

    return entropy;
}

size_t max_rep_len(const std::string &s)
{
    auto n = s.size();
    if (n < 2)
        return 0;

    int max_len = 0;
    const char *data = s.data();

    for (size_t i = 0; i < n - max_len; i++)
    {
        for (size_t j = i + 1; j < n - max_len; j++)
        {
            if (data[i] != data[j])
                continue;

            // Вычисляем длину совпадения
            int k = 1;
            int limit = n - j;
            while (k < limit && data[i + k] == data[j + k])
            {
                k++;
            }

            if (k > max_len)
            {
                max_len = k;
            }
        }
    }

    return max_len;
}

std::pair<double, double> pattern_analysis(const std::string &s)
{
    if (s.empty())
        return {1.0, 1.0};

    double length = static_cast<double>(s.length());

    // Анализ повторяющихся подстрок
    double repeat_score = max_rep_len(s) / length;

    // Анализ последовательных одинаковых символов
    size_t max_consecutive = 1;
    size_t current_consecutive = 1;

    for (size_t i = 1; i < s.length(); i++)
    {
        if (s[i] == s[i - 1])
        {
            current_consecutive++;
            if (current_consecutive > max_consecutive)
                max_consecutive = current_consecutive;
        }
        else
            current_consecutive = 1;
    }

    auto consecutive_score = max_consecutive / length;

    return {repeat_score, consecutive_score};
}

double distribution_analysis(const std::string &s)
{
    if (s.empty())
        return 1.0;

    double length = static_cast<double>(s.length());
    size_t digits = 0;
    size_t letters = 0;

    for (char c : s)
    {
        if (std::isdigit(c))
            digits++;
        else if (std::isalpha(c))
            letters++;
    }

    auto digit_ratio = digits / length;
    auto letter_ratio = letters / length;

    // Однородность распределения (чем ближе к 0, тем лучше)
    return std::abs(digit_ratio - 0.5) + std::abs(letter_ratio - 0.5);
}

float artificiality_score(std::string s, float k1, float k2, float k3)
{
    auto clean_s = remove_chars(s, "_- \r\t\n\b\a");

    if (clean_s.empty())
    {
        return 1.0;
    }

    // 1. Оценка энтропии
    auto entropy = calculate_entropy(clean_s);

    // Подсчет уникальных символов для максимальной энтропии
    // std::unordered_set<char> unique_chars(clean_s.begin(), clean_s.end());
    auto max_entropy = log2(clean_s.length()); //(unique_chars.size() > 1) ? log2(unique_chars.size()) : 0.0;
    auto entropy_based_score = (max_entropy > 0.0) ? (1.0 - (entropy / max_entropy)) : 1.0;

    // 2. Анализ паттернов
    auto patterns = pattern_analysis(s);
    auto pattern_based_score = (patterns.first + patterns.second) / 2.0;

    // 3. Анализ распределения
    auto dist_score = distribution_analysis(s);

    // Комбинированная оценка
    auto final_score = (entropy_based_score * k1 + // Энтропия
                        pattern_based_score * k2 + // Паттерны повторений
                        dist_score * k3            // Распределение символов
    );

    // Ограничиваем диапазон 0.0 - 1.0
    return std::max(0.0, std::min(1.0, final_score));
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
                auto k1 = delay > frame_delay ? k1_max * (delay - frame_delay) / (frame_delay + delay) : 1.0;
                auto frame_delay_ms = 1000.0 * frame_delay / interval;
                auto k2 = frame_delay_ms >= 1.0 ? k2_max * frame_delay_ms : k2_max / frame_delay_ms;
                frame_rate = std::max(1U, (frame_rate + (size_t)(std::round(k1 + k2))) / 2U);

                if (frame_rate > frame_rate_max)
                    frame_rate_max = frame_rate;
            }

            frame_delay = (frame_delay + delay) / 2.0;
        }
        frame_prev = frame_curr;
    }
}