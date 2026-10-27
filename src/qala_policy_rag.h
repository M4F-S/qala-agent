/* src/qala_policy_rag.h - Sovereign Policy RAG & Statutory Corpus FTS5 Engine
 * Dubai Create AI Agents Championship 2026-2027 | Best Business AI Agent
 * Conforms strictly to ISO C99 (-std=c99 -Wall -Wextra -Werror -O2)
 * Pure C99 + SQLite FTS5 (BM25 ranking, no external models, no network)
 */

#ifndef QALA_POLICY_RAG_H
#define QALA_POLICY_RAG_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "grounding_shield.h"

#ifdef __cplusplus
extern "C" {
#endif

#define QALA_MAX_RAG_RESULTS 16

/* Structured statutory policy chunk */
typedef struct {
    char   doc_id[64];          /* e.g. "DESC-ISR-V3.1", "TAU3-FINOPS-POLICY-2026" */
    char   doc_title[128];      /* Formal title */
    char   section[64];         /* e.g. "Domain 14.1", "Policy 3.1" */
    char   section_title[128];  /* Section description */
    int    page;                /* Page number */
    char   passage_text[1024];  /* Exact statutory text */
    char   passage_sha256[65];  /* SHA-256 hex digest of passage_text */
    double numerical_value;     /* Associated statutory metric (e.g. 50000.0, 0.05) */
    bool   has_numerical;       /* True if passage specifies numerical rule */
    double bm25_rank;           /* SQLite FTS5 BM25 score */
} qala_policy_chunk_t;

/* Opaque Policy RAG handle */
typedef struct qala_policy_rag qala_policy_rag_t;

/* Open or initialize SQLite FTS5 Policy RAG engine.
 * Pass NULL or ":memory:" for transient in-memory database.
 */
qala_policy_rag_t *qala_policy_rag_open(const char *db_path);

/* Close database handle and free all internal resources */
void qala_policy_rag_close(qala_policy_rag_t *rag);

/* Ingest corpus directory by verifying manifest.sha256 and indexing all text files */
int qala_policy_rag_index_corpus(qala_policy_rag_t *rag, const char *manifest_path);

/* Ingest a single structured corpus text file */
int qala_policy_rag_index_file(qala_policy_rag_t *rag, const char *file_path);

/* Insert a section chunk directly into FTS5 index and manifest table */
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
);

/* Query policy corpus using SQLite FTS5 BM25 ranking */
int qala_policy_rag_query(
    qala_policy_rag_t   *rag,
    const char          *query,
    qala_policy_chunk_t *out_chunks,
    size_t               max_results
);

/* Verify whether cited doc_id, section, and passage_sha256 match authoritative corpus */
bool qala_policy_rag_verify_hash(
    qala_policy_rag_t *rag,
    const char        *doc_id,
    const char        *section,
    const char        *passage_sha256
);

/* Format retrieval results into an aligned presentation buffer */
size_t qala_policy_rag_format_results(
    const char                *query,
    const qala_policy_chunk_t *chunks,
    size_t                     chunk_count,
    char                      *out_buf,
    size_t                     out_cap
);

/* Answer a policy query and produce grounded verification result */
grounding_result_t qala_policy_rag_answer_and_verify(
    qala_policy_rag_t   *rag,
    const char          *question_or_claim,
    qala_policy_chunk_t *out_top_chunk
);

/* Register an active Policy RAG instance as the global policy verifier for policy_verify_call */
void qala_policy_rag_register_as_global_verifier(qala_policy_rag_t *rag);

#ifdef __cplusplus
}
#endif

#endif /* QALA_POLICY_RAG_H */
