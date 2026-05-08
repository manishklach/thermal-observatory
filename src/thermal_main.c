#define _GNU_SOURCE
#include "../include/thermal_monitor.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t keep_running = 1;

static void handle_signal(int signo) {
    (void)signo;
    keep_running = 0;
}

int tm_collect_snapshot(tm_context_t *ctx, tm_snapshot_t *snap) {
    memset(snap, 0, sizeof(*snap));
    snap->arch = ctx->arch;
    clock_gettime(CLOCK_REALTIME, &snap->timestamp);

    tm_collect_generic_linux(ctx, snap);
    if (ctx->arch == TM_ARCH_X86_64) {
        tm_collect_x86(ctx, snap);
    } else if (ctx->arch == TM_ARCH_ARM64) {
        tm_collect_arm64(ctx, snap);
    }
    tm_collect_nvidia(ctx, snap);
    tm_collect_nvidia_cuda(ctx, snap);
    tm_collect_nvidia_dcgm(ctx, snap);
    tm_collect_amd(ctx, snap);
    tm_collect_ipmi(ctx, snap);
    tm_collect_redfish(ctx, snap);
    return 0;
}

int main(int argc, char **argv) {
    tm_context_t ctx;
    tm_snapshot_t snap;
    int watch = 0;
    int interval = 1;
    int want_prometheus = 0;
    const char *prometheus_textfile = NULL;

    tm_context_init(&ctx);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0) {
            ctx.want_json = true;
        } else if (strcmp(argv[i], "--prometheus") == 0) {
            want_prometheus = 1;
        } else if (strcmp(argv[i], "--prometheus-textfile") == 0 && i + 1 < argc) {
            prometheus_textfile = argv[++i];
        } else if (strcmp(argv[i], "--watch") == 0 || strcmp(argv[i], "-w") == 0) {
            watch = 1;
        } else if (strcmp(argv[i], "--interval") == 0 && i + 1 < argc) {
            interval = atoi(argv[++i]);
            if (interval < 1) {
                interval = 1;
            }
        } else if (strcmp(argv[i], "--experimental-kernel") == 0) {
            ctx.want_experimental_kernel = true;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: %s [--json] [--prometheus] [--prometheus-textfile PATH] [--watch] [--interval N] [--experimental-kernel]\n", argv[0]);
            return 0;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    do {
        char json_buf[65536];
        char prom_buf[65536];

        tm_collect_snapshot(&ctx, &snap);
        if (prometheus_textfile) {
            FILE *fp = fopen(prometheus_textfile, "w");
            if (!fp) {
                perror("fopen");
                return 1;
            }
            if (tm_snapshot_to_prometheus(&snap, prom_buf, sizeof(prom_buf)) >= 0) {
                fputs(prom_buf, fp);
            }
            fclose(fp);
        } else if (want_prometheus) {
            if (tm_snapshot_to_prometheus(&snap, prom_buf, sizeof(prom_buf)) >= 0) {
                fputs(prom_buf, stdout);
            }
        } else if (ctx.want_json) {
            if (tm_snapshot_to_json(&snap, json_buf, sizeof(json_buf)) >= 0) {
                puts(json_buf);
            }
        } else {
            tm_print_snapshot_text(&snap);
        }

        if (watch && keep_running) {
            sleep((unsigned int)interval);
        }
    } while (watch && keep_running);

    return 0;
}
