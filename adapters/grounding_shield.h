/* grounding_shield.h - Deterministic C99 Search-Verification & Fact-Checking Engine
 * Target: Matches Cloud Model Accuracy via Grounded Verification Loops
 * Compliance: DESC ISR v3.1 Privacy-Preserving Search & Zero-Data-Leakage Rule
 */
#ifndef GROUNDING_SHIELD_H
#define GROUNDING_SHIELD_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GROUNDING_VERIFIED            = 0, /* Claim matches authoritative reference */
    GROUNDING_HALLUCINATION_TRIPPED = 1, /* Discrepancy detected (numbers/dates/codes) */
    GROUNDING_UNVERIFIABLE        = 2, /* Reference lacks sufficient grounding data */
    GROUNDING_PII_BLOCKED         = 3, /* Query contained PII and was safely redacted */
    GROUNDING_ERROR               = 4  /* Malformed input */
} grounding_verdict_t;

/* Extracted factual claim structure */
typedef struct {
    const char *claim_text;       /* e.g., "DHA Ejadah delay penalty is 0.03% per day" */
    const char *entity_name;      /* e.g., "DHA Ejadah", "DESC ISR v3.1" */
    double      numerical_value;  /* Extracted key metric (e.g., 0.03, 50000.0) */
    bool        has_numerical;    /* True if claim hinges on a specific quantity */
} grounding_claim_t;

/* Authoritative reference source snippet */
typedef struct {
    const char *source_id;        /* e.g., "DHA_SN_03_2019", "DESC_ISR_V3.1_SEC_4" */
    const char *source_title;     /* Formal regulation or manual title */
    const char *snippet_text;     /* Authoritative factual text */
    double      source_numerical; /* Verified statutory number in source */
} grounding_source_t;

/* Verification result envelope */
typedef struct {
    grounding_verdict_t verdict;
    double              confidence;           /* 0.0 to 1.0 */
    char                citation[128];        /* Matched source ID */
    char                discrepancy_note[256];/* Detailed explanation if hallucination */
} grounding_result_t;

/* Function Prototypes */

/**
 * Scrubs user PII and extracts generalized regulatory search keywords
 * to ensure 100% compliance with DESC ISR v3.1 zero-exfiltration mandates.
 */
size_t grounding_scrub_search_query(
    const char *raw_user_prompt,
    char       *out_scrubbed_query,
    size_t      out_cap
);

/**
 * AF-02: Non-destructive in-place PII redactor. Copies UTF-8 prompts verbatim
 * while redacting only Emirates IDs, DOBs, phone numbers, emails, and patient names.
 * Preserves all clinical Arabic text, dosages, prescription names, ICD-10 codes, and numbers.
 */
size_t grounding_redact_prompt_pii_c99(
    const char *src,
    char       *dst,
    size_t      dst_cap
);

/**
 * Deterministically cross-checks a generated hypothesis claim against
 * retrieved authoritative sources.
 */
grounding_result_t grounding_verify_claim(
    const grounding_claim_t  *claim,
    const grounding_source_t *sources,
    size_t                    source_count
);

/* F-15 Fix: Pointer-based ABI export to prevent large struct return-by-value across compiler/FFI boundaries */
grounding_verdict_t grounding_verify_claim_ptr(
    const grounding_claim_t  *claim,
    const grounding_source_t *sources,
    size_t                    source_count,
    grounding_result_t       *out_result
);

#ifdef __cplusplus
}
#endif

#endif /* GROUNDING_SHIELD_H */
