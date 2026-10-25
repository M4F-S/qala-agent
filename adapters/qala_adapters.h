/* qala_adapters.h - Enterprise Policy & Stateless MCP Adapter Layer for Qala
 * Target: Best Business AI Agent | Dubai Create AI Agents Championship 2026-2027
 */
#ifndef QALA_ADAPTERS_H
#define QALA_ADAPTERS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "grounding_shield.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Standard Qala Verdict Envelopes */
typedef enum {
    VERDICT_SAFE            = 0, /* Safe to execute immediately */
    VERDICT_CAUTION         = 1, /* Low-risk warning logged; execution permitted */
    VERDICT_REVIEW          = 2, /* Gated mutation: requires single-use approval code */
    VERDICT_UNSAFE          = 3, /* Hard block: Landlock / DESC ISR v3 violation */
    VERDICT_AUTH_BLK        = 4, /* Upstream credential missing or expired */
    VERDICT_HALLUCINATION   = 5, /* Grounding Shield blocked unverified/hallucinated claim */
    VERDICT_ERROR           = 6  /* Malformed tool arguments or transport fault */
} qala_verdict_t;

/* Single-use, scope-bound human approval token slot */
typedef struct {
    char     token[64];       /* Cryptographic code issued via Telegram/Slack */
    char     scope[32];       /* e.g., "finops_remediate", "prod_deploy" */
    uint64_t expires_utc;     /* Unix epoch expiration timestamp (UTC) */
    bool     consumed;        /* Strictly single-use */
} qala_approval_slot_t;

/* Tool call inspection context */
typedef struct {
    const char               *tool_name;        /* e.g., "fs_read", "erp_customs_settlement" */
    const char               *target_path;      /* Target filesystem path */
    const char               *raw_args_json;    /* Raw JSON arguments payload */
    double                    amount_aed;       /* Transaction value in AED */
    bool                      is_mutating;      /* True if tool modifies state/database/APIs */
    const char               *required_scope;   /* Required scope token */
    const char               *supplied_code;    /* Approval code supplied by caller */
    const grounding_claim_t  *grounding_claim;  /* Optional factual claim to verify */
    const grounding_source_t *grounding_corpus; /* Retrieved authoritative references */
    size_t                    grounding_count;  /* Number of grounding reference sources */
} qala_tool_call_ctx_t;

/* Result envelope */
typedef struct {
    qala_verdict_t verdict;
    char           code[64];      /* Contract error code */
    char           reason[256];   /* Human & HUD explanation */
} qala_policy_result_t;

/* Function Prototypes */
qala_policy_result_t policy_verify_call(
    const qala_tool_call_ctx_t *ctx,
    qala_approval_slot_t       *slots,
    size_t                      slot_count,
    uint64_t                    now_utc,
    const char                 *audit_log_path
);

/* F-15 Fix: Pointer-based ABI export to prevent large struct return-by-value across compiler/FFI boundaries */
qala_verdict_t policy_verify_call_ptr(
    const qala_tool_call_ctx_t *ctx,
    qala_approval_slot_t       *slots,
    size_t                      slot_count,
    uint64_t                    now_utc,
    const char                 *audit_log_path,
    qala_policy_result_t       *out_result
);

int mcp_stateless_build_request(
    char       *out_http_buf,
    size_t      buf_cap,
    const char *host,
    const char *endpoint_path,
    const char *bearer_token,
    const char *mcp_method,
    const char *jsonrpc_params,
    uint64_t    request_id
);

#ifdef __cplusplus
}
#endif

#endif /* QALA_ADAPTERS_H */
