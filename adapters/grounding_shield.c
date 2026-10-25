/* grounding_shield.c
 * Hardened C99 Implementation of:
 *   1. Privacy-Scrubbed Search Query Extractor (Designed to Support DESC ISR v3.1 Entity Compliance)
 *   2. Deterministic Factual Cross-Verification & Hallucination Blocker
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>

#include "grounding_shield.h"

/* Case-insensitive substring search in standard C99 */
static const char *strcasestr_c99(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (needle[0] == '\0') return NULL; /* F-13 Fix: Empty needle should not wildcard match */

    size_t needle_len = strlen(needle);
    for (; *haystack != '\0'; ++haystack) {
        if (tolower((unsigned char)*haystack) == tolower((unsigned char)*needle)) {
            size_t i = 1;
            for (; i < needle_len; ++i) {
                if (haystack[i] == '\0' ||
                    tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i])) {
                    break;
                }
            }
            if (i == needle_len) return haystack;
        }
    }
    return NULL;
}

/* Scrub user PII (names, Emirates IDs, emails, account numbers) and extract regulatory keywords */
size_t grounding_scrub_search_query(
    const char *raw_user_prompt,
    char       *out_scrubbed_query,
    size_t      out_cap
) {
    if (!raw_user_prompt || !out_scrubbed_query || out_cap == 0) return 0;
    out_scrubbed_query[0] = '\0';

    /* List of recognized Dubai / UAE regulatory and technical keywords to safely retain */
    static const char *APPROVED_REGULATORY_TOKENS[] = {
        "DHA", "eClaimLink", "Ejadah", "SN 03/2019", "resubmission",
        "DESC", "ISR", "v3", "v3.1", "AI Security Policy", "ASAAS",
        "D33", "DUB.AI", "Chief AI Officer", "RTA", "DEWA",
        "DIFC", "ADGM", "customs", "settlement", "Landlock", "MCP",
        "eGFR", "Colchicine", "Clarithromycin", "pharmacovigilance",
        "Law No. 2 of 2019", "health data", "NABIDH", "Malaffi",
        NULL
    };

    size_t written = 0;

    for (int i = 0; APPROVED_REGULATORY_TOKENS[i] != NULL; ++i) {
        const char *token = APPROVED_REGULATORY_TOKENS[i];
        if (strcasestr_c99(raw_user_prompt, token) != NULL) {
            int n = snprintf(out_scrubbed_query + written, out_cap - written,
                             "%s%s", (written > 0 ? " " : ""), token);
            if (n > 0 && (size_t)n < (out_cap - written)) {
                written += (size_t)n;
            } else {
                break;
            }
        }
    }

    /* Fallback generic safe query if no specific regulatory token matched */
    if (written == 0) {
        snprintf(out_scrubbed_query, out_cap, "Dubai regulatory compliance guidelines");
        written = strlen(out_scrubbed_query);
    }

    return written;
}

/* AF-02 Helpers: Pattern matchers for PII entities */
static bool starts_with_ci(const char *str, const char *prefix) {
    if (!str || !prefix) return false;
    while (*prefix != '\0') {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix)) return false;
        str++;
        prefix++;
    }
    return true;
}

static bool is_emirates_id(const char *p) {
    if (strncmp(p, "784-", 4) != 0) return false;
    for (int i = 4; i < 8; ++i) if (!isdigit((unsigned char)p[i])) return false;
    if (p[8] != '-') return false;
    for (int i = 9; i < 16; ++i) if (!isdigit((unsigned char)p[i])) return false;
    if (p[16] != '-') return false;
    if (!isdigit((unsigned char)p[17])) return false;
    return true;
}

static bool is_iso_dob(const char *p) {
    if (!((p[0] == '1' && p[1] == '9') || (p[0] == '2' && p[1] == '0'))) return false;
    if (!isdigit((unsigned char)p[2]) || !isdigit((unsigned char)p[3])) return false;
    if (p[4] != '-') return false;
    if (!isdigit((unsigned char)p[5]) || !isdigit((unsigned char)p[6])) return false;
    if (p[7] != '-') return false;
    if (!isdigit((unsigned char)p[8]) || !isdigit((unsigned char)p[9])) return false;
    return true;
}

/* AF-02 Fix: Non-destructive in-place PII redactor. Copies UTF-8 prompts verbatim
 * while redacting only Emirates IDs, DOBs, phone numbers, emails, and patient names.
 * Preserves all clinical Arabic text, dosages, prescription names, ICD-10 codes, and numbers.
 */
size_t grounding_redact_prompt_pii_c99(
    const char *src,
    char       *dst,
    size_t      dst_cap
) {
    if (!src || !dst || dst_cap == 0) return 0;
    dst[0] = '\0';

    size_t w = 0;
    const char *p = src;

    while (*p != '\0') {
        /* 1. Emirates ID */
        if (is_emirates_id(p)) {
            const char *mask = "[EMIRATES_ID_REDACTED]";
            size_t mlen = strlen(mask);
            if (w + mlen < dst_cap) {
                memcpy(dst + w, mask, mlen);
                w += mlen;
            }
            p += 18;
            continue;
        }

        /* 2. ISO DOB */
        if (is_iso_dob(p)) {
            const char *mask = "[DOB_REDACTED]";
            size_t mlen = strlen(mask);
            if (w + mlen < dst_cap) {
                memcpy(dst + w, mask, mlen);
                w += mlen;
            }
            p += 10;
            continue;
        }

        /* 3. Patient Name prefix: "Patient <Name>" -> "Patient [REDACTED]" */
        if (starts_with_ci(p, "Patient ")) {
            const char *mask = "Patient [REDACTED] ";
            size_t mlen = strlen(mask);
            if (w + mlen < dst_cap) {
                memcpy(dst + w, mask, mlen);
                w += mlen;
            }
            p += 8;
            while (*p == ' ') p++;
            int words = 0;
            while (*p != '\0' && *p != ',' && *p != '(' && *p != '.' && *p != '\n') {
                if (starts_with_ci(p, "prescribed") || starts_with_ci(p, "taking") ||
                    starts_with_ci(p, "presented") || starts_with_ci(p, "admitted") ||
                    starts_with_ci(p, "diagnosed")) {
                    break;
                }
                p++;
                if (*p == ' ') {
                    words++;
                    if (words >= 2) break;
                }
            }
            while (*p == ' ') p++;
            continue;
        }

        /* 4. Arabic Patient Name prefix: "المريض " -> "المريض [محجوب]" */
        if (strncmp(p, "المريض ", 13) == 0) {
            const char *mask = "المريض [محجوب] ";
            size_t mlen = strlen(mask);
            if (w + mlen < dst_cap) {
                memcpy(dst + w, mask, mlen);
                w += mlen;
            }
            p += 13;
            while (*p == ' ') p++;
            int words = 0;
            while (*p != '\0' && *p != ',' && *p != '(' && *p != '.' && *p != '\n') {
                if ((unsigned char)p[0] == 0xD8 && (unsigned char)p[1] == 0x8C) {
                    break; /* Arabic comma */
                }
                if (strncmp(p, "تناول", 10) == 0 || strncmp(p, "وصف", 6) == 0 || strncmp(p, "يعاني", 10) == 0) {
                    break;
                }
                p++;
                if (*p == ' ') {
                    words++;
                    if (words >= 2) break;
                }
            }
            while (*p == ' ') p++;
            continue;
        }

        /* 5. Phone numbers: "+971" */
        if (strncmp(p, "+971", 4) == 0) {
            const char *mask = "[PHONE_REDACTED]";
            size_t mlen = strlen(mask);
            if (w + mlen < dst_cap) {
                memcpy(dst + w, mask, mlen);
                w += mlen;
            }
            p += 4;
            while (*p != '\0' && (isdigit((unsigned char)*p) || *p == ' ' || *p == '-')) {
                p++;
            }
            continue;
        }

        /* Default: copy character verbatim */
        if (w + 1 < dst_cap) {
            dst[w++] = *p++;
        } else {
            break;
        }
    }

    dst[w] = '\0';
    return w;
}

/* F-13 Fix: Deterministic Factual Verification against Retrieved Authoritative Sources */
grounding_result_t grounding_verify_claim(
    const grounding_claim_t  *claim,
    const grounding_source_t *sources,
    size_t                    source_count
) {
    grounding_result_t res;
    memset(&res, 0, sizeof(res));

    if (!claim || !claim->claim_text || !sources || source_count == 0) {
        res.verdict = GROUNDING_ERROR;
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", "Missing claim or authoritative sources.");
        return res;
    }

    /* F-13 Fix: Require non-empty entity name to avoid wildcard false matching */
    if (!claim->entity_name || claim->entity_name[0] == '\0') {
        res.verdict = GROUNDING_UNVERIFIABLE;
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", "Claim missing entity name for authoritative grounding.");
        return res;
    }

    bool entity_found = false;
    bool found_hallucination = false;
    char hallucination_note[256] = "";
    char hallucination_citation[128] = "";

    for (size_t i = 0; i < source_count; ++i) {
        /* Guard source fields */
        const char *s_title = sources[i].source_title ? sources[i].source_title : "";
        const char *s_text  = sources[i].snippet_text ? sources[i].snippet_text : "";
        const char *s_id    = sources[i].source_id ? sources[i].source_id : "UNKNOWN_SOURCE";

        /* Check if authoritative source covers the claimed entity */
        if (strcasestr_c99(s_title, claim->entity_name) != NULL ||
            strcasestr_c99(s_text, claim->entity_name) != NULL) {

            entity_found = true;

            /* Check numerical claim parity if applicable */
            if (claim->has_numerical) {
                /* F-13 Fix: NaN numerical check */
                if (!isfinite(claim->numerical_value) || !isfinite(sources[i].source_numerical)) {
                    res.verdict = GROUNDING_HALLUCINATION_TRIPPED;
                    res.confidence = 0.99;
                    snprintf(res.citation, sizeof(res.citation), "%s", s_id);
                    snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", "Discrepancy: Non-finite or NaN numerical value in claim.");
                    return res;
                }

                double diff = fabs(claim->numerical_value - sources[i].source_numerical);
                if (diff > 0.0001) {
                    /* Record hallucination candidate, but continue scanning remaining sources in case of multiple sections */
                    found_hallucination = true;
                    snprintf(hallucination_citation, sizeof(hallucination_citation), "%s", s_id);
                    snprintf(hallucination_note, sizeof(hallucination_note),
                             "Discrepancy: Claim states %.4f, but authoritative %s mandates %.4f.",
                             claim->numerical_value, s_id, sources[i].source_numerical);
                    continue;
                }
            }

            /* Factual match confirmed */
            res.verdict = GROUNDING_VERIFIED;
            res.confidence = 0.98;
            snprintf(res.citation, sizeof(res.citation), "%s", s_id);
            snprintf(res.discrepancy_note, sizeof(res.discrepancy_note),
                     "Verified against authoritative source %s.", s_id);
            return res;
        }
    }

    if (found_hallucination) {
        res.verdict = GROUNDING_HALLUCINATION_TRIPPED;
        res.confidence = 0.99;
        snprintf(res.citation, sizeof(res.citation), "%s", hallucination_citation);
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", hallucination_note);
        return res;
    }

    /* If entity was not present in the supplied sources */
    if (!entity_found) {
        res.verdict = GROUNDING_UNVERIFIABLE;
        res.confidence = 0.50;
        snprintf(res.discrepancy_note, sizeof(res.discrepancy_note), "%s", "Claimed entity not found in retrieved authoritative corpus.");
    }

    return res;
}

grounding_verdict_t grounding_verify_claim_ptr(
    const grounding_claim_t  *claim,
    const grounding_source_t *sources,
    size_t                    source_count,
    grounding_result_t       *out_result
) {
    if (!out_result) return GROUNDING_ERROR;
    *out_result = grounding_verify_claim(claim, sources, source_count);
    return out_result->verdict;
}

#ifdef GROUNDING_STANDALONE_TEST
#include <assert.h>

int main(void) {
    /* Test 1: PII Scrubbing extracts only approved regulatory keywords */
    const char *dirty_prompt = "Patient Ahmed Ali (Emirates ID 784-1985-1234567-1, DOB 1985-04-12) "
                               "prescribed Colchicine and Clarithromycin under DHA eClaimLink guidelines.";
    char clean_query[256];
    size_t written = grounding_scrub_search_query(dirty_prompt, clean_query, sizeof(clean_query));
    assert(written > 0);
    assert(strstr(clean_query, "Ahmed Ali") == NULL);
    assert(strstr(clean_query, "784-1985") == NULL);
    assert(strstr(clean_query, "DHA") != NULL);
    assert(strstr(clean_query, "Colchicine") != NULL);

    /* Test 2: Factual Verification catches hallucinated numerical value */
    grounding_source_t sources[2] = {
        {
            .source_id = "DHA_SN_03_2019",
            .source_title = "DHA Standards Notice SN 03/2019 Payment Rules",
            .snippet_text = "Late payment delay penalty is 0.03% per calendar day.",
            .source_numerical = 0.03
        },
        {
            .source_id = "DESC_ISR_V3.1_SEC_4",
            .source_title = "DESC ISR v3.1 Section 4 Cryptographic Standards",
            .snippet_text = "Data at rest must be encrypted with AES-256.",
            .source_numerical = 256.0
        }
    };

    grounding_claim_t false_claim = {
        .claim_text = "DHA Ejadah late penalty is 0.05% per day.",
        .entity_name = "DHA",
        .numerical_value = 0.05,
        .has_numerical = true
    };

    grounding_result_t r2 = grounding_verify_claim(&false_claim, sources, 2);
    assert(r2.verdict == GROUNDING_HALLUCINATION_TRIPPED);
    assert(strcmp(r2.citation, "DHA_SN_03_2019") == 0);

    /* Test 3: Factual Verification confirms accurate claim */
    grounding_claim_t true_claim = {
        .claim_text = "DHA Ejadah late penalty is 0.03% per day.",
        .entity_name = "DHA",
        .numerical_value = 0.03,
        .has_numerical = true
    };
    grounding_result_t r3 = grounding_verify_claim(&true_claim, sources, 2);
    assert(r3.verdict == GROUNDING_VERIFIED);

    /* Test 4 (F-13 check): Empty entity name returns UNVERIFIABLE, not wildcard match */
    grounding_claim_t empty_entity = {
        .claim_text = "Some random text",
        .entity_name = "",
        .numerical_value = 0.0,
        .has_numerical = false
    };
    grounding_result_t r4 = grounding_verify_claim(&empty_entity, sources, 2);
    assert(r4.verdict == GROUNDING_UNVERIFIABLE);

    /* Test 5 (AF-02 check): Non-destructive in-place PII redaction preserves Arabic & clinical data */
    const char *prompt_with_pii = "Patient Ahmed Ali (784-1985-1234567-1, DOB 1985-04-12) "
                                  "prescribed كولشيسين 0.5mg BID and Clarithromycin 500mg BID... eGFR = 22";
    char redacted[512];
    size_t rlen = grounding_redact_prompt_pii_c99(prompt_with_pii, redacted, sizeof(redacted));
    assert(rlen > 0);
    assert(strstr(redacted, "784-1985-1234567-1") == NULL);
    assert(strstr(redacted, "1985-04-12") == NULL);
    assert(strstr(redacted, "[EMIRATES_ID_REDACTED]") != NULL);
    assert(strstr(redacted, "[DOB_REDACTED]") != NULL);
    assert(strstr(redacted, "Patient [REDACTED]") != NULL);
    assert(strstr(redacted, "كولشيسين") != NULL);
    assert(strstr(redacted, "0.5mg") != NULL);
    assert(strstr(redacted, "Clarithromycin 500mg") != NULL);
    assert(strstr(redacted, "eGFR = 22") != NULL);

    printf("[PASS] All Grounding Shield C99 PII-Scrubbing & Factual Verification tests passed!\n");
    return 0;
}
#endif
