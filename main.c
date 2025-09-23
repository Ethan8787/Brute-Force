#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <windows.h>

const char *charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static double now_seconds() {
    return (double) clock() / (double) CLOCKS_PER_SEC;
}

static void log_info(const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm *tmnow = localtime(&t);

    char timestr[16];
    strftime(timestr, sizeof(timestr), "%H:%M:%S", tmnow);

    printf("[%s INFO]: ", timestr);

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    fflush(stdout);
}

static void EnableVTMode(void) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN;
    SetConsoleMode(hOut, dwMode);
}

static void format_commas_unsigned(unsigned long long v, char *buf, size_t bufsz) {
    if (bufsz == 0) return;
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

static void log_progress(unsigned long long attempts, const char *total_str,
                         double rate, int fraction_valid, double fraction, int final) {
    time_t t = time(NULL);
    struct tm *tmnow = localtime(&t);
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

int main(void) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    EnableVTMode();

    char pw[256];
    printf("\n");
    printf("\x1b[94m███████╗████████╗██╗  ██╗ █████╗ ███╗   ██╗\x1b[0m\n");
    printf("\x1b[94m██╔════╝╚══██╔══╝██║  ██║██╔══██╗████╗  ██║\x1b[0m\n");
    printf("\x1b[94m█████╗     ██║   ███████║███████║██╔██╗ ██║\x1b[0m\n");
    printf("\x1b[94m██╔══╝     ██║   ██╔══██║██╔══██║██║╚██╗██║\x1b[0m\n");
    printf("\x1b[94m███████╗   ██║   ██║  ██║██║  ██║██║ ╚████║\x1b[0m\n");
    printf("\x1b[94m╚══════╝   ╚═╝   ╚═╝  ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝\x1b[0m\n");
    printf("\n");
    printf("================ \x1b[33mBrute-Force\x1b[0m ================\n");
    printf("\x1b[93mEnter the target password\x1b[97m:\n> ");

    fflush(stdout);

    if (!fgets(pw, sizeof(pw), stdin))
        return 1;
    size_t ln = strlen(pw);
    if (ln > 0 && pw[ln - 1] == '\n') pw[ln - 1] = '\0';

    int target_len = (int) strlen(pw);
    int fill = target_len;
    if (fill <= 0) {
        printf("No password entered. Press Enter to continue...");
        getchar();
        return 0;
    }

    unsigned long long attempts = 0ULL;
    double start = now_seconds();
    double last_report = start;
    int base = (int) strlen(charset);

    int *indices = malloc(sizeof(int) * fill);
    for (int i = 0; i < fill; ++i) indices[i] = 0;
    char *guess = malloc(fill + 1);
    int found = 0;

    unsigned long long total_needed = 1ULL;
    int overflow = 0;
    for (int i = 0; i < fill; ++i) {
        if (total_needed > ULLONG_MAX / (unsigned long long) base) {
            overflow = 1;
            break;
        }
        total_needed *= (unsigned long long) base;
    }
    char total_str[64];
    if (!overflow) snprintf(total_str, sizeof(total_str), "%llu", total_needed);
    else snprintf(total_str, sizeof(total_str), "Extremely large");

    while (1) {
        for (int i = 0; i < fill; ++i) guess[i] = charset[indices[i]];
        guess[fill] = '\0';

        attempts++;

        if (strcmp(guess, pw) == 0) {
            double elapsed = now_seconds() - start;
            double rate = (elapsed > 0.0) ? ((double) attempts / elapsed) : 0.0;
            log_progress(attempts, total_str, rate, !overflow, 1.0, 0);

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
            printf("\x1b[93mAttempts\x1b[97m: %llu\n\x1b[93mElapsed time\x1b[97m: %.2fs\x1b[0m\n", attempts, elapsed);
            printf("================ \x1b[33mCrack-successful\x1b[0m ================\n");
            found = 1;
            break;
        }

        double now = now_seconds();
        double elapsed_since_start = now - start;
        if (now - last_report >= 1.0) {
            double rate = (elapsed_since_start > 0.0) ? ((double) attempts / elapsed_since_start) : 0.0;
            int fraction_valid = !overflow;
            double fraction = 0.0;
            if (fraction_valid) fraction = (double) attempts / (double) total_needed;
            log_progress(attempts, total_str, rate, fraction_valid, fraction, 0);
            last_report = now;
        }

        if (!increment_counter(indices, fill, base)) break;
    }

    if (!found) {
        double elapsed = now_seconds() - start;
        printf("\n");
        printf("\n");
        printf("\x1b[31m███████╗ █████╗ ██╗██╗     ███████╗██████╗ \x1b[0m\n");
        printf("\x1b[31m██╔════╝██╔══██╗██║██║     ██╔════╝██╔══██╗\x1b[0m\n");
        printf("\x1b[31m█████╗  ███████║██║██║     █████╗  ██║  ██║\x1b[0m\n");
        printf("\x1b[31m██╔══╝  ██╔══██║██║██║     ██╔══╝  ██║  ██║\x1b[0m\n");
        printf("\x1b[31m██║     ██║  ██║██║███████╗███████╗██████╔╝\x1b[0m\n");
        printf("\x1b[31m╚═╝     ╚═╝  ╚═╝╚═╝╚══════╝╚══════╝╚═════╝ \x1b[0m\n");
        printf("\n");
        log_info("Invalid character. \nTried: %llu\nTime elapsed: %.2fs\n", attempts, elapsed);
    }
    {
        double elapsed_since_start = now_seconds() - start;
        double rate = (elapsed_since_start > 0.0) ? ((double) attempts / elapsed_since_start) : 0.0;
        int fraction_valid = !overflow;
        double fraction = 0.0;
        if (fraction_valid) fraction = (double) attempts / (double) total_needed;
        log_progress(attempts, total_str, rate, fraction_valid, fraction, 1);
        printf("\n");
    }

    free(indices);
    free(guess);

    printf("Press Enter to continue...");
    fflush(stdout);
    getchar();
    return 0;
}
