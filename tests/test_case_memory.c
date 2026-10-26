/* tests/test_case_memory.c - Standalone Acceptance Suite for SQLite FTS5 Case Memory
 * Track: Best Business AI Agent | Dubai Create AI Agents Championship 2026-2027
 * Conforms strictly to ISO C99 (-std=c99 -Wall -Wextra -Werror -O2)
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "qala_case_memory.h"

int main(void) {
    printf("==================================================================\n");
    printf(" RUNNING QALA SQLITE FTS5 CASE MEMORY VERIFICATION SUITE\n");
    printf("==================================================================\n");

    /* Test 1: Open in-memory FTS5 database */
    printf("[TEST 1] Opening in-memory SQLite FTS5 Case Memory database...\n");
    qala_case_memory_t *cm = qala_case_memory_open(":memory:");
    assert(cm != NULL);
    printf("         [PASS] SQLite FTS5 database initialized successfully.\n");

    /* Test 2: Ingest the three benchmark cases from PLAN.md Round 2:
     * - Case 1: One approval
     * - Case 2: One rejection
     * - Case 3: One replay attempt
     */
    printf("[TEST 2] Inserting 3 benchmark decision cases (approval, rejection, replay)...\n");

    /* Case 1: Approval */
    int rc1 = qala_case_memory_insert(
        cm,
        1792600100ULL,
        "erp_customs_settlement",
        "SAFE",
        "policy_verified",
        "DP World Jebel Ali terminal clearance approved for container MSCU9921",
        125000.00
    );
    assert(rc1 == 0);

    /* Case 2: Rejection */
    int rc2 = qala_case_memory_insert(
        cm,
        1792600200ULL,
        "fs_read",
        "UNSAFE",
        "workspace_containment_denied",
        "Access to restricted path /workspace/../../etc/shadow blocked by sandbox",
        0.00
    );
    assert(rc2 == 0);

    /* Case 3: Replay attempt */
    int rc3 = qala_case_memory_insert(
        cm,
        1792600300ULL,
        "erp_customs_settlement",
        "UNSAFE",
        "invalid_or_expired_approval",
        "DP World Jebel Ali container MSCU9921 token re-used (replay attack blocked)",
        125000.00
    );
    assert(rc3 == 0);
    printf("         [PASS] 3 benchmark cases inserted into FTS5 index.\n");

    /* Test 3: Query "DP World" -> MUST return exactly the right two (Case 1 and Case 3) */
    printf("[TEST 3] Query 'DP World': must return approval and replay, exclude rejection...\n");
    qala_case_record_t results[10];
    int count = qala_case_memory_query(cm, "DP World", results, 10);
    assert(count == 2);

    /* Verify both returned cases cite DP World and their respective timestamps */
    bool found_approval = false;
    bool found_replay = false;
    for (int i = 0; i < count; ++i) {
        if (results[i].timestamp_utc == 1792600100ULL && strcmp(results[i].verdict, "SAFE") == 0) {
            found_approval = true;
            assert(strcmp(results[i].code, "policy_verified") == 0);
        }
        if (results[i].timestamp_utc == 1792600300ULL && strcmp(results[i].verdict, "UNSAFE") == 0) {
            found_replay = true;
            assert(strcmp(results[i].code, "invalid_or_expired_approval") == 0);
        }
        /* Verify Case 2 (/etc/shadow) is NEVER returned */
        assert(results[i].timestamp_utc != 1792600200ULL);
    }
    assert(found_approval && found_replay);
    printf("         [PASS] Query 'DP World' correctly returned exactly the 2 relevant cases.\n");

    /* Test 4: Query "shadow" -> MUST return Case 2 only */
    printf("[TEST 4] Query 'shadow': must return Case 2 only...\n");
    count = qala_case_memory_query(cm, "shadow", results, 10);
    assert(count == 1);
    assert(results[0].timestamp_utc == 1792600200ULL);
    assert(strcmp(results[0].verdict, "UNSAFE") == 0);
    assert(strcmp(results[0].code, "workspace_containment_denied") == 0);
    printf("         [PASS] Query 'shadow' isolated path containment violation.\n");

    /* Test 5: Query "125000" -> MUST return Cases 1 and 3 */
    printf("[TEST 5] Query '125000': must return both 125k transactions...\n");
    count = qala_case_memory_query(cm, "125000", results, 10);
    assert(count == 2);
    printf("         [PASS] Query '125000' matched both high-value transactions.\n");

    /* Test 6: Query non-existent entity */
    printf("[TEST 6] Query non-existent vendor -> must return 0 results...\n");
    count = qala_case_memory_query(cm, "NonExistentVendorX99", results, 10);
    assert(count == 0);
    printf("         [PASS] 0 results returned for unknown entity.\n");

    /* Test 7: Ingest from JSONL audit file + Deduplication check */
    printf("[TEST 7] Ingesting JSONL audit log file and verifying deduplication...\n");
    const char *test_file = "build/test_case_memory_audit.jsonl";
    FILE *fp = fopen(test_file, "w");
    assert(fp != NULL);
    fprintf(fp, "{\"timestamp_utc\":1793000001,\"tool\":\"customs/calculate_duty\",\"verdict\":\"SAFE\",\"code\":\"policy_verified\",\"reason\":\"Calculated Duty: AED 5000.00 on HS 8471.30\",\"amount_aed\":100000.00}\n");
    fprintf(fp, "{\"timestamp_utc\":1793000002,\"tool\":\"customs/clear_shipment\",\"verdict\":\"UNSAFE\",\"code\":\"invalid_or_expired_approval\",\"reason\":\"Clearance token invalid\",\"amount_aed\":150000.00}\n");
    fclose(fp);

    int new_records = qala_case_memory_index_file(cm, test_file);
    assert(new_records == 2);

    /* Re-index same file -> deduplication must add 0 records */
    int dup_records = qala_case_memory_index_file(cm, test_file);
    assert(dup_records == 0);

    /* Query newly indexed record */
    count = qala_case_memory_query(cm, "8471.30", results, 10);
    assert(count == 1);
    assert(results[0].timestamp_utc == 1793000001ULL);
    unlink(test_file);
    printf("         [PASS] File indexing and deduplication verified deterministically.\n");

    /* Test 8: Output formatting citing timestamp_utc */
    printf("[TEST 8] Formatted table generation with timestamp citations...\n");
    char formatted_buf[2048];
    count = qala_case_memory_query(cm, "DP World", results, 10);
    int fmt_len = qala_case_memory_format_results("DP World", results, (size_t)count, formatted_buf, sizeof(formatted_buf));
    assert(fmt_len > 0);
    assert(strstr(formatted_buf, "1792600100") != NULL);
    assert(strstr(formatted_buf, "1792600300") != NULL);
    assert(strstr(formatted_buf, "SAFE") != NULL);
    assert(strstr(formatted_buf, "UNSAFE") != NULL);
    printf("         [PASS] Formatted output citations verified:\n%s\n", formatted_buf);

    qala_case_memory_close(cm);
    printf("==================================================================\n");
    printf(" [SUCCESS] All 8 SQLite FTS5 Case Memory Tests Passed!\n");
    printf("==================================================================\n");
    return 0;
}
