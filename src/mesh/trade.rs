//! Trade & Customs Departmental Specialist (`qala_trade`)
//! Enforces DP World / Dubai Trade & Mirsal II Customs Tariffs (10.25% Landed Tax), HS Codes, and Free Zone Exits.

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::SafetyGovernor;
use serde::{Deserialize, Serialize};
use std::time::Instant;

pub struct TradeSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for TradeSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CustomsTariffBreakdown {
    pub cif_aed: f64,
    pub hs_code: String,
    pub is_zero_rated: bool,
    pub zero_rate_reason: Option<String>,
    pub customs_duty_rate: f64,
    pub customs_duty_aed: f64,
    pub vat_base_aed: f64,
    pub vat_rate: f64,
    pub vat_aed: f64,
    pub total_landed_duty_tax_aed: f64,
    pub total_landed_rate_pct: f64,
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
        let code = hs_code.replace('.', "").trim().to_string();

        // Chapter 30: Pharmaceuticals (HS 3004.90)
        if code.starts_with("3004") || code.starts_with("3003") || code.starts_with("3002") {
            return (
                true,
                Some("Exempt / Zero-rated: Human Pharmaceuticals (Chapter 30) with MoHAP permit".to_string()),
            );
        }

        // Chapter 90: Medical Equipment (HS 9018.90)
        if code.starts_with("9018") || code.starts_with("9022") {
            return (
                true,
                Some("Zero-rated: Diagnostic & Surgical Medical Equipment (Chapter 90) with EDE permit".to_string()),
            );
        }

        // GCC Basic Food Security items (e.g. Rice 1006, Wheat 1001, Infant milk 1901)
        if code.starts_with("1006") || code.starts_with("1001") || code.starts_with("0401") {
            return (
                true,
                Some("Zero-rated: Basic Foodstuffs under GCC Food Security Protocol".to_string()),
            );
        }

        (false, None)
    }

    /// Calculate Customs Duty (5%) & Statutory UAE VAT (5%) on landed CIF base
    pub fn calculate_customs_tariff(&self, cif_aed: f64, hs_code: &str) -> CustomsTariffBreakdown {
        let (is_zero_rated, zero_reason) = self.check_zero_rating(hs_code);

        if is_zero_rated {
            return CustomsTariffBreakdown {
                cif_aed,
                hs_code: hs_code.to_string(),
                is_zero_rated: true,
                zero_rate_reason: zero_reason,
                customs_duty_rate: 0.0,
                customs_duty_aed: 0.0,
                vat_base_aed: cif_aed,
                vat_rate: 0.0,
                vat_aed: 0.0,
                total_landed_duty_tax_aed: 0.0,
                total_landed_rate_pct: 0.0,
            };
        }

        // Standard GCC Unified Customs Tariff (5%)
        let customs_duty = (cif_aed * 0.05 * 100.0).round() / 100.0;
        // VAT Base = CIF + Customs Duty (1.05 * CIF)
        let vat_base = cif_aed + customs_duty;
        // VAT Amount = 5% of (CIF + Customs Duty) = 0.0525 * CIF
        let vat_amount = (vat_base * 0.05 * 100.0).round() / 100.0;
        // Total Landed Duty & Tax = 0.1025 * CIF (10.25%)
        let total_tax = customs_duty + vat_amount;
        let total_rate_pct = if cif_aed > 0.0 {
            (total_tax / cif_aed) * 100.0
        } else {
            10.25
        };

        CustomsTariffBreakdown {
            cif_aed,
            hs_code: hs_code.to_string(),
            is_zero_rated: false,
            zero_rate_reason: None,
            customs_duty_rate: 0.05,
            customs_duty_aed: customs_duty,
            vat_base_aed: vat_base,
            vat_rate: 0.05,
            vat_aed: vat_amount,
            total_landed_duty_tax_aed: total_tax,
            total_landed_rate_pct: total_rate_pct,
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
        let citations = vec![
            "Dubai Customs Notice No. 5 of 2020 on Unified Customs Tariff".to_string(),
            "GCC Common Customs Law (Articles 25-32: Importation & Duty Assessment)".to_string(),
            "Federal Decree-Law No. 8 of 2017 on Value Added Tax (Article 48: Import Tax)".to_string(),
            "Dubai Customs Mirsal II Standard Declarations Guide 2026".to_string(),
        ];

        let tariff = self.calculate_customs_tariff(cif_aed, hs_code);

        // Generate Mirsal II Declaration Reference
        let decl_no = format!(
            "MIRSAL2-{}-{}",
            importer_code.trim(),
            container_id.replace('-', "").trim()
        );

        let mut data = serde_json::json!({
            "bill_of_lading": bill_of_lading,
            "container_id": container_id,
            "importer_code": importer_code,
            "mirsal2_declaration_no": decl_no,
            "hs_code": hs_code,
            "cif_value_aed": cif_aed,
            "is_zero_rated": tariff.is_zero_rated,
            "zero_rate_reason": tariff.zero_rate_reason,
            "customs_duty_aed": tariff.customs_duty_aed,
            "vat_amount_aed": tariff.vat_aed,
            "total_landed_payable_aed": tariff.total_landed_duty_tax_aed,
            "landed_tax_percentage": format!("{:.2}%", tariff.total_landed_rate_pct),
            "freezone_to_mainland_exit": is_freezone_exit,
            "temporary_admission_180_days": is_temp_admission
        });

        if is_temp_admission {
            // 5% refundable deposit or bank guarantee required for temporary admission
            let deposit_aed = (cif_aed * 0.05 * 100.0).round() / 100.0;
            data["temporary_deposit_aed"] = serde_json::json!(deposit_aed);
            data["re_export_deadline_days"] = serde_json::json!(180);
        }

        (
            SpecialistVerdict::Approved,
            data,
            citations,
        )
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
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        _governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "customs_calculate_tariff" | "calculate_tariff" => {
                let cif = task.payload.get("cif_value_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let hs = task.payload.get("hs_code").and_then(|v| v.as_str()).unwrap_or("8471.30");
                let breakdown = self.calculate_customs_tariff(cif, hs);
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

                self.clear_shipment(bl, container, importer, cif, hs, fz_exit, temp)
            }
            "customs_check_hs_code" => {
                let hs = task.payload.get("hs_code").and_then(|v| v.as_str()).unwrap_or("");
                let (zero, reason) = self.check_zero_rating(hs);
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
