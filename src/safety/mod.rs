//! Safe, idiomatic Rust API wrapping the inviolable C99 Qala Safety Kernel.

pub mod ffi;

pub use ffi::{GroundingVerdict, QalaVerdict};
use ffi::{QalaApprovalSlot, QalaPolicyResult, QalaToolCallCtx};
use serde::{Deserialize, Serialize};
use std::ffi::CString;
use std::os::raw::c_char;
use std::sync::Mutex;

/// High-level safe Policy Result
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct PolicyResult {
    pub verdict: QalaVerdict,
    pub code: String,
    pub reason: String,
    pub cited_doc_id: String,
    pub cited_section: String,
    pub passage_sha256: String,
}

impl PolicyResult {
    pub fn is_safe(&self) -> bool {
        self.verdict == QalaVerdict::Safe || self.verdict == QalaVerdict::Caution
    }

    pub fn requires_approval(&self) -> bool {
        self.verdict == QalaVerdict::Review
    }

    pub fn is_blocked(&self) -> bool {
        matches!(
            self.verdict,
            QalaVerdict::Unsafe | QalaVerdict::AuthBlock | QalaVerdict::Hallucination | QalaVerdict::Error
        )
    }
}

impl From<QalaPolicyResult> for PolicyResult {
    fn from(r: QalaPolicyResult) -> Self {
        Self {
            verdict: r.verdict,
            code: c_chars_to_string(&r.code),
            reason: c_chars_to_string(&r.reason),
            cited_doc_id: c_chars_to_string(&r.cited_doc_id),
            cited_section: c_chars_to_string(&r.cited_section),
            passage_sha256: c_chars_to_string(&r.passage_sha256),
        }
    }
}

/// High-level safe Approval Slot
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ApprovalSlot {
    pub token: String,
    pub scope: String,
    pub expires_utc: u64,
    pub consumed: bool,
}

impl ApprovalSlot {
    pub fn new(token: impl Into<String>, scope: impl Into<String>, expires_utc: u64) -> Self {
        Self {
            token: token.into(),
            scope: scope.into(),
            expires_utc,
            consumed: false,
        }
    }

    pub fn to_raw(&self) -> QalaApprovalSlot {
        let mut slot = QalaApprovalSlot::default();
        copy_string_to_c_chars(&self.token, &mut slot.token);
        copy_string_to_c_chars(&self.scope, &mut slot.scope);
        slot.expires_utc = self.expires_utc;
        slot.consumed = self.consumed;
        slot
    }
}

/// High-level Tool Call Context
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ToolCallContext {
    pub tool_name: String,
    pub target_path: Option<String>,
    pub raw_args_json: Option<String>,
    pub amount_aed: f64,
    pub is_mutating: bool,
    pub required_scope: Option<String>,
    pub supplied_code: Option<String>,
    pub cited_doc_id: Option<String>,
    pub cited_section: Option<String>,
    pub cited_passage_sha256: Option<String>,
}

impl ToolCallContext {
    pub fn new(tool_name: impl Into<String>) -> Self {
        Self {
            tool_name: tool_name.into(),
            target_path: None,
            raw_args_json: None,
            amount_aed: 0.0,
            is_mutating: false,
            required_scope: None,
            supplied_code: None,
            cited_doc_id: None,
            cited_section: None,
            cited_passage_sha256: None,
        }
    }

    pub fn with_amount(mut self, amount: f64) -> Self {
        self.amount_aed = amount;
        self
    }

    pub fn with_mutating(mut self, mutating: bool) -> Self {
        self.is_mutating = mutating;
        self
    }

    pub fn with_path(mut self, path: impl Into<String>) -> Self {
        self.target_path = Some(path.into());
        self
    }

    pub fn with_scope(mut self, scope: impl Into<String>) -> Self {
        self.required_scope = Some(scope.into());
        self
    }

    pub fn with_approval_code(mut self, code: impl Into<String>) -> Self {
        self.supplied_code = Some(code.into());
        self
    }
}

/// Global mutex to serialize C FFI calls since C adapter manages global state (workspace root, token store)
static SAFETY_MUTEX: Mutex<()> = Mutex::new(());

/// The Qala Policy & Safety Governor
pub struct SafetyGovernor {
    workspace_root: String,
    token_store_path: String,
    audit_log_path: String,
    slots: Vec<ApprovalSlot>,
}

impl SafetyGovernor {
    pub fn new(
        workspace_root: impl Into<String>,
        token_store_path: impl Into<String>,
        audit_log_path: impl Into<String>,
    ) -> Self {
        let ws = workspace_root.into();
        let ts = token_store_path.into();
        let al = audit_log_path.into();

        let _guard = SAFETY_MUTEX.lock().unwrap();
        unsafe {
            let c_ws = CString::new(ws.clone()).unwrap_or_default();
            ffi::qala_set_workspace_root(c_ws.as_ptr());

            let c_ts = CString::new(ts.clone()).unwrap_or_default();
            ffi::qala_set_token_store_path(c_ts.as_ptr());
        }

        Self {
            workspace_root: ws,
            token_store_path: ts,
            audit_log_path: al,
            slots: Vec::new(),
        }
    }

    pub fn add_approval_slot(&mut self, slot: ApprovalSlot) {
        self.slots.push(slot);
    }

    pub fn slots(&self) -> &[ApprovalSlot] {
        &self.slots
    }

    pub fn is_token_consumed(&self, token: &str) -> bool {
        let _guard = SAFETY_MUTEX.lock().unwrap();
        let c_store = CString::new(self.token_store_path.clone()).unwrap_or_default();
        let c_token = CString::new(token).unwrap_or_default();
        unsafe { ffi::qala_token_store_is_consumed(c_store.as_ptr(), c_token.as_ptr()) }
    }

    /// Verifies a tool call through C99 deterministic safety kernel.
    pub fn verify_call(&mut self, ctx: &ToolCallContext, now_utc: u64) -> PolicyResult {
        let _guard = SAFETY_MUTEX.lock().unwrap();

        // Ensure C99 state matches current governor config
        unsafe {
            let c_ws = CString::new(self.workspace_root.clone()).unwrap_or_default();
            ffi::qala_set_workspace_root(c_ws.as_ptr());

            let c_ts = CString::new(self.token_store_path.clone()).unwrap_or_default();
            ffi::qala_set_token_store_path(c_ts.as_ptr());
        }

        let c_tool_name = CString::new(ctx.tool_name.clone()).unwrap_or_default();
        let c_target_path = ctx.target_path.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_raw_args = ctx.raw_args_json.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_scope = ctx.required_scope.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_code = ctx.supplied_code.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_doc_id = ctx.cited_doc_id.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_sec = ctx.cited_section.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_hash = ctx.cited_passage_sha256.as_ref().and_then(|s| CString::new(s.as_str()).ok());
        let c_audit = CString::new(self.audit_log_path.clone()).unwrap_or_default();

        let raw_ctx = QalaToolCallCtx {
            tool_name: c_tool_name.as_ptr(),
            target_path: c_target_path.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            raw_args_json: c_raw_args.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            amount_aed: ctx.amount_aed,
            is_mutating: ctx.is_mutating,
            required_scope: c_scope.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            supplied_code: c_code.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            grounding_claim: std::ptr::null(),
            grounding_corpus: std::ptr::null(),
            grounding_count: 0,
            cited_doc_id: c_doc_id.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            cited_section: c_sec.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
            cited_passage_sha256: c_hash.as_ref().map_or(std::ptr::null(), |s| s.as_ptr()),
        };

        let mut raw_slots: Vec<QalaApprovalSlot> = self.slots.iter().map(|s| s.to_raw()).collect();
        let mut raw_result = QalaPolicyResult::default();

        unsafe {
            ffi::policy_verify_call_ptr(
                &raw_ctx,
                raw_slots.as_mut_ptr(),
                raw_slots.len(),
                now_utc,
                c_audit.as_ptr(),
                &mut raw_result,
            );
        }

        // Sync consumed state back into self.slots
        for (i, raw_slot) in raw_slots.iter().enumerate() {
            if i < self.slots.len() {
                self.slots[i].consumed = raw_slot.consumed;
            }
        }

        PolicyResult::from(raw_result)
    }

    /// Evaluates Tau-3 FinOps Circuit Breaker (> AED 50,000 threshold).
    pub fn check_tau3_gate(
        &mut self,
        tool_name: &str,
        amount_aed: f64,
        scope: &str,
        supplied_token: Option<&str>,
        now_utc: u64,
    ) -> PolicyResult {
        let is_gated = amount_aed > 50000.0;
        let mut ctx = ToolCallContext::new(tool_name)
            .with_amount(amount_aed)
            .with_mutating(is_gated)
            .with_scope(scope);

        if let Some(token) = supplied_token {
            ctx = ctx.with_approval_code(token);
        }

        self.verify_call(&ctx, now_utc)
    }

    /// Evaluates DESC ISR v3.1 Path Sandbox containment.
    pub fn check_desc_path(&mut self, path: &str, now_utc: u64) -> PolicyResult {
        let ctx = ToolCallContext::new("fs_read")
            .with_path(path)
            .with_mutating(false);
        self.verify_call(&ctx, now_utc)
    }

    /// Scrub PII from query adhering to DESC ISR v3.1 zero-data-leakage rule.
    pub fn redact_pii(&self, prompt: &str) -> String {
        let c_src = CString::new(prompt).unwrap_or_default();
        let mut buf = vec![0u8; prompt.len() * 2 + 256];
        unsafe {
            let written = ffi::grounding_redact_prompt_pii_c99(
                c_src.as_ptr(),
                buf.as_mut_ptr() as *mut c_char,
                buf.len(),
            );
            if written > 0 && written < buf.len() {
                buf.truncate(written);
                String::from_utf8(buf).unwrap_or_else(|_| prompt.to_string())
            } else {
                prompt.to_string()
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

fn c_chars_to_string(buf: &[c_char]) -> String {
    let mut bytes = Vec::with_capacity(buf.len());
    for &c in buf {
        if c == 0 {
            break;
        }
        bytes.push(c as u8);
    }
    String::from_utf8_lossy(&bytes).trim().to_string()
}

fn copy_string_to_c_chars(src: &str, dst: &mut [c_char]) {
    let bytes = src.as_bytes();
    let max = dst.len().saturating_sub(1);
    let len = bytes.len().min(max);
    for i in 0..len {
        dst[i] = bytes[i] as c_char;
    }
    dst[len] = 0;
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_c99_ffi_tau3_circuit_breaker() {
        let tmp_store = format!("/tmp/qala_test_tokens_{}.jsonl", std::process::id());
        let tmp_audit = format!("/tmp/qala_test_audit_{}.jsonl", std::process::id());
        let _ = std::fs::remove_file(&tmp_store);
        let _ = std::fs::remove_file(&tmp_audit);

        let mut governor = SafetyGovernor::new("/workspace", &tmp_store, &tmp_audit);

        // 1. Transaction under 50k non-mutating -> Safe
        let low_ctx = ToolCallContext::new("finops_query_balance")
            .with_amount(5_000.0)
            .with_mutating(false);
        let res_low = governor.verify_call(&low_ctx, 1700000000);
        assert_eq!(res_low.verdict, QalaVerdict::Safe);

        // 2. Transaction over 50k mutating without approval token -> Review
        let high_ctx = ToolCallContext::new("finops_request_wire")
            .with_amount(120_000.0)
            .with_mutating(true)
            .with_scope("finops_wire");
        let res_high = governor.verify_call(&high_ctx, 1700000000);
        assert_eq!(res_high.verdict, QalaVerdict::Review);
        assert_eq!(res_high.code, "approval_required");

        // 3. Add approval slot and verify with valid token -> Safe
        governor.add_approval_slot(ApprovalSlot::new(
            "QALA-AUTH-TEST-7711",
            "finops_wire",
            1800000000,
        ));
        let approved_ctx = ToolCallContext::new("finops_request_wire")
            .with_amount(120_000.0)
            .with_mutating(true)
            .with_scope("finops_wire")
            .with_approval_code("QALA-AUTH-TEST-7711");
        let res_appr = governor.verify_call(&approved_ctx, 1700000000);
        assert_eq!(res_appr.verdict, QalaVerdict::Safe);

        // 4. Replay attack: reusing same token must fail -> Unsafe
        let replay_ctx = ToolCallContext::new("finops_request_wire")
            .with_amount(120_000.0)
            .with_mutating(true)
            .with_scope("finops_wire")
            .with_approval_code("QALA-AUTH-TEST-7711");
        let res_replay = governor.verify_call(&replay_ctx, 1700000001);
        assert_eq!(res_replay.verdict, QalaVerdict::Unsafe);

        // 5. DESC ISR v3.1 Path Sandbox: Directory escape blocked
        let escape_ctx = ToolCallContext::new("fs_read")
            .with_path("/workspace/../../etc/shadow")
            .with_mutating(false);
        let res_esc = governor.verify_call(&escape_ctx, 1700000000);
        assert_eq!(res_esc.verdict, QalaVerdict::Unsafe);

        let _ = std::fs::remove_file(&tmp_store);
        let _ = std::fs::remove_file(&tmp_audit);
    }
}
