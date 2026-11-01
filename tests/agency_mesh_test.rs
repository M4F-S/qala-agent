//! Automated Integration Test Battery for QALA Standalone Rust Crate & Corporate Agency Mesh
//! Acceptance Criteria: agency_mesh_6_depts == true, tau3_50k_gate_active == true

use qala::clearance::{run_clearance_workflow, ContainerClearanceRequest};
use qala::mesh::{
    AgencyMesh, Department, MeshTask, SpecialistVerdict,
};
use qala::safety::{ApprovalSlot, QalaVerdict, SafetyGovernor};

fn create_test_mesh(test_id: &str) -> AgencyMesh {
    let tmp_tokens = format!("/tmp/qala_test_tokens_{}.jsonl", test_id);
    let tmp_audit = format!("/tmp/qala_test_audit_{}.jsonl", test_id);
    let _ = std::fs::remove_file(&tmp_tokens);
    let _ = std::fs::remove_file(&tmp_audit);

    let governor = SafetyGovernor::new("/workspace", tmp_tokens, tmp_audit);
    AgencyMesh::new(governor)
}

#[test]
fn test_c99_ffi_linkage_and_desc_isr_v31_compliance() {
    let test_id = format!("c99_desc_{}", std::process::id());
    let mut mesh = create_test_mesh(&test_id);

    // 1. DESC ISR v3.1 Kernel Path Containment:
    // Authorized path inside /workspace
    let safe_res = mesh.governor.check_desc_path("/workspace/trade_docs/inv.json", 1700000000);
    assert_eq!(safe_res.verdict, QalaVerdict::Safe);

    // Directory traversal escape (/../../etc/shadow) MUST fail closed under VERDICT_UNSAFE
    let escape_res = mesh.governor.check_desc_path("/workspace/../../etc/shadow", 1700000000);
    assert_eq!(escape_res.verdict, QalaVerdict::Unsafe);
    assert!(escape_res.reason.contains("DESC ISR v3.1"));

    // Secret access (.env) MUST fail closed under VERDICT_UNSAFE
    let secret_res = mesh.governor.check_desc_path("/workspace/.env", 1700000000);
    assert_eq!(secret_res.verdict, QalaVerdict::Unsafe);

    // 2. DESC ISR v3.1 Zero-Exfiltration PII Redaction
    let raw_prompt = "Patient Emirates ID 784-1985-1234567-1 and DOB 1985-04-12 importing medical equipment.";
    let redacted = mesh.governor.redact_pii(raw_prompt);
    assert!(!redacted.contains("784-1985-1234567-1"));
    assert!(!redacted.contains("1985-04-12"));
    assert!(redacted.contains("[EMIRATES_ID_REDACTED]"));
    assert!(redacted.contains("[DOB_REDACTED]"));
}

#[test]
fn test_all_6_departmental_specialists_initialization_and_md_coordination() {
    let test_id = format!("mesh_6depts_{}", std::process::id());
    let mut mesh = create_test_mesh(&test_id);

    // Verify all 6 departmental specialists + MD are reported in status
    let report = mesh.status_report();
    assert_eq!(report.len(), 7, "Must contain Managing Director + 6 Specialists");

    let dept_names: Vec<Department> = report.iter().map(|s| s.department).collect();
    assert!(dept_names.contains(&Department::Executive));
    assert!(dept_names.contains(&Department::Finance));
    assert!(dept_names.contains(&Department::Legal));
    assert!(dept_names.contains(&Department::Trade));
    assert!(dept_names.contains(&Department::Hr));
    assert!(dept_names.contains(&Department::Procurement));
    assert!(dept_names.contains(&Department::Operations));

    // 1. Finance Specialist: Corporate Tax calculation
    let fin_task = MeshTask::new(
        "T-FIN-01",
        Department::Finance,
        "finops_calculate_corporate_tax",
        serde_json::json!({ "net_profit_aed": 500_000.0 }),
    );
    let fin_resp = mesh.dispatch(&fin_task).expect("Finance task failed");
    assert_eq!(fin_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(
        fin_resp.data.get("corporate_tax_due_aed").unwrap().as_f64().unwrap(),
        11_250.0
    );

    // 2. Legal Specialist: DESC data classification check
    let legal_task = MeshTask::new(
        "T-LEG-01",
        Department::Legal,
        "legal_check_desc_compliance",
        serde_json::json!({
            "system_id": "CORE-SRV",
            "data_classification": "Confidential",
            "target_destination": "UAE-AIRGAP"
        }),
    );
    let legal_resp = mesh.dispatch(&legal_task).expect("Legal task failed");
    assert_eq!(legal_resp.verdict, SpecialistVerdict::Approved);

    // 3. Trade Specialist: Tariff calculation (5% duty + 5% VAT on CIF base = 10.25%)
    let trade_task = MeshTask::new(
        "T-TRD-01",
        Department::Trade,
        "customs_calculate_tariff",
        serde_json::json!({
            "cif_value_aed": 100_000.0,
            "hs_code": "8471.30"
        }),
    );
    let trade_resp = mesh.dispatch(&trade_task).expect("Trade task failed");
    assert_eq!(trade_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(
        trade_resp.data.get("customs_duty_aed").unwrap().as_f64().unwrap(),
        5_000.0
    );
    assert_eq!(
        trade_resp.data.get("vat_aed").unwrap().as_f64().unwrap(),
        5_250.0
    );
    assert_eq!(
        trade_resp.data.get("total_landed_duty_tax_aed").unwrap().as_f64().unwrap(),
        10_250.0
    );

    // 4. HR Specialist: Article 51 Gratuity calculation (3 years tenure, 20,000 basic)
    // 3 years * 21 days * (20,000 / 30) = 3 * 21 * 666.67 = 42,000.00 AED
    let hr_task = MeshTask::new(
        "T-HR-01",
        Department::Hr,
        "hr_calculate_gratuity",
        serde_json::json!({
            "basic_monthly_salary_aed": 20_000.0,
            "tenure_years": 3.0,
            "is_part_time": false
        }),
    );
    let hr_resp = mesh.dispatch(&hr_task).expect("HR task failed");
    assert_eq!(hr_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(
        hr_resp.data.get("final_statutory_gratuity_aed").unwrap().as_f64().unwrap(),
        42_000.0
    );

    // 5. Procurement Specialist: Sanctions check (Clear vendor)
    let proc_task = MeshTask::new(
        "T-PRC-01",
        Department::Procurement,
        "procurement_sanctions_check",
        serde_json::json!({
            "vendor_name": "Dubai Global Logistics LLC",
            "country_of_origin": "United Arab Emirates",
            "uae_tax_id": "100123456789012"
        }),
    );
    let proc_resp = mesh.dispatch(&proc_task).expect("Procurement task failed");
    assert_eq!(proc_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(proc_resp.data.get("screening_status").unwrap().as_str().unwrap(), "CLEAR");

    // 6. Operations Specialist: Container tracking within free time
    let ops_task = MeshTask::new(
        "T-OPS-01",
        Department::Operations,
        "ops_track_container",
        serde_json::json!({
            "container_id": "MSKU1234567",
            "terminal": "Jebel Ali Port Terminal 2",
            "days_in_yard": 2,
            "free_days_limit": 5
        }),
    );
    let ops_resp = mesh.dispatch(&ops_task).expect("Operations task failed");
    assert_eq!(ops_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(ops_resp.data.get("status").unwrap().as_str().unwrap(), "WITHIN_FREE_TIME");
}

#[test]
fn test_tau3_circuit_breaker_fails_closed_above_50k() {
    let test_id = format!("tau3_{}", std::process::id());
    let mut mesh = create_test_mesh(&test_id);

    // Case 1: Transaction under AED 50,000 (AED 25,000) -> Safe autonomous wire
    let sub50k_task = MeshTask::new(
        "T-FIN-SUB50K",
        Department::Finance,
        "finops_request_wire",
        serde_json::json!({
            "beneficiary": "Office Supplies LLC",
            "iban": "AE120330000000123456789",
            "amount_aed": 25_000.0,
            "memo": "Office Equipment"
        }),
    );
    let sub50k_resp = mesh.dispatch(&sub50k_task).expect("Finance task failed");
    assert_eq!(sub50k_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(
        sub50k_resp.data.get("status").unwrap().as_str().unwrap(),
        "WIRE_DISPATCHED"
    );

    // Case 2: Transaction exceeding AED 50,000 (AED 85,000) WITHOUT approval code
    // MUST trip Tau-3 circuit breaker and fail closed with Escrow Lock (Gated)
    let high_task = MeshTask::new(
        "T-FIN-HIGH",
        Department::Finance,
        "finops_request_wire",
        serde_json::json!({
            "beneficiary": "Heavy Machinery FZE",
            "iban": "AE440330000000987654321",
            "amount_aed": 85_000.0,
            "memo": "Excavator Procurement"
        }),
    );
    let high_resp = mesh.dispatch(&high_task).expect("Finance task failed");
    match high_resp.verdict {
        SpecialistVerdict::Gated {
            required_scope,
            amount_aed,
            ..
        } => {
            assert_eq!(required_scope, "finops_wire");
            assert_eq!(amount_aed, 85_000.0);
            assert_eq!(
                high_resp.data.get("status").unwrap().as_str().unwrap(),
                "ESCROW_LOCKED_PENDING_APPROVAL"
            );
        }
        other => panic!("Expected SpecialistVerdict::Gated, got: {:?}", other),
    }

    // Case 3: Supply valid dual-key cryptographic code -> Clears Tau-3 Gate
    let token_code = "QALA-AUTH-FIN-5544";
    mesh.governor.add_approval_slot(ApprovalSlot::new(
        token_code,
        "finops_wire",
        1900000000,
    ));

    let approved_task = MeshTask::new(
        "T-FIN-APPR",
        Department::Finance,
        "finops_request_wire",
        serde_json::json!({
            "beneficiary": "Heavy Machinery FZE",
            "iban": "AE440330000000987654321",
            "amount_aed": 85_000.0,
            "memo": "Excavator Procurement",
            "approval_code": token_code
        }),
    );
    let appr_resp = mesh.dispatch(&approved_task).expect("Approved wire failed");
    assert_eq!(appr_resp.verdict, SpecialistVerdict::Approved);
    assert_eq!(
        appr_resp.data.get("status").unwrap().as_str().unwrap(),
        "WIRE_DISPATCHED"
    );

    // Case 4: Replay attack with same consumed token -> MUST fail closed under Rejected
    let replay_task = MeshTask::new(
        "T-FIN-REPLAY",
        Department::Finance,
        "finops_request_wire",
        serde_json::json!({
            "beneficiary": "Heavy Machinery FZE",
            "iban": "AE440330000000987654321",
            "amount_aed": 85_000.0,
            "memo": "Duplicate Payout",
            "approval_code": token_code
        }),
    );
    let replay_resp = mesh.dispatch(&replay_task).expect("Replay dispatch failed");
    match replay_resp.verdict {
        SpecialistVerdict::Rejected { code, .. } => {
            assert_eq!(code, "invalid_or_expired_approval");
        }
        other => panic!("Expected SpecialistVerdict::Rejected, got: {:?}", other),
    }
}

#[test]
fn test_end_to_end_container_clearance_workflow() {
    let test_id = format!("clearance_{}", std::process::id());
    let mut mesh = create_test_mesh(&test_id);

    // 1. Autonomous sub-50k clearance (CIF AED 100,000 -> Duty AED 5,000, VAT AED 5,250 = Total AED 10,250 <= 50,000)
    let low_req = ContainerClearanceRequest {
        container_id: "MSKU1002003".to_string(),
        cif_value_aed: 100_000.0,
        hs_code: Some("8471.30".to_string()),
        bill_of_lading: Some("BL-DPW-1002003".to_string()),
        importer_code: Some("DXB-IMP-772".to_string()),
        approval_token: None,
    };
    let low_res = run_clearance_workflow(&mut mesh, low_req).expect("Low clearance failed");
    assert!(low_res.cleared);
    assert!(!low_res.tau3_escrow_gated);
    assert_eq!(low_res.customs_duty_aed, 5_000.0);
    assert_eq!(low_res.vat_amount_aed, 5_250.0);
    assert_eq!(low_res.total_tax_payable_aed, 10_250.0);
    assert!(low_res.terminal_release_status.contains("GATE_PASS_ISSUED"));
    assert!(!low_res.consensus_hash_sha256.is_empty());

    // 2. High-value container clearance (CIF AED 800,000 -> Duty AED 40,000, VAT AED 42,000 = Total AED 82,000 > 50,000)
    // Without token: MUST trip Tau-3 circuit breaker and hold container in escrow
    let high_req = ContainerClearanceRequest {
        container_id: "MSKU8009001".to_string(),
        cif_value_aed: 800_000.0,
        hs_code: Some("8471.30".to_string()),
        bill_of_lading: Some("BL-DPW-8009001".to_string()),
        importer_code: Some("DXB-IMP-772".to_string()),
        approval_token: None,
    };
    let high_res = run_clearance_workflow(&mut mesh, high_req).expect("High clearance dispatch failed");
    assert!(!high_res.cleared, "High clearance without token must not clear");
    assert!(high_res.tau3_escrow_gated, "Must be gated under Tau-3");
    assert_eq!(high_res.total_tax_payable_aed, 82_000.0);
    assert!(high_res.terminal_release_status.contains("HELD_AT_CUSTOMS_GATE"));

    // 3. High-value container clearance WITH valid dual-key approval token -> Cleared and Gate Pass Issued
    let auth_token = "QALA-AUTH-DPW-CL99";
    mesh.governor.add_approval_slot(ApprovalSlot::new(
        auth_token,
        "finops_wire",
        1900000000,
    ));

    let approved_req = ContainerClearanceRequest {
        container_id: "MSKU8009001".to_string(),
        cif_value_aed: 800_000.0,
        hs_code: Some("8471.30".to_string()),
        bill_of_lading: Some("BL-DPW-8009001".to_string()),
        importer_code: Some("DXB-IMP-772".to_string()),
        approval_token: Some(auth_token.to_string()),
    };
    let app_res = run_clearance_workflow(&mut mesh, approved_req).expect("Approved clearance failed");
    assert!(app_res.cleared, "High clearance with valid token must clear");
    assert!(!app_res.tau3_escrow_gated);
    assert!(app_res.terminal_release_status.contains("GATE_PASS_ISSUED"));
    assert!(!app_res.consensus_hash_sha256.is_empty());
}

#[test]
fn test_pharmaceutical_zero_rating_exemption() {
    let test_id = format!("pharma_{}", std::process::id());
    let mut mesh = create_test_mesh(&test_id);

    // Human pharmaceuticals Chapter 30 (HS 3004.90) under MoHAP permit
    let req = ContainerClearanceRequest {
        container_id: "MEDU3004901".to_string(),
        cif_value_aed: 500_000.0,
        hs_code: Some("3004.90".to_string()),
        bill_of_lading: Some("BL-DPW-MED300".to_string()),
        importer_code: Some("DXB-IMP-PHARMA".to_string()),
        approval_token: None,
    };
    let res = run_clearance_workflow(&mut mesh, req).expect("Pharma clearance failed");
    assert!(res.cleared);
    assert_eq!(res.customs_duty_aed, 0.0);
    assert_eq!(res.vat_amount_aed, 0.0);
    assert_eq!(res.total_tax_payable_aed, 0.0);
}
