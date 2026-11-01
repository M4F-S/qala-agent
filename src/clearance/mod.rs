//! Dubai Customs Container Clearance & Tariff Payment Workflow
//! Multi-departmental orchestration: Trade -> Legal -> Finance (Tau-3 Gate) -> Operations -> Managing Director

use crate::mesh::{AgencyMesh, Department, MeshTask, SpecialistVerdict};
use serde::{Deserialize, Serialize};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContainerClearanceRequest {
    pub container_id: String,
    pub cif_value_aed: f64,
    pub hs_code: Option<String>,
    pub bill_of_lading: Option<String>,
    pub importer_code: Option<String>,
    pub approval_token: Option<String>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ContainerClearanceResult {
    pub container_id: String,
    pub cif_value_aed: f64,
    pub customs_duty_aed: f64,
    pub vat_amount_aed: f64,
    pub total_tax_payable_aed: f64,
    pub landed_rate_pct: f64,
    pub tau3_escrow_gated: bool,
    pub cleared: bool,
    pub mirsal2_declaration_no: String,
    pub terminal_release_status: String,
    pub executive_status: String,
    pub consensus_hash_sha256: String,
    pub statutory_citations: Vec<String>,
    pub raw_consensus: serde_json::Value,
}

/// Executes the full container clearance workflow across the Agency Mesh
pub fn run_clearance_workflow(
    mesh: &mut AgencyMesh,
    req: ContainerClearanceRequest,
) -> Result<ContainerClearanceResult, String> {
    let hs_code = req.hs_code.unwrap_or_else(|| "8471.30".to_string());
    let bl = req.bill_of_lading.unwrap_or_else(|| format!("BL-DPW-{}", &req.container_id));
    let importer = req.importer_code.unwrap_or_else(|| "DXB-IMP-772".to_string());

    let payload = serde_json::json!({
        "container_id": req.container_id,
        "cif_value_aed": req.cif_value_aed,
        "hs_code": hs_code,
        "bill_of_lading": bl,
        "importer_code": importer,
        "approval_code": req.approval_token
    });

    let executive_task = MeshTask::new(
        format!("WF-CLEARANCE-{}", req.container_id),
        Department::Executive,
        "container_clearance_workflow",
        payload,
    );

    let resp = mesh.dispatch(&executive_task)?;

    // Extract trade data
    let trade_resp = resp
        .data
        .get("department_responses")
        .and_then(|r| r.as_array())
        .and_then(|arr| arr.iter().find(|d| d.get("department") == Some(&serde_json::json!("trade"))));

    let duty_aed = trade_resp
        .and_then(|t| t.get("data"))
        .and_then(|d| d.get("customs_duty_aed"))
        .and_then(|v| v.as_f64())
        .unwrap_or(req.cif_value_aed * 0.05);

    let vat_aed = trade_resp
        .and_then(|t| t.get("data"))
        .and_then(|d| d.get("vat_amount_aed"))
        .and_then(|v| v.as_f64())
        .unwrap_or(req.cif_value_aed * 0.0525);

    let total_tax_aed = trade_resp
        .and_then(|t| t.get("data"))
        .and_then(|d| d.get("total_landed_payable_aed"))
        .and_then(|v| v.as_f64())
        .unwrap_or(duty_aed + vat_aed);

    let decl_no = trade_resp
        .and_then(|t| t.get("data"))
        .and_then(|d| d.get("mirsal2_declaration_no"))
        .and_then(|v| v.as_str())
        .unwrap_or("MIRSAL2-PENDING")
        .to_string();

    let hash = resp
        .data
        .get("consensus_hash_sha256")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();

    let (is_gated, is_cleared, exec_msg, terminal_msg) = match &resp.verdict {
        SpecialistVerdict::Approved => (
            false,
            true,
            "EXECUTIVE_APPROVED: Clearance certificate emitted with SHA-256 tamper-evident seal.".to_string(),
            "GATE_PASS_ISSUED: Container released for domestic transport from Jebel Ali Terminal.".to_string(),
        ),
        SpecialistVerdict::Gated {
            required_scope,
            amount_aed,
            reason,
        } => (
            true,
            false,
            format!(
                "ESCROW_LOCKED: Tau-3 Circuit Breaker tripped on AED {:.2} (scope: {}). {}",
                amount_aed, required_scope, reason
            ),
            "HELD_AT_CUSTOMS_GATE: Pending dual-key CEO authorization and wire release.".to_string(),
        ),
        SpecialistVerdict::Caution { reason } => (
            false,
            true,
            format!("EXECUTIVE_CAUTION: {}", reason),
            "GATE_PASS_ISSUED_WITH_ADVISORY".to_string(),
        ),
        SpecialistVerdict::Rejected { code, reason } => (
            false,
            false,
            format!("EXECUTIVE_REJECTED ({}): {}", code, reason),
            "HARD_BLOCK: Terminal gate pass denied under statutory policy.".to_string(),
        ),
    };

    let landed_rate = if req.cif_value_aed > 0.0 {
        (total_tax_aed / req.cif_value_aed) * 100.0
    } else {
        10.25
    };

    Ok(ContainerClearanceResult {
        container_id: req.container_id,
        cif_value_aed: req.cif_value_aed,
        customs_duty_aed: duty_aed,
        vat_amount_aed: vat_aed,
        total_tax_payable_aed: total_tax_aed,
        landed_rate_pct: landed_rate,
        tau3_escrow_gated: is_gated,
        cleared: is_cleared,
        mirsal2_declaration_no: decl_no,
        terminal_release_status: terminal_msg,
        executive_status: exec_msg,
        consensus_hash_sha256: hash,
        statutory_citations: resp.statutory_citations,
        raw_consensus: resp.data,
    })
}
