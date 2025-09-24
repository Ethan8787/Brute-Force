#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <windows.h>

 // 字元集
const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

 // 回傳秒數 計算速率與時間
static double now_seconds() {
    return (double) clock() / (double) CLOCKS_PER_SEC;
}

 // Minecraft 格式時間戳
static void log_info(const char *fmt, ...) {
    const time_t t = time(NULL);
    const struct tm *tmnow = localtime(&t);
    char timestr[16];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", tmnow);
    printf("[%s INFO]: ", timestr);

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
    fflush(stdout);
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
    long s = (long)(secs + 0.5);
    const long h = s / 3600; s %= 3600;
    const long m = s / 60;  s %= 60;

    if (h > 0) {
        snprintf(buf, bufsz, "%ldh %ldmin", h, m);
    } else if (m > 0) {
        if (s == 0) snprintf(buf, bufsz, "%ldmin");
        else        snprintf(buf, bufsz, "%ldmin %lds", m, s);
    } else {
        snprintf(buf, bufsz, "%lds", s);
    }
}

 // 數字格式化 (1,000,000)
static void format_commas_unsigned(unsigned long long v, char *buf, size_t bufsz) {
    char tmp[64];
    int pos = 0;
    if (v == 0) {
        snprintf(buf, bufsz, "0");
        return;
    }
    while (v > 0 && pos + 1 < (int) sizeof(tmp)) {
        tmp[pos++] = '0' + (v % 10);
        v /= 10;
    }
    char out[96];
    int outpos = 0;
    for (int i = 0; i < pos; ++i) {
        if (i > 0 && (i % 3) == 0) out[outpos++] = ',';
        out[outpos++] = tmp[i];
    }
    for (int i = 0; i < outpos / 2; ++i) {
        char c = out[i];
        out[i] = out[outpos - 1 - i];
        out[outpos - 1 - i] = c;
    }
    out[outpos] = '\0';
    strncpy(buf, out, bufsz - 1);
    buf[bufsz - 1] = '\0';
}

 // 進度條
static void log_progress(const unsigned long long attempts, const char *total_str,
                         const double rate, const int fraction_valid, double fraction, const int final) {
    const time_t t = time(NULL);
    const struct tm *tmnow = localtime(&t);
    char timestr[16];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", tmnow);

    char tps_s[64];
    unsigned long long tps_int = (unsigned long long)(rate + 0.5);
    format_commas_unsigned(tps_int, tps_s, sizeof(tps_s));

    const int BAR_LEN = 20;
    int filled = 0;
    if (fraction_valid) {
        if (fraction < 0.0) fraction = 0.0;
        if (fraction > 1.0) fraction = 1.0;
        filled = (int)(fraction * (double)BAR_LEN + 0.5);
        if (filled < 0) filled = 0;
        if (filled > BAR_LEN) filled = BAR_LEN;
    } else {
        filled = (int)(attempts % (unsigned long long)(BAR_LEN + 1));
    }

    if (final) return;

    printf("\r[%s INFO]: \x1b[93mProgress\x1b[97m: [", timestr);
    for (int i = 0; i < filled; ++i)       printf("\x1b[92m█\x1b[0m");
    for (int i = filled; i < BAR_LEN; ++i) printf("\x1b[90m█\x1b[0m");
    printf("] ");
    fflush(stdout);
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
    printf("\x1b[94m███████╗████████╗██╗  ██╗ █████╗ ███╗   ██╗\x1b[0m\n");
    printf("\x1b[94m██╔════╝╚══██╔══╝██║  ██║██╔══██╗████╗  ██║\x1b[0m\n");
    printf("\x1b[94m█████╗     ██║   ███████║███████║██╔██╗ ██║\x1b[0m\n");
    printf("\x1b[94m██╔══╝     ██║   ██╔══██║██╔══██║██║╚██╗██║\x1b[0m\n");
    printf("\x1b[94m███████╗   ██║   ██║  ██║██║  ██║██║ ╚████║\x1b[0m\n");
    printf("\x1b[94m╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝\x1b[0m\n");
    printf("\n");
    printf("================ \x1b[33mBrute-Force\x1b[0m ================\n");
    printf("\x1b[93mEnter the target password\x1b[97m:\n> "); // 提示使用者輸入密碼

    fflush(stdout);

    if (!fgets(pw, sizeof(pw), stdin)) // 讀取輸入
        return 1;
    const size_t ln = strlen(pw); // 輸入長度
    if (ln > 0 && pw[ln - 1] == '\n') pw[ln - 1] = '\0'; // 移除換行符號

    const int target_len = (int) strlen(pw);
    const int fill = target_len;
    if (fill <= 0) {
        printf("No password entered. Press Enter to continue...");
        getchar();
        return 0;
    }

    unsigned long long attempts = 0ULL; // 已嘗試
    const double start = now_seconds(); // 記錄時間
    double last_report = start; // 上次時間
    const int base = (int) strlen(charset); // char 大小

    int *indices = malloc(sizeof(int) * fill); // 配置陣列
    for (int i = 0; i < fill; ++i) indices[i] = 0; // 初始化
    char *guess = malloc(fill + 1); // 存放目前字串
    int found = 0; // 是否找到密碼的旗標（0 = 尚未找到）

    unsigned long long total_needed = 1ULL; // 總需嘗試
    int overflow = 0;
    for (int i = 0; i < fill; ++i) {
        if (total_needed > ULLONG_MAX / (unsigned long long) base) {
            overflow = 1;
            break;
        }
        total_needed *= (unsigned long long) base; // 總組合數 = base^fill
    }
    char total_str[64];
    if (!overflow) snprintf(total_str, sizeof(total_str), "%llu", total_needed);
    else snprintf(total_str, sizeof(total_str), "Extremely large");

    while (1) {
        for (int i = 0; i < fill; ++i) guess[i] = charset[indices[i]];
        guess[fill] = '\0';

        attempts++; // 嘗試次數+1

        if (strcmp(guess, pw) == 0) { // 猜中
            double elapsed = now_seconds() - start; // 花費時間
            double rate = (elapsed > 0.0) ? ((double) attempts / elapsed) : 0.0; // 速率
            log_progress(attempts, total_str, rate, !overflow, 1.0, 0); // 設定進度 -> 完成
            double avg_rate = (elapsed > 0.0) ? ((double)attempts / elapsed) : 0.0;
            char elapsed_str[64];
            format_duration(elapsed, elapsed_str, sizeof(elapsed_str));

            // 成功訊息
            printf("\n");
            printf("\n");
            printf("\x1b[33m███████╗██╗   ██╗ ██████╗ ██████╗███████╗███████╗███████╗\x1b[0m\n");
            printf("\x1b[33m██╔════╝██║   ██║██╔════╝██╔════╝██╔════╝██╔════╝██╔════╝\x1b[0m\n");
            printf("\x1b[33m███████╗██║   ██║██║     ██║     █████╗  ███████╗███████╗\x1b[0m\n");
            printf("\x1b[33m╚════██║██║   ██║██║     ██║     ██╔══╝  ╚════██║╚════██║\x1b[0m\n");
            printf("\x1b[33m███████║╚██████╔╝╚██████╗╚██████╗███████╗███████║███████║\x1b[0m\n");
            printf("\x1b[33m╚══════╝ ╚═════╝  ╚═════╝ ╚═════╝╚══════╝╚══════╝╚══════╝\x1b[0m\n");
            printf("\n");
            printf("\x1b[93mPassword found\x1b[97m: \x1b[1;97m%s\x1b[0m\n", guess);
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
        if (now - last_report >= 1.0) { // > 1s
            double rate = (elapsed_since_start > 0.0) ? ((double) attempts / elapsed_since_start) : 0.0; // 計算速率
            int fraction_valid = !overflow;
            double fraction = 0.0;
            if (fraction_valid) fraction = (double) attempts / (double) total_needed;
            log_progress(attempts, total_str, rate, fraction_valid, fraction, 0);
            last_report = now;
        }

        if (!increment_counter(indices, fill, base)) break;
    }

    if (!found) {
        // 失敗訊息
        printf("\n");
        printf("\n");
        printf("\x1b[31m███████╗ █████╗ ██╗██╗     ███████╗██████╗ \x1b[0m\n");
        printf("\x1b[31m██╔════╝██╔══██╗██║██║     ██╔════╝██╔══██╗\x1b[0m\n");
        printf("\x1b[31m█████╗  ███████║██║██║     █████╗  ██║  ██║\x1b[0m\n");
        printf("\x1b[31m██╔══╝  ██╔══██║██║██║     ██╔══╝  ██║  ██║\x1b[0m\n");
        printf("\x1b[31m██║     ██║  ██║██║███████╗███████╗██████╔╝\x1b[0m\n");
        printf("\x1b[31m╚═╝     ╚═╝  ╚═╝╚═╝╚══════╝╚══════╝╚═════╝ \x1b[0m\n");
        printf("\n");
        log_info("Invalid character.");
    }
    {
        double elapsed_since_start = now_seconds() - start;
        double rate = (elapsed_since_start > 0.0) ? ((double) attempts / elapsed_since_start) : 0.0; // 速率
        int fraction_valid = !overflow;
        double fraction = 0.0;
        if (fraction_valid) fraction = (double) attempts / (double) total_needed;
        log_progress(attempts, total_str, rate, fraction_valid, fraction, 1);
        printf("\n");
    }

    free(indices); // 釋放記憶體
    free(guess); // 釋放記憶體

    printf("Press Enter to continue...");
    fflush(stdout);
    getchar();
    return 0;
}
