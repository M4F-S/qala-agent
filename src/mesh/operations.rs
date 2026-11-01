//! Operations Departmental Specialist (`qala_ops`)
//! Handles Facilities Management, DEWA Commercial Utility Audits, Jebel Ali Logistics & Enterprise SLAs.

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::SafetyGovernor;
use std::time::Instant;

pub struct OperationsSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for OperationsSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

impl OperationsSpecialist {
    pub fn new() -> Self {
        Self {
            name: "qala_ops",
            department: Department::Operations,
        }
    }

    /// Audit DEWA Commercial Electricity Bill using statutory slab tariffs
    pub fn audit_dewa_commercial_bill(
        &self,
        account_no: &str,
        kwh_consumed: f64,
        billed_aed: f64,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "Dubai Electricity and Water Authority (DEWA) Commercial Slab Tariff Regulations 2026".to_string(),
            "Supreme Council of Energy Directive on Fuel Surcharge Adjustments".to_string(),
        ];

        // Commercial Electricity Slab Tariffs (fils / kWh):
        // 1 - 2,000: 23 fils (0.23 AED)
        // 2,001 - 4,000: 28 fils (0.28 AED)
        // 4,001 - 6,000: 32 fils (0.32 AED)
        // > 6,000: 38 fils (0.38 AED)
        // Fuel surcharge: 5.0 fils (0.05 AED) per kWh

        let mut rem = kwh_consumed;
        let mut base_tariff_aed = 0.0;

        let slab1 = rem.min(2000.0);
        base_tariff_aed += slab1 * 0.23;
        rem -= slab1;

        if rem > 0.0 {
            let slab2 = rem.min(2000.0);
            base_tariff_aed += slab2 * 0.28;
            rem -= slab2;
        }

        if rem > 0.0 {
            let slab3 = rem.min(2000.0);
            base_tariff_aed += slab3 * 0.32;
            rem -= slab3;
        }

        if rem > 0.0 {
            base_tariff_aed += rem * 0.38;
        }

        let fuel_surcharge_aed = kwh_consumed * 0.05;
        let expected_total_aed = (base_tariff_aed + fuel_surcharge_aed * 100.0).round() / 100.0;
        let diff = (billed_aed - expected_total_aed).abs();

        if diff > 1.0 {
            return (
                SpecialistVerdict::Caution {
                    reason: format!(
                        "DEWA commercial bill variance: Billed AED {:.2}, expected tariff AED {:.2} (Variance: AED {:.2}).",
                        billed_aed, expected_total_aed, diff
                    ),
                },
                serde_json::json!({
                    "account_no": account_no,
                    "kwh_consumed": kwh_consumed,
                    "billed_aed": billed_aed,
                    "expected_aed": expected_total_aed,
                    "variance_aed": diff,
                    "fuel_surcharge_aed": fuel_surcharge_aed,
                    "reconciled": false
                }),
                citations,
            );
        }

        (
            SpecialistVerdict::Approved,
            serde_json::json!({
                "account_no": account_no,
                "kwh_consumed": kwh_consumed,
                "billed_aed": billed_aed,
                "expected_aed": expected_total_aed,
                "fuel_surcharge_aed": fuel_surcharge_aed,
                "reconciled": true
            }),
            citations,
        )
    }

    /// Track DP World Jebel Ali Container Terminal Status & Demurrage Risk
    pub fn track_container(
        &self,
        container_id: &str,
        terminal: &str,
        days_in_yard: u32,
        free_days_limit: u32,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "DP World Jebel Ali Port Tariff & Regulations Book (Section 4: Storage & Demurrage)".to_string(),
            "Dubai Trade Container Tracking Service Level Agreement".to_string(),
        ];

        let free_days = if free_days_limit == 0 { 5 } else { free_days_limit };
        let demurrage_rate_per_day = 350.0; // Standard 20ft/40ft dry demurrage in AED

        if days_in_yard > free_days {
            let overstay_days = days_in_yard - free_days;
            let demurrage_aed = (overstay_days as f64) * demurrage_rate_per_day;

            (
                SpecialistVerdict::Caution {
                    reason: format!(
                        "Demurrage Incurred: Container {} overstayed by {} day(s). Accumulated charges: AED {:.2}.",
                        container_id, overstay_days, demurrage_aed
                    ),
                },
                serde_json::json!({
                    "container_id": container_id,
                    "terminal": terminal,
                    "days_in_yard": days_in_yard,
                    "free_days_allowed": free_days,
                    "overstay_days": overstay_days,
                    "demurrage_charges_aed": demurrage_aed,
                    "status": "DEMURRAGE_ACCUMULATING"
                }),
                citations,
            )
        } else {
            let days_remaining = free_days - days_in_yard;
            (
                SpecialistVerdict::Approved,
                serde_json::json!({
                    "container_id": container_id,
                    "terminal": terminal,
                    "days_in_yard": days_in_yard,
                    "free_days_allowed": free_days,
                    "free_days_remaining": days_remaining,
                    "demurrage_charges_aed": 0.0,
                    "status": "WITHIN_FREE_TIME",
                    "gate_pass_ready": true
                }),
                citations,
            )
        }
    }

    /// Enterprise SLA Compliance Evaluation
    pub fn check_sla(
        &self,
        service_id: &str,
        response_time_ms: f64,
        max_threshold_ms: f64,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "Dubai Digital Authority Service Level Governance Standard 2026".to_string(),
        ];

        let compliant = response_time_ms <= max_threshold_ms;

        if compliant {
            (
                SpecialistVerdict::Approved,
                serde_json::json!({
                    "service_id": service_id,
                    "response_time_ms": response_time_ms,
                    "threshold_ms": max_threshold_ms,
                    "sla_compliant": true
                }),
                citations,
            )
        } else {
            (
                SpecialistVerdict::Caution {
                    reason: format!(
                        "SLA Breach: Service '{}' took {:.1} ms, exceeding threshold of {:.1} ms.",
                        service_id, response_time_ms, max_threshold_ms
                    ),
                },
                serde_json::json!({
                    "service_id": service_id,
                    "response_time_ms": response_time_ms,
                    "threshold_ms": max_threshold_ms,
                    "sla_compliant": false
                }),
                citations,
            )
        }
    }
}

impl DepartmentSpecialist for OperationsSpecialist {
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
            "ops_dewa_audit_commercial_bill",
            "ops_track_container",
            "ops_check_sla",
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        _governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "ops_dewa_audit_commercial_bill" | "dewa_audit" => {
                let account = task.payload.get("account_no").and_then(|v| v.as_str()).unwrap_or("DEWA-COMM-992");
                let kwh = task.payload.get("kwh_consumed").and_then(|v| v.as_f64()).unwrap_or(3_500.0);
                let billed = task.payload.get("billed_aed").and_then(|v| v.as_f64()).unwrap_or(1_055.0);
                self.audit_dewa_commercial_bill(account, kwh, billed)
            }
            "ops_track_container" | "track_container" => {
                let container = task.payload.get("container_id").and_then(|v| v.as_str()).unwrap_or("MSKU1234567");
                let terminal = task.payload.get("terminal").and_then(|v| v.as_str()).unwrap_or("Jebel Ali Terminal 2");
                let days = task.payload.get("days_in_yard").and_then(|v| v.as_u64()).unwrap_or(2) as u32;
                let limit = task.payload.get("free_days_limit").and_then(|v| v.as_u64()).unwrap_or(5) as u32;
                self.track_container(container, terminal, days, limit)
            }
            "ops_check_sla" | "check_sla" => {
                let srv = task.payload.get("service_id").and_then(|v| v.as_str()).unwrap_or("customs_api");
                let time_ms = task.payload.get("response_time_ms").and_then(|v| v.as_f64()).unwrap_or(12.5);
                let thresh = task.payload.get("max_threshold_ms").and_then(|v| v.as_f64()).unwrap_or(50.0);
                self.check_sla(srv, time_ms, thresh)
            }
            _ => {
                return Err(format!("OperationsSpecialist unknown action: '{}'", task.action));
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
