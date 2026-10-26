/* src/qala_case_memory.c - SQLite FTS5 Case Memory & Decision History Index
 * Dubai Create AI Agents Championship 2026-2027 | Best Business AI Agent
 * Conforms strictly to ISO C99 (-std=c99 -Wall -Wextra -Werror -O2)
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <ctype.h>
#include <sqlite3.h>

#include "qala_case_memory.h"
#include "cJSON.h"

struct qala_case_memory {
    sqlite3 *db;
    char     db_path[512];
};

qala_case_memory_t *qala_case_memory_open(const char *db_path) {
    const char *path = (db_path && db_path[0] != '\0') ? db_path : ":memory:";
    sqlite3 *db = NULL;
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK || !db) {
        if (db) sqlite3_close(db);
        return NULL;
    }

    /* Initialize deduplication store and FTS5 full-text search table */
    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS case_dedup ("
        "  sig TEXT PRIMARY KEY"
        ");"
        "CREATE VIRTUAL TABLE IF NOT EXISTS cases_fts USING fts5("
        "  timestamp_utc,"
        "  tool,"
        "  verdict,"
        "  code,"
        "  reason,"
        "  amount_aed,"
        "  tokenize='unicode61'"
        ");";

    char *err = NULL;
    rc = sqlite3_exec(db, schema_sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return NULL;
    }

    qala_case_memory_t *cm = (qala_case_memory_t *)calloc(1, sizeof(qala_case_memory_t));
    if (!cm) {
        sqlite3_close(db);
        return NULL;
    }
    cm->db = db;
    snprintf(cm->db_path, sizeof(cm->db_path), "%s", path);
    return cm;
}

void qala_case_memory_close(qala_case_memory_t *cm) {
    if (!cm) return;
    if (cm->db) {
        sqlite3_close(cm->db);
        cm->db = NULL;
    }
    free(cm);
}

int qala_case_memory_insert(
    qala_case_memory_t *cm,
    uint64_t           timestamp_utc,
    const char        *tool,
    const char        *verdict,
    const char        *code,
    const char        *reason,
    double             amount_aed
) {
    if (!cm || !cm->db) return -1;

    const char *safe_tool = (tool && tool[0] != '\0') ? tool : "unknown";
    const char *safe_verdict = (verdict && verdict[0] != '\0') ? verdict : "UNKNOWN";
    const char *safe_code = (code && code[0] != '\0') ? code : "unknown";
    const char *safe_reason = (reason && reason[0] != '\0') ? reason : "none";
    double safe_amount = isfinite(amount_aed) ? amount_aed : 0.0;

    /* Check deduplication signature */
    char sig[1024];
    snprintf(sig, sizeof(sig), "%llu|%s|%s|%s|%s|%.2f",
             (unsigned long long)timestamp_utc, safe_tool, safe_verdict, safe_code, safe_reason, safe_amount);

    sqlite3_stmt *stmt_check = NULL;
    int rc = sqlite3_prepare_v2(cm->db, "INSERT OR IGNORE INTO case_dedup(sig) VALUES (?);", -1, &stmt_check, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt_check, 1, sig, -1, SQLITE_STATIC);
        sqlite3_step(stmt_check);
        sqlite3_finalize(stmt_check);
        if (sqlite3_changes(cm->db) == 0) {
            return 1; /* Duplicate: skipped */
        }
    }

    /* Insert into FTS5 index */
    const char *insert_sql =
        "INSERT INTO cases_fts(timestamp_utc, tool, verdict, code, reason, amount_aed) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(cm->db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -2;

    char ts_str[32];
    snprintf(ts_str, sizeof(ts_str), "%llu", (unsigned long long)timestamp_utc);
    char amt_str[32];
    snprintf(amt_str, sizeof(amt_str), "%.2f", safe_amount);

    sqlite3_bind_text(stmt, 1, ts_str, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, safe_tool, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, safe_verdict, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, safe_code, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, safe_reason, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, amt_str, -1, SQLITE_TRANSIENT);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -3;
}

int qala_case_memory_index_file(qala_case_memory_t *cm, const char *audit_log_path) {
    if (!cm || !cm->db || !audit_log_path) return -1;
    FILE *fp = fopen(audit_log_path, "r");
    if (!fp) return -2;

    char line[4096];
    int indexed_count = 0;

    char *err = NULL;
    sqlite3_exec(cm->db, "BEGIN TRANSACTION;", NULL, NULL, &err);
    if (err) sqlite3_free(err);

    while (fgets(line, sizeof(line), fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        if (len == 0) continue;

        cJSON *root = cJSON_Parse(line);
        if (!root) continue;

        cJSON *ts_item = cJSON_GetObjectItem(root, "timestamp_utc");
        cJSON *tool_item = cJSON_GetObjectItem(root, "tool");
        cJSON *verdict_item = cJSON_GetObjectItem(root, "verdict");
        cJSON *code_item = cJSON_GetObjectItem(root, "code");
        cJSON *reason_item = cJSON_GetObjectItem(root, "reason");
        cJSON *amt_item = cJSON_GetObjectItem(root, "amount_aed");

        uint64_t ts = ts_item ? (uint64_t)ts_item->valuedouble : 0;
        const char *tool = (tool_item && tool_item->valuestring) ? tool_item->valuestring : "unknown";
        const char *verdict = (verdict_item && verdict_item->valuestring) ? verdict_item->valuestring : "UNKNOWN";
        const char *code = (code_item && code_item->valuestring) ? code_item->valuestring : "unknown";
        const char *reason = (reason_item && reason_item->valuestring) ? reason_item->valuestring : "none";
        double amt = amt_item ? amt_item->valuedouble : 0.0;

        if (qala_case_memory_insert(cm, ts, tool, verdict, code, reason, amt) == 0) {
            indexed_count++;
        }

        cJSON_Delete(root);
    }

    fclose(fp);
    sqlite3_exec(cm->db, "COMMIT;", NULL, NULL, &err);
    if (err) sqlite3_free(err);

    return indexed_count;
}

/* Helper to sanitize raw user query for FTS5 syntax safety */
static void sanitize_fts5_query(const char *raw, char *dst, size_t dst_cap) {
    if (!raw || !dst || dst_cap == 0) return;
    dst[0] = '\0';

    size_t w = 0;
    const char *p = raw;

    while (*p != '\0') {
        while (*p != '\0' && isspace((unsigned char)*p)) p++;
        if (*p == '\0') break;

        const char *start = p;
        while (*p != '\0' && !isspace((unsigned char)*p)) p++;
        size_t tok_len = (size_t)(p - start);

        if (w > 0 && (w + 1) < dst_cap) {
            dst[w++] = ' ';
        }

        /* Wrap token in double quotes, stripping any internal double quotes */
        if ((w + 2) < dst_cap) {
            dst[w++] = '"';
        }
        for (size_t i = 0; i < tok_len && (w + 2) < dst_cap; ++i) {
            if (start[i] != '"') {
                dst[w++] = start[i];
            }
        }
        if ((w + 1) < dst_cap) {
            dst[w++] = '"';
        }
    }
    dst[w] = '\0';
}

int qala_case_memory_query(
    qala_case_memory_t *cm,
    const char         *query_str,
    qala_case_record_t *out_results,
    size_t              max_results
) {
    if (!cm || !cm->db || !query_str || query_str[0] == '\0' || !out_results || max_results == 0) {
        return 0;
    }

    const char *match_sql =
        "SELECT timestamp_utc, tool, verdict, code, reason, amount_aed, rank "
        "FROM cases_fts WHERE cases_fts MATCH ? ORDER BY rank ASC LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(cm->db, match_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return -1;
    }

    /* First attempt: raw query */
    sqlite3_bind_text(stmt, 1, query_str, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, (int)max_results);

    size_t count = 0;
    rc = sqlite3_step(stmt);

    /* If raw query had FTS5 syntax error (e.g. unquoted slashes or punctuation), retry with sanitized query */
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        stmt = NULL;

        char sanitized[1024];
        sanitize_fts5_query(query_str, sanitized, sizeof(sanitized));

        rc = sqlite3_prepare_v2(cm->db, match_sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) return -1;

        sqlite3_bind_text(stmt, 1, sanitized, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, (int)max_results);
        rc = sqlite3_step(stmt);
    }

    while (rc == SQLITE_ROW && count < max_results) {
        const char *ts_txt = (const char *)sqlite3_column_text(stmt, 0);
        const char *tool = (const char *)sqlite3_column_text(stmt, 1);
        const char *verdict = (const char *)sqlite3_column_text(stmt, 2);
        const char *code = (const char *)sqlite3_column_text(stmt, 3);
        const char *reason = (const char *)sqlite3_column_text(stmt, 4);
        const char *amt_txt = (const char *)sqlite3_column_text(stmt, 5);
        double rank = sqlite3_column_double(stmt, 6);

        out_results[count].timestamp_utc = ts_txt ? (uint64_t)strtoull(ts_txt, NULL, 10) : 0;
        snprintf(out_results[count].tool, sizeof(out_results[count].tool), "%s", tool ? tool : "unknown");
        snprintf(out_results[count].verdict, sizeof(out_results[count].verdict), "%s", verdict ? verdict : "UNKNOWN");
        snprintf(out_results[count].code, sizeof(out_results[count].code), "%s", code ? code : "unknown");
        snprintf(out_results[count].reason, sizeof(out_results[count].reason), "%s", reason ? reason : "none");
        out_results[count].amount_aed = amt_txt ? atof(amt_txt) : 0.0;
        out_results[count].rank_score = rank;

        count++;
        rc = sqlite3_step(stmt);
    }

    sqlite3_finalize(stmt);

    /* If FTS5 MATCH returned 0 rows, fallback to LIKE search for substring matching */
    if (count == 0) {
        const char *like_sql =
            "SELECT timestamp_utc, tool, verdict, code, reason, amount_aed, 0.0 "
            "FROM cases_fts WHERE tool LIKE ? OR reason LIKE ? OR verdict LIKE ? OR code LIKE ? "
            "LIMIT ?;";
        rc = sqlite3_prepare_v2(cm->db, like_sql, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            char pattern[512];
            snprintf(pattern, sizeof(pattern), "%%%s%%", query_str);
            sqlite3_bind_text(stmt, 1, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, pattern, -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, 5, (int)max_results);

            rc = sqlite3_step(stmt);
            while (rc == SQLITE_ROW && count < max_results) {
                const char *ts_txt = (const char *)sqlite3_column_text(stmt, 0);
                const char *tool = (const char *)sqlite3_column_text(stmt, 1);
                const char *verdict = (const char *)sqlite3_column_text(stmt, 2);
                const char *code = (const char *)sqlite3_column_text(stmt, 3);
                const char *reason = (const char *)sqlite3_column_text(stmt, 4);
                const char *amt_txt = (const char *)sqlite3_column_text(stmt, 5);

                out_results[count].timestamp_utc = ts_txt ? (uint64_t)strtoull(ts_txt, NULL, 10) : 0;
                snprintf(out_results[count].tool, sizeof(out_results[count].tool), "%s", tool ? tool : "unknown");
                snprintf(out_results[count].verdict, sizeof(out_results[count].verdict), "%s", verdict ? verdict : "UNKNOWN");
                snprintf(out_results[count].code, sizeof(out_results[count].code), "%s", code ? code : "unknown");
                snprintf(out_results[count].reason, sizeof(out_results[count].reason), "%s", reason ? reason : "none");
                out_results[count].amount_aed = amt_txt ? atof(amt_txt) : 0.0;
                out_results[count].rank_score = 0.0;

                count++;
                rc = sqlite3_step(stmt);
            }
            sqlite3_finalize(stmt);
        }
    }

    return (int)count;
}

int qala_case_memory_format_results(
    const char               *query_str,
    const qala_case_record_t *results,
    size_t                    count,
    char                     *out_buf,
    size_t                    buf_cap
) {
    if (!out_buf || buf_cap == 0) return -1;
    out_buf[0] = '\0';

    if (count == 0 || !results) {
        return snprintf(out_buf, buf_cap,
            "[CASE MEMORY] No prior decision cases found matching query: '%s'\n",
            query_str ? query_str : "");
    }

    size_t w = 0;
    int n = snprintf(out_buf + w, buf_cap - w,
        "\n┌────────────────────────── CASE MEMORY: PRIOR DECISIONS ────────────────────────┐\n"
        "│ Query: %-48.48s Matches: %2zu cases │\n"
        "├────────────┬────────────────────────┬─────────┬──────────────┬─────────────────┤\n"
        "│ TIMESTAMP  │ TOOL                   │ VERDICT │ AMOUNT (AED) │ CONTRACT CODE   │\n"
        "├────────────┼────────────────────────┼─────────┼──────────────┼─────────────────┤\n",
        query_str ? query_str : "", count);

    if (n < 0 || (size_t)n >= (buf_cap - w)) return -1;
    w += (size_t)n;

    for (size_t i = 0; i < count; ++i) {
        n = snprintf(out_buf + w, buf_cap - w,
            "│ %-10llu │ %-22.22s │ %-7.7s │ %12.2f │ %-15.15s │\n"
            "│   Reason:  %-66.66s │\n",
            (unsigned long long)results[i].timestamp_utc,
            results[i].tool,
            results[i].verdict,
            results[i].amount_aed,
            results[i].code,
            results[i].reason);

        if (n < 0 || (size_t)n >= (buf_cap - w)) break;
        w += (size_t)n;
    }

    n = snprintf(out_buf + w, buf_cap - w,
        "└────────────┴────────────────────────┴─────────┴──────────────┴─────────────────┘\n");
    if (n > 0 && (size_t)n < (buf_cap - w)) {
        w += (size_t)n;
    }

    return (int)w;
}
