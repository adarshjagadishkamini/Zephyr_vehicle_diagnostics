#ifndef RUNTIME_STATS_H
#define RUNTIME_STATS_H

#include <zephyr/kernel.h>

struct runtime_stats {
    uint64_t start_time;
    uint64_t total_runtime;
    uint32_t execution_count;
    uint32_t max_execution_time;
    uint32_t min_execution_time;
    uint32_t avg_execution_time;
    uint32_t deadline_misses;
};

void runtime_stats_init(void);
void runtime_stats_start_task(const char *task_name);
void runtime_stats_end_task(const char *task_name);
void runtime_stats_get(const char *task_name, struct runtime_stats *stats);
void runtime_stats_report(void);

#endif /* RUNTIME_STATS_H */
