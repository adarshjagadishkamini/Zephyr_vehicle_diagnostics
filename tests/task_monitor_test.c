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
        record_control_flow_point(CHECKPOINT_TASK_START);
        k_sleep(K_MSEC(50));
        record_control_flow_point(CHECKPOINT_TASK_END);
    }
}

static void *test_setup(void) {
    // Initialize task monitoring
    task_monitor_init();

    // Create test thread
    test_thread_id = k_thread_create(&test_thread, test_stack,
                                   TEST_THREAD_STACK_SIZE,
                                   test_task_entry,
                                   NULL, NULL, NULL,
                                   TEST_THREAD_PRIORITY, 0, K_NO_WAIT);
                                   
    // Register thread for monitoring
    register_monitored_task(test_thread_id, MAX_EXECUTION_TIME_MS);
    return NULL;
}

static void test_cleanup(void *data) {
    k_thread_abort(test_thread_id);
}

ZTEST_SUITE(task_monitor_tests, NULL, test_setup, NULL, NULL, test_cleanup);

// Test deadline monitoring
ZTEST(task_monitor_tests, test_deadline_monitoring)
{
    // Normal execution
    k_sleep(K_MSEC(75));
    zassert_true(check_task_deadlines(), "Valid task execution marked as deadline miss");

    // Simulate deadline miss
    k_sleep(K_MSEC(MAX_EXECUTION_TIME_MS + 50));
    zassert_false(check_task_deadlines(), "Deadline miss not detected");
    
    // Verify error handling
    struct runtime_stats stats;
    get_task_statistics(test_thread_id, &stats);
    zassert_true(stats.deadline_misses > 0, "Deadline miss not recorded in statistics");
}

// Test control flow monitoring
ZTEST(task_monitor_tests, test_control_flow)
{
    // Reset control flow sequence
    reset_control_flow_monitoring();
    
    // Verify normal sequence
    record_control_flow_point(CHECKPOINT_INIT);
    record_control_flow_point(CHECKPOINT_TASK_START);
    record_control_flow_point(CHECKPOINT_TASK_END);
    
    zassert_true(verify_control_flow(), "Valid control flow marked as violation");
    
    // Test invalid sequence
    record_control_flow_point(CHECKPOINT_TASK_END); // Out of order
    zassert_false(verify_control_flow(), "Control flow violation not detected");
    
    // Verify statistics
    struct runtime_stats stats;
    get_task_statistics(test_thread_id, &stats);
    zassert_true(stats.control_flow_violations > 0, 
                 "Control flow violation not recorded");
}

// Test stack monitoring
ZTEST(task_monitor_tests, test_stack_monitoring)
{
    // Get initial stack usage
    struct runtime_stats stats;
    get_task_statistics(test_thread_id, &stats);
    uint32_t initial_stack_usage = stats.stack_usage;
    
    // Perform some stack-intensive operations in test thread
    // This will be monitored by the stack usage tracking
    k_sleep(K_MSEC(200));
    
    // Check updated stack usage
    get_task_statistics(test_thread_id, &stats);
    zassert_true(stats.stack_usage >= initial_stack_usage,
                 "Stack usage monitoring failed");
                 
    // Verify stack overflow detection
    zassert_false(stats.stack_overflow,
                 "False stack overflow detection");
}

// Test runtime monitoring
ZTEST(task_monitor_tests, test_runtime_monitoring)
{
    struct runtime_stats initial_stats;
    get_task_statistics(test_thread_id, &initial_stats);
    
    // Let task run for a while
    k_sleep(K_MSEC(500));
    
    struct runtime_stats current_stats;
    get_task_statistics(test_thread_id, &current_stats);
    
    // Verify runtime accumulation
    zassert_true(current_stats.total_runtime > initial_stats.total_runtime,
                 "Runtime not accumulated");
    
    // Verify execution count
    zassert_true(current_stats.execution_count > initial_stats.execution_count,
                 "Execution count not incremented");
}

// Test error recovery
ZTEST(task_monitor_tests, test_error_recovery)
{
    // Simulate multiple deadline misses
    for (int i = 0; i < 3; i++) {
        k_sleep(K_MSEC(MAX_EXECUTION_TIME_MS + 50));
        check_task_deadlines();
    }
    
    struct runtime_stats stats;
    get_task_statistics(test_thread_id, &stats);
    
    // Verify degraded mode transition
    zassert_equal(get_safety_state(), SAFETY_STATE_DEGRADED,
                 "Failed to transition to degraded state");
    
    // Test recovery
    reset_task_monitoring(test_thread_id);
    k_sleep(K_MSEC(75)); // Normal execution
    
    // Verify recovery
    zassert_equal(get_safety_state(), SAFETY_STATE_NORMAL,
                 "Failed to recover from degraded state");
}

// Test load monitoring
ZTEST(task_monitor_tests, test_load_monitoring)
{
    struct runtime_stats stats;
    
    // Monitor CPU load over time
    k_sleep(K_MSEC(1000));
    get_task_statistics(test_thread_id, &stats);
    
    // Verify CPU load calculation
    zassert_true(stats.cpu_load >= 0 && stats.cpu_load <= 100,
                 "Invalid CPU load calculation");
    
    // Test overload detection
    bool overload = is_cpu_overloaded();
    if (overload) {
        zassert_true(stats.cpu_load > 80,
                     "Incorrect overload detection");
    }
}

// Test multi-task interference
ZTEST(task_monitor_tests, test_task_interference)
{
    struct runtime_stats stats_before, stats_after;
    get_task_statistics(test_thread_id, &stats_before);
    
    // Create high-priority interfering task
    k_thread_create(&test_thread, test_stack,
                   TEST_THREAD_STACK_SIZE,
                   test_task_entry,
                   NULL, NULL, NULL,
                   TEST_THREAD_PRIORITY - 1, 0, K_NO_WAIT);
                   
    k_sleep(K_MSEC(500));
    get_task_statistics(test_thread_id, &stats_after);
    
    // Verify interference detection
    zassert_true(stats_after.interference_count > stats_before.interference_count,
                 "Task interference not detected");
}