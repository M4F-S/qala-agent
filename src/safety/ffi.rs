//! Low-level C99 FFI bindings for Qala C99 Safety Kernel.
//! Links directly to `libqala_adapters` (compiled from C99 safety engine).

use serde::{Deserialize, Serialize};
use std::os::raw::{c_char, c_double, c_int};

/// Standard Qala Verdict Envelopes
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum QalaVerdict {
    Safe = 0,
    Caution = 1,
    Review = 2,
    Unsafe = 3,
    AuthBlock = 4,
    Hallucination = 5,
    Error = 6,
}

impl Default for QalaVerdict {
    fn default() -> Self {
        QalaVerdict::Safe
    }
}

/// Single-use, scope-bound human approval token slot
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct QalaApprovalSlot {
    pub token: [c_char; 64],
    pub scope: [c_char; 32],
    pub expires_utc: u64,
    pub consumed: bool,
}

impl Default for QalaApprovalSlot {
    fn default() -> Self {
        Self {
            token: [0; 64],
            scope: [0; 32],
            expires_utc: 0,
            consumed: false,
        }
    }
}

/// Grounding Shield Verdict
#[repr(u32)]
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub enum GroundingVerdict {
    Verified = 0,
    HallucinationTripped = 1,
    Unverifiable = 2,
    PiiBlocked = 3,
    Error = 4,
}

impl Default for GroundingVerdict {
    fn default() -> Self {
        GroundingVerdict::Verified
    }
}

/// Extracted factual claim structure
#[repr(C)]
#[derive(Debug, Clone)]
pub struct GroundingClaim {
    pub claim_text: *const c_char,
    pub entity_name: *const c_char,
    pub numerical_value: c_double,
    pub has_numerical: bool,
}

impl Default for GroundingClaim {
    fn default() -> Self {
        Self {
            claim_text: std::ptr::null(),
            entity_name: std::ptr::null(),
            numerical_value: 0.0,
            has_numerical: false,
        }
    }
}

/// Authoritative reference source snippet
#[repr(C)]
#[derive(Debug, Clone)]
pub struct GroundingSource {
    pub source_id: *const c_char,
    pub source_title: *const c_char,
    pub snippet_text: *const c_char,
    pub source_numerical: c_double,
    pub doc_id: *const c_char,
    pub section: *const c_char,
    pub page: c_int,
    pub passage_sha256: [c_char; 65],
}

impl Default for GroundingSource {
    fn default() -> Self {
        Self {
            source_id: std::ptr::null(),
            source_title: std::ptr::null(),
            snippet_text: std::ptr::null(),
            source_numerical: 0.0,
            doc_id: std::ptr::null(),
            section: std::ptr::null(),
            page: 0,
            passage_sha256: [0; 65],
        }
    }
}

/// Verification result envelope
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct GroundingResult {
    pub verdict: GroundingVerdict,
    pub confidence: c_double,
    pub citation: [c_char; 128],
    pub discrepancy_note: [c_char; 256],
    pub doc_id: [c_char; 64],
    pub section: [c_char; 64],
    pub page: c_int,
    pub passage_sha256: [c_char; 65],
}

impl Default for GroundingResult {
    fn default() -> Self {
        Self {
            verdict: GroundingVerdict::Verified,
            confidence: 0.0,
            citation: [0; 128],
            discrepancy_note: [0; 256],
            doc_id: [0; 64],
            section: [0; 64],
            page: 0,
            passage_sha256: [0; 65],
        }
    }
}

/// Tool call inspection context for policy verification
#[repr(C)]
#[derive(Debug, Clone)]
pub struct QalaToolCallCtx {
    pub tool_name: *const c_char,
    pub target_path: *const c_char,
    pub raw_args_json: *const c_char,
    pub amount_aed: c_double,
    pub is_mutating: bool,
    pub required_scope: *const c_char,
    pub supplied_code: *const c_char,
    pub grounding_claim: *const GroundingClaim,
    pub grounding_corpus: *const GroundingSource,
    pub grounding_count: usize,
    pub cited_doc_id: *const c_char,
    pub cited_section: *const c_char,
    pub cited_passage_sha256: *const c_char,
}

impl Default for QalaToolCallCtx {
    fn default() -> Self {
        Self {
            tool_name: std::ptr::null(),
            target_path: std::ptr::null(),
            raw_args_json: std::ptr::null(),
            amount_aed: 0.0,
            is_mutating: false,
            required_scope: std::ptr::null(),
            supplied_code: std::ptr::null(),
            grounding_claim: std::ptr::null(),
            grounding_corpus: std::ptr::null(),
            grounding_count: 0,
            cited_doc_id: std::ptr::null(),
            cited_section: std::ptr::null(),
            cited_passage_sha256: std::ptr::null(),
        }
    }
}

/// Policy result envelope
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct QalaPolicyResult {
    pub verdict: QalaVerdict,
    pub code: [c_char; 64],
    pub reason: [c_char; 256],
    pub cited_doc_id: [c_char; 64],
    pub cited_section: [c_char; 64],
    pub passage_sha256: [c_char; 65],
}

impl Default for QalaPolicyResult {
    fn default() -> Self {
        Self {
            verdict: QalaVerdict::Safe,
            code: [0; 64],
            reason: [0; 256],
            cited_doc_id: [0; 64],
            cited_section: [0; 64],
            passage_sha256: [0; 65],
        }
    }
}

extern "C" {
    pub fn policy_verify_call(
        ctx: *const QalaToolCallCtx,
        slots: *mut QalaApprovalSlot,
        slot_count: usize,
        now_utc: u64,
        audit_log_path: *const c_char,
    ) -> QalaPolicyResult;

    pub fn policy_verify_call_ptr(
        ctx: *const QalaToolCallCtx,
        slots: *mut QalaApprovalSlot,
        slot_count: usize,
        now_utc: u64,
        audit_log_path: *const c_char,
        out_result: *mut QalaPolicyResult,
    ) -> QalaVerdict;

    pub fn qala_set_workspace_root(root: *const c_char);
    pub fn qala_get_workspace_root() -> *const c_char;

    pub fn qala_set_token_store_path(path: *const c_char);
    pub fn qala_get_token_store_path() -> *const c_char;

    pub fn qala_token_store_is_consumed(
        store_path: *const c_char,
        token: *const c_char,
    ) -> bool;

    pub fn qala_token_store_record_consumed(
        store_path: *const c_char,
        token: *const c_char,
        scope: *const c_char,
        tool_name: *const c_char,
        amount_aed: c_double,
        consumed_utc: u64,
    ) -> c_int;

    pub fn qala_token_store_sync_slots(
        store_path: *const c_char,
        slots: *mut QalaApprovalSlot,
        slot_count: usize,
    ) -> c_int;

    pub fn grounding_verify_claim_ptr(
        claim: *const GroundingClaim,
        corpus: *const GroundingSource,
        corpus_count: usize,
        out_result: *mut GroundingResult,
    ) -> GroundingVerdict;

    pub fn grounding_scrub_search_query(
        raw_user_prompt: *const c_char,
        out_scrubbed_query: *mut c_char,
        out_cap: usize,
    ) -> usize;

    pub fn grounding_redact_prompt_pii_c99(
        src: *const c_char,
        dst: *mut c_char,
        dst_cap: usize,
    ) -> usize;
}
