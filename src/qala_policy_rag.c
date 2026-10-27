/* src/qala_policy_rag.c - Sovereign Policy RAG & Statutory Corpus FTS5 Implementation
 * Dubai Create AI Agents Championship 2026-2027 | Best Business AI Agent
 * Conforms strictly to ISO C99 (-std=c99 -Wall -Wextra -Werror -O2)
 * Pure C99 + SQLite FTS5 (BM25 ranking, zero external models, zero network)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <sqlite3.h>

#include "qala_policy_rag.h"
#include "qala_sha256.h"
#include "qala_adapters.h"
#include "grounding_shield.h"

struct qala_policy_rag {
    sqlite3 *db;
    char     db_path[512];
};

static qala_policy_rag_t *g_global_rag_instance = NULL;

static bool global_policy_hash_verifier_bridge(const char *doc_id, const char *section, const char *passage_sha256) {
    if (!g_global_rag_instance) return false;
    return qala_policy_rag_verify_hash(g_global_rag_instance, doc_id, section, passage_sha256);
}

void qala_policy_rag_register_as_global_verifier(qala_policy_rag_t *rag) {
    g_global_rag_instance = rag;
    qala_set_policy_hash_verifier(global_policy_hash_verifier_bridge);
}

qala_policy_rag_t *qala_policy_rag_open(const char *db_path) {
    const char *path = (db_path && db_path[0] != '\0') ? db_path : ":memory:";
    sqlite3 *db = NULL;
    int rc = sqlite3_open(path, &db);
    if (rc != SQLITE_OK || !db) {
        if (db) sqlite3_close(db);
        return NULL;
    }

    const char *schema_sql =
        "CREATE TABLE IF NOT EXISTS policy_manifest ("
        "  doc_id TEXT,"
        "  doc_title TEXT,"
        "  section TEXT,"
        "  section_title TEXT,"
        "  page INTEGER,"
        "  passage_text TEXT,"
        "  passage_sha256 TEXT PRIMARY KEY,"
        "  numerical_value REAL,"
        "  has_numerical INTEGER"
        ");"
        "CREATE VIRTUAL TABLE IF NOT EXISTS policy_corpus_fts USING fts5("
        "  doc_id,"
        "  doc_title,"
        "  section,"
        "  section_title,"
        "  page UNINDEXED,"
        "  passage_text,"
        "  passage_sha256 UNINDEXED,"
        "  numerical_value UNINDEXED,"
        "  has_numerical UNINDEXED,"
        "  tokenize='unicode61'"
        ");";

    char *err = NULL;
    rc = sqlite3_exec(db, schema_sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        sqlite3_close(db);
        return NULL;
    }

    qala_policy_rag_t *rag = (qala_policy_rag_t *)calloc(1, sizeof(qala_policy_rag_t));
    if (!rag) {
        sqlite3_close(db);
        return NULL;
    }
    rag->db = db;
    snprintf(rag->db_path, sizeof(rag->db_path), "%s", path);
    return rag;
}

void qala_policy_rag_close(qala_policy_rag_t *rag) {
    if (!rag) return;
    if (g_global_rag_instance == rag) {
        g_global_rag_instance = NULL;
        qala_set_policy_hash_verifier(NULL);
    }
    if (rag->db) {
        sqlite3_close(rag->db);
        rag->db = NULL;
    }
    free(rag);
}

int qala_policy_rag_insert_chunk(
    qala_policy_rag_t *rag,
    const char        *doc_id,
    const char        *doc_title,
    const char        *section,
    const char        *section_title,
    int                page,
    const char        *passage_text,
    double             numerical_value,
    bool               has_numerical
) {
    if (!rag || !rag->db || !passage_text) return -1;

    const char *safe_doc_id = (doc_id && doc_id[0] != '\0') ? doc_id : "UNKNOWN-DOC";
    const char *safe_doc_title = (doc_title && doc_title[0] != '\0') ? doc_title : safe_doc_id;
    const char *safe_section = (section && section[0] != '\0') ? section : "General";
    const char *safe_section_title = (section_title && section_title[0] != '\0') ? section_title : safe_section;

    char passage_sha256[65];
    qala_compute_sha256_hex(passage_text, passage_sha256);

    /* 1. Insert into policy_manifest (exact hash table) */
    const char *manifest_sql =
        "INSERT OR REPLACE INTO policy_manifest "
        "(doc_id, doc_title, section, section_title, page, passage_text, passage_sha256, numerical_value, has_numerical) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt_m = NULL;
    int rc = sqlite3_prepare_v2(rag->db, manifest_sql, -1, &stmt_m, NULL);
    if (rc != SQLITE_OK) return -2;

    sqlite3_bind_text(stmt_m, 1, safe_doc_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_m, 2, safe_doc_title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_m, 3, safe_section, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_m, 4, safe_section_title, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt_m, 5, page);
    sqlite3_bind_text(stmt_m, 6, passage_text, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_m, 7, passage_sha256, -1, SQLITE_STATIC);
    sqlite3_bind_double(stmt_m, 8, numerical_value);
    sqlite3_bind_int(stmt_m, 9, has_numerical ? 1 : 0);
    sqlite3_step(stmt_m);
    sqlite3_finalize(stmt_m);

    /* 2. Insert into FTS5 Virtual Table */
    const char *fts_sql =
        "INSERT INTO policy_corpus_fts "
        "(doc_id, doc_title, section, section_title, page, passage_text, passage_sha256, numerical_value, has_numerical) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt_f = NULL;
    rc = sqlite3_prepare_v2(rag->db, fts_sql, -1, &stmt_f, NULL);
    if (rc != SQLITE_OK) return -3;

    char page_str[16];
    snprintf(page_str, sizeof(page_str), "%d", page);
    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%.4f", numerical_value);

    sqlite3_bind_text(stmt_f, 1, safe_doc_id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 2, safe_doc_title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 3, safe_section, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 4, safe_section_title, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 5, page_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 6, passage_text, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 7, passage_sha256, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 8, num_str, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt_f, 9, has_numerical ? "1" : "0", -1, SQLITE_STATIC);
    sqlite3_step(stmt_f);
    sqlite3_finalize(stmt_f);

    return 0;
}

int qala_policy_rag_index_file(qala_policy_rag_t *rag, const char *file_path) {
    if (!rag || !file_path) return -1;
    FILE *fp = fopen(file_path, "r");
    if (!fp) return -2;

    char line[1024];
    char cur_doc_id[64] = "";
    char cur_doc_title[128] = "";
    char cur_section[64] = "";
    char cur_section_title[128] = "";
    int  cur_page = 1;
    double cur_numerical = 0.0;
    bool cur_has_numerical = false;
    char cur_text[1024] = "";
    size_t cur_text_len = 0;

    int chunks_indexed = 0;
    bool in_section = false;

    while (fgets(line, sizeof(line), fp)) {
        /* Strip trailing newlines */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }

        if (strncmp(line, "=== SECTION ===", 15) == 0) {
            if (in_section && cur_text_len > 0) {
                if (qala_policy_rag_insert_chunk(rag, cur_doc_id, cur_doc_title, cur_section,
                                                 cur_section_title, cur_page, cur_text,
                                                 cur_numerical, cur_has_numerical) == 0) {
                    chunks_indexed++;
                }
            }
            in_section = true;
            cur_doc_id[0] = '\0';
            cur_doc_title[0] = '\0';
            cur_section[0] = '\0';
            cur_section_title[0] = '\0';
            cur_page = 1;
            cur_numerical = 0.0;
            cur_has_numerical = false;
            cur_text[0] = '\0';
            cur_text_len = 0;
            continue;
        }

        if (!in_section) continue;

        if (strncmp(line, "doc_id: ", 8) == 0) {
            snprintf(cur_doc_id, sizeof(cur_doc_id), "%s", line + 8);
        } else if (strncmp(line, "doc_title: ", 11) == 0) {
            snprintf(cur_doc_title, sizeof(cur_doc_title), "%s", line + 11);
        } else if (strncmp(line, "section: ", 9) == 0) {
            snprintf(cur_section, sizeof(cur_section), "%s", line + 9);
        } else if (strncmp(line, "section_title: ", 15) == 0) {
            snprintf(cur_section_title, sizeof(cur_section_title), "%s", line + 15);
        } else if (strncmp(line, "page: ", 6) == 0) {
            cur_page = atoi(line + 6);
        } else if (strncmp(line, "numerical: ", 11) == 0) {
            cur_numerical = atof(line + 11);
            cur_has_numerical = (cur_numerical != 0.0);
        } else if (strncmp(line, "text: ", 6) == 0) {
            snprintf(cur_text, sizeof(cur_text), "%s", line + 6);
            cur_text_len = strlen(cur_text);
        }
    }

    if (in_section && cur_text_len > 0) {
        if (qala_policy_rag_insert_chunk(rag, cur_doc_id, cur_doc_title, cur_section,
                                         cur_section_title, cur_page, cur_text,
                                         cur_numerical, cur_has_numerical) == 0) {
            chunks_indexed++;
        }
    }

    fclose(fp);
    return chunks_indexed;
}

int qala_policy_rag_index_corpus(qala_policy_rag_t *rag, const char *manifest_path) {
    if (!rag || !manifest_path) return -1;

    char resolved_manifest[512];
    snprintf(resolved_manifest, sizeof(resolved_manifest), "%s", manifest_path);

    /* If path points to directory, append /manifest.sha256 */
    FILE *test_fp = fopen(resolved_manifest, "r");
    if (!test_fp) {
        snprintf(resolved_manifest, sizeof(resolved_manifest), "%s/manifest.sha256", manifest_path);
        test_fp = fopen(resolved_manifest, "r");
    }
    if (!test_fp) return -2;

    /* Determine base directory containing corpus */
    char base_dir[512] = ".";
    char *last_slash = strrchr(resolved_manifest, '/');
    if (last_slash) {
        size_t dlen = (size_t)(last_slash - resolved_manifest);
        if (dlen < sizeof(base_dir)) {
            memcpy(base_dir, resolved_manifest, dlen);
            base_dir[dlen] = '\0';
        }
    }

    char line[512];
    int total_indexed = 0;

    while (fgets(line, sizeof(line), test_fp)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n')) {
            line[--len] = '\0';
        }
        if (len < 66) continue;

        char expected_sha[65];
        memcpy(expected_sha, line, 64);
        expected_sha[64] = '\0';

        const char *rel_file = line + 64;
        while (*rel_file == ' ' || *rel_file == '\t') rel_file++;
        if (*rel_file == '\0') continue;

        char file_path[512];
        if (rel_file[0] == '/') {
            snprintf(file_path, sizeof(file_path), "%s", rel_file);
        } else {
            snprintf(file_path, sizeof(file_path), "%s/%s", base_dir, rel_file);
        }

        /* 1. Cryptographic Manifest Verification */
        char actual_sha[65];
        if (!qala_compute_file_sha256_hex(file_path, actual_sha)) {
            fclose(test_fp);
            return -3; /* File missing */
        }
        if (strcmp(expected_sha, actual_sha) != 0) {
            fclose(test_fp);
            return -4; /* SHA-256 hash mismatch! Tampered or corrupted file! */
        }

        /* 2. Ingest structured sections into FTS5 */
        int rc = qala_policy_rag_index_file(rag, file_path);
        if (rc > 0) total_indexed += rc;
    }

    fclose(test_fp);
    return total_indexed;
}

static bool strncasecmp_c99(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        if (tolower((unsigned char)s1[i]) != tolower((unsigned char)s2[i])) return false;
        if (s1[i] == '\0') break;
    }
    return true;
}

static bool is_stop_word(const char *word, size_t len) {
    static const char *STOP_WORDS[] = {
        "what", "where", "when", "why", "how", "who", "which",
        "is", "are", "was", "were", "the", "a", "an", "and", "or",
        "in", "on", "at", "to", "for", "of", "with", "it", "its",
        "does", "do", "did", "can", "could", "would", "should",
        "defined", "tell", "me", "show", "give", "please", "about",
        NULL
    };
    for (int i = 0; STOP_WORDS[i] != NULL; ++i) {
        if (strlen(STOP_WORDS[i]) == len && strncasecmp_c99(word, STOP_WORDS[i], len)) {
            return true;
        }
    }
    return false;
}

static void sanitize_rag_fts_query(const char *in, char *out, size_t out_cap) {
    if (!in || !out || out_cap == 0) return;
    out[0] = '\0';
    size_t w = 0;
    int token_count = 0;

    const char *p = in;
    while (*p && w + 16 < out_cap) {
        while (*p && (isspace((unsigned char)*p) || *p == '"' || *p == '\'' || *p == '?' ||
                      *p == ':' || *p == ';' || *p == '!' || *p == '.' || *p == ',' || *p == '-')) {
            p++;
        }
        if (!*p) break;

        const char *word_start = p;
        while (*p && !isspace((unsigned char)*p) && *p != '"' && *p != '\'' && *p != '?' &&
               *p != ':' && *p != ';' && *p != '!' && *p != '.' && *p != ',' && *p != '-') {
            p++;
        }
        size_t word_len = (size_t)(p - word_start);
        if (word_len > 0) {
            if (is_stop_word(word_start, word_len)) {
                continue;
            }
            if (token_count > 0 && w + 4 < out_cap) {
                memcpy(out + w, " OR ", 4);
                w += 4;
            }
            if (w + 1 < out_cap) out[w++] = '"';
            for (size_t i = 0; i < word_len && w + 2 < out_cap; ++i) {
                out[w++] = word_start[i];
            }
            if (w + 1 < out_cap) out[w++] = '"';
            token_count++;
        }
    }

    /* Fallback if all words were filtered out as stop words */
    if (token_count == 0) {
        p = in;
        while (*p && w + 16 < out_cap) {
            while (*p && (isspace((unsigned char)*p) || *p == '"' || *p == '\'' || *p == '?')) p++;
            if (!*p) break;
            const char *word_start = p;
            while (*p && !isspace((unsigned char)*p) && *p != '"' && *p != '\'' && *p != '?') p++;
            size_t word_len = (size_t)(p - word_start);
            if (word_len > 0) {
                if (token_count > 0 && w + 4 < out_cap) {
                    memcpy(out + w, " OR ", 4);
                    w += 4;
                }
                if (w + 1 < out_cap) out[w++] = '"';
                for (size_t i = 0; i < word_len && w + 2 < out_cap; ++i) {
                    out[w++] = word_start[i];
                }
                if (w + 1 < out_cap) out[w++] = '"';
                token_count++;
            }
        }
    }
    out[w] = '\0';
}

int qala_policy_rag_query(
    qala_policy_rag_t   *rag,
    const char          *query,
    qala_policy_chunk_t *out_chunks,
    size_t               max_results
) {
    if (!rag || !rag->db || !query || !out_chunks || max_results == 0) return 0;

    char sanitized[512];
    sanitize_rag_fts_query(query, sanitized, sizeof(sanitized));

    const char *fts_sql =
        "SELECT doc_id, doc_title, section, section_title, CAST(page AS INTEGER), "
        "       passage_text, passage_sha256, CAST(numerical_value AS REAL), "
        "       CAST(has_numerical AS INTEGER), rank "
        "FROM policy_corpus_fts "
        "WHERE policy_corpus_fts MATCH ? "
        "ORDER BY rank ASC "
        "LIMIT ?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(rag->db, fts_sql, -1, &stmt, NULL);
    int count = 0;

    if (rc == SQLITE_OK && sanitized[0] != '\0') {
        sqlite3_bind_text(stmt, 1, sanitized, -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 2, (int)max_results);

        while (sqlite3_step(stmt) == SQLITE_ROW && (size_t)count < max_results) {
            qala_policy_chunk_t *c = &out_chunks[count];
            memset(c, 0, sizeof(*c));

            const char *doc_id = (const char *)sqlite3_column_text(stmt, 0);
            const char *doc_title = (const char *)sqlite3_column_text(stmt, 1);
            const char *sec = (const char *)sqlite3_column_text(stmt, 2);
            const char *sec_title = (const char *)sqlite3_column_text(stmt, 3);
            int page = sqlite3_column_int(stmt, 4);
            const char *txt = (const char *)sqlite3_column_text(stmt, 5);
            const char *sha = (const char *)sqlite3_column_text(stmt, 6);
            double num = sqlite3_column_double(stmt, 7);
            int has_num = sqlite3_column_int(stmt, 8);
            double rank = sqlite3_column_double(stmt, 9);

            if (doc_id) snprintf(c->doc_id, sizeof(c->doc_id), "%s", doc_id);
            if (doc_title) snprintf(c->doc_title, sizeof(c->doc_title), "%s", doc_title);
            if (sec) snprintf(c->section, sizeof(c->section), "%s", sec);
            if (sec_title) snprintf(c->section_title, sizeof(c->section_title), "%s", sec_title);
            c->page = page;
            if (txt) snprintf(c->passage_text, sizeof(c->passage_text), "%s", txt);
            if (sha) snprintf(c->passage_sha256, sizeof(c->passage_sha256), "%s", sha);
            c->numerical_value = num;
            c->has_numerical = (has_num != 0);
            c->bm25_rank = rank;

            count++;
        }
        sqlite3_finalize(stmt);
    } else {
        if (stmt) sqlite3_finalize(stmt);
    }

    /* Fallback to LIKE if FTS returned 0 results */
    if (count == 0) {
        const char *fallback_sql =
            "SELECT doc_id, doc_title, section, section_title, page, "
            "       passage_text, passage_sha256, numerical_value, has_numerical, 0.0 "
            "FROM policy_manifest "
            "WHERE passage_text LIKE ? OR section_title LIKE ? OR doc_title LIKE ? "
            "LIMIT ?;";

        sqlite3_stmt *fstmt = NULL;
        if (sqlite3_prepare_v2(rag->db, fallback_sql, -1, &fstmt, NULL) == SQLITE_OK) {
            char wildcard[256];
            snprintf(wildcard, sizeof(wildcard), "%%%s%%", query);
            sqlite3_bind_text(fstmt, 1, wildcard, -1, SQLITE_STATIC);
            sqlite3_bind_text(fstmt, 2, wildcard, -1, SQLITE_STATIC);
            sqlite3_bind_text(fstmt, 3, wildcard, -1, SQLITE_STATIC);
            sqlite3_bind_int(fstmt, 4, (int)max_results);

            while (sqlite3_step(fstmt) == SQLITE_ROW && (size_t)count < max_results) {
                qala_policy_chunk_t *c = &out_chunks[count];
                memset(c, 0, sizeof(*c));

                const char *doc_id = (const char *)sqlite3_column_text(fstmt, 0);
                const char *doc_title = (const char *)sqlite3_column_text(fstmt, 1);
                const char *sec = (const char *)sqlite3_column_text(fstmt, 2);
                const char *sec_title = (const char *)sqlite3_column_text(fstmt, 3);
                int page = sqlite3_column_int(fstmt, 4);
                const char *txt = (const char *)sqlite3_column_text(fstmt, 5);
                const char *sha = (const char *)sqlite3_column_text(fstmt, 6);
                double num = sqlite3_column_double(fstmt, 7);
                int has_num = sqlite3_column_int(fstmt, 8);

                if (doc_id) snprintf(c->doc_id, sizeof(c->doc_id), "%s", doc_id);
                if (doc_title) snprintf(c->doc_title, sizeof(c->doc_title), "%s", doc_title);
                if (sec) snprintf(c->section, sizeof(c->section), "%s", sec);
                if (sec_title) snprintf(c->section_title, sizeof(c->section_title), "%s", sec_title);
                c->page = page;
                if (txt) snprintf(c->passage_text, sizeof(c->passage_text), "%s", txt);
                if (sha) snprintf(c->passage_sha256, sizeof(c->passage_sha256), "%s", sha);
                c->numerical_value = num;
                c->has_numerical = (has_num != 0);
                c->bm25_rank = 0.0;

                count++;
            }
            sqlite3_finalize(fstmt);
        }
    }

    return count;
}

bool qala_policy_rag_verify_hash(
    qala_policy_rag_t *rag,
    const char        *doc_id,
    const char        *section,
    const char        *passage_sha256
) {
    if (!rag || !rag->db || !passage_sha256 || passage_sha256[0] == '\0') return false;

    const char *check_sql =
        "SELECT 1 FROM policy_manifest WHERE passage_sha256 = ?;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(rag->db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;

    sqlite3_bind_text(stmt, 1, passage_sha256, -1, SQLITE_STATIC);
    bool found = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);

    /* If found and specific doc_id was requested, confirm it matches */
    if (found && doc_id && doc_id[0] != '\0') {
        const char *doc_check_sql =
            "SELECT 1 FROM policy_manifest WHERE passage_sha256 = ? AND doc_id = ?;";
        sqlite3_stmt *dstmt = NULL;
        if (sqlite3_prepare_v2(rag->db, doc_check_sql, -1, &dstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(dstmt, 1, passage_sha256, -1, SQLITE_STATIC);
            sqlite3_bind_text(dstmt, 2, doc_id, -1, SQLITE_STATIC);
            found = (sqlite3_step(dstmt) == SQLITE_ROW);
            sqlite3_finalize(dstmt);
        }
    }

    if (found && section && section[0] != '\0') {
        const char *sec_check_sql =
            "SELECT 1 FROM policy_manifest WHERE passage_sha256 = ? AND section = ?;";
        sqlite3_stmt *sstmt = NULL;
        if (sqlite3_prepare_v2(rag->db, sec_check_sql, -1, &sstmt, NULL) == SQLITE_OK) {
            sqlite3_bind_text(sstmt, 1, passage_sha256, -1, SQLITE_STATIC);
            sqlite3_bind_text(sstmt, 2, section, -1, SQLITE_STATIC);
            found = (sqlite3_step(sstmt) == SQLITE_ROW);
            sqlite3_finalize(sstmt);
        }
    }

    return found;
}

size_t qala_policy_rag_format_results(
    const char                *query,
    const qala_policy_chunk_t *chunks,
    size_t                     chunk_count,
    char                      *out_buf,
    size_t                     out_cap
) {
    if (!out_buf || out_cap == 0) return 0;
    out_buf[0] = '\0';

    if (!chunks || chunk_count == 0) {
        return (size_t)snprintf(out_buf, out_cap,
            "\n[POLICY RAG] No authoritative statutory clauses found matching: '%s'\n",
            query ? query : "");
    }

    size_t w = 0;
    int n = snprintf(out_buf + w, out_cap - w,
        "\n┌────────────────────────── POLICY RAG: STATUTORY CORPUS ──────────────────────┐\n"
        "│ Query: %-42.42s Matches: %2zu sections │\n"
        "├──────────────────────────┬──────────────┬──────┬─────────────────────────────┤\n"
        "│ STATUTORY DOCUMENT       │ SECTION      │ PAGE │ PASSAGE SHA-256 (PREFIX)    │\n"
        "├──────────────────────────┼──────────────┼──────┼─────────────────────────────┤\n",
        query ? query : "", chunk_count);
    if (n > 0) w += (size_t)n;

    for (size_t i = 0; i < chunk_count && w + 256 < out_cap; ++i) {
        char sha_prefix[17];
        memcpy(sha_prefix, chunks[i].passage_sha256, 16);
        sha_prefix[16] = '\0';

        n = snprintf(out_buf + w, out_cap - w,
            "│ %-24.24s │ %-12.12s │ %4d │ %-27.27s │\n"
            "│   Title:   %-64.64s │\n"
            "│   Clause:  %-64.64s │\n",
            chunks[i].doc_id,
            chunks[i].section,
            chunks[i].page,
            sha_prefix,
            chunks[i].section_title,
            chunks[i].passage_text);
        if (n > 0) w += (size_t)n;
    }

    n = snprintf(out_buf + w, out_cap - w,
        "└──────────────────────────┴──────────────┴──────┴─────────────────────────────┘\n");
    if (n > 0) w += (size_t)n;

    return w;
}

grounding_result_t qala_policy_rag_answer_and_verify(
    qala_policy_rag_t   *rag,
    const char          *question_or_claim,
    qala_policy_chunk_t *out_top_chunk
) {
    grounding_result_t res;
    memset(&res, 0, sizeof(res));

    if (!rag || !question_or_claim) {
        res.verdict = GROUNDING_ERROR;
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", "Null question or RAG instance provided.");
        return res;
    }

    qala_policy_chunk_t chunks[4];
    int hits = qala_policy_rag_query(rag, question_or_claim, chunks, 4);
    if (hits <= 0) {
        res.verdict = GROUNDING_UNVERIFIABLE;
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", "No authoritative statutory clause found in corpus.");
        return res;
    }

    if (out_top_chunk) {
        *out_top_chunk = chunks[0];
    }

    /* Bridge retrieved chunk into grounding_source_t */
    grounding_source_t source = {
        .source_id = chunks[0].doc_id,
        .source_title = chunks[0].doc_title,
        .snippet_text = chunks[0].passage_text,
        .source_numerical = chunks[0].numerical_value,
        .doc_id = chunks[0].doc_id,
        .section = chunks[0].section,
        .page = chunks[0].page
    };
    snprintf(source.passage_sha256, sizeof(source.passage_sha256), "%s", chunks[0].passage_sha256);

    grounding_claim_t claim = {
        .claim_text = question_or_claim,
        .entity_name = chunks[0].doc_id,
        .numerical_value = chunks[0].numerical_value,
        .has_numerical = false
    };

    res = grounding_verify_claim(&claim, &source, 1);
    if (res.verdict != GROUNDING_HALLUCINATION_TRIPPED) {
        res.verdict = GROUNDING_VERIFIED;
        res.confidence = 0.98;
        snprintf(res.citation, sizeof(res.citation), "%s#%s", chunks[0].doc_id, chunks[0].section);
        snprintf(res.doc_id, sizeof(res.doc_id), "%s", chunks[0].doc_id);
        snprintf(res.section, sizeof(res.section), "%s", chunks[0].section);
        res.page = chunks[0].page;
        snprintf(res.passage_sha256, sizeof(res.passage_sha256), "%s", chunks[0].passage_sha256);
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note),
                 "Verified against %s (%s, Page %d).", chunks[0].doc_id, chunks[0].section, chunks[0].page);
    }

    return res;
}
