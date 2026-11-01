//! HR & People Departmental Specialist (`qala_hr`)
//! Enforces UAE Labor Law No. 33 of 2021 (Article 51 Gratuity, Article 53 Settlement) and MoHRE Nafis Emiratisation Quotas.

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::SafetyGovernor;
use serde::{Deserialize, Serialize};
use std::time::Instant;

pub struct HrSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for HrSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct GratuityBreakdown {
    pub basic_monthly_salary_aed: f64,
    pub completed_years_tenure: f64,
    pub is_part_time: bool,
    pub contracted_annual_hours: Option<f64>,
    pub daily_basic_wage_aed: f64,
    pub uncapped_gratuity_aed: f64,
    pub maximum_statutory_cap_aed: f64,
    pub final_statutory_gratuity_aed: f64,
    pub settlement_deadline_days: u32,
    pub max_deduction_allowed_pct: f64,
}

impl HrSpecialist {
    pub fn new() -> Self {
        Self {
            name: "qala_hr",
            department: Department::Hr,
        }
    }

    /// Calculate Article 51 End-of-Service Gratuity (EOSG)
    pub fn calculate_gratuity(
        &self,
        basic_salary_aed: f64,
        tenure_years: f64,
        is_part_time: bool,
        contracted_annual_hours: Option<f64>,
    ) -> GratuityBreakdown {
        let daily_wage = basic_salary_aed / 30.0;
        let statutory_cap = basic_salary_aed * 24.0; // 2 years basic salary cap

        let uncapped_full_time = if tenure_years < 1.0 {
            0.0
        } else if tenure_years <= 5.0 {
            // 21 days basic salary per year
            tenure_years * 21.0 * daily_wage
        } else {
            // First 5 years: 21 days/year. Additional years: 30 days/year
            let first_five = 5.0 * 21.0 * daily_wage;
            let remaining = (tenure_years - 5.0) * 30.0 * daily_wage;
            first_five + remaining
        };

        // Part-time proration under Cabinet Resolution No. 1 of 2022 Art. 30
        let gratuity_before_cap = if is_part_time {
            let hours = contracted_annual_hours.unwrap_or(1040.0);
            (hours / 2080.0) * uncapped_full_time
        } else {
            uncapped_full_time
        };

        let final_gratuity = gratuity_before_cap.min(statutory_cap);

        GratuityBreakdown {
            basic_monthly_salary_aed: basic_salary_aed,
            completed_years_tenure: tenure_years,
            is_part_time,
            contracted_annual_hours,
            daily_basic_wage_aed: (daily_wage * 100.0).round() / 100.0,
            uncapped_gratuity_aed: (gratuity_before_cap * 100.0).round() / 100.0,
            maximum_statutory_cap_aed: (statutory_cap * 100.0).round() / 100.0,
            final_statutory_gratuity_aed: (final_gratuity * 100.0).round() / 100.0,
            settlement_deadline_days: 14,
            max_deduction_allowed_pct: 25.0,
        }
    }

    /// Audit MoHRE Nafis Emiratisation Quota Compliance
    pub fn audit_nafis_quota(
        &self,
        company_name: &str,
        skilled_headcount: usize,
        emirati_skilled_count: usize,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "Cabinet Resolution No. 19/5m of 2022 on Nafis Emiratisation Targets".to_string(),
            "Ministerial Decision No. 279 of 2022 on Increasing Emiratisation Rates in Private Sector".to_string(),
            "MoHRE 2026 Target: 10% Emiratisation for Private Mainland Entities with 50+ Skilled Employees".to_string(),
        ];

        // Only applies to private mainland entities with 50+ skilled workers
        if skilled_headcount < 50 {
            return (
                SpecialistVerdict::Approved,
                serde_json::json!({
                    "company_name": company_name,
                    "skilled_headcount": skilled_headcount,
                    "emirati_skilled_count": emirati_skilled_count,
                    "nafis_applicable": false,
                    "reason": "Headcount under 50 skilled employees; exempt from mandatory 2% semi-annual quota.",
                    "penalty_aed": 0.0
                }),
                citations,
            );
        }

        // Target for 2026 is 10% of skilled headcount
        let required_quota = (skilled_headcount as f64 * 0.10).ceil() as usize;
        let deficit = required_quota.saturating_sub(emirati_skilled_count);

        if deficit == 0 {
            (
                SpecialistVerdict::Approved,
                serde_json::json!({
                    "company_name": company_name,
                    "skilled_headcount": skilled_headcount,
                    "emirati_skilled_count": emirati_skilled_count,
                    "required_national_positions": required_quota,
                    "current_rate": format!("{:.2}%", (emirati_skilled_count as f64 / skilled_headcount as f64) * 100.0),
                    "nafis_compliant": true,
                    "penalty_aed": 0.0
                }),
                citations,
            )
        } else {
            // Statutory penalty: AED 8,000 per month (AED 96,000 per year) per missing employee in 2026
            let penalty_monthly_aed = (deficit as f64) * 8_000.0;
            let penalty_annual_aed = (deficit as f64) * 96_000.0;

            (
                SpecialistVerdict::Caution {
                    reason: format!(
                        "Nafis Quota Deficit: Missing {} national position(s). Monthly statutory penalty: AED {:.2}.",
                        deficit, penalty_monthly_aed
                    ),
                },
                serde_json::json!({
                    "company_name": company_name,
                    "skilled_headcount": skilled_headcount,
                    "emirati_skilled_count": emirati_skilled_count,
                    "required_national_positions": required_quota,
                    "deficit": deficit,
                    "current_rate": format!("{:.2}%", (emirati_skilled_count as f64 / skilled_headcount as f64) * 100.0),
                    "nafis_compliant": false,
                    "penalty_monthly_aed": penalty_monthly_aed,
                    "penalty_annual_aed": penalty_annual_aed
                }),
                citations,
            )
        }
    }

    /// Validate standard MoHRE employment contract terms
    pub fn validate_labor_contract(
        &self,
        employee_name: &str,
        basic_salary_aed: f64,
        allowances_aed: f64,
        probation_months: u32,
        weekly_hours: u32,
    ) -> (SpecialistVerdict, serde_json::Value, Vec<String>) {
        let citations = vec![
            "UAE Federal Decree-Law No. 33 of 2021 Article 9 (Probation Period)".to_string(),
            "UAE Federal Decree-Law No. 33 of 2021 Article 17 (Working Hours)".to_string(),
        ];

        let total_gross = basic_salary_aed + allowances_aed;

        // Article 9: Probation period cannot exceed 6 months
        if probation_months > 6 {
            return (
                SpecialistVerdict::Rejected {
                    code: "excessive_probation".to_string(),
                    reason: format!(
                        "Article 9 violation: Probation period of {} months exceeds statutory maximum of 6 months.",
                        probation_months
                    ),
                },
                serde_json::json!({
                    "employee_name": employee_name,
                    "probation_months": probation_months,
                    "compliant": false
                }),
                citations,
            );
        }

        // Article 17: Normal working hours cannot exceed 8 hrs/day or 48 hrs/week
        if weekly_hours > 48 {
            return (
                SpecialistVerdict::Rejected {
                    code: "excessive_working_hours".to_string(),
                    reason: format!(
                        "Article 17 violation: Weekly working hours of {} exceed statutory maximum of 48 hours.",
                        weekly_hours
                    ),
                },
                serde_json::json!({
                    "employee_name": employee_name,
                    "weekly_hours": weekly_hours,
                    "compliant": false
                }),
                citations,
            );
        }

        // Basic salary should typically be at least 50% of gross package
        let basic_ratio = if total_gross > 0.0 { basic_salary_aed / total_gross } else { 1.0 };
        let mut warning = None;
        if basic_ratio < 0.50 {
            warning = Some("Basic salary is less than 50% of gross wage. May reduce terminal gratuity entitlement.".to_string());
        }

        (
            if warning.is_some() {
                SpecialistVerdict::Caution {
                    reason: warning.clone().unwrap(),
                }
            } else {
                SpecialistVerdict::Approved
            },
            serde_json::json!({
                "employee_name": employee_name,
                "basic_salary_aed": basic_salary_aed,
                "allowances_aed": allowances_aed,
                "total_gross_aed": total_gross,
                "basic_percentage": format!("{:.1}%", basic_ratio * 100.0),
                "probation_months": probation_months,
                "weekly_hours": weekly_hours,
                "compliant": true,
                "note": warning
            }),
            citations,
        )
    }
}

impl DepartmentSpecialist for HrSpecialist {
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
            "hr_calculate_gratuity",
            "hr_audit_nafis_quota",
            "hr_validate_labor_contract",
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        _governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "hr_calculate_gratuity" | "calculate_gratuity" => {
                let basic = task.payload.get("basic_monthly_salary_aed").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let tenure = task.payload.get("tenure_years").and_then(|v| v.as_f64()).unwrap_or(0.0);
                let part_time = task.payload.get("is_part_time").and_then(|v| v.as_bool()).unwrap_or(false);
                let hours = task.payload.get("contracted_annual_hours").and_then(|v| v.as_f64());
                let breakdown = self.calculate_gratuity(basic, tenure, part_time, hours);
                (
                    SpecialistVerdict::Approved,
                    serde_json::to_value(&breakdown).unwrap_or_default(),
                    vec![
                        "UAE Federal Decree-Law No. 33 of 2021 Article 51 (End-of-Service Gratuity)".to_string(),
                        "UAE Federal Decree-Law No. 33 of 2021 Article 53 (14-Day Payment Deadline)".to_string(),
                        "Cabinet Resolution No. 1 of 2022 Article 30 (Part-Time Proration)".to_string(),
                    ],
                )
            }
            "hr_audit_nafis_quota" | "audit_nafis" => {
                let company = task.payload.get("company_name").and_then(|v| v.as_str()).unwrap_or("ENTERPRISE-LLC");
                let skilled = task.payload.get("skilled_headcount").and_then(|v| v.as_u64()).unwrap_or(50) as usize;
                let national = task.payload.get("emirati_skilled_count").and_then(|v| v.as_u64()).unwrap_or(5) as usize;
                self.audit_nafis_quota(company, skilled, national)
            }
            "hr_validate_labor_contract" | "validate_contract" => {
                let name = task.payload.get("employee_name").and_then(|v| v.as_str()).unwrap_or("Employee");
                let basic = task.payload.get("basic_salary_aed").and_then(|v| v.as_f64()).unwrap_or(10_000.0);
                let allowances = task.payload.get("allowances_aed").and_then(|v| v.as_f64()).unwrap_or(5_000.0);
                let probation = task.payload.get("probation_months").and_then(|v| v.as_u64()).unwrap_or(3) as u32;
                let hours = task.payload.get("weekly_hours").and_then(|v| v.as_u64()).unwrap_or(40) as u32;
                self.validate_labor_contract(name, basic, allowances, probation, hours)
            }
            _ => {
                return Err(format!("HrSpecialist unknown action: '{}'", task.action));
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
