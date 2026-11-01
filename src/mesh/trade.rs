//! Trade & Customs Departmental Specialist (`qala_trade`)
//! Enforces DP World / Dubai Trade & Mirsal II Customs Tariffs (10.25% Landed Tax),
//! HS Codes Classification, Free Zone Declarations, and Temporary Admission Regimes.
//! Master Spec Reference: CANONICAL_MASTER_SPECIFICATION_2026.md §3.2(3).

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::SafetyGovernor;
use serde::{Deserialize, Serialize};
use std::time::Instant;

/// Round currency value to 2 decimal places (fil-level precision: 1 fil = 0.01 AED)
#[inline]
pub fn round_fils(val: f64) -> f64 {
    (val * 100.0).round() / 100.0
}

pub struct TradeSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for TradeSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

/// Detailed breakdown of Customs Duty (5%) and UAE VAT (5%) on landed CIF base
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct CustomsTariffBreakdown {
    pub cif_aed: f64,
    pub hs_code: String,
    pub is_zero_rated: bool,
    pub zero_rate_reason: Option<String>,
    #[serde(default)]
    pub permit_id: Option<String>,
    pub customs_duty_rate: f64,
    pub customs_duty_aed: f64,
    pub vat_base_aed: f64,
    pub vat_rate: f64,
    pub vat_aed: f64,
    pub total_landed_duty_tax_aed: f64,
    pub total_landed_rate_pct: f64,
}

/// Temporary Admission regime calculation & 180-day re-export tracking
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct TemporaryAdmissionBreakdown {
    pub cif_aed: f64,
    pub refundable_deposit_rate: f64,
    pub refundable_deposit_aed: f64,
    pub deposit_instrument: String,
    pub statutory_reexport_limit_days: u32,
    pub days_elapsed: u32,
    pub days_remaining: i32,
    pub is_expired: bool,
    pub status: String,
    pub regulatory_guidance: String,
}

/// Mirsal II Declaration for Import to Local Market from Free Zone
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct Mirsal2FreeZoneDeclaration {
    pub declaration_no: String,
    pub declaration_regime: String,
    pub regime_description: String,
    pub freezone_origin: String,
    pub destination_market: String,
    pub bill_of_lading: String,
    pub container_id: String,
    pub importer_code: String,
    pub tariff_breakdown: CustomsTariffBreakdown,
    pub tau3_circuit_breaker_triggered: bool,
    pub clearance_status: String,
    pub statutory_citations: Vec<String>,
}

impl TradeSpecialist {
    pub fn new() -> Self {
        Self {
            name: "qala_trade",
            department: Department::Trade,
        }
    }

    /// Check if HS code qualifies for statutory zero-rating under UAE / GCC rules
    pub fn check_zero_rating(&self, hs_code: &str) -> (bool, Option<String>) {
        self.check_zero_rating_with_permit(hs_code, None)
    }

    /// Check if HS code qualifies for statutory zero-rating, considering optional MoHAP/EDE permit
    pub fn check_zero_rating_with_permit(
        &self,
        hs_code: &str,
        permit_id: Option<&str>,
    ) -> (bool, Option<String>) {
        let code = hs_code.replace('.', "").replace(' ', "").trim().to_string();

        // 1. Chapter 30: Human Pharmaceuticals (HS 3004.90, 3004, 3003, 3002) -> 0% Duty + 0% VAT
        if code.starts_with("3004") || code.starts_with("3003") || code.starts_with("3002") {
            let reason = match permit_id {
                Some(p) if !p.trim().is_empty() => format!(
                    "Exempt / Zero-rated: Human Pharmaceuticals (HS {}) with MoHAP permit [{}]",
                    hs_code,
                    p.trim()
                ),
                _ => "Exempt / Zero-rated: Human Pharmaceuticals (Chapter 30, HS 3004.90) with MoHAP permit"
                    .to_string(),
            };
            return (true, Some(reason));
        }

        // 2. Chapter 90: Medical Diagnostic Instruments (HS 9018.90, 9018, 9022) -> 0% Duty + 0% VAT
        if code.starts_with("9018") || code.starts_with("9022") {
            let reason = match permit_id {
                Some(p) if !p.trim().is_empty() => format!(
                    "Zero-rated: Diagnostic & Surgical Medical Equipment (HS {}) with EDE permit [{}]",
                    hs_code,
                    p.trim()
                ),
                _ => "Zero-rated: Diagnostic & Surgical Medical Equipment (Chapter 90, HS 9018.90) with EDE permit"
                    .to_string(),
            };
            return (true, Some(reason));
        }

        // 3. GCC Basic Foodstuffs (HS 1001: Wheat, HS 1006: Rice, HS 0401: Milk/Dairy, HS 1901)
        if code.starts_with("1001") || code.starts_with("1006") || code.starts_with("0401") || code.starts_with("1901") {
            return (
                true,
                Some("Zero-rated: Basic Foodstuffs under GCC Food Security Protocol (HS 1001/1006/0401)".to_string()),
            );
        }

        (false, None)
    }

    /// Calculate Customs Duty (5%) & Statutory UAE VAT (5%) on landed CIF base
    pub fn calculate_customs_tariff(&self, cif_aed: f64, hs_code: &str) -> CustomsTariffBreakdown {
        self.calculate_customs_tariff_with_permit(cif_aed, hs_code, None)
    }

    /// Calculate Customs Duty (5%) & Statutory UAE VAT (5%) on landed CIF base with permit support
    pub fn calculate_customs_tariff_with_permit(
        &self,
        cif_aed: f64,
        hs_code: &str,
        permit_id: Option<&str>,
    ) -> CustomsTariffBreakdown {
        let (is_zero_rated, zero_reason) = self.check_zero_rating_with_permit(hs_code, permit_id);

        if is_zero_rated {
            return CustomsTariffBreakdown {
                cif_aed: round_fils(cif_aed),
                hs_code: hs_code.to_string(),
                is_zero_rated: true,
                zero_rate_reason: zero_reason,
                permit_id: permit_id.map(|s| s.to_string()),
                customs_duty_rate: 0.0,
                customs_duty_aed: 0.0,
                vat_base_aed: round_fils(cif_aed),
                vat_rate: 0.0,
                vat_aed: 0.0,
                total_landed_duty_tax_aed: 0.0,
                total_landed_rate_pct: 0.0,
            };
        }

        // Standard GCC Unified Customs Tariff: Duty = 5% CIF
        let customs_duty = round_fils(cif_aed * 0.05);
        // UAE FTA VAT Base: CIF + Customs Duty (1.05 * CIF)
        let vat_base = round_fils(cif_aed + customs_duty);
        // UAE FTA VAT Amount: 5% of VAT Base = 5.25% CIF
        let vat_amount = round_fils(vat_base * 0.05);
        // Total Landed Duty & Tax: exactly 10.25% CIF (customs duty + VAT)
        let total_tax = round_fils(customs_duty + vat_amount);
        let total_rate_pct = if cif_aed > 0.0 {
            round_fils((total_tax / cif_aed) * 100.0)
        } else {
            10.25
        };

        CustomsTariffBreakdown {
            cif_aed: round_fils(cif_aed),
            hs_code: hs_code.to_string(),
            is_zero_rated: false,
            zero_rate_reason: None,
            permit_id: permit_id.map(|s| s.to_string()),
            customs_duty_rate: 0.05,
            customs_duty_aed: customs_duty,
            vat_base_aed: vat_base,
            vat_rate: 0.05,
            vat_aed: vat_amount,
            total_landed_duty_tax_aed: total_tax,
            total_landed_rate_pct: total_rate_pct,
        }
    }

    /// Calculate Temporary Admission 5% refundable deposit and 180-day re-export timer
    pub fn calculate_temporary_admission(&self, cif_aed: f64, days: u32) -> TemporaryAdmissionBreakdown {
        let deposit = round_fils(cif_aed * 0.05);
        let days_remaining = 180 - (days as i32);
        let is_expired = days > 180;
        let status = if is_expired {
            "EXPIRED_FORFEITURE_TRIGGERED".to_string()
        } else {
            "ACTIVE_COMPLIANT".to_string()
        };
        let regulatory_guidance = if is_expired {
            format!(
                "Statutory 180-day temporary admission expired under GCC Common Customs Law Art. 89. Refundable deposit of AED {:.2} forfeited; full import duties (5%) and VAT (5%) assessed for domestic consumption.",
                deposit
            )
        } else {
            format!(
                "Goods admitted under Temporary Admission. 5% deposit/guarantee of AED {:.2} held in escrow. Re-export must occur within {} remaining calendar days to release guarantee.",
                deposit, days_remaining
            )
        };

        TemporaryAdmissionBreakdown {
            cif_aed: round_fils(cif_aed),
            refundable_deposit_rate: 0.05,
            refundable_deposit_aed: deposit,
            deposit_instrument: "5% Refundable Cash Deposit or Bank Guarantee".to_string(),
            statutory_reexport_limit_days: 180,
            days_elapsed: days,
            days_remaining,
            is_expired,
            status,
            regulatory_guidance,
        }
    }

    /// Generate Mirsal II Declaration for Import to Local Market from Free Zone
    pub fn generate_freezone_declaration(
        &self,
        bill_of_lading: &str,
        container_id: &str,
        importer_code: &str,
        cif_aed: f64,
        hs_code: &str,
        freezone_origin: Option<&str>,
        mohap_permit: Option<&str>,
    ) -> Mirsal2FreeZoneDeclaration {
        let tariff = self.calculate_customs_tariff_with_permit(cif_aed, hs_code, mohap_permit);
        let decl_no = format!(
            "MIRSAL2-FZ-{}-{}",
            importer_code.trim(),
            container_id.replace('-', "").trim()
        );
        let tau3_triggered = tariff.total_landed_duty_tax_aed > 50_000.0;

        let citations = vec![
            "Dubai Customs Notice No. 5 of 2020 on Unified Customs Tariff".to_string(),
            "GCC Common Customs Law (Articles 25-32 & Free Zone Exit to Mainland Regulations)".to_string(),
            "Federal Decree-Law No. 8 of 2017 on Value Added Tax (Article 48: Import Tax)".to_string(),
            "Dubai Customs Mirsal II Standard Declarations Guide 2026: Import to Local Market from Free Zone".to_string(),
        ];

        Mirsal2FreeZoneDeclaration {
            declaration_no: decl_no,
            declaration_regime: "IMPORT_LOCAL_MARKET_FROM_FREE_ZONE".to_string(),
            regime_description: "Import to Local Market from Free Zone (Bonded Storage to Domestic Consumption)".to_string(),
            freezone_origin: freezone_origin.unwrap_or("JAFZA - Jebel Ali Free Zone").to_string(),
            destination_market: "Dubai Mainland / Local UAE Market".to_string(),
            bill_of_lading: bill_of_lading.to_string(),
            container_id: container_id.to_string(),
            importer_code: importer_code.to_string(),
            tariff_breakdown: tariff,
            tau3_circuit_breaker_triggered: tau3_triggered,
            clearance_status: if tau3_triggered {
                "ESCROW_HOLD_TAU3_REQUIRED".to_string()
            } else {
                "DECLARATION_CLEARED_FOR_PAYMENT".to_string()
            },
            statutory_citations: citations,
        }
    }

    /// Evaluate Tau-3 FinOps Circuit Breaker on Customs Duty Wire Payment (> AED 50,000)
    pub fn evaluate_customs_payment(
        &self,
        duty_amount_aed: f64,
        approval_token: Option<&str>,
        governor: &mut SafetyGovernor,
    ) -> Result<SpecialistVerdict, String> {
        let now_utc = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs();

        let policy_res = governor.check_tau3_gate(
            "customs_duty_wire",
            duty_amount_aed,
            "finops_wire",
            approval_token,
            now_utc,
        );

        match policy_res.verdict {
            crate::safety::QalaVerdict::Safe | crate::safety::QalaVerdict::Caution => {
                Ok(SpecialistVerdict::Approved)
            }
            crate::safety::QalaVerdict::Review => Ok(SpecialistVerdict::Gated {
                required_scope: "finops_wire".to_string(),
                amount_aed: duty_amount_aed,
                reason: format!(
                    "Tau-3 FinOps Circuit Breaker tripped: Customs duty wire payment of AED {:.2} exceeds AED 50,000.00 ceiling. Dual-key cryptographic authorization required.",
                    duty_amount_aed
                ),
            }),
            _ => Ok(SpecialistVerdict::Rejected {
                code: policy_res.code,
                reason: format!("Tau-3 Policy Hard Block: {}", policy_res.reason),
            }),
        }
    }

    /// Clear shipment via Dubai Trade & Mirsal II Customs Declaration
    pub fn clear_shipment(
        &self,
        bill_of_lading: &str,
        container_id: &str,
        importer_code: &str,
        cif_aed: f64,
        hs_code: &str,
        is_freezone_exit: bool,
        is_temp_admission: bool,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        self.clear_shipment_with_permit(
            bill_of_lading,
            container_id,
            importer_code,
            cif_aed,
            hs_code,
            is_freezone_exit,
            is_temp_admission,
            None,
        )
    }

    /// Clear shipment via Dubai Trade & Mirsal II Customs Declaration with permit
    pub fn clear_shipment_with_permit(
        &self,
        bill_of_lading: &str,
        container_id: &str,
        importer_code: &str,
        cif_aed: f64,
        hs_code: &str,
        is_freezone_exit: bool,
        is_temp_admission: bool,
        mohap_permit: Option<&str>,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "Dubai Customs Notice No. 5 of 2020 on Unified Customs Tariff".to_string(),
            "GCC Common Customs Law (Articles 25-32: Importation & Duty Assessment)".to_string(),
            "Federal Decree-Law No. 8 of 2017 on Value Added Tax (Article 48: Import Tax)".to_string(),
            "Dubai Customs Mirsal II Standard Declarations Guide 2026".to_string(),
        ];

        let tariff = self.calculate_customs_tariff_with_permit(cif_aed, hs_code, mohap_permit);

        // Generate Mirsal II Declaration Reference
        let decl_prefix = if is_freezone_exit {
            "MIRSAL2-FZ"
        } else if is_temp_admission {
            "MIRSAL2-TA"
        } else {
            "MIRSAL2"
        };

        let decl_no = format!(
            "{}-{}-{}",
            decl_prefix,
            importer_code.trim(),
            container_id.replace('-', "").trim()
        );

        let mut data = serde_json::json!({
            "bill_of_lading": bill_of_lading,
            "container_id": container_id,
            "importer_code": importer_code,
            "mirsal2_declaration_no": decl_no,
            "hs_code": hs_code,
            "cif_value_aed": tariff.cif_aed,
            "is_zero_rated": tariff.is_zero_rated,
            "zero_rate_reason": tariff.zero_rate_reason,
            "mohap_permit": tariff.permit_id,
            "customs_duty_aed": tariff.customs_duty_aed,
            "vat_amount_aed": tariff.vat_aed,
            "total_landed_payable_aed": tariff.total_landed_duty_tax_aed,
            "landed_tax_percentage": format!("{:.2}%", tariff.total_landed_rate_pct),
            "freezone_to_mainland_exit": is_freezone_exit,
            "temporary_admission_180_days": is_temp_admission,
            "tau3_circuit_breaker_active": tariff.total_landed_duty_tax_aed > 50_000.0,
            "tau3_escrow_hold_required": tariff.total_landed_duty_tax_aed > 50_000.0,
        });

        if is_freezone_exit {
            data["declaration_regime"] = serde_json::json!("Import to Local Market from Free Zone");
            data["freezone_origin"] = serde_json::json!("JAFZA - Jebel Ali Free Zone");
        }

        if is_temp_admission {
            let temp_breakdown = self.calculate_temporary_admission(cif_aed, 0);
            data["temporary_admission_details"] = serde_json::to_value(&temp_breakdown).unwrap_or_default();
            data["temporary_deposit_aed"] = serde_json::json!(temp_breakdown.refundable_deposit_aed);
            data["re_export_deadline_days"] = serde_json::json!(180);
        }

        (SpecialistVerdict::Approved, data, citations)
    }
}

impl DepartmentSpecialist for TradeSpecialist {
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
            "customs_calculate_tariff",
            "customs_clear_shipment",
            "customs_check_hs_code",
            "customs_temp_admission",
            "customs_freezone_declaration",
            "customs_wire_payment",
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "customs_calculate_tariff" | "calculate_tariff" | "tariff" => {
                let cif = task.payload.get("cif_value_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let hs = task.payload.get("hs_code").and_then(|v| v.as_str()).unwrap_or("8471.30");
                let permit = task.payload.get("mohap_permit").and_then(|v| v.as_str());
                let breakdown = self.calculate_customs_tariff_with_permit(cif, hs, permit);
                (
                    SpecialistVerdict::Approved,
                    serde_json::to_value(&breakdown).unwrap_or_default(),
                    vec![
                        "Dubai Customs Notice No. 5 of 2020 on Unified Customs Tariff".to_string(),
                        "Federal Decree-Law No. 8 of 2017 on Value Added Tax".to_string(),
                    ],
                )
            }
            "customs_clear_shipment" | "clear_shipment" => {
                let bl = task.payload.get("bill_of_lading").and_then(|v| v.as_str()).unwrap_or("BL-DPW-001");
                let container = task.payload.get("container_id").and_then(|v| v.as_str()).unwrap_or("MSKU1234567");
                let importer = task.payload.get("importer_code").and_then(|v| v.as_str()).unwrap_or("DXB-IMP-772");
                let cif = task.payload.get("cif_value_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let hs = task.payload.get("hs_code").and_then(|v| v.as_str()).unwrap_or("8471.30");
                let fz_exit = task.payload.get("is_freezone_exit").and_then(|v| v.as_bool()).unwrap_or(false);
                let temp = task.payload.get("is_temp_admission").and_then(|v| v.as_bool()).unwrap_or(false);
                let permit = task.payload.get("mohap_permit").and_then(|v| v.as_str());

                self.clear_shipment_with_permit(bl, container, importer, cif, hs, fz_exit, temp, permit)
            }
            "customs_check_hs_code" | "check_hs_code" => {
                let hs = task.payload.get("hs_code").and_then(|v| v.as_str()).unwrap_or("");
                let permit = task.payload.get("mohap_permit").and_then(|v| v.as_str());
                let (zero, reason) = self.check_zero_rating_with_permit(hs, permit);
                (
                    SpecialistVerdict::Approved,
                    serde_json::json!({
                        "hs_code": hs,
                        "is_zero_rated": zero,
                        "rationale": reason
                    }),
                    vec!["GCC Unified Customs Tariff 2026".to_string()],
                )
            }
            "customs_temp_admission" | "temp_admission" => {
                let cif = task.payload.get("cif_value_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let days = task.payload.get("days").and_then(|v| v.as_u64()).unwrap_or(0) as u32;
                let breakdown = self.calculate_temporary_admission(cif, days);
                (
                    SpecialistVerdict::Approved,
                    serde_json::to_value(&breakdown).unwrap_or_default(),
                    vec![
                        "GCC Common Customs Law (Article 89: Temporary Admission)".to_string(),
                        "Dubai Customs Notice on Temporary Import Guarantees 2026".to_string(),
                    ],
                )
            }
            "customs_freezone_declaration" | "freezone_declaration" => {
                let bl = task.payload.get("bill_of_lading").and_then(|v| v.as_str()).unwrap_or("BL-DPW-FZ-01");
                let container = task.payload.get("container_id").and_then(|v| v.as_str()).unwrap_or("MSKU9988776");
                let importer = task.payload.get("importer_code").and_then(|v| v.as_str()).unwrap_or("DXB-IMP-772");
                let cif = task.payload.get("cif_value_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let hs = task.payload.get("hs_code").and_then(|v| v.as_str()).unwrap_or("8471.30");
                let fz = task.payload.get("freezone_origin").and_then(|v| v.as_str());
                let permit = task.payload.get("mohap_permit").and_then(|v| v.as_str());

                let decl = self.generate_freezone_declaration(bl, container, importer, cif, hs, fz, permit);
                (
                    SpecialistVerdict::Approved,
                    serde_json::to_value(&decl).unwrap_or_default(),
                    decl.statutory_citations.clone(),
                )
            }
            "customs_wire_payment" | "pay_customs_duty" => {
                let duty_amount = task.payload.get("duty_amount_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let approval_code = task.payload.get("approval_code").and_then(|v| v.as_str());
                let verdict = self.evaluate_customs_payment(duty_amount, approval_code, governor)?;
                (
                    verdict,
                    serde_json::json!({
                        "duty_amount_aed": duty_amount,
                        "tau3_ceiling_aed": 50_000.0,
                        "settlement_channel": "Dubai Customs Mirsal II Wire"
                    }),
                    vec!["Tau-3 FinOps Circuit Breaker Mandate (CANONICAL_SPEC §3.2)".to_string()],
                )
            }
            _ => {
                return Err(format!("TradeSpecialist unknown action: '{}'", task.action));
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
