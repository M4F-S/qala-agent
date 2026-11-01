//! Finance & FinOps Departmental Specialist (`qala_finance`)
//! Enforces UAE VAT (5%), Federal Corporate Tax (9%), and Tau-3 FinOps Circuit Breaker (> AED 50k).

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::{QalaVerdict, SafetyGovernor};
use std::time::Instant;

pub struct FinanceSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for FinanceSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

impl FinanceSpecialist {
    pub fn new() -> Self {
        Self {
            name: "qala_finance",
            department: Department::Finance,
        }
    }

    /// UAE FTA 5% VAT Reconciliation
    pub fn reconcile_vat(
        &self,
        invoice_no: &str,
        trn: &str,
        net_aed: f64,
        vat_aed: f64,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "UAE Federal Decree-Law No. 8 of 2017 on Value Added Tax".to_string(),
            "Cabinet Decision No. 52 of 2017 on Executive Regulations of VAT Law".to_string(),
        ];

        // Validate TRN: 15 digits, starts with 100
        let trn_clean = trn.trim().replace('-', "").replace(' ', "");
        let valid_trn = trn_clean.len() == 15
            && trn_clean.starts_with("100")
            && trn_clean.chars().all(|c| c.is_ascii_digit());

        if !valid_trn {
            return (
                SpecialistVerdict::Rejected {
                    code: "invalid_trn".to_string(),
                    reason: format!(
                        "Invalid UAE Tax Registration Number (TRN): '{}'. Must be 15 digits starting with 100.",
                        trn
                    ),
                },
                serde_json::json!({
                    "invoice_no": invoice_no,
                    "trn": trn,
                    "valid": false,
                }),
                citations,
            );
        }

        let expected_vat = (net_aed * 0.05 * 100.0).round() / 100.0;
        let diff = (vat_aed - expected_vat).abs();

        if diff > 0.02 {
            return (
                SpecialistVerdict::Rejected {
                    code: "vat_mismatch".to_string(),
                    reason: format!(
                        "VAT calculation mismatch for invoice {}: charged AED {:.2}, expected 5% AED {:.2} (difference: AED {:.2}).",
                        invoice_no, vat_aed, expected_vat, diff
                    ),
                },
                serde_json::json!({
                    "invoice_no": invoice_no,
                    "trn": trn_clean,
                    "net_aed": net_aed,
                    "vat_billed_aed": vat_aed,
                    "vat_expected_aed": expected_vat,
                    "variance_aed": diff,
                    "reconciled": false
                }),
                citations,
            );
        }

        (
            SpecialistVerdict::Approved,
            serde_json::json!({
                "invoice_no": invoice_no,
                "trn": trn_clean,
                "net_aed": net_aed,
                "vat_billed_aed": vat_aed,
                "vat_expected_aed": expected_vat,
                "variance_aed": diff,
                "reconciled": true,
                "rate": "5%"
            }),
            citations,
        )
    }

    /// Federal Corporate Tax Calculation (9% on taxable profit > AED 375,000)
    pub fn calculate_corporate_tax(
        &self,
        net_profit_aed: f64,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "UAE Federal Decree-Law No. 47 of 2022 on the Taxation of Corporations and Businesses".to_string(),
            "Cabinet Decision No. 116 of 2022 on the Determination of Taxable Income Threshold".to_string(),
        ];

        let exempt_threshold = 375_000.0;
        let (taxable_base, tax_amount) = if net_profit_aed <= exempt_threshold {
            (0.0, 0.0)
        } else {
            let base = net_profit_aed - exempt_threshold;
            let tax = (base * 0.09 * 100.0).round() / 100.0;
            (base, tax)
        };

        (
            SpecialistVerdict::Approved,
            serde_json::json!({
                "net_profit_aed": net_profit_aed,
                "statutory_exempt_threshold_aed": exempt_threshold,
                "taxable_base_aed": taxable_base,
                "effective_rate": if net_profit_aed > 0.0 { format!("{:.2}%", (tax_amount / net_profit_aed) * 100.0) } else { "0.00%".to_string() },
                "statutory_rate": "9.00%",
                "corporate_tax_due_aed": tax_amount
            }),
            citations,
        )
    }

    /// Tau-3 FinOps Wire Request with Dual-Key Escrow Gate (> AED 50k)
    pub fn request_wire(
        &self,
        beneficiary: &str,
        iban: &str,
        amount_aed: f64,
        memo: &str,
        approval_code: Option<&str>,
        governor: &mut SafetyGovernor,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "Tau-3 FinOps Policy (Policy 3.1: Automated Approval Ceiling AED 50,000.00)".to_string(),
            "UAE Central Bank Automated Clearing System Regulations".to_string(),
        ];

        let now_utc = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();

        let policy_res = governor.check_tau3_gate(
            "finops_request_wire",
            amount_aed,
            "finops_wire",
            approval_code,
            now_utc,
        );

        match policy_res.verdict {
            QalaVerdict::Safe | QalaVerdict::Caution => (
                SpecialistVerdict::Approved,
                serde_json::json!({
                    "beneficiary": beneficiary,
                    "iban": iban,
                    "amount_aed": amount_aed,
                    "memo": memo,
                    "status": "WIRE_DISPATCHED",
                    "tau3_gate_cleared": true,
                    "approval_token_used": approval_code.unwrap_or("AUTONOMOUS_SUB_50K")
                }),
                citations,
            ),
            QalaVerdict::Review => (
                SpecialistVerdict::Gated {
                    required_scope: "finops_wire".to_string(),
                    amount_aed,
                    reason: format!(
                        "Tau-3 Circuit Breaker Tripped: Transaction of AED {:.2} exceeds AED 50,000.00 autonomous ceiling. Escrow lock engaged. Dual-key cryptographic code required (scope: finops_wire).",
                        amount_aed
                    ),
                },
                serde_json::json!({
                    "beneficiary": beneficiary,
                    "iban": iban,
                    "amount_aed": amount_aed,
                    "status": "ESCROW_LOCKED_PENDING_APPROVAL",
                    "tau3_gate_active": true,
                    "threshold_aed": 50000.0,
                    "governor_code": policy_res.code,
                    "governor_reason": policy_res.reason
                }),
                citations,
            ),
            _ => (
                SpecialistVerdict::Rejected {
                    code: policy_res.code.clone(),
                    reason: format!("Tau-3 Policy Hard Block: {}", policy_res.reason),
                },
                serde_json::json!({
                    "beneficiary": beneficiary,
                    "iban": iban,
                    "amount_aed": amount_aed,
                    "status": "TRANSACTION_BLOCKED",
                    "governor_code": policy_res.code,
                    "governor_reason": policy_res.reason
                }),
                citations,
            ),
        }
    }

    /// Wages Protection System (WPS) Payroll Audit
    pub fn audit_payroll(
        &self,
        month: &str,
        total_wps_aed: f64,
        employee_count: usize,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "Ministerial Resolution No. 43 of 2022 on Wages Protection System (WPS)".to_string(),
            "UAE Federal Decree-Law No. 33 of 2021 Article 22".to_string(),
        ];

        if employee_count == 0 {
            return (
                SpecialistVerdict::Rejected {
                    code: "zero_headcount".to_string(),
                    reason: "WPS audit requires at least one registered employee.".to_string(),
                },
                serde_json::json!({"valid": false}),
                citations,
            );
        }

        let avg_salary = total_wps_aed / (employee_count as f64);
        let compliant = total_wps_aed > 0.0 && avg_salary >= 1000.0;

        (
            if compliant {
                SpecialistVerdict::Approved
            } else {
                SpecialistVerdict::Caution {
                    reason: "Average wage below standard baseline or zero allocation.".to_string(),
                }
            },
            serde_json::json!({
                "month": month,
                "total_wps_aed": total_wps_aed,
                "employee_count": employee_count,
                "average_salary_aed": avg_salary,
                "wps_compliant": compliant,
                "statutory_deadline_days": 15
            }),
            citations,
        )
    }
}

impl DepartmentSpecialist for FinanceSpecialist {
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
            "finops_reconcile_vat",
            "finops_calculate_corporate_tax",
            "finops_request_wire",
            "finops_audit_payroll",
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "finops_reconcile_vat" | "vat_reconcile" => {
                let invoice_no = task.payload.get("invoice_no").and_then(|v| v.as_str()).unwrap_or("INV-001");
                let trn = task.payload.get("trn").and_then(|v| v.as_str()).unwrap_or("");
                let net_aed = task.payload.get("net_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let vat_aed = task.payload.get("vat_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                self.reconcile_vat(invoice_no, trn, net_aed, vat_aed)
            }
            "finops_calculate_corporate_tax" | "corporate_tax" => {
                let profit = task.payload.get("net_profit_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                self.calculate_corporate_tax(profit)
            }
            "finops_request_wire" | "wire_transfer" => {
                let beneficiary = task.payload.get("beneficiary").and_then(|v| v.as_str()).unwrap_or("UNKNOWN");
                let iban = task.payload.get("iban").and_then(|v| v.as_str()).unwrap_or("");
                let amount = task.payload.get("amount_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let memo = task.payload.get("memo").and_then(|v| v.as_str()).unwrap_or("");
                let token = task.payload.get("approval_code").and_then(|v| v.as_str());
                self.request_wire(beneficiary, iban, amount, memo, token, governor)
            }
            "finops_audit_payroll" | "wps_audit" => {
                let month = task.payload.get("month").and_then(|v| v.as_str()).unwrap_or("CURRENT");
                let total = task.payload.get("total_wps_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let count = task.payload.get("employee_count").and_then(|v| v.as_u64()).unwrap_or(0) as usize;
                self.audit_payroll(month, total, count)
            }
            _ => {
                return Err(format!("FinanceSpecialist unknown action: '{}'", task.action));
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
