//! Managing Director / Chief of Staff Agent (`src/mesh/director.rs`)
//! Orchestrates the 6 Departmental Specialists, executive routing, query decomposition,
//! cross-departmental fan-out, and multi-agent consensus aggregation.

use super::{
    Department, DepartmentSpecialist, FinanceSpecialist, HrSpecialist, LegalSpecialist,
    MeshTask, OperationsSpecialist, ProcurementSpecialist, SpecialistResponse, SpecialistVerdict,
    TradeSpecialist,
};
use crate::safety::SafetyGovernor;
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::time::Instant;

pub struct ManagingDirector {
    pub name: &'static str,
}

impl Default for ManagingDirector {
    fn default() -> Self {
        Self::new()
    }
}

/// Consolidated executive decision emitted by the Managing Director
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ExecutiveConsensusDecision {
    pub executive_task_id: String,
    pub intent: String,
    pub overall_verdict: SpecialistVerdict,
    pub department_responses: Vec<SpecialistResponse>,
    pub aggregated_citations: Vec<String>,
    pub total_financial_commitment_aed: f64,
    pub consensus_hash_sha256: String,
    pub total_execution_time_ms: f64,
}

impl ManagingDirector {
    pub fn new() -> Self {
        Self {
            name: "qala_managing_director",
        }
    }

    pub fn name(&self) -> &'static str {
        self.name
    }

    pub fn status(&self) -> &'static str {
        "ONLINE_ORCHESTRATING"
    }

    pub fn capabilities(&self) -> Vec<&'static str> {
        vec![
            "executive_route",
            "executive_decompose",
            "cross_departmental_fanout",
            "consensus_aggregation",
            "ceo_escalation",
        ]
    }

    /// Handles top-level executive workflows and decomposes them across specialists
    pub fn handle_executive_task(
        &mut self,
        task: &MeshTask,
        finance: &mut FinanceSpecialist,
        legal: &mut LegalSpecialist,
        trade: &mut TradeSpecialist,
        hr: &mut HrSpecialist,
        procurement: &mut ProcurementSpecialist,
        operations: &mut OperationsSpecialist,
        governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        match task.action.as_str() {
            "container_clearance_workflow" | "dubai_customs_clearance" => {
                let decision = self.execute_container_clearance(
                    &task.task_id,
                    &task.payload,
                    trade,
                    legal,
                    finance,
                    operations,
                    governor,
                )?;

                let verdict = decision.overall_verdict.clone();
                let citations = decision.aggregated_citations.clone();
                let data = serde_json::to_value(&decision).map_err(|e| e.to_string())?;

                Ok(SpecialistResponse {
                    specialist_name: self.name.to_string(),
                    department: Department::Executive,
                    action: task.action.clone(),
                    verdict,
                    data,
                    statutory_citations: citations,
                    execution_time_ms: start.elapsed().as_secs_f64() * 1000.0,
                })
            }
            "employee_offboarding_workflow" => {
                let decision = self.execute_employee_offboarding(
                    &task.task_id,
                    &task.payload,
                    hr,
                    finance,
                    governor,
                )?;

                let verdict = decision.overall_verdict.clone();
                let citations = decision.aggregated_citations.clone();
                let data = serde_json::to_value(&decision).map_err(|e| e.to_string())?;

                Ok(SpecialistResponse {
                    specialist_name: self.name.to_string(),
                    department: Department::Executive,
                    action: task.action.clone(),
                    verdict,
                    data,
                    statutory_citations: citations,
                    execution_time_ms: start.elapsed().as_secs_f64() * 1000.0,
                })
            }
            "vendor_procurement_workflow" => {
                let decision = self.execute_vendor_procurement(
                    &task.task_id,
                    &task.payload,
                    procurement,
                    legal,
                    finance,
                    governor,
                )?;

                let verdict = decision.overall_verdict.clone();
                let citations = decision.aggregated_citations.clone();
                let data = serde_json::to_value(&decision).map_err(|e| e.to_string())?;

                Ok(SpecialistResponse {
                    specialist_name: self.name.to_string(),
                    department: Department::Executive,
                    action: task.action.clone(),
                    verdict,
                    data,
                    statutory_citations: citations,
                    execution_time_ms: start.elapsed().as_secs_f64() * 1000.0,
                })
            }
            _ => Err(format!("Managing Director unknown action: '{}'", task.action)),
        }
    }

    /// End-to-End Dubai Customs Container Clearance & Tariff Payment Workflow
    pub fn execute_container_clearance(
        &mut self,
        workflow_id: &str,
        payload: &serde_json::Value,
        trade: &mut TradeSpecialist,
        legal: &mut LegalSpecialist,
        finance: &mut FinanceSpecialist,
        operations: &mut OperationsSpecialist,
        governor: &mut SafetyGovernor,
    ) -> Result<ExecutiveConsensusDecision, String> {
        let start = Instant::now();
        let mut responses = Vec::new();
        let mut citations = Vec::new();
        let mut total_financial_commitment = 0.0;

        let container_id = payload
            .get("container_id")
            .and_then(|v| v.as_str())
            .unwrap_or("MSKU1234567");
        let cif_aed = payload
            .get("cif_value_aed")
            .and_then(|v| v.as_f64())
            .unwrap_or(100_000.0);
        let hs_code = payload
            .get("hs_code")
            .and_then(|v| v.as_str())
            .unwrap_or("8471.30");
        let bill_of_lading = payload
            .get("bill_of_lading")
            .and_then(|v| v.as_str())
            .unwrap_or("BL-DPW-9901");
        let importer_code = payload
            .get("importer_code")
            .and_then(|v| v.as_str())
            .unwrap_or("DXB-IMP-001");
        let approval_code = payload
            .get("approval_code")
            .and_then(|v| v.as_str());

        // Step 1: Trade & Customs Specialist - Tariff Calculation & Mirsal II Declaration
        let trade_task = MeshTask::new(
            format!("{}-01-trade", workflow_id),
            Department::Trade,
            "customs_clear_shipment",
            serde_json::json!({
                "bill_of_lading": bill_of_lading,
                "container_id": container_id,
                "importer_code": importer_code,
                "cif_value_aed": cif_aed,
                "hs_code": hs_code,
                "is_freezone_exit": false,
                "is_temp_admission": false
            }),
        );
        let trade_resp = trade.handle_task(&trade_task, governor)?;
        let landed_payable_aed = trade_resp
            .data
            .get("total_landed_payable_aed")
            .and_then(|v| v.as_f64())
            .unwrap_or(cif_aed * 0.1025);
        total_financial_commitment += landed_payable_aed;
        citations.extend(trade_resp.statutory_citations.clone());
        responses.push(trade_resp);

        // Step 2: Legal Specialist - DESC ISR v3.1 Security & Trade Compliance Check
        let legal_task = MeshTask::new(
            format!("{}-02-legal", workflow_id),
            Department::Legal,
            "legal_check_desc_compliance",
            serde_json::json!({
                "system_id": "DP-WORLD-MIRSAL-GATE",
                "data_classification": "Confidential",
                "target_destination": "UAE-SOVEREIGN-MIRROR"
            }),
        );
        let legal_resp = legal.handle_task(&legal_task, governor)?;
        citations.extend(legal_resp.statutory_citations.clone());
        let legal_blocked = matches!(legal_resp.verdict, SpecialistVerdict::Rejected { .. });
        responses.push(legal_resp);

        if legal_blocked {
            return self.aggregate_consensus(
                workflow_id,
                "dubai_customs_clearance",
                responses,
                citations,
                total_financial_commitment,
                start.elapsed().as_secs_f64() * 1000.0,
            );
        }

        // Step 3: Finance Specialist - Customs Wire Transfer with Tau-3 Circuit Breaker
        let finance_task = MeshTask::new(
            format!("{}-03-finance", workflow_id),
            Department::Finance,
            "finops_request_wire",
            serde_json::json!({
                "beneficiary": "Dubai Customs - Mirsal II Settlement",
                "iban": "AE030330000000000000001",
                "amount_aed": landed_payable_aed,
                "memo": format!("Customs Duty & VAT for container {}", container_id),
                "approval_code": approval_code
            }),
        );
        let fin_resp = finance.handle_task(&finance_task, governor)?;
        citations.extend(fin_resp.statutory_citations.clone());
        let is_gated = matches!(fin_resp.verdict, SpecialistVerdict::Gated { .. });
        let is_rejected = matches!(fin_resp.verdict, SpecialistVerdict::Rejected { .. });
        responses.push(fin_resp);

        if is_gated || is_rejected {
            // Escrow lock engaged or rejected - cannot proceed to terminal gate release yet
            return self.aggregate_consensus(
                workflow_id,
                "dubai_customs_clearance",
                responses,
                citations,
                total_financial_commitment,
                start.elapsed().as_secs_f64() * 1000.0,
            );
        }

        // Step 4: Operations Specialist - Terminal Tracking & Gate Pass Issuance
        let ops_task = MeshTask::new(
            format!("{}-04-ops", workflow_id),
            Department::Operations,
            "ops_track_container",
            serde_json::json!({
                "container_id": container_id,
                "terminal": "Jebel Ali Container Terminal 1",
                "days_in_yard": 1,
                "free_days_limit": 5
            }),
        );
        let ops_resp = operations.handle_task(&ops_task, governor)?;
        citations.extend(ops_resp.statutory_citations.clone());
        responses.push(ops_resp);

        self.aggregate_consensus(
            workflow_id,
            "dubai_customs_clearance",
            responses,
            citations,
            total_financial_commitment,
            start.elapsed().as_secs_f64() * 1000.0,
        )
    }

    /// End-to-End Employee Offboarding & Terminal Gratuity Payout Workflow
    pub fn execute_employee_offboarding(
        &mut self,
        workflow_id: &str,
        payload: &serde_json::Value,
        hr: &mut HrSpecialist,
        finance: &mut FinanceSpecialist,
        governor: &mut SafetyGovernor,
    ) -> Result<ExecutiveConsensusDecision, String> {
        let start = Instant::now();
        let mut responses = Vec::new();
        let mut citations = Vec::new();

        let basic = payload.get("basic_monthly_salary_aed").and_then(|v| v.as_f64()).unwrap_or(20_000.0);
        let tenure = payload.get("tenure_years").and_then(|v| v.as_f64()).unwrap_or(3.0);
        let employee_name = payload.get("employee_name").and_then(|v| v.as_str()).unwrap_or("John Doe");
        let approval_code = payload.get("approval_code").and_then(|v| v.as_str());

        // Step 1: HR Article 51 Gratuity Calculation
        let hr_task = MeshTask::new(
            format!("{}-01-hr", workflow_id),
            Department::Hr,
            "hr_calculate_gratuity",
            serde_json::json!({
                "basic_monthly_salary_aed": basic,
                "tenure_years": tenure,
                "is_part_time": false
            }),
        );
        let hr_resp = hr.handle_task(&hr_task, governor)?;
        let gratuity_aed = hr_resp.data.get("final_statutory_gratuity_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
        citations.extend(hr_resp.statutory_citations.clone());
        responses.push(hr_resp);

        // Step 2: Finance Wire Payout with Tau-3 Escrow Gate
        let fin_task = MeshTask::new(
            format!("{}-02-finance", workflow_id),
            Department::Finance,
            "finops_request_wire",
            serde_json::json!({
                "beneficiary": employee_name,
                "iban": "AE550220000000123456789",
                "amount_aed": gratuity_aed,
                "memo": format!("Article 51 Terminal Gratuity for {}", employee_name),
                "approval_code": approval_code
            }),
        );
        let fin_resp = finance.handle_task(&fin_task, governor)?;
        citations.extend(fin_resp.statutory_citations.clone());
        responses.push(fin_resp);

        self.aggregate_consensus(
            workflow_id,
            "employee_offboarding_workflow",
            responses,
            citations,
            gratuity_aed,
            start.elapsed().as_secs_f64() * 1000.0,
        )
    }

    /// End-to-End Vendor Procurement Workflow
    pub fn execute_vendor_procurement(
        &mut self,
        workflow_id: &str,
        payload: &serde_json::Value,
        procurement: &mut ProcurementSpecialist,
        legal: &mut LegalSpecialist,
        finance: &mut FinanceSpecialist,
        governor: &mut SafetyGovernor,
    ) -> Result<ExecutiveConsensusDecision, String> {
        let start = Instant::now();
        let mut responses = Vec::new();
        let mut citations = Vec::new();

        let vendor = payload.get("vendor_name").and_then(|v| v.as_str()).unwrap_or("Gulf Logistics FZE");
        let country = payload.get("country_of_origin").and_then(|v| v.as_str()).unwrap_or("United Arab Emirates");
        let trn = payload.get("trn").and_then(|v| v.as_str()).unwrap_or("100234567890123");
        let invoice_amount_aed = payload.get("amount_aed").and_then(|v| v.as_f64()).unwrap_or(25_000.0);
        let approval_code = payload.get("approval_code").and_then(|v| v.as_str());

        // Step 1: Procurement Sanctions Screening
        let sanctions_task = MeshTask::new(
            format!("{}-01-sanctions", workflow_id),
            Department::Procurement,
            "procurement_sanctions_check",
            serde_json::json!({
                "vendor_name": vendor,
                "country_of_origin": country,
                "uae_tax_id": trn
            }),
        );
        let sanctions_resp = procurement.handle_task(&sanctions_task, governor)?;
        citations.extend(sanctions_resp.statutory_citations.clone());
        let hit = matches!(sanctions_resp.verdict, SpecialistVerdict::Rejected { .. });
        responses.push(sanctions_resp);

        if hit {
            return self.aggregate_consensus(
                workflow_id,
                "vendor_procurement_workflow",
                responses,
                citations,
                invoice_amount_aed,
                start.elapsed().as_secs_f64() * 1000.0,
            );
        }

        // Step 2: Legal Review of Vendor Contract
        let legal_task = MeshTask::new(
            format!("{}-02-legal", workflow_id),
            Department::Legal,
            "legal_review_contract",
            serde_json::json!({
                "contract_title": format!("Vendor Agreement - {}", vendor),
                "governing_law": "DIFC",
                "contract_text": "Standard Master Services Agreement"
            }),
        );
        let legal_resp = legal.handle_task(&legal_task, governor)?;
        citations.extend(legal_resp.statutory_citations.clone());
        responses.push(legal_resp);

        // Step 3: Finance Payout Wire with Tau-3 Gate
        let fin_task = MeshTask::new(
            format!("{}-03-finance", workflow_id),
            Department::Finance,
            "finops_request_wire",
            serde_json::json!({
                "beneficiary": vendor,
                "iban": "AE440330000000987654321",
                "amount_aed": invoice_amount_aed,
                "memo": format!("Supplier settlement for {}", vendor),
                "approval_code": approval_code
            }),
        );
        let fin_resp = finance.handle_task(&fin_task, governor)?;
        citations.extend(fin_resp.statutory_citations.clone());
        responses.push(fin_resp);

        self.aggregate_consensus(
            workflow_id,
            "vendor_procurement_workflow",
            responses,
            citations,
            invoice_amount_aed,
            start.elapsed().as_secs_f64() * 1000.0,
        )
    }

    /// Aggregates specialist verdicts into an executive consensus decision with SHA-256 integrity hash
    fn aggregate_consensus(
        &self,
        workflow_id: &str,
        intent: &str,
        responses: Vec<SpecialistResponse>,
        citations: Vec<String>,
        total_financial_commitment: f64,
        total_time_ms: f64,
    ) -> Result<ExecutiveConsensusDecision, String> {
        let mut dedup_citations = citations;
        dedup_citations.sort();
        dedup_citations.dedup();

        // Determine overall consensus verdict:
        // 1. Any rejection -> Rejected
        // 2. Any gated condition (Tau-3 escrow lock) -> Gated
        // 3. Any caution -> Caution
        // 4. Otherwise -> Approved
        let mut overall_verdict = SpecialistVerdict::Approved;

        for r in &responses {
            match &r.verdict {
                SpecialistVerdict::Rejected { code, reason } => {
                    overall_verdict = SpecialistVerdict::Rejected {
                        code: code.clone(),
                        reason: reason.clone(),
                    };
                    break;
                }
                SpecialistVerdict::Gated {
                    required_scope,
                    amount_aed,
                    reason,
                } => {
                    overall_verdict = SpecialistVerdict::Gated {
                        required_scope: required_scope.clone(),
                        amount_aed: *amount_aed,
                        reason: reason.clone(),
                    };
                }
                SpecialistVerdict::Caution { reason } => {
                    if overall_verdict == SpecialistVerdict::Approved {
                        overall_verdict = SpecialistVerdict::Caution {
                            reason: reason.clone(),
                        };
                    }
                }
                SpecialistVerdict::Approved => {}
            }
        }

        // Compute tamper-evident SHA-256 hash of the executive consensus
        let mut hasher = Sha256::new();
        hasher.update(workflow_id.as_bytes());
        hasher.update(intent.as_bytes());
        hasher.update(format!("{:.2}", total_financial_commitment).as_bytes());
        for r in &responses {
            hasher.update(r.specialist_name.as_bytes());
            hasher.update(r.action.as_bytes());
        }
        let hash_hex = hex::encode(hasher.finalize());

        Ok(ExecutiveConsensusDecision {
            executive_task_id: workflow_id.to_string(),
            intent: intent.to_string(),
            overall_verdict,
            department_responses: responses,
            aggregated_citations: dedup_citations,
            total_financial_commitment_aed: total_financial_commitment,
            consensus_hash_sha256: hash_hex,
            total_execution_time_ms: total_time_ms,
        })
    }
}
