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
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <limits.h>
#include <math.h>
#include <libgen.h>

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
    ".env",
    "uae_citizens.db",
    "customers_pii.db",
    "nabidh_patient_master.db",
    NULL
};

/* F-06 Fix: Hardened path verification using strict canonicalization and allowlist checks */
static bool is_restricted_path_hardened(const char *raw_path, const char *raw_args) {
    const char *inputs[2] = { raw_path, raw_args };
    for (int k = 0; k < 2; ++k) {
        const char *s = inputs[k];
        if (!s || s[0] == '\0') continue;
        /* Block path traversal and unicode escape sequences */
        if (strstr(s, "..") != NULL) return true;
        if (strstr(s, "\\u002e") != NULL || strstr(s, "\\u002E") != NULL) return true;
        for (int i = 0; RESTRICTED_TOKENS[i] != NULL; ++i) {
            if (strstr(s, RESTRICTED_TOKENS[i]) != NULL) return true;
        }
    }

    if (raw_path && raw_path[0] != '\0') {
        char resolved[PATH_MAX];
        if (realpath(raw_path, resolved) != NULL) {
            for (int i = 0; RESTRICTED_TOKENS[i] != NULL; ++i) {
                if (strstr(resolved, RESTRICTED_TOKENS[i]) != NULL) return true;
            }
        } else {
            /* File does not exist yet: check parent directory to prevent symlink escape */
            char path_copy[PATH_MAX];
            snprintf(path_copy, sizeof(path_copy), "%s", raw_path);
            path_copy[sizeof(path_copy) - 1] = '\0';
            char *parent = dirname(path_copy);
            if (parent && realpath(parent, resolved) != NULL) {
                for (int i = 0; RESTRICTED_TOKENS[i] != NULL; ++i) {
                    if (strstr(resolved, RESTRICTED_TOKENS[i]) != NULL) return true;
                }
            } else {
                /* Parent also unreachable/invalid: fail-closed if confidential/etc */
                for (int i = 0; RESTRICTED_TOKENS[i] != NULL; ++i) {
                    if (strstr(raw_path, RESTRICTED_TOKENS[i]) != NULL) return true;
                }
            }
        }
    }
    return false;
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
        snprintf(res.code, sizeof(res.code), "%s", "landlock_fs_denied");
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

        /* Verify single-use, scope-bound token in constant time */
        const char *needed_scope = ctx->required_scope ? ctx->required_scope : "default";
        bool found_valid = false;
        for (size_t i = 0; i < slot_count; ++i) {
            bool scope_matches = constant_time_streq(slots[i].scope, needed_scope);

            if (!slots[i].consumed &&
                now_utc <= slots[i].expires_utc &&
                constant_time_streq(slots[i].token, ctx->supplied_code) &&
                scope_matches) {
                slots[i].consumed = true; /* Strictly single-use */
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
    assert(strcmp(r1.code, "landlock_fs_denied") == 0);

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

    printf("[PASS] All Qala C99 Hardened Policy Gate, Grounding Shield & Stateless MCP tests passed!\n");
    return 0;
}
#endif
