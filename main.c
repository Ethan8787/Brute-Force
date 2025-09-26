#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <windows.h>
#include <ctype.h>

// 字元集
const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// 最大長度
enum { MAX_LEN = 8 };

// 回傳秒數 計算速率與時間
static double now_seconds(void) {
    return (double) GetTickCount64() / 1000.0;
}

// 色碼支援
static void EnableVTMode(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(hOut, dwMode);
}

// 時間格式化 (1h 2min 3s)
static void format_duration(double secs, char *buf, size_t bufsz) {
    long s = (long) (secs + 0.5);
    const long h = s / 3600;
    s %= 3600;
    const long m = s / 60;
    s %= 60;

    if (h > 0) {
        snprintf(buf, bufsz, "%ldh %ldmin %lds", h, m, s);
    } else if (m > 0) {
        if (s == 0) snprintf(buf, bufsz, "%ldmin", m);
        else snprintf(buf, bufsz, "%ldmin %lds", m, s);
    } else {
        snprintf(buf, bufsz, "%lds", s);
    }
}

// 僅允許 A–Z / 0–9
static int is_allowed(int c) {
    return ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9'));
}

// 轉大寫後有效
static int upper_valid(char *s) {
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n') { s[--n] = '\0'; }

    for (size_t i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char) s[i];
        if (!is_allowed(ch)) return 0;
        if (ch >= 'a' && ch <= 'z') s[i] = (char) toupper(ch);
    }
    return 1;
}

// 進度條
static void log_progress(const unsigned long long attempts, const char *total_str,
                         const double rate, const int fraction_valid, double fraction, const int final) {
    static int printed_once = 0;

    const time_t t = time(NULL);
    const struct tm *tmnow = localtime(&t);
    char timestr[16];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", tmnow);

    const int BAR_LEN = 20;
    int filled = 0;
    if (fraction_valid) {
        if (fraction < 0.0) fraction = 0.0;
        if (fraction > 1.0) fraction = 1.0;
        filled = (int) (fraction * (double) BAR_LEN + 0.5);
    } else {
        filled = (int) (attempts % (unsigned long long) (BAR_LEN + 1));
    }
    if (filled < 0) filled = 0;
    if (filled > BAR_LEN) filled = BAR_LEN;

    char pctbuf[32];
    if (fraction_valid) snprintf(pctbuf, sizeof(pctbuf), "(%.2f%%)", fraction * 100.0);
    else snprintf(pctbuf, sizeof(pctbuf), "(--%%)");

    unsigned long long rate_raw = (unsigned long long) (rate + 0.5);

    if (printed_once) {
        printf("\x1b[3A");
    } else {
        printed_once = 1;
    }

    printf("\x1b[2K");
    printf("\x1b[0m[%s INFO]: \x1b[93mProgress\x1b[97m: \x1b[0m[", timestr);
    for (int i = 0; i < filled; ++i) printf("\x1b[96m█\x1b[0m");
    for (int i = filled; i < BAR_LEN; ++i) printf("\x1b[90m█\x1b[0m");
    printf("] %s", pctbuf);
    printf("\n");

    printf("\x1b[2K");
    printf("\x1b[0m[%s INFO]: \x1b[93mAttempts\x1b[97m: \x1b[0m%llu/%s", timestr, attempts, total_str);
    printf("\n");

    printf("\x1b[2K");
    printf("\x1b[0m[%s INFO]: \x1b[93mSpeed\x1b[97m: \x1b[0m%llu/s", timestr, rate_raw);
    printf("\n");
}

// 進位遞增器(這是我想到最好的名稱了)
int increment_counter(int *indices, int fill, int base) {
    int i = fill - 1;
    indices[i]++;
    while (indices[i] == base) {
        indices[i--] = 0;
        if (i < 0) return 0;
        indices[i]++;
    }
    return 1;
}

// 主程式
int main(void) {
    SetConsoleOutputCP(CP_UTF8); // 設定輸出 UTF8
    SetConsoleCP(CP_UTF8); // 設定輸入 UTF8
    EnableVTMode(); // 啟用終端機色彩支援

    time_t start_wall = time(NULL);
    char start_time_str[16];
    strftime(start_time_str, sizeof(start_time_str), "%H:%M:%S", localtime(&start_wall));

    char pw[256];
    // 歡迎畫面
    printf("\n");
    printf("\x1b[91m███████╗████████╗██╗  ██╗ █████╗ ███╗   ██╗\x1b[0m\n");
    printf("\x1b[91m██╔════╝╚══██╔══╝██║  ██║██╔══██╗████╗  ██║\x1b[0m\n");
    printf("\x1b[91m█████╗     ██║   ███████║███████║██╔██╗ ██║\x1b[0m\n");
    printf("\x1b[91m██╔══╝     ██║   ██╔══██║██╔══██║██║╚██╗██║\x1b[0m\n");
    printf("\x1b[91m███████╗   ██║   ██║  ██║██║  ██║██║ ╚████║\x1b[0m\n");
    printf("\x1b[91m╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝\x1b[0m\n");
    printf("\n");
    printf("================ \x1b[33mBrute-Force\x1b[0m ================\n");
    printf("\x1b[93mEnter the target password (A-Z, 0-9, max %d)", MAX_LEN);
    for (;;) {
        printf("\x1b[97m\n> ");
        fflush(stdout);

        if (!fgets(pw, sizeof(pw), stdin)) return 1;

        if (!upper_valid(pw)) {
            printf("\x1b[91mInvalid input");
            continue;
        }
        const int target_len = (int) strlen(pw);

        if (target_len == 0) {
            printf("\x1b[91mNo password entered");
            continue;
        }
        if (target_len > MAX_LEN) {
            printf("\x1b[91mToo long \x1b[37m(Maximum is %d)", MAX_LEN);
            continue;
        }

        break;
    }

    const int target_len = (int) strlen(pw);
    const int fill = target_len;

    unsigned long long attempts = 0ULL; // 已嘗試
    const double start = now_seconds(); // 記錄時間
    double last_report = start; // 上次時間
    const int base = (int) strlen(charset); // char 大小

    int *indices = malloc(sizeof(int) * fill); // 配置陣列
    for (int i = 0; i < fill; ++i) indices[i] = 0; // 初始化
    char *guess = malloc(fill + 1); // 存放目前字串
    int found = 0;

    unsigned long long total_needed = 1ULL; // 總需嘗試
    int overflow = 0;
    for (int i = 0; i < fill; ++i) {
        if (total_needed > ULLONG_MAX / (unsigned long long) base) {
            overflow = 1;
            break;
        }
        total_needed *= (unsigned long long) base; // 總組合數 = base^fill
    }
    char total_str[256];
    snprintf(total_str, sizeof(total_str), "%llu", total_needed);

    while (1) {
        for (int i = 0; i < fill; ++i) guess[i] = charset[indices[i]];
        guess[fill] = '\0';

        attempts++;

        if (strcmp(guess, pw) == 0) {
            // 猜中
            double elapsed = now_seconds() - start; // 花費時間
            double rate = (elapsed > 0.0) ? ((double) attempts / elapsed) : 0.0; // 速率
            log_progress(attempts, total_str, rate, !overflow, 1.0, 0); // 設定進度 -> 完成
            double avg_rate = (elapsed > 0.0) ? ((double) attempts / elapsed) : 0.0;
            char elapsed_str[64];
            format_duration(elapsed, elapsed_str, sizeof(elapsed_str));

            // 成功訊息
            printf("\n");
            printf("\x1b[33m███████╗██╗   ██╗ ██████╗ ██████╗███████╗███████╗███████╗\x1b[0m\n");
            printf("\x1b[33m██╔════╝██║   ██║██╔════╝██╔════╝██╔════╝██╔════╝██╔════╝\x1b[0m\n");
            printf("\x1b[33m███████╗██║   ██║██║     ██║     █████╗  ███████╗███████╗\x1b[0m\n");
            printf("\x1b[33m╚════██║██║   ██║██║     ██║     ██╔══╝  ╚════██║╚════██║\x1b[0m\n");
            printf("\x1b[33m███████║╚██████╔╝╚██████╗╚██████╗███████╗███████║███████║\x1b[0m\n");
            printf("\x1b[33m╚══════╝ ╚═════╝  ╚═════╝ ╚═════╝╚══════╝╚══════╝╚══════╝\x1b[0m\n");
            printf("\n");
            printf("\x1b[93mPassword found\x1b[97m: \x1b[1;92m%s\x1b[0m\n", guess);
            printf("\x1b[93mAttempts\x1b[97m: %llu\n", attempts);
            printf("\x1b[93mElapsed\x1b[97m: %s\n", elapsed_str);
            printf("\x1b[93mAverage rate\x1b[97m: %.0f attempts/s\x1b[0m\n", avg_rate);
            printf("\x1b[93mStart time\x1b[97m: %s\n", start_time_str);
            printf("================ \x1b[33mCrack-successful\x1b[0m ================\n");
            found = 1;
            break;
        }

        double now = now_seconds();
        double elapsed_since_start = now - start;
        if (now - last_report >= 1.0) {
            // > 1s
            double rate = (elapsed_since_start > 0.0) ? ((double) attempts / elapsed_since_start) : 0.0; // 計算速率
            int fraction_valid = !overflow;
            double fraction = 0.0;
            if (fraction_valid) fraction = (double) attempts / (double) total_needed;
            log_progress(attempts, total_str, rate, fraction_valid, fraction, 0);
            last_report = now;
        }

        if (!increment_counter(indices, fill, base)) break;
    }

    free(indices); // 釋放記憶體
    free(guess); // 釋放記憶體

    printf("\n");
    printf("Press Enter to continue...");
    fflush(stdout);
    getchar();
    return 0;
}
