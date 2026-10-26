/* src/qala_case_memory.h - SQLite FTS5 Case Memory & Decision History Index
 * Dubai Create AI Agents Championship 2026-2027 | Best Business AI Agent
 * Conforms strictly to ISO C99 (-std=c99 -Wall -Wextra -Werror -O2)
 */

#ifndef QALA_CASE_MEMORY_H
#define QALA_CASE_MEMORY_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QALA_MAX_CASE_QUERY_RESULTS 64

/* Structured representation of a retrieved historical decision case */
typedef struct {
    uint64_t timestamp_utc;
    char     tool[64];
    char     verdict[32];
    char     code[64];
    char     reason[512];
    double   amount_aed;
    double   rank_score;   /* FTS5 BM25 relevance score */
} qala_case_record_t;

/* Opaque Case Memory database handle */
typedef struct qala_case_memory qala_case_memory_t;

/* Open or initialize SQLite FTS5 case memory.
 * Pass NULL or ":memory:" for transient in-memory database.
 * Returns pointer to handle on success, NULL on error.
 */
qala_case_memory_t *qala_case_memory_open(const char *db_path);

/* Close database handle and free all internal resources */
void qala_case_memory_close(qala_case_memory_t *cm);

/* Ingest an immutable append-only JSONL audit ledger file into FTS5 index.
 * Deduplicates automatically. Returns count of new records indexed, or <0 on error.
 */
int qala_case_memory_index_file(qala_case_memory_t *cm, const char *audit_log_path);

/* Insert a single decision record directly into the FTS5 index.
 * Returns 0 on success, non-zero on error.
 */
int qala_case_memory_insert(
    qala_case_memory_t *cm,
    uint64_t           timestamp_utc,
    const char        *tool,
    const char        *verdict,
    const char        *code,
    const char        *reason,
    double             amount_aed
);

/* Query prior decision cases using SQLite FTS5 BM25 ranked full-text search.
 * Searches across tool, verdict, code, reason, and transaction amounts.
 * Returns number of matching cases found (up to max_results).
 */
int qala_case_memory_query(
    qala_case_memory_t *cm,
    const char         *query_str,
    qala_case_record_t *out_results,
    size_t              max_results
);

/* Formats retrieved cases into a clean human-readable table citing timestamp_utc.
 * Returns number of bytes formatted, or <0 on error.
 */
int qala_case_memory_format_results(
    const char               *query_str,
    const qala_case_record_t *results,
    size_t                    count,
    char                     *out_buf,
    size_t                    buf_cap
);

#ifdef __cplusplus
}
#endif

#endif /* QALA_CASE_MEMORY_H */
