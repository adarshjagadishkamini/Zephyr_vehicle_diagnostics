#include <zephyr/ztest.h>
#include "task_monitor.h"
#include "error_handler.h"
#include "asil.h"
#include "control_flow.h"
#include "runtime_stats.h"

#define TEST_THREAD_STACK_SIZE 1024
#define TEST_THREAD_PRIORITY 5
#define MAX_EXECUTION_TIME_MS 100

static K_THREAD_STACK_DEFINE(test_stack, TEST_THREAD_STACK_SIZE);
static struct k_thread test_thread;
static uint32_t test_thread_id;

static void test_task_entry(void *p1, void *p2, void *p3) {
    while (1) {
        k_sleep(K_MSEC(10));
    }
}

static void busy_task_entry(void *p1, void *p2, void *p3) {
    volatile int i = 0;
    while (1) {
        i++; // Consume CPU
    }
}

static void *test_setup(void) {
    task_monitor_init();

    struct task_config config = {
        .name = "test_task",
        .deadline_ms = MAX_EXECUTION_TIME_MS,
        .max_runtime_ms = MAX_EXECUTION_TIME_MS,
        .min_stack_remaining = 256,
        .is_critical = false
    };

    test_thread_id = k_thread_create(&test_thread, test_stack,
                                   TEST_THREAD_STACK_SIZE,
                                   test_task_entry,
                                   NULL, NULL, NULL,
                                   TEST_THREAD_PRIORITY, 0, K_NO_WAIT);

    register_monitored_task(&config);
    return NULL;
}

static void test_cleanup(void *data) {
    k_thread_abort(test_thread_id);
}

ZTEST_SUITE(task_monitor_tests, NULL, test_setup, NULL, NULL, test_cleanup);

// Test deadline monitoring
ZTEST(task_monitor_tests, test_deadline_monitoring) {
    // Normal execution
    k_sleep(K_MSEC(75));
    zassert_true(check_task_deadlines(), "Valid task execution marked as deadline miss");

    // Simulate deadline miss
    k_sleep(K_MSEC(MAX_EXECUTION_TIME_MS + 50));
    zassert_false(check_task_deadlines(), "Deadline miss not detected");
    
    // Verify error handling
    struct task_statistics stats;
    get_task_statistics("test_task", &stats);
    zassert_true(stats.deadline_misses > 0, "Deadline miss not recorded in statistics");
}

// Test control flow monitoring
ZTEST(task_monitor_tests, test_control_flow) {
    // Reset control flow sequence
    reset_task_monitoring("test_task");
    
    // Verify normal sequence
    uint32_t checkpoints[] = {1, 2, 3, 4};
    for (int i = 0; i < 4; i++) {
        task_monitor_checkpoint("test_task");
        k_sleep(K_MSEC(10));
    }
    
    struct task_statistics stats;
    get_task_statistics("test_task", &stats);
    zassert_equal(stats.control_flow_violations, 0, 
                  "Valid control flow marked as violation");

    // Test invalid sequence by skipping checkpoint
    task_monitor_checkpoint("test_task");
    k_sleep(K_MSEC(MAX_EXECUTION_TIME_MS + 10));
    task_monitor_checkpoint("test_task");
    
    get_task_statistics("test_task", &stats);
    zassert_true(stats.control_flow_violations > 0, 
                 "Control flow violation not detected");
}

// Test stack monitoring
ZTEST(task_monitor_tests, test_stack_monitoring) {
    struct task_statistics stats_before;
    get_task_statistics("test_task", &stats_before);
    
    // Recursively consume stack
    volatile uint8_t array[512];
    for (int i = 0; i < 512; i++) {
        array[i] = i;
    }
    k_sleep(K_MSEC(10));
    
    struct task_statistics stats_after;
    get_task_statistics("test_task", &stats_after);
    
    zassert_true(stats_after.stack_usage_peak > stats_before.stack_usage_peak,
                 "Stack usage increase not detected");
}

// Test runtime monitoring
ZTEST(task_monitor_tests, test_runtime_monitoring) {
    struct task_statistics stats_before;
    get_task_statistics("test_task", &stats_before);
    
    // Run CPU intensive task
    K_THREAD_STACK_DEFINE(busy_stack, 1024);
    k_tid_t busy_tid = k_thread_create(&test_thread, busy_stack,
                                     1024, busy_task_entry,
                                     NULL, NULL, NULL,
                                     TEST_THREAD_PRIORITY - 1, 0, K_NO_WAIT);
    k_sleep(K_MSEC(500));
    k_thread_abort(busy_tid);
    
    struct task_statistics stats_after;
    get_task_statistics("test_task", &stats_after);
    
    zassert_true(stats_after.total_runtime_ms > stats_before.total_runtime_ms,
                 "Runtime tracking not working");
    zassert_true(stats_after.cpu_usage_percent > stats_before.cpu_usage_percent,
                 "CPU usage tracking not working");
}

// Test task interference
ZTEST(task_monitor_tests, test_task_interference) {
    struct task_statistics stats_before;
    get_task_statistics("test_task", &stats_before);
    
    // Create multiple interfering tasks
    K_THREAD_STACK_DEFINE(interfere_stack1, 1024);
    K_THREAD_STACK_DEFINE(interfere_stack2, 1024);
    
    k_tid_t t1 = k_thread_create(&test_thread, interfere_stack1,
                                1024, busy_task_entry,
                                NULL, NULL, NULL,
                                TEST_THREAD_PRIORITY - 1, 0, K_NO_WAIT);
                                
    k_tid_t t2 = k_thread_create(&test_thread, interfere_stack2,  
                                1024, busy_task_entry,
                                NULL, NULL, NULL,
                                TEST_THREAD_PRIORITY - 1, 0, K_NO_WAIT);
    
    k_sleep(K_MSEC(200));
    
    struct task_statistics stats_after;
    get_task_statistics("test_task", &stats_after);
    
    zassert_true(stats_after.interference_count > stats_before.interference_count,
                 "Task interference not detected");
    
    k_thread_abort(t1);
    k_thread_abort(t2);
    
    // Verify recovery
    reset_task_monitoring("test_task");
    struct task_statistics stats_recovery;
    get_task_statistics("test_task", &stats_recovery);
    zassert_equal(stats_recovery.interference_count, 0,
                 "Interference count not reset after recovery");
}