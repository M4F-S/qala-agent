//! HR & People Departmental Specialist (`qala_hr`)
//! Enforces UAE Labor Law No. 33 of 2021 (Article 51 Gratuity, Article 53 Settlement & Deductions)
//! and MoHRE Nafis Emiratisation Quotas (10% target in 2026, AED 8,000/month statutory penalties).
//! Master Spec Reference: CANONICAL_MASTER_SPECIFICATION_2026.md §3.2(4).

use super::{Department, DepartmentSpecialist, MeshTask, SpecialistResponse, SpecialistVerdict};
use crate::safety::SafetyGovernor;
use serde::{Deserialize, Serialize};
use std::time::Instant;

/// Round currency value to 2 decimal places (fil-level precision: 1 fil = 0.01 AED)
#[inline]
pub fn round_fils(val: f64) -> f64 {
    (val * 100.0).round() / 100.0
}

pub struct HrSpecialist {
    pub name: &'static str,
    pub department: Department,
}

impl Default for HrSpecialist {
    fn default() -> Self {
        Self::new()
    }
}

/// Detailed breakdown of Article 51 End-of-Service Gratuity (EOSG)
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct GratuityBreakdown {
    pub basic_monthly_salary_aed: f64,
    pub completed_years_tenure: f64,
    #[serde(default)]
    pub tenure_years: f64,
    #[serde(default)]
    pub tenure_months: u32,
    #[serde(default)]
    pub tenure_days: u32,
    pub is_part_time: bool,
    pub contracted_annual_hours: Option<f64>,
    #[serde(default = "default_part_time_ratio")]
    pub part_time_ratio: f64,
    pub daily_basic_wage_aed: f64,
    #[serde(default)]
    pub first_five_years_entitlement_days: f64,
    #[serde(default)]
    pub additional_years_entitlement_days: f64,
    #[serde(default)]
    pub total_entitlement_days: f64,
    #[serde(default)]
    pub full_time_gratuity_aed: f64,
    pub uncapped_gratuity_aed: f64,
    pub maximum_statutory_cap_aed: f64,
    #[serde(default)]
    pub is_capped_at_24_months: bool,
    pub final_statutory_gratuity_aed: f64,
    #[serde(default)]
    pub requested_deductions_aed: f64,
    #[serde(default)]
    pub max_deduction_allowed_aed: f64,
    #[serde(default)]
    pub actual_deductions_applied_aed: f64,
    #[serde(default)]
    pub deduction_breach_flag: bool,
    #[serde(default)]
    pub net_terminal_payout_aed: f64,
    pub settlement_deadline_days: u32,
    pub max_deduction_allowed_pct: f64,
    #[serde(default = "default_contract_regime")]
    pub contract_regime: String,
    #[serde(default)]
    pub resignation_deductions_applied: bool,
}

fn default_part_time_ratio() -> f64 {
    1.0
}

fn default_contract_regime() -> String {
    "Unified Limited Term (Federal Decree-Law No. 33/2021) - Resignation Deductions Abolished".to_string()
}

/// Article 53 Terminal Settlement Compliance Status
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct TerminalSettlementCompliance {
    pub statutory_deadline_days: u32,
    pub days_since_termination: u32,
    pub is_compliant: bool,
    pub status: String,
    pub message: String,
    pub legal_reference: String,
}

/// MoHRE Nafis Emiratisation Remedial Step
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct NafisRemedialStep {
    pub phase: String,
    pub timeline: String,
    pub action: String,
}

/// MoHRE Nafis Emiratisation Audit Report
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct NafisAuditReport {
    pub company_name: String,
    pub skilled_headcount: usize,
    pub emirati_skilled_count: usize,
    pub is_applicable: bool,
    pub statutory_target_quota_pct: f64,
    pub current_emiratisation_rate_pct: f64,
    pub required_national_positions: usize,
    pub national_deficit: usize,
    pub is_compliant: bool,
    pub statutory_monthly_penalty_per_position_aed: f64,
    pub total_monthly_penalty_aed: f64,
    pub total_annual_penalty_aed: f64,
    pub semi_annual_audit_cycles: Vec<String>,
    pub remedial_hiring_plan: Vec<NafisRemedialStep>,
}

impl HrSpecialist {
    pub fn new() -> Self {
        Self {
            name: "qala_hr",
            department: Department::Hr,
        }
    }

    /// Calculate Article 51 End-of-Service Gratuity (EOSG) - simplified signature
    pub fn calculate_gratuity(
        &self,
        basic_salary_aed: f64,
        tenure_years: f64,
        is_part_time: bool,
        contracted_annual_hours: Option<f64>,
    ) -> GratuityBreakdown {
        self.calculate_gratuity_detailed(
            basic_salary_aed,
            tenure_years,
            0,
            0,
            is_part_time,
            contracted_annual_hours,
            0.0,
        )
    }

    /// Calculate Article 51 End-of-Service Gratuity (EOSG) with full fractional tenure and Article 53 deduction check
    pub fn calculate_gratuity_detailed(
        &self,
        basic_salary_aed: f64,
        years: f64,
        months: u32,
        days: u32,
        is_part_time: bool,
        contracted_annual_hours: Option<f64>,
        requested_deductions_aed: f64,
    ) -> GratuityBreakdown {
        // Daily rate strictly defined as Basic Monthly Salary / 30 under Article 51
        let daily_wage = basic_salary_aed / 30.0;
        let statutory_cap = basic_salary_aed * 24.0; // Statutory 2-Year Cap: 24 months basic salary

        // Fractional tenure resolution
        let effective_tenure = years + (months as f64 / 12.0) + (days as f64 / 365.0);

        let (entitlement_first_5, entitlement_after_5, full_time_gratuity) = if effective_tenure < 1.0 {
            // Less than 1 year continuous service = 0 payout
            (0.0, 0.0, 0.0)
        } else if effective_tenure <= 5.0 {
            // 1 <= Y <= 5: 21 days' basic wage per completed year
            let days_entitled = effective_tenure * 21.0;
            (days_entitled, 0.0, days_entitled * daily_wage)
        } else {
            // Y > 5: (5 * 21 days) + ((Y - 5) * 30 days)
            let first_five_days = 5.0 * 21.0; // 105 days
            let after_five_days = (effective_tenure - 5.0) * 30.0;
            let total_days = first_five_days + after_five_days;
            (first_five_days, after_five_days, total_days * daily_wage)
        };

        let total_entitlement_days = entitlement_first_5 + entitlement_after_5;

        // Part-Time Proration (Cabinet Resolution No. 1/2022 Art. 30):
        // PartTimeGratuity = (ContractedHours / 2080) * FullTimeGratuity
        let (part_time_ratio, uncapped_gratuity) = if is_part_time || contracted_annual_hours.is_some() {
            let hours = contracted_annual_hours.unwrap_or(1040.0);
            let ratio = hours / 2080.0;
            (ratio, full_time_gratuity * ratio)
        } else {
            (1.0, full_time_gratuity)
        };

        // Statutory 2-Year Cap (Maximum payout cannot exceed 24 months' basic salary)
        let is_capped = uncapped_gratuity > statutory_cap;
        let gross_gratuity = uncapped_gratuity.min(statutory_cap);

        // Article 53 Terminal Settlement Compliance:
        // Employer deductions strictly capped at maximum 25% of terminal package
        let max_deduction_allowed = gross_gratuity * 0.25;
        let deduction_breach = requested_deductions_aed > max_deduction_allowed;
        let actual_deductions = requested_deductions_aed.min(max_deduction_allowed);
        let net_payout = gross_gratuity - actual_deductions;

        GratuityBreakdown {
            basic_monthly_salary_aed: round_fils(basic_salary_aed),
            completed_years_tenure: round_fils(effective_tenure),
            tenure_years: years,
            tenure_months: months,
            tenure_days: days,
            is_part_time: is_part_time || contracted_annual_hours.is_some(),
            contracted_annual_hours,
            part_time_ratio: round_fils(part_time_ratio * 100.0) / 100.0,
            daily_basic_wage_aed: round_fils(daily_wage),
            first_five_years_entitlement_days: round_fils(entitlement_first_5),
            additional_years_entitlement_days: round_fils(entitlement_after_5),
            total_entitlement_days: round_fils(total_entitlement_days),
            full_time_gratuity_aed: round_fils(full_time_gratuity),
            uncapped_gratuity_aed: round_fils(uncapped_gratuity),
            maximum_statutory_cap_aed: round_fils(statutory_cap),
            is_capped_at_24_months: is_capped,
            final_statutory_gratuity_aed: round_fils(gross_gratuity),
            requested_deductions_aed: round_fils(requested_deductions_aed),
            max_deduction_allowed_aed: round_fils(max_deduction_allowed),
            actual_deductions_applied_aed: round_fils(actual_deductions),
            deduction_breach_flag: deduction_breach,
            net_terminal_payout_aed: round_fils(net_payout),
            settlement_deadline_days: 14,
            max_deduction_allowed_pct: 25.0,
            contract_regime: default_contract_regime(),
            resignation_deductions_applied: false,
        }
    }

    /// Check Article 53 statutory 14 calendar days terminal settlement deadline
    pub fn check_terminal_settlement_deadline(
        &self,
        days_since_termination: u32,
    ) -> TerminalSettlementCompliance {
        let is_compliant = days_since_termination <= 14;
        let status = if is_compliant {
            "COMPLIANT_WITHIN_DEADLINE".to_string()
        } else {
            "NON_COMPLIANT_STATUTORY_DEADLINE_BREACH".to_string()
        };
        let message = if is_compliant {
            format!(
                "Compliant: Payout within statutory 14-day window ({} days elapsed).",
                days_since_termination
            )
        } else {
            format!(
                "Statutory Breach: {} days elapsed since termination exceeds the mandatory 14-calendar-day deadline under Article 53.",
                days_since_termination
            )
        };

        TerminalSettlementCompliance {
            statutory_deadline_days: 14,
            days_since_termination,
            is_compliant,
            status,
            message,
            legal_reference: "Federal Decree-Law No. 33 of 2021 Article 53 (Settlement of Entitlements)".to_string(),
        }
    }

    /// Audit MoHRE Nafis Emiratisation Quota Compliance and generate structured report
    pub fn audit_nafis_report(
        &self,
        company_name: &str,
        skilled_headcount: usize,
        emirati_skilled_count: usize,
    ) -> NafisAuditReport {
        let is_applicable = skilled_headcount >= 50;
        let current_rate = if skilled_headcount > 0 {
            round_fils((emirati_skilled_count as f64 / skilled_headcount as f64) * 100.0)
        } else {
            0.0
        };

        if !is_applicable {
            return NafisAuditReport {
                company_name: company_name.to_string(),
                skilled_headcount,
                emirati_skilled_count,
                is_applicable: false,
                statutory_target_quota_pct: 10.0,
                current_emiratisation_rate_pct: current_rate,
                required_national_positions: 0,
                national_deficit: 0,
                is_compliant: true,
                statutory_monthly_penalty_per_position_aed: 8_000.0,
                total_monthly_penalty_aed: 0.0,
                total_annual_penalty_aed: 0.0,
                semi_annual_audit_cycles: vec![
                    "June 30, 2026 (Semi-Annual Audit 1)".to_string(),
                    "December 31, 2026 (Semi-Annual Audit 2)".to_string(),
                ],
                remedial_hiring_plan: vec![],
            };
        }

        // 2026 Target: 10% of skilled headcount
        let required_quota = (skilled_headcount as f64 * 0.10).ceil() as usize;
        let deficit = required_quota.saturating_sub(emirati_skilled_count);
        let is_compliant = deficit == 0;

        // Statutory penalty: exactly AED 8,000 per month (AED 96,000 per year) per unfilled national position
        let penalty_monthly = (deficit as f64) * 8_000.0;
        let penalty_annual = (deficit as f64) * 96_000.0;

        let remedial_plan = if deficit > 0 {
            vec![
                NafisRemedialStep {
                    phase: "Phase 1: MoHRE & Nafis Job Advertising".to_string(),
                    timeline: "Days 1–30".to_string(),
                    action: format!(
                        "Post {} national vacancies on the official MoHRE Nafis portal and initiate UAE national candidate sourcing.",
                        deficit
                    ),
                },
                NafisRemedialStep {
                    phase: "Phase 2: Candidate Screening & Interviews".to_string(),
                    timeline: "Days 31–60".to_string(),
                    action: "Conduct competency-based interviews, select qualified Emirati talent, and extend formal MoHRE offers.".to_string(),
                },
                NafisRemedialStep {
                    phase: "Phase 3: Onboarding & GPSSA Pension Registration".to_string(),
                    timeline: "Days 61–90".to_string(),
                    action: "Complete MoHRE work permit issuance, GPSSA retirement fund registration, and verify Nafis portal compliance before next semi-annual audit.".to_string(),
                },
            ]
        } else {
            vec![]
        };

        NafisAuditReport {
            company_name: company_name.to_string(),
            skilled_headcount,
            emirati_skilled_count,
            is_applicable: true,
            statutory_target_quota_pct: 10.0,
            current_emiratisation_rate_pct: current_rate,
            required_national_positions: required_quota,
            national_deficit: deficit,
            is_compliant,
            statutory_monthly_penalty_per_position_aed: 8_000.0,
            total_monthly_penalty_aed: penalty_monthly,
            total_annual_penalty_aed: penalty_annual,
            semi_annual_audit_cycles: vec![
                "June 30, 2026 (Semi-Annual Audit 1)".to_string(),
                "December 31, 2026 (Semi-Annual Audit 2)".to_string(),
            ],
            remedial_hiring_plan: remedial_plan,
        }
    }

    /// Audit MoHRE Nafis Emiratisation Quota Compliance (Verdict wrapper)
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
            "Statutory Penalty: AED 8,000/month (AED 96,000/year) per unfilled national position (2026)".to_string(),
        ];

        let report = self.audit_nafis_report(company_name, skilled_headcount, emirati_skilled_count);

        if !report.is_applicable {
            return (
                SpecialistVerdict::Approved,
                serde_json::to_value(&report).unwrap_or_default(),
                citations,
            );
        }

        if report.is_compliant {
            (
                SpecialistVerdict::Approved,
                serde_json::to_value(&report).unwrap_or_default(),
                citations,
            )
        } else {
            (
                SpecialistVerdict::Caution {
                    reason: format!(
                        "Nafis Quota Deficit: Missing {} national position(s). Monthly statutory penalty: AED {:.2}.",
                        report.national_deficit, report.total_monthly_penalty_aed
                    ),
                },
                serde_json::to_value(&report).unwrap_or_default(),
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
        let basic_ratio = if total_gross > 0.0 {
            basic_salary_aed / total_gross
        } else {
            1.0
        };
        let mut warning = None;
        if basic_ratio < 0.50 {
            warning = Some(
                "Basic salary is less than 50% of gross wage. May reduce terminal gratuity entitlement."
                    .to_string(),
            );
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
            "hr_check_settlement_deadline",
        ]
    }

    fn handle_task(
        &mut self,
        task: &MeshTask,
        _governor: &mut SafetyGovernor,
    ) -> Result<SpecialistResponse, String> {
        let start = Instant::now();

        let (verdict, data, citations) = match task.action.as_str() {
            "hr_calculate_gratuity" | "calculate_gratuity" | "gratuity" => {
                let basic = task.payload.get("basic_monthly_salary_aed")
                    .or_else(|| task.payload.get("basic"))
                    .and_then(|v| v.as_f64())
                    .unwrap_or(0.0);
                let years = task.payload.get("tenure_years")
                    .or_else(|| task.payload.get("years"))
                    .and_then(|v| v.as_f64())
                    .unwrap_or(0.0);
                let months = task.payload.get("tenure_months")
                    .or_else(|| task.payload.get("months"))
                    .and_then(|v| v.as_u64())
                    .unwrap_or(0) as u32;
                let days = task.payload.get("tenure_days")
                    .or_else(|| task.payload.get("days"))
                    .and_then(|v| v.as_u64())
                    .unwrap_or(0) as u32;
                let part_time = task.payload.get("is_part_time")
                    .and_then(|v| v.as_bool())
                    .unwrap_or(false);
                let hours = task.payload.get("contracted_annual_hours")
                    .or_else(|| task.payload.get("part_time_hours"))
                    .and_then(|v| v.as_f64());
                let deductions = task.payload.get("requested_deductions_aed")
                    .or_else(|| task.payload.get("deductions"))
                    .and_then(|v| v.as_f64())
                    .unwrap_or(0.0);

                let breakdown = self.calculate_gratuity_detailed(
                    basic,
                    years,
                    months,
                    days,
                    part_time,
                    hours,
                    deductions,
                );

                (
                    SpecialistVerdict::Approved,
                    serde_json::to_value(&breakdown).unwrap_or_default(),
                    vec![
                        "UAE Federal Decree-Law No. 33 of 2021 Article 51 (End-of-Service Gratuity)".to_string(),
                        "UAE Federal Decree-Law No. 33 of 2021 Article 53 (14-Day Payment Deadline & 25% Deduction Limit)".to_string(),
                        "Cabinet Resolution No. 1 of 2022 Article 30 (Part-Time Proration Formula)".to_string(),
                    ],
                )
            }
            "hr_audit_nafis_quota" | "audit_nafis" | "nafis" => {
                let company = task.payload.get("company_name").and_then(|v| v.as_str()).unwrap_or("ENTERPRISE-LLC");
                let skilled = task.payload.get("skilled_headcount")
                    .or_else(|| task.payload.get("skilled"))
                    .and_then(|v| v.as_u64())
                    .unwrap_or(50) as usize;
                let national = task.payload.get("emirati_skilled_count")
                    .or_else(|| task.payload.get("emirati"))
                    .and_then(|v| v.as_u64())
                    .unwrap_or(5) as usize;
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
            "hr_check_settlement_deadline" | "settlement_deadline" => {
                let days = task.payload.get("days_since_termination").and_then(|v| v.as_u64()).unwrap_or(0) as u32;
                let compliance = self.check_terminal_settlement_deadline(days);
                (
                    if compliance.is_compliant {
                        SpecialistVerdict::Approved
                    } else {
                        SpecialistVerdict::Caution {
                            reason: compliance.message.clone(),
                        }
                    },
                    serde_json::to_value(&compliance).unwrap_or_default(),
                    vec![compliance.legal_reference],
                )
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
