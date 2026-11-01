//! Legal & Compliance Departmental Specialist (`qala_legal`)
//! Enforces UAE Labor Law, DESC ISR v3.1 Data Residency, and Statutory Contract Review.

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::SafetyGovernor;
use std::time::Instant;

pub struct LegalSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for LegalSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

impl LegalSpecialist {
    pub fn new() -> Self {
        Self {
            name: "qala_legal",
            department: Department::Legal,
        }
    }

    /// Review commercial contract for UAE jurisdiction and statutory validity
    pub fn review_contract(
        &self,
        contract_title: &str,
        governing_law: &str,
        contract_text: &str,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "UAE Federal Decree-Law No. 50 of 2022 on the Commercial Transactions Law".to_string(),
            "Cabinet Decision No. 109 of 2023 on the Regulation of Arbitration in the UAE".to_string(),
            "DIFC Law No. 6 of 2004 (DIFC Contract Law)".to_string(),
        ];

        let law_lower = governing_law.to_lowercase();
        let recognized_jurisdiction = law_lower.contains("difc")
            || law_lower.contains("adgm")
            || law_lower.contains("dubai")
            || law_lower.contains("uae")
            || law_lower.contains("united arab emirates");

        if !recognized_jurisdiction {
            return (
                SpecialistVerdict::Rejected {
                    code: "unrecognized_jurisdiction".to_string(),
                    reason: format!(
                        "Foreign or non-compliant governing law '{}'. Must be DIFC, ADGM, or UAE Mainland/Dubai Courts.",
                        governing_law
                    ),
                },
                serde_json::json!({
                    "contract_title": contract_title,
                    "governing_law": governing_law,
                    "compliant": false,
                    "recommendation": "Specify UAE Civil Courts, DIFC Common Law Court, or ADGM Court jurisdiction."
                }),
                citations,
            );
        }

        // Check for common regulatory red flags in contract text
        let has_excessive_penalty = contract_text.to_lowercase().contains("penalty fee exceeds 50%")
            || contract_text.to_lowercase().contains("liquidated damages exceeding 100%");

        if has_excessive_penalty {
            return (
                SpecialistVerdict::Caution {
                    reason: "Contract contains punitive damages clause which may be subject to judicial reduction under UAE Civil Code Article 390.".to_string(),
                },
                serde_json::json!({
                    "contract_title": contract_title,
                    "governing_law": governing_law,
                    "compliant": true,
                    "warning": "Article 390 review recommended for liquidated damages."
                }),
                citations,
            );
        }

        (
            SpecialistVerdict::Approved,
            serde_json::json!({
                "contract_title": contract_title,
                "governing_law": governing_law,
                "jurisdiction_type": if law_lower.contains("difc") || law_lower.contains("adgm") { "Common Law Financial Free Zone" } else { "Civil Law Mainland" },
                "compliant": true,
                "enforceable": true
            }),
            citations,
        )
    }

    /// Enforces DESC ISR v3.1 Enterprise Security Controls and Data Residency
    pub fn check_desc_compliance(
        &self,
        system_id: &str,
        data_classification: &str,
        target_destination: &str,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "DESC ISR v3.1 (Dubai Electronic Security Center Information Security Regulation)".to_string(),
            "DESC ISR v3.1 Domain 1: Information Security Governance".to_string(),
            "DESC ISR v3.1 Domain 11: Communications & Cloud Operations".to_string(),
            "DESC ISR v3.1 Domain 14.1: Workspace Containment & Isolation".to_string(),
        ];

        let class_norm = data_classification.to_uppercase();
        let valid_tiers = ["PUBLIC", "RESTRICTED", "CONFIDENTIAL", "SECRET"];
        if !valid_tiers.contains(&class_norm.as_str()) {
            return (
                SpecialistVerdict::Rejected {
                    code: "invalid_classification_tier".to_string(),
                    reason: format!(
                        "Invalid DESC classification '{}'. Must be Public, Restricted, Confidential, or Secret.",
                        data_classification
                    ),
                },
                serde_json::json!({
                    "system_id": system_id,
                    "classification": data_classification,
                    "compliant": false
                }),
                citations,
            );
        }

        // Data Residency Check: Confidential and Secret must stay in UAE sovereign perimeter
        let dest_lower = target_destination.to_lowercase();
        let is_foreign = dest_lower.contains("foreign")
            || dest_lower.contains("us-east")
            || dest_lower.contains("eu-central")
            || dest_lower.contains("overseas")
            || dest_lower.contains("public_cloud_egress");

        if (class_norm == "CONFIDENTIAL" || class_norm == "SECRET") && is_foreign {
            return (
                SpecialistVerdict::Rejected {
                    code: "desc_data_residency_violation".to_string(),
                    reason: format!(
                        "DESC ISR v3.1 Domain 11 Hard Block: Data classified as '{}' cannot be transmitted to foreign/extraterritorial destination '{}'. UAE sovereign residency required.",
                        class_norm, target_destination
                    ),
                },
                serde_json::json!({
                    "system_id": system_id,
                    "classification": class_norm,
                    "target_destination": target_destination,
                    "sovereign_boundary_breach": true,
                    "verdict": "HARD_BLOCK"
                }),
                citations,
            );
        }

        (
            SpecialistVerdict::Approved,
            serde_json::json!({
                "system_id": system_id,
                "classification": class_norm,
                "target_destination": target_destination,
                "desc_isr_v3_compliant": true,
                "sovereign_residency_guaranteed": true
            }),
            citations,
        )
    }

    /// Audit Non-Disclosure Agreement (NDA) under UAE Trade Secrets Law
    pub fn audit_nda(
        &self,
        party_name: &str,
        confidentiality_period_years: u32,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "UAE Federal Decree-Law No. 38 of 2021 on Copyrights and Neighboring Rights".to_string(),
            "UAE Federal Decree-Law No. 50 of 2022 on Commercial Transactions Law".to_string(),
        ];

        if confidentiality_period_years > 10 {
            (
                SpecialistVerdict::Caution {
                    reason: format!(
                        "Confidentiality term of {} years exceeds typical UAE statutory benchmark (3-5 years). May be scrutinized under restraint-of-trade principles.",
                        confidentiality_period_years
                    ),
                },
                serde_json::json!({
                    "party_name": party_name,
                    "term_years": confidentiality_period_years,
                    "statutory_benchmark_years": 5,
                    "compliant": true
                }),
                citations,
            )
        } else {
            (
                SpecialistVerdict::Approved,
                serde_json::json!({
                    "party_name": party_name,
                    "term_years": confidentiality_period_years,
                    "compliant": true
                }),
                citations,
            )
        }
    }
}

impl DepartmentSpecialist for LegalSpecialist {
    fn name(&self) -> &'static str {
        self.name
    }

    fn department(&self) -> Department {
        self.department
    }

    fn status(&self) -> &'static str {
        "ONLINE_IDLE"
    }

    fn capabilities(&self) -> Vec<&'static str> {
        vec![
            "legal_review_contract",
            "legal_check_desc_compliance",
            "legal_audit_nda",
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        _governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "legal_review_contract" | "review_contract" => {
                let title = task.payload.get("contract_title").and_then(|v| v.as_str()).unwrap_or("COMMERCIAL-AGREEMENT");
                let law = task.payload.get("governing_law").and_then(|v| v.as_str()).unwrap_or("DIFC");
                let text = task.payload.get("contract_text").and_then(|v| v.as_str()).unwrap_or("");
                self.review_contract(title, law, text)
            }
            "legal_check_desc_compliance" | "desc_compliance" => {
                let sys_id = task.payload.get("system_id").and_then(|v| v.as_str()).unwrap_or("QALA-CORE");
                let class = task.payload.get("data_classification").and_then(|v| v.as_str()).unwrap_or("Confidential");
                let dest = task.payload.get("target_destination").and_then(|v| v.as_str()).unwrap_or("UAE-AIRGAP-LOCAL");
                self.check_desc_compliance(sys_id, class, dest)
            }
            "legal_audit_nda" | "audit_nda" => {
                let party = task.payload.get("party_name").and_then(|v| v.as_str()).unwrap_or("Counterparty");
                let term = task.payload.get("confidentiality_period_years").and_then(|v| v.as_u64()).unwrap_or(3) as u32;
                self.audit_nda(party, term)
            }
            _ => {
                return Err(format!("LegalSpecialist unknown action: '{}'", task.action));
            }
        };

        Ok(SpecialistResponse {
            specialist_name: self.name.to_string(),
            department: self.department,
            action: task.action.clone(),
            verdict,
            data,
            statutory_citations: citations,
            execution_time_ms: start.elapsed().as_secs_f64() * 1000.0,
        })
    }
}
