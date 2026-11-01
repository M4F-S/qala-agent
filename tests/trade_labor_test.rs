//! SPRINT-06 Comprehensive Regulatory Test Suite: Trade & Labor Regulatory Engines
//! Verifies:
//! 1. Dubai Customs Mirsal II 10.25% landed duty & tax engine with fil-level precision.
//! 2. Zero-rating exemptions for Chapter 30 (HS 3004.90), Chapter 90 (HS 9018.90), and Food Security (HS 1001/1006/0401).
//! 3. Temporary Admission 5% refundable deposit and 180-day re-export timer tracking.
//! 4. Mirsal II Free Zone exit declaration generation.
//! 5. Tau-3 FinOps Circuit Breaker dual-key escrow hold on customs payments > AED 50,000.
//! 6. UAE Labor Law (Decree-Law No. 33/2021) Article 51 Gratuity across all tenure brackets (<1 yr, 3 yrs, 7.5 yrs, 25+ yrs).
//! 7. Cabinet Resolution No. 1/2022 Article 30 part-time proration formula.
//! 8. Article 53 statutory 14-day settlement deadline and 25% deduction ceiling.
//! 9. MoHRE Nafis Emiratisation engine: 10% 2026 quota and AED 8,000/month penalty per unfilled position.

use qala::mesh::{SpecialistVerdict, TradeSpecialist, HrSpecialist};
use qala::safety::{ApprovalSlot, SafetyGovernor};

fn create_test_governor(test_id: &str) -> SafetyGovernor {
    let tmp_tokens = format!("/tmp/qala_test_trade_tokens_{}.jsonl", test_id);
    let tmp_audit = format!("/tmp/qala_test_trade_audit_{}.jsonl", test_id);
    let _ = std::fs::remove_file(&tmp_tokens);
    let _ = std::fs::remove_file(&tmp_audit);
    SafetyGovernor::new("/workspace", tmp_tokens, tmp_audit)
}

// =========================================================================
// PART A: DUBAI CUSTOMS MIRSAL II TRADE REGULATORY ENGINE TESTS
// =========================================================================

#[test]
fn test_mirsal2_tariff_standard_goods_10_25_pct_fil_precision() {
    let trade = TradeSpecialist::new();

    // Case 1: Standard benchmark CIF AED 100,000.00
    // Duty (5%) = AED 5,000.00
    // VAT Base (CIF + Duty) = AED 105,000.00
    // VAT Amount (5% of VAT Base) = AED 5,250.00 (5.25% CIF)
    // Total Landed Duty & Tax = AED 10,250.00 (exactly 10.25% CIF)
    let res = trade.calculate_customs_tariff(100_000.0, "8471.30");
    assert!(!res.is_zero_rated);
    assert_eq!(res.customs_duty_rate, 0.05);
    assert_eq!(res.customs_duty_aed, 5_000.00);
    assert_eq!(res.vat_base_aed, 105_000.00);
    assert_eq!(res.vat_rate, 0.05);
    assert_eq!(res.vat_aed, 5_250.00);
    assert_eq!(res.total_landed_duty_tax_aed, 10_250.00);
    assert_eq!(res.total_landed_rate_pct, 10.25);

    // Case 2: Odd amount verifying fil-level precision rounding: CIF AED 12,345.67
    // Duty (5% * 12345.67) = 617.2835 -> rounded to 617.28
    // VAT Base = 12345.67 + 617.28 = 12962.95
    // VAT Amount (5% * 12962.95) = 648.1475 -> rounded to 648.15
    // Total Landed Tax = 617.28 + 648.15 = 1265.43
    let res_odd = trade.calculate_customs_tariff(12_345.67, "8517.12");
    assert_eq!(res_odd.cif_aed, 12_345.67);
    assert_eq!(res_odd.customs_duty_aed, 617.28);
    assert_eq!(res_odd.vat_base_aed, 12_962.95);
    assert_eq!(res_odd.vat_aed, 648.15);
    assert_eq!(res_odd.total_landed_duty_tax_aed, 1_265.43);
}

#[test]
fn test_mirsal2_tariff_zero_rating_pharmaceuticals_chapter_30() {
    let trade = TradeSpecialist::new();

    // HS 3004.90: Human Pharmaceuticals with MoHAP permit
    let res = trade.calculate_customs_tariff_with_permit(
        500_000.0,
        "3004.90",
        Some("MOHAP-IMPORT-PERMIT-2026-992"),
    );
    assert!(res.is_zero_rated);
    assert_eq!(res.customs_duty_aed, 0.0);
    assert_eq!(res.vat_aed, 0.0);
    assert_eq!(res.total_landed_duty_tax_aed, 0.0);
    assert_eq!(res.total_landed_rate_pct, 0.0);
    assert!(res.zero_rate_reason.unwrap().contains("Human Pharmaceuticals"));
    assert_eq!(res.permit_id.as_deref(), Some("MOHAP-IMPORT-PERMIT-2026-992"));

    // HS 3003.20 without explicit permit ID still recognized as zero-rated under statutory classification
    let res_generic = trade.calculate_customs_tariff(250_000.0, "3003.20");
    assert!(res_generic.is_zero_rated);
    assert_eq!(res_generic.total_landed_duty_tax_aed, 0.0);
}

#[test]
fn test_mirsal2_tariff_zero_rating_medical_instruments_chapter_90() {
    let trade = TradeSpecialist::new();

    // HS 9018.90: Medical Diagnostic Instruments with EDE permit
    let res = trade.calculate_customs_tariff_with_permit(
        750_000.0,
        "9018.90",
        Some("EDE-DEVICE-CLEARANCE-774"),
    );
    assert!(res.is_zero_rated);
    assert_eq!(res.customs_duty_aed, 0.0);
    assert_eq!(res.vat_aed, 0.0);
    assert_eq!(res.total_landed_duty_tax_aed, 0.0);
    assert!(res.zero_rate_reason.unwrap().contains("Diagnostic & Surgical Medical Equipment"));
    assert_eq!(res.permit_id.as_deref(), Some("EDE-DEVICE-CLEARANCE-774"));
}

#[test]
fn test_mirsal2_tariff_zero_rating_food_security_exemptions() {
    let trade = TradeSpecialist::new();

    // HS 1001: Wheat and meslin
    let res_wheat = trade.calculate_customs_tariff(300_000.0, "1001.99");
    assert!(res_wheat.is_zero_rated);
    assert_eq!(res_wheat.total_landed_duty_tax_aed, 0.0);

    // HS 1006: Rice
    let res_rice = trade.calculate_customs_tariff(200_000.0, "1006.30");
    assert!(res_rice.is_zero_rated);
    assert_eq!(res_rice.total_landed_duty_tax_aed, 0.0);

    // HS 0401: Milk and cream
    let res_milk = trade.calculate_customs_tariff(150_000.0, "0401.20");
    assert!(res_milk.is_zero_rated);
    assert_eq!(res_milk.total_landed_duty_tax_aed, 0.0);
}

#[test]
fn test_temporary_admission_5_pct_deposit_and_180_day_timer() {
    let trade = TradeSpecialist::new();

    // CIF AED 250,000.00 -> 5% refundable deposit = AED 12,500.00
    // Case 1: 45 days elapsed (compliant, 135 days remaining)
    let temp_active = trade.calculate_temporary_admission(250_000.0, 45);
    assert_eq!(temp_active.refundable_deposit_rate, 0.05);
    assert_eq!(temp_active.refundable_deposit_aed, 12_500.00);
    assert_eq!(temp_active.statutory_reexport_limit_days, 180);
    assert_eq!(temp_active.days_elapsed, 45);
    assert_eq!(temp_active.days_remaining, 135);
    assert!(!temp_active.is_expired);
    assert_eq!(temp_active.status, "ACTIVE_COMPLIANT");
    assert!(temp_active.regulatory_guidance.contains("Re-export must occur"));

    // Case 2: Exactly 180 days elapsed (boundary check, compliant)
    let temp_boundary = trade.calculate_temporary_admission(250_000.0, 180);
    assert_eq!(temp_boundary.days_remaining, 0);
    assert!(!temp_boundary.is_expired);
    assert_eq!(temp_boundary.status, "ACTIVE_COMPLIANT");

    // Case 3: 195 days elapsed (> 180 days: statutory forfeiture triggered)
    let temp_expired = trade.calculate_temporary_admission(250_000.0, 195);
    assert!(temp_expired.is_expired);
    assert_eq!(temp_expired.days_remaining, -15);
    assert_eq!(temp_expired.status, "EXPIRED_FORFEITURE_TRIGGERED");
    assert!(temp_expired.regulatory_guidance.contains("forfeited"));
}

#[test]
fn test_mirsal2_freezone_declaration_generation() {
    let trade = TradeSpecialist::new();

    let decl = trade.generate_freezone_declaration(
        "BL-JAFZA-9912",
        "MSKU-7788990",
        "DXB-IMP-007",
        150_000.0,
        "8471.30",
        Some("JAFZA North"),
        None,
    );

    assert_eq!(decl.declaration_no, "MIRSAL2-FZ-DXB-IMP-007-MSKU7788990");
    assert_eq!(decl.declaration_regime, "IMPORT_LOCAL_MARKET_FROM_FREE_ZONE");
    assert_eq!(decl.freezone_origin, "JAFZA North");
    assert_eq!(decl.destination_market, "Dubai Mainland / Local UAE Market");
    assert_eq!(decl.tariff_breakdown.customs_duty_aed, 7_500.00);
    assert_eq!(decl.tariff_breakdown.vat_aed, 7_875.00);
    assert_eq!(decl.tariff_breakdown.total_landed_duty_tax_aed, 15_375.00);
    assert!(!decl.tau3_circuit_breaker_triggered);
    assert_eq!(decl.clearance_status, "DECLARATION_CLEARED_FOR_PAYMENT");
}

#[test]
fn test_tau3_circuit_breaker_on_customs_wire_payments() {
    let test_id = format!("trade_tau3_{}", std::process::id());
    let mut governor = create_test_governor(&test_id);
    let trade = TradeSpecialist::new();

    // Case 1: Customs payment <= AED 50,000 (AED 25,000) -> Cleared autonomously
    let low_verdict = trade
        .evaluate_customs_payment(25_000.0, None, &mut governor)
        .expect("Low payment evaluation failed");
    assert_eq!(low_verdict, SpecialistVerdict::Approved);

    // Case 2: Customs payment > AED 50,000 (AED 65,000) WITHOUT approval token -> Gated under Tau-3
    let high_verdict = trade
        .evaluate_customs_payment(65_000.0, None, &mut governor)
        .expect("High payment evaluation failed");
    match high_verdict {
        SpecialistVerdict::Gated {
            required_scope,
            amount_aed,
            ..
        } => {
            assert_eq!(required_scope, "finops_wire");
            assert_eq!(amount_aed, 65_000.0);
        }
        other => panic!("Expected Gated verdict, got: {:?}", other),
    }

    // Case 3: Customs payment > AED 50,000 WITH valid single-use cryptographic token -> Approved
    let auth_token = "QALA-AUTH-CUSTOMS-9921";
    governor.add_approval_slot(ApprovalSlot::new(auth_token, "finops_wire", 1999999999));

    let approved_verdict = trade
        .evaluate_customs_payment(65_000.0, Some(auth_token), &mut governor)
        .expect("Approved payment evaluation failed");
    assert_eq!(approved_verdict, SpecialistVerdict::Approved);

    // Case 4: Token replayed -> Hard rejected
    let replay_verdict = trade
        .evaluate_customs_payment(65_000.0, Some(auth_token), &mut governor)
        .expect("Replay evaluation failed");
    match replay_verdict {
        SpecialistVerdict::Rejected { code, .. } => {
            assert_eq!(code, "invalid_or_expired_approval");
        }
        other => panic!("Expected Rejected verdict on replay attack, got: {:?}", other),
    }
}

// =========================================================================
// PART B: UAE LABOR LAW (DECREE-LAW NO. 33/2021) REGULATORY ENGINE TESTS
// =========================================================================

#[test]
fn test_article_51_gratuity_under_one_year_tenure_yields_zero() {
    let hr = HrSpecialist::new();

    // Less than 1 full year continuous service (9 months / 0.75 years) -> 0.00 payout
    let res = hr.calculate_gratuity(15_000.0, 0.75, false, None);
    assert_eq!(res.uncapped_gratuity_aed, 0.0);
    assert_eq!(res.final_statutory_gratuity_aed, 0.0);
    assert_eq!(res.net_terminal_payout_aed, 0.0);
    assert_eq!(res.daily_basic_wage_aed, 500.00); // 15,000 / 30
}

#[test]
fn test_article_51_gratuity_between_one_and_five_years() {
    let hr = HrSpecialist::new();

    // Basic = AED 20,000.00, Tenure = 3.0 years
    // Daily wage = 20,000 / 30 = 666.6667
    // Entitlement = 3 years * 21 days * (20,000 / 30) = 63 * 666.6667 = AED 42,000.00
    let res = hr.calculate_gratuity(20_000.0, 3.0, false, None);
    assert_eq!(res.total_entitlement_days, 63.0);
    assert_eq!(res.uncapped_gratuity_aed, 42_000.00);
    assert_eq!(res.final_statutory_gratuity_aed, 42_000.00);
    assert_eq!(res.maximum_statutory_cap_aed, 480_000.00); // 24 * 20,000
    assert!(!res.is_capped_at_24_months);
}

#[test]
fn test_article_51_gratuity_over_five_years_tenure_fractional_days() {
    let hr = HrSpecialist::new();

    // Basic = AED 15,000.00 (Daily wage = 500.00), Tenure = 7.5 years (7 years, 6 months)
    // First 5 years: 5 * 21 days = 105 days * 500 = AED 52,500.00
    // Remaining 2.5 years: 2.5 * 30 days = 75 days * 500 = AED 37,500.00
    // Total Gratuity = 52,500 + 37,500 = AED 90,000.00
    let res = hr.calculate_gratuity_detailed(15_000.0, 7.0, 6, 0, false, None, 0.0);
    assert_eq!(res.completed_years_tenure, 7.50);
    assert_eq!(res.first_five_years_entitlement_days, 105.0);
    assert_eq!(res.additional_years_entitlement_days, 75.0);
    assert_eq!(res.total_entitlement_days, 180.0);
    assert_eq!(res.final_statutory_gratuity_aed, 90_000.00);
    assert!(!res.is_capped_at_24_months);
}

#[test]
fn test_article_51_gratuity_24_month_statutory_cap() {
    let hr = HrSpecialist::new();

    // Basic = AED 10,000.00 (Daily wage = 333.3333)
    // Statutory 2-Year Cap = 24 * 10,000.00 = AED 240,000.00
    //
    // At 25 years:
    // First 5 years: 105 days (3.5 months)
    // Next 20 years: 20 * 30 = 600 days (20 months)
    // Uncapped = 705 days = 23.5 months = AED 235,000.00
    let res_25 = hr.calculate_gratuity(10_000.0, 25.0, false, None);
    assert_eq!(res_25.uncapped_gratuity_aed, 235_000.00);
    assert_eq!(res_25.final_statutory_gratuity_aed, 235_000.00);
    assert!(!res_25.is_capped_at_24_months);

    // At 27 years:
    // First 5 years: 105 days (3.5 months)
    // Next 22 years: 22 * 30 = 660 days (22 months)
    // Uncapped = 765 days = 25.5 months * 10,000 = AED 255,000.00
    // MUST be strictly capped at 24 months (AED 240,000.00)
    let res_27 = hr.calculate_gratuity(10_000.0, 27.0, false, None);
    assert_eq!(res_27.uncapped_gratuity_aed, 255_000.00);
    assert_eq!(res_27.maximum_statutory_cap_aed, 240_000.00);
    assert_eq!(res_27.final_statutory_gratuity_aed, 240_000.00);
    assert!(res_27.is_capped_at_24_months);
}

#[test]
fn test_cabinet_resolution_1_2022_part_time_proration() {
    let hr = HrSpecialist::new();

    // Full-Time benchmark: Basic = AED 20,000.00, Tenure = 3.0 years -> AED 42,000.00
    // Part-time contracted annual hours = 1,040 hours (exactly 50% of 2,080 annual hours benchmark)
    // Part-time gratuity = (1,040 / 2,080) * 42,000.00 = AED 21,000.00
    let res = hr.calculate_gratuity(20_000.0, 3.0, true, Some(1040.0));
    assert_eq!(res.full_time_gratuity_aed, 42_000.00);
    assert_eq!(res.part_time_ratio, 0.50);
    assert_eq!(res.uncapped_gratuity_aed, 21_000.00);
    assert_eq!(res.final_statutory_gratuity_aed, 21_000.00);
}

#[test]
fn test_article_53_deductions_25_pct_ceiling_enforcement() {
    let hr = HrSpecialist::new();

    // Basic = AED 20,000.00, Tenure = 3.0 years -> Gross Gratuity = AED 42,000.00
    // Article 53 Maximum Permitted Deduction: 25% of 42,000 = AED 10,500.00

    // Scenario A: Lawful deduction within 25% ceiling (AED 5,000.00)
    let lawful = hr.calculate_gratuity_detailed(20_000.0, 3.0, 0, 0, false, None, 5_000.0);
    assert_eq!(lawful.final_statutory_gratuity_aed, 42_000.00);
    assert_eq!(lawful.max_deduction_allowed_aed, 10_500.00);
    assert_eq!(lawful.actual_deductions_applied_aed, 5_000.00);
    assert!(!lawful.deduction_breach_flag);
    assert_eq!(lawful.net_terminal_payout_aed, 37_000.00);

    // Scenario B: Employer attempts excessive deduction exceeding 25% ceiling (AED 16,000.00)
    // Must be capped at AED 10,500.00 and flag statutory breach
    let excessive = hr.calculate_gratuity_detailed(20_000.0, 3.0, 0, 0, false, None, 16_000.0);
    assert_eq!(excessive.final_statutory_gratuity_aed, 42_000.00);
    assert_eq!(excessive.max_deduction_allowed_aed, 10_500.00);
    assert_eq!(excessive.actual_deductions_applied_aed, 10_500.00);
    assert!(excessive.deduction_breach_flag);
    assert_eq!(excessive.net_terminal_payout_aed, 31_500.00); // 42,000 - 10,500
}

#[test]
fn test_article_53_statutory_14_day_deadline() {
    let hr = HrSpecialist::new();

    // Case 1: 10 calendar days elapsed -> Compliant
    let comp = hr.check_terminal_settlement_deadline(10);
    assert!(comp.is_compliant);
    assert_eq!(comp.status, "COMPLIANT_WITHIN_DEADLINE");

    // Case 2: Exactly 14 calendar days elapsed -> Compliant
    let boundary = hr.check_terminal_settlement_deadline(14);
    assert!(boundary.is_compliant);

    // Case 3: 15 calendar days elapsed -> Non-compliant statutory breach
    let breach = hr.check_terminal_settlement_deadline(15);
    assert!(!breach.is_compliant);
    assert_eq!(breach.status, "NON_COMPLIANT_STATUTORY_DEADLINE_BREACH");
}

#[test]
fn test_mohre_nafis_quota_and_statutory_penalties() {
    let hr = HrSpecialist::new();

    // Case 1: Private entity with 60 skilled workers, having 2 Emirati employees
    // 2026 Target: 10% of 60 = 6 national positions
    // Deficit: 6 - 2 = 4 national positions
    // Statutory Penalty: 4 * AED 8,000 = AED 32,000.00 / month (AED 384,000.00 / year)
    let report_deficit = hr.audit_nafis_report("LOGISTICS-FZCO", 60, 2);
    assert!(report_deficit.is_applicable);
    assert_eq!(report_deficit.statutory_target_quota_pct, 10.0);
    assert_eq!(report_deficit.required_national_positions, 6);
    assert_eq!(report_deficit.national_deficit, 4);
    assert!(!report_deficit.is_compliant);
    assert_eq!(report_deficit.statutory_monthly_penalty_per_position_aed, 8_000.0);
    assert_eq!(report_deficit.total_monthly_penalty_aed, 32_000.00);
    assert_eq!(report_deficit.total_annual_penalty_aed, 384_000.00);
    assert_eq!(report_deficit.remedial_hiring_plan.len(), 3);

    // Case 2: Entity with 100 skilled workers meeting 10% quota (10 Emiratis) -> Compliant
    let report_comp = hr.audit_nafis_report("EMIRATES-TECH-LLC", 100, 10);
    assert!(report_comp.is_applicable);
    assert_eq!(report_comp.required_national_positions, 10);
    assert_eq!(report_comp.national_deficit, 0);
    assert!(report_comp.is_compliant);
    assert_eq!(report_comp.total_monthly_penalty_aed, 0.0);

    // Case 3: Entity with < 50 skilled workers (45 employees) -> Exempt from 10% mandate
    let report_exempt = hr.audit_nafis_report("BOUTIQUE-AGENCY-LLC", 45, 1);
    assert!(!report_exempt.is_applicable);
    assert_eq!(report_exempt.required_national_positions, 0);
    assert_eq!(report_exempt.national_deficit, 0);
    assert!(report_exempt.is_compliant);
    assert_eq!(report_exempt.total_monthly_penalty_aed, 0.0);
}
