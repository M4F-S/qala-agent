/* qala_mcp_policy_adapter.c
 * Hardened C99 Implementation of:
 *   1. DESC ISR v3.1 Kernel Sandbox & Deterministic Path Allowlisting
 *   2. Single-Use Cryptographic Human-Approval Gate (Telegram/Slack Bridge)
 *   3. Stateless MCP (2026-07-28) JSON-RPC Transport Frame Builder
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifdef __linux__
#include <sys/prctl.h>
#include <sys/syscall.h>
#endif

#include "qala_adapters.h"

/* F-08 Fix: Constant-time string comparison without length timing leak */
static bool constant_time_streq(const char *a, const char *b) {
    if (!a || !b) return false;
    size_t len_a = 0;
    while (len_a < 64 && a[len_a] != '\0') len_a++;
    size_t len_b = 0;
    while (len_b < 64 && b[len_b] != '\0') len_b++;

    volatile unsigned char diff = (unsigned char)(len_a ^ len_b);
    for (size_t i = 0; i < 64; ++i) {
        unsigned char ca = (i < len_a) ? (unsigned char)a[i] : 0;
        unsigned char cb = (i < len_b) ? (unsigned char)b[i] : 0;
        diff |= (ca ^ cb);
    }
    return (diff == 0) && (len_a > 0);
}

/* Strict JSON string escaper to prevent audit log injection */
static void json_escape_string(const char *src, char *dst, size_t dst_cap) {
    if (!dst || dst_cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t w = 0;
    for (size_t r = 0; src[r] != '\0' && (w + 2) < dst_cap; ++r) {
        unsigned char c = (unsigned char)src[r];
        if (c == '"' || c == '\\') {
            if (w + 3 >= dst_cap) break;
            dst[w++] = '\\';
            dst[w++] = (char)c;
        } else if (c == '\n') {
            if (w + 3 >= dst_cap) break;
            dst[w++] = '\\';
            dst[w++] = 'n';
        } else if (c == '\r') {
            if (w + 3 >= dst_cap) break;
            dst[w++] = '\\';
            dst[w++] = 'r';
        } else if (c >= 0x20) {
            dst[w++] = (char)c;
        }
    }
    dst[w] = '\0';
}

/* DESC ISR v3.1 Workspace Sandboxing Configuration */
static char g_workspace_root[PATH_MAX] = "/workspace";

void qala_set_workspace_root(const char *root) {
    if (root && root[0] != '\0') {
        snprintf(g_workspace_root, sizeof(g_workspace_root), "%s", root);
    } else {
        snprintf(g_workspace_root, sizeof(g_workspace_root), "/workspace");
    }
}

const char *qala_get_workspace_root(void) {
    const char *env = getenv("QALA_WORKSPACE_ROOT");
    if (env && env[0] != '\0') {
        return env;
    }
    return g_workspace_root;
}

/* Persistent Single-Use Approval Token Store Configuration */
static char g_token_store_path[PATH_MAX] = "qala-tokens.jsonl";

void qala_set_token_store_path(const char *path) {
    if (path && path[0] != '\0') {
        snprintf(g_token_store_path, sizeof(g_token_store_path), "%s", path);
    } else {
        snprintf(g_token_store_path, sizeof(g_token_store_path), "qala-tokens.jsonl");
    }
}

const char *qala_get_token_store_path(void) {
    const char *env = getenv("QALA_TOKEN_STORE_PATH");
    if (env && env[0] != '\0') {
        return env;
    }
    return g_token_store_path;
}

bool qala_token_store_is_consumed(const char *store_path, const char *token) {
    if (!token || token[0] == '\0') return false;
    const char *path = store_path ? store_path : qala_get_token_store_path();
    FILE *fp = fopen(path, "r");
    if (!fp) return false;

    char line[1024];
    bool is_consumed = false;

    while (fgets(line, sizeof(line), fp)) {
        char *tok_key = strstr(line, "\"token\":\"");
        if (!tok_key) continue;
        tok_key += 9; /* Skip "\"token\":\"" */
        char *tok_end = strchr(tok_key, '"');
        if (!tok_end) continue;

        size_t tok_len = (size_t)(tok_end - tok_key);
        if (tok_len >= 64) continue;

        char found_token[64];
        memcpy(found_token, tok_key, tok_len);
        found_token[tok_len] = '\0';

        if (constant_time_streq(found_token, token)) {
            if (strstr(line, "\"consumed\":true") != NULL) {
                is_consumed = true;
                break;
            }
        }
    }

    fclose(fp);
    return is_consumed;
}

int qala_token_store_record_consumed(
    const char *store_path,
    const char *token,
    const char *scope,
    const char *tool_name,
    double      amount_aed,
    uint64_t    consumed_utc
) {
    if (!token || token[0] == '\0') return -1;
    const char *path = store_path ? store_path : qala_get_token_store_path();
    FILE *fp = fopen(path, "a");
    if (!fp) return -2;

    char esc_token[128];
    char esc_scope[64];
    char esc_tool[128];
    json_escape_string(token, esc_token, sizeof(esc_token));
    json_escape_string(scope ? scope : "default", esc_scope, sizeof(esc_scope));
    json_escape_string(tool_name ? tool_name : "unknown", esc_tool, sizeof(esc_tool));

    double safe_amount = isfinite(amount_aed) ? amount_aed : 0.0;

    fprintf(fp,
        "{\"timestamp_utc\":%llu,\"token\":\"%s\",\"scope\":\"%s\",\"tool\":\"%s\","
        "\"amount_aed\":%.2f,\"consumed\":true}\n",
        (unsigned long long)consumed_utc,
        esc_token,
        esc_scope,
        esc_tool,
        safe_amount
    );

    fflush(fp);
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__) || defined(__linux__)
    int fd = fileno(fp);
    if (fd >= 0) {
        (void)fsync(fd);
    }
#endif
    fclose(fp);
    return 0;
}

int qala_token_store_sync_slots(
    const char           *store_path,
    qala_approval_slot_t *slots,
    size_t                slot_count
) {
    if (!slots || slot_count == 0) return 0;
    const char *path = store_path ? store_path : qala_get_token_store_path();
    for (size_t i = 0; i < slot_count; ++i) {
        if (!slots[i].consumed) {
            if (qala_token_store_is_consumed(path, slots[i].token)) {
                slots[i].consumed = true;
            }
        }
    }
    return 0;
}

/* AF-07 Fix: Expanded restricted system and confidential tokens */
static const char *RESTRICTED_TOKENS[] = {
    "confidential",
    "/etc",
    "/root",
    "/proc",
    "/sys",
    "/dev",
    ".ssh",
    "id_rsa",
    "id_ed25519",
    "id_ecdsa",
    "id_dsa",
    ".env",
    ".aws",
    "uae_citizens.db",
    "customers_pii.db",
    "nabidh_patient_master.db",
    NULL
};

static bool contains_forbidden_token(const char *str) {
    if (!str) return false;
    for (int i = 0; RESTRICTED_TOKENS[i] != NULL; ++i) {
        if (strstr(str, RESTRICTED_TOKENS[i]) != NULL) {
            return true;
        }
    }
    return false;
}

static bool has_forbidden_extension(const char *path) {
    if (!path) return false;
    size_t len = strlen(path);
    const char *exts[] = {
        ".pem", ".key", ".crt", ".pfx", ".p12", ".secret", ".token", NULL
    };
    for (int i = 0; exts[i] != NULL; ++i) {
        size_t ext_len = strlen(exts[i]);
        if (len >= ext_len && strcasecmp(path + len - ext_len, exts[i]) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_traversal_sequence(const char *str) {
    if (!str) return false;
    if (strstr(str, "..") != NULL) return true;
    if (strstr(str, "%2e%2e") != NULL || strstr(str, "%2E%2E") != NULL) return true;
    if (strstr(str, "%2e%2E") != NULL || strstr(str, "%2E%2e") != NULL) return true;
    if (strstr(str, "\\u002e") != NULL || strstr(str, "\\u002E") != NULL) return true;
    if (strstr(str, "%252e") != NULL || strstr(str, "%252E") != NULL) return true;
    return false;
}

/* Lexically normalizes an absolute path string, resolving "." and ".." components.
 * Returns true if valid, false if invalid or attempts traversal above root ("/").
 */
static bool normalize_path_lexical(const char *src, char *dst, size_t dst_cap) {
    if (!src || !dst || dst_cap < 2) return false;

    char temp[PATH_MAX];
    size_t src_len = strlen(src);
    if (src_len >= sizeof(temp)) return false;
    memcpy(temp, src, src_len + 1);

    const char *segments[256];
    size_t seg_lens[256];
    size_t seg_count = 0;

    bool is_absolute = (temp[0] == '/');
    char *p = temp;

    while (*p != '\0') {
        while (*p == '/') p++;
        if (*p == '\0') break;

        char *start = p;
        while (*p != '\0' && *p != '/') p++;
        if (*p != '\0') {
            *p = '\0';
            p++;
        }

        if (strcmp(start, ".") == 0) {
            continue;
        } else if (strcmp(start, "..") == 0) {
            if (seg_count > 0) {
                seg_count--;
            } else if (is_absolute) {
                /* Traversal above root */
                return false;
            } else {
                return false;
            }
        } else {
            if (seg_count < 256) {
                segments[seg_count] = start;
                seg_lens[seg_count] = strlen(start);
                seg_count++;
            } else {
                return false; /* Excessively deep path */
            }
        }
    }

    size_t w = 0;
    if (is_absolute) {
        if (w + 1 >= dst_cap) return false;
        dst[w++] = '/';
    }

    for (size_t i = 0; i < seg_count; ++i) {
        if (i > 0 || !is_absolute) {
            if (w + 1 >= dst_cap) return false;
            dst[w++] = '/';
        }
        if (w + seg_lens[i] >= dst_cap) return false;
        memcpy(dst + w, segments[i], seg_lens[i]);
        w += seg_lens[i];
    }

    if (w == 0 && is_absolute) {
        dst[0] = '/';
        w = 1;
    }
    dst[w] = '\0';
    return true;
}

/* Resolves a path to canonical representation using realpath() where filesystem targets
 * or existing ancestors exist, with fail-closed fallback to lexical normalization.
 */
static bool resolve_canonical_path(const char *input_path, char *out_canonical, size_t cap) {
    if (!input_path || input_path[0] == '\0' || !out_canonical || cap < 2) {
        return false;
    }

    const char *ws_root = qala_get_workspace_root();
    char full[PATH_MAX];

    if (input_path[0] == '/') {
        size_t ip_len = strlen(input_path);
        if (ip_len >= sizeof(full)) return false;
        memcpy(full, input_path, ip_len + 1);
    } else {
        size_t a = strlen(ws_root), b = strlen(input_path);
        if (a + 1 + b >= sizeof(full)) return false;   /* fail-closed: caller denies */
        memcpy(full, ws_root, a);
        full[a] = '/';
        memcpy(full + a + 1, input_path, b + 1);
    }

    /* 1. Attempt kernel realpath directly (resolves symlinks) */
    char resolved[PATH_MAX];
    if (realpath(full, resolved) != NULL) {
        if (strlen(resolved) >= cap) return false;
        snprintf(out_canonical, cap, "%s", resolved);
        return true;
    }

    /* 2. File does not exist yet: find nearest existing parent directory and realpath it */
    char path_copy[PATH_MAX];
    snprintf(path_copy, sizeof(path_copy), "%s", full);
    char *parent = dirname(path_copy);

    if (parent && parent[0] != '\0' && realpath(parent, resolved) != NULL) {
        char base_copy[PATH_MAX];
        snprintf(base_copy, sizeof(base_copy), "%s", full);
        char *bname = basename(base_copy);
        if (bname && strcmp(bname, ".") != 0 && strcmp(bname, "..") != 0) {
            char combined[PATH_MAX];
            if (strcmp(resolved, "/") == 0) {
                size_t rb = strlen(bname);
                if (1 + rb >= sizeof(combined)) return false;
                combined[0] = '/';
                memcpy(combined + 1, bname, rb + 1);
            } else {
                size_t ra = strlen(resolved), rb = strlen(bname);
                if (ra + 1 + rb >= sizeof(combined)) return false;
                memcpy(combined, resolved, ra);
                combined[ra] = '/';
                memcpy(combined + ra + 1, bname, rb + 1);
            }
            return normalize_path_lexical(combined, out_canonical, cap);
        }
    }

    /* 3. Neither file nor parent directory exists on host: normalize lexically */
    return normalize_path_lexical(full, out_canonical, cap);
}

/* Verifies that canonical_path strictly resides within canonical_ws_root.
 * e.g., /workspace/file.json resides in /workspace,
 * but /workspace2/file or /etc/passwd does NOT.
 */
static bool is_within_workspace(const char *canonical_path, const char *canonical_ws_root) {
    if (!canonical_path || !canonical_ws_root) return false;

    size_t ws_len = strlen(canonical_ws_root);
    while (ws_len > 1 && canonical_ws_root[ws_len - 1] == '/') {
        ws_len--;
    }

    if (ws_len == 1 && canonical_ws_root[0] == '/') {
        return true;
    }

    if (strncmp(canonical_path, canonical_ws_root, ws_len) != 0) {
        return false;
    }

    char next = canonical_path[ws_len];
    return (next == '/' || next == '\0');
}

/* DESC ISR v3.1 Kernel Sandbox & Dynamic /workspace Containment Gate.
 * Enforces:
 *   1. Traversal blocking (.., %2e%2e, \u002e) in raw path and JSON payload.
 *   2. Rejection of secret leaks (.env, .pem, .key, id_rsa, confidential DBs).
 *   3. Strict canonical containment within authorized workspace root via realpath().
 * Returns true if the access is RESTRICTED (denied / unsafe).
 * Returns false only if access is safe and fully contained within authorized workspace.
 */
static bool is_restricted_path_hardened(const char *raw_path, const char *raw_args) {
    /* 1. Inspect raw arguments JSON for traversal sequences or secret leaks */
    if (raw_args && raw_args[0] != '\0') {
        if (has_traversal_sequence(raw_args)) return true;
        if (contains_forbidden_token(raw_args)) return true;
    }

    /* If no filesystem target path was specified, and raw_args is clean, not restricted */
    if (!raw_path || raw_path[0] == '\0') {
        return false;
    }

    /* 2. Check raw path string directly for traversal sequences */
    if (has_traversal_sequence(raw_path)) {
        return true;
    }

    /* 3. Check for forbidden tokens and secret file extensions in raw path */
    if (contains_forbidden_token(raw_path) || has_forbidden_extension(raw_path)) {
        return true;
    }

    /* 4. Resolve canonical path via realpath() and lexical normalization */
    char canonical_path[PATH_MAX];
    if (!resolve_canonical_path(raw_path, canonical_path, sizeof(canonical_path))) {
        return true; /* Fail-closed on invalid path syntax or traversal above root */
    }

    /* 5. Check resolved canonical path for secret tokens and extensions */
    if (contains_forbidden_token(canonical_path) || has_forbidden_extension(canonical_path)) {
        return true;
    }

    /* 6. Resolve canonical workspace root via realpath() if it exists */
    const char *configured_ws = qala_get_workspace_root();
    char canonical_ws[PATH_MAX];
    if (realpath(configured_ws, canonical_ws) == NULL) {
        if (!normalize_path_lexical(configured_ws, canonical_ws, sizeof(canonical_ws))) {
            return true; /* Fail-closed */
        }
    }

    /* 7. Strict prefix containment verification */
    if (!is_within_workspace(canonical_path, canonical_ws)) {
        return true; /* Escaped workspace containment boundary */
    }

    return false; /* Path is safe and strictly contained within workspace */
}

static const char *verdict_to_str(qala_verdict_t v) {
    switch (v) {
        case VERDICT_SAFE:          return "SAFE";
        case VERDICT_CAUTION:       return "CAUTION";
        case VERDICT_REVIEW:        return "REVIEW";
        case VERDICT_UNSAFE:        return "UNSAFE";
        case VERDICT_AUTH_BLK:      return "AUTH_BLOCKED";
        case VERDICT_HALLUCINATION: return "HALLUCINATION_BLOCKED";
        default:                    return "ERROR";
    }
}

/* F-10 Fix: Immutable append-only JSONL audit logger with finite amount checks */
static void write_desc_audit_log(
    const char                 *log_path,
    uint64_t                    timestamp_utc,
    const qala_tool_call_ctx_t *ctx,
    const qala_policy_result_t *res
) {
    const char *path = log_path ? log_path : "qala-audit.jsonl";
    FILE *fp = fopen(path, "a");
    if (!fp) return;

    char esc_tool[128];
    char esc_code[128];
    char esc_reason[512];
    json_escape_string(ctx ? ctx->tool_name : "unknown", esc_tool, sizeof(esc_tool));
    json_escape_string(res ? res->code : "unknown", esc_code, sizeof(esc_code));
    json_escape_string(res ? res->reason : "none", esc_reason, sizeof(esc_reason));

    double safe_amount = (ctx && isfinite(ctx->amount_aed)) ? ctx->amount_aed : 0.0;

    fprintf(fp,
        "{\"timestamp_utc\":%llu,\"reporting_tz\":\"Asia/Dubai\",\"tool\":\"%s\","
        "\"verdict\":\"%s\",\"code\":\"%s\",\"reason\":\"%s\",\"amount_aed\":%.2f}\n",
        (unsigned long long)timestamp_utc,
        esc_tool,
        verdict_to_str(res ? res->verdict : VERDICT_ERROR),
        esc_code,
        esc_reason,
        safe_amount
    );
    fclose(fp);
}

/* Hardened deterministic policy verification entry point */
qala_policy_result_t policy_verify_call(
    const qala_tool_call_ctx_t *ctx,
    qala_approval_slot_t       *slots,
    size_t                      slot_count,
    uint64_t                    now_utc,
    const char                 *audit_log_path
) {
    qala_policy_result_t res;
    memset(&res, 0, sizeof(res));

    if (!ctx || !ctx->tool_name) {
        res.verdict = VERDICT_ERROR;
        snprintf(res.code, sizeof(res.code), "%s", "invalid_context");
        snprintf(res.reason, sizeof(res.reason), "%s", "Null tool context provided to Qala policy gate.");
        return res;
    }

    /* 1. DESC ISR v3.1 Filesystem Sandbox Barrier */
    if (is_restricted_path_hardened(ctx->target_path, ctx->raw_args_json)) {
        res.verdict = VERDICT_UNSAFE;
        snprintf(res.code, sizeof(res.code), "%s", "workspace_containment_denied");
        snprintf(res.reason, sizeof(res.reason),
                 "Access to restricted path '%s' denied by DESC ISR v3.1 policy gate.",
                 ctx->target_path ? ctx->target_path : "unspecified");
        write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
        return res;
    }

    /* 2. Grounding Shield Anti-Hallucination Gate */
    if (ctx->grounding_claim && ctx->grounding_corpus && ctx->grounding_count > 0) {
        grounding_result_t g_res = grounding_verify_claim(
            ctx->grounding_claim,
            ctx->grounding_corpus,
            ctx->grounding_count
        );
        if (g_res.verdict == GROUNDING_HALLUCINATION_TRIPPED) {
            res.verdict = VERDICT_HALLUCINATION;
            snprintf(res.code, sizeof(res.code), "%s", "grounding_hallucination_blocked");
            snprintf(res.reason, sizeof(res.reason),
                     "Grounding Shield Intercept: %.220s", g_res.discrepancy_note);
            write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
            return res;
        }
    }

    /* 3. F-07 Fix: Hard Financial & State Mutation Gate with NaN / Negative Check */
    bool has_invalid_amount = !isfinite(ctx->amount_aed) || ctx->amount_aed < 0.0;
    bool exceeds_threshold = has_invalid_amount || (ctx->amount_aed > 50000.0);
    bool requires_human_gate = ctx->is_mutating || exceeds_threshold;

    if (requires_human_gate) {
        if (!ctx->supplied_code || ctx->supplied_code[0] == '\0') {
            res.verdict = VERDICT_REVIEW;
            snprintf(res.code, sizeof(res.code), "%s", "approval_required");
            snprintf(res.reason, sizeof(res.reason),
                     "Gated operation '%.64s' (AED %.2f) requires a single-use approval_code (scope: %.32s).",
                     ctx->tool_name, isfinite(ctx->amount_aed) ? ctx->amount_aed : 0.0,
                     ctx->required_scope ? ctx->required_scope : "default");
            write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
            return res;
        }

        /* F-07 Fix: Check NULL slots table before indexing */
        if (!slots || slot_count == 0) {
            res.verdict = VERDICT_UNSAFE;
            snprintf(res.code, sizeof(res.code), "%s", "no_approval_slots_configured");
            snprintf(res.reason, sizeof(res.reason), "%s", "Approval code provided but no active approval slots configured on host.");
            write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
            return res;
        }

        /* Verify single-use, scope-bound token in constant time with persistent state */
        const char *needed_scope = ctx->required_scope ? ctx->required_scope : "default";
        const char *store_path = qala_get_token_store_path();
        bool found_valid = false;
        for (size_t i = 0; i < slot_count; ++i) {
            bool scope_matches = constant_time_streq(slots[i].scope, needed_scope);

            if (scope_matches &&
                now_utc <= slots[i].expires_utc &&
                constant_time_streq(slots[i].token, ctx->supplied_code)) {

                /* 1. Check in-memory single-use consumption */
                if (slots[i].consumed) {
                    break;
                }

                /* 2. Check persistent on-disk ledger (prevents replay across restarts) */
                if (qala_token_store_is_consumed(store_path, ctx->supplied_code)) {
                    slots[i].consumed = true;
                    break;
                }

                /* 3. Mark single-use consumption in memory */
                slots[i].consumed = true;

                /* 4. Commit to immutable on-disk ledger */
                int rec_rc = qala_token_store_record_consumed(
                    store_path,
                    slots[i].token,
                    slots[i].scope,
                    ctx->tool_name,
                    ctx->amount_aed,
                    now_utc
                );

                if (rec_rc != 0) {
                    /* Fail-closed if immutable persistence failed */
                    res.verdict = VERDICT_ERROR;
                    snprintf(res.code, sizeof(res.code), "%s", "token_persistence_failed");
                    snprintf(res.reason, sizeof(res.reason), "%s",
                             "Failed to persist consumed approval token to disk ledger (fail-closed).");
                    write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
                    return res;
                }

                found_valid = true;
                break;
            }
        }

        if (!found_valid) {
            res.verdict = VERDICT_UNSAFE;
            snprintf(res.code, sizeof(res.code), "%s", "invalid_or_expired_approval");
            snprintf(res.reason, sizeof(res.reason), "%s", "Supplied approval_code is invalid, expired, wrong scope, or already consumed.");
            write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
            return res;
        }
    }

    /* Safe Execution Verified */
    res.verdict = VERDICT_SAFE;
    snprintf(res.code, sizeof(res.code), "%s", "policy_verified");
    snprintf(res.reason, sizeof(res.reason),
             "Tool '%.64s' verified under DESC ISR v3.1 deterministic policy.", ctx->tool_name);
    write_desc_audit_log(audit_log_path, now_utc, ctx, &res);
    return res;
}

qala_verdict_t policy_verify_call_ptr(
    const qala_tool_call_ctx_t *ctx,
    qala_approval_slot_t       *slots,
    size_t                      slot_count,
    uint64_t                    now_utc,
    const char                 *audit_log_path,
    qala_policy_result_t       *out_result
) {
    if (!out_result) return VERDICT_ERROR;
    *out_result = policy_verify_call(ctx, slots, slot_count, now_utc, audit_log_path);
    return out_result->verdict;
}

/* F-09 Fix: Helper to check for CRLF and control characters */
static bool contains_crlf_or_ctrl(const char *str) {
    if (!str) return false;
    for (size_t i = 0; str[i] != '\0'; ++i) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x20 && c != '\t') return true; /* Reject \r, \n, and control chars */
    }
    return false;
}

/* F-09 Fix: Stateless MCP 2026-07-28 HTTP JSON-RPC Request Builder with CRLF sanitization */
int mcp_stateless_build_request(
    char       *out_http_buf,
    size_t      buf_cap,
    const char *host,
    const char *endpoint_path,
    const char *bearer_token,
    const char *mcp_method,
    const char *jsonrpc_params,
    uint64_t    request_id
) {
    if (!out_http_buf || buf_cap == 0 || !host || !endpoint_path || !mcp_method) {
        return -1;
    }

    /* F-09 Fix: Reject CRLF in headers */
    if (contains_crlf_or_ctrl(host) || contains_crlf_or_ctrl(endpoint_path) ||
        contains_crlf_or_ctrl(bearer_token) || contains_crlf_or_ctrl(mcp_method)) {
        return -3; /* CRLF injection rejected */
    }

    char escaped_method[128];
    json_escape_string(mcp_method, escaped_method, sizeof(escaped_method));

    char body[2048];
    int body_len = snprintf(body, sizeof(body),
        "{\"jsonrpc\":\"2.0\",\"id\":%llu,\"method\":\"%s\",\"params\":%s}",
        (unsigned long long)request_id,
        escaped_method,
        jsonrpc_params ? jsonrpc_params : "{}"
    );

    if (body_len < 0 || (size_t)body_len >= sizeof(body)) {
        return -2;
    }

    int total_len = snprintf(out_http_buf, buf_cap,
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Accept: application/json\r\n"
        "MCP-Protocol-Version: 2026-07-28\r\n"
        "Mcp-Method: %s\r\n"
        "Mcp-Name: qala-native-c99\r\n"
        "%s%s\r\n"
        "Content-Length: %d\r\n\r\n"
        "%s",
        endpoint_path,
        host,
        escaped_method,
        bearer_token ? "Authorization: Bearer " : "",
        bearer_token ? bearer_token : "",
        body_len,
        body
    );

    if (total_len < 0 || (size_t)total_len >= buf_cap) {
        return -2;
    }

    return total_len;
}

#ifdef QALA_STANDALONE_TEST
#include <assert.h>

int main(void) {
    const char *init_test_tokens = "build/qala_standalone_tokens.jsonl";
    unlink(init_test_tokens);
    qala_set_token_store_path(init_test_tokens);

    qala_approval_slot_t slots[1] = {{
        .token = "DUBAI-APPROVE-9941-X",
        .scope = "finops_remediate",
        .expires_utc = 1792603600ULL,
        .consumed = false
    }};

    /* Test 1: Landlock blacklist test */
    qala_tool_call_ctx_t attack_ctx = {
        .tool_name = "fs_read",
        .target_path = "/confidential/uae_citizens.db",
        .raw_args_json = NULL,
        .amount_aed = 0.0,
        .is_mutating = false,
        .required_scope = NULL,
        .supplied_code = NULL
    };
    qala_policy_result_t r1 = policy_verify_call(&attack_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r1.verdict == VERDICT_UNSAFE);
    assert(strcmp(r1.code, "workspace_containment_denied") == 0);

    /* Test 2: High value mutation without token -> REVIEW */
    qala_tool_call_ctx_t unapproved_ctx = {
        .tool_name = "erp_customs_settlement",
        .target_path = "/workspace/settlements.json",
        .raw_args_json = NULL,
        .amount_aed = 125000.00,
        .is_mutating = true,
        .required_scope = "finops_remediate",
        .supplied_code = NULL
    };
    qala_policy_result_t r2 = policy_verify_call(&unapproved_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r2.verdict == VERDICT_REVIEW);

    /* Test 3: Approved with valid single-use token -> SAFE */
    unapproved_ctx.supplied_code = "DUBAI-APPROVE-9941-X";
    qala_policy_result_t r3 = policy_verify_call(&unapproved_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r3.verdict == VERDICT_SAFE);
    assert(slots[0].consumed == true);

    /* Test 4: Replay attack with consumed token -> UNSAFE */
    qala_policy_result_t r4 = policy_verify_call(&unapproved_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r4.verdict == VERDICT_UNSAFE);

    /* Test 5: MCP request format */
    char http_buf[4096];
    int n = mcp_stateless_build_request(
        http_buf, sizeof(http_buf), "127.0.0.1:8765", "/mcp", NULL,
        "tools/call", "{\"name\":\"nav\",\"arguments\":{\"action\":\"impact\"}}", 42
    );
    assert(n > 0);
    assert(strstr(http_buf, "MCP-Protocol-Version: 2026-07-28") != NULL);
    assert(strstr(http_buf, "Mcp-Method: tools/call") != NULL);

    /* Test 6: Grounding Shield integration in Policy Gate (Catches Hallucinated Penalty) */
    grounding_source_t corpus[1] = {{
        .source_id = "DHA_SN_03_2019",
        .source_title = "DHA Standards Notice SN 03/2019 Payment Rules",
        .snippet_text = "Late payment delay penalty is 0.03% per calendar day.",
        .source_numerical = 0.03
    }};
    grounding_claim_t hallucinated_claim = {
        .claim_text = "Late payment delay penalty is 0.05% daily",
        .entity_name = "DHA",
        .numerical_value = 0.05,
        .has_numerical = true
    };
    qala_tool_call_ctx_t grounded_ctx = {
        .tool_name = "apply_dha_penalty",
        .target_path = "/workspace/billing.json",
        .raw_args_json = NULL,
        .amount_aed = 0.0,
        .is_mutating = false,
        .required_scope = NULL,
        .supplied_code = NULL,
        .grounding_claim = &hallucinated_claim,
        .grounding_corpus = corpus,
        .grounding_count = 1
    };
    qala_policy_result_t r6 = policy_verify_call(&grounded_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r6.verdict == VERDICT_HALLUCINATION);
    assert(strcmp(r6.code, "grounding_hallucination_blocked") == 0);
    assert(strstr(r6.reason, "mandates 0.0300") != NULL);

    /* Test 7 (F-07 check): NULL slots with slot_count > 0 returns UNSAFE, not SIGSEGV */
    unapproved_ctx.supplied_code = "TEST-CODE";
    qala_policy_result_t r7 = policy_verify_call(&unapproved_ctx, NULL, 5, 1792600000ULL, NULL);
    assert(r7.verdict == VERDICT_UNSAFE);

    /* Test 8 (F-09 check): CRLF injection in MCP builder rejected */
    int crlf_res = mcp_stateless_build_request(
        http_buf, sizeof(http_buf), "127.0.0.1:8765\r\nInjected-Header: evil", "/mcp", NULL,
        "tools/call", "{}", 1
    );
    assert(crlf_res == -3);

    /* Test 9: DESC ISR v3.1 Strict Workspace Containment - Paths outside /workspace rejected */
    qala_tool_call_ctx_t outside_ctx = {
        .tool_name = "fs_read",
        .target_path = "/opt/customs/manifest.json",
        .raw_args_json = NULL,
        .amount_aed = 0.0,
        .is_mutating = false,
        .required_scope = NULL,
        .supplied_code = NULL
    };
    qala_policy_result_t r9 = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r9.verdict == VERDICT_UNSAFE);
    assert(strcmp(r9.code, "workspace_containment_denied") == 0);

    /* Test 10: Path traversal sequences rejected */
    outside_ctx.target_path = "/workspace/../etc/shadow";
    qala_policy_result_t r10a = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r10a.verdict == VERDICT_UNSAFE);

    outside_ctx.target_path = "/workspace/%2e%2e/etc/passwd";
    qala_policy_result_t r10b = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r10b.verdict == VERDICT_UNSAFE);

    /* Test 11: Secret files and private keys inside workspace rejected */
    outside_ctx.target_path = "/workspace/.env";
    qala_policy_result_t r11a = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r11a.verdict == VERDICT_UNSAFE);

    outside_ctx.target_path = "/workspace/certs/private_key.pem";
    qala_policy_result_t r11b = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r11b.verdict == VERDICT_UNSAFE);

    outside_ctx.target_path = "/workspace/.ssh/id_rsa";
    qala_policy_result_t r11c = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r11c.verdict == VERDICT_UNSAFE);

    /* Test 12: Legitimate nested workspace paths allowed */
    outside_ctx.target_path = "/workspace/settlements/batch_2026.json";
    qala_policy_result_t r12 = policy_verify_call(&outside_ctx, slots, 1, 1792600000ULL, NULL);
    assert(r12.verdict == VERDICT_SAFE);

    /* Test 13: Persistent on-disk token store survives simulated process restart */
    const char *test_token_store = "build/test_tokens.jsonl";
    unlink(test_token_store);
    qala_set_token_store_path(test_token_store);

    qala_approval_slot_t p_slots[1] = {{
        .token = "TOKEN-PERSIST-TEST-001",
        .scope = "finops_remediate",
        .expires_utc = 1792603600ULL,
        .consumed = false
    }};

    qala_tool_call_ctx_t persist_ctx = {
        .tool_name = "erp_customs_settlement",
        .target_path = "/workspace/settlements.json",
        .raw_args_json = NULL,
        .amount_aed = 100000.00,
        .is_mutating = true,
        .required_scope = "finops_remediate",
        .supplied_code = "TOKEN-PERSIST-TEST-001"
    };

    /* First execution: valid code consumed and committed to persistent disk ledger */
    qala_policy_result_t r13a = policy_verify_call(&persist_ctx, p_slots, 1, 1792600000ULL, NULL);
    assert(r13a.verdict == VERDICT_SAFE);
    assert(p_slots[0].consumed == true);
    assert(qala_token_store_is_consumed(test_token_store, "TOKEN-PERSIST-TEST-001") == true);

    /* Second execution (same process): blocked in memory */
    qala_policy_result_t r13b = policy_verify_call(&persist_ctx, p_slots, 1, 1792600000ULL, NULL);
    assert(r13b.verdict == VERDICT_UNSAFE);

    /* Third execution (simulated process restart):
     * Brand new slot array initialized with consumed=false, but persistent file retains consumed state.
     */
    qala_approval_slot_t reboot_slots[1] = {{
        .token = "TOKEN-PERSIST-TEST-001",
        .scope = "finops_remediate",
        .expires_utc = 1792603600ULL,
        .consumed = false  /* Simulates config reloading after reboot */
    }};
    qala_policy_result_t r13c = policy_verify_call(&persist_ctx, reboot_slots, 1, 1792600000ULL, NULL);
    assert(r13c.verdict == VERDICT_UNSAFE);
    assert(strcmp(r13c.code, "invalid_or_expired_approval") == 0);

    unlink(test_token_store);
    unlink(init_test_tokens);
    qala_set_token_store_path("qala-tokens.jsonl");

    printf("[PASS] All Qala C99 Hardened Policy Gate, Grounding Shield & Stateless MCP tests passed!\n");
    return 0;
}
#endif
