# QALA (القلعة) TECHNICAL ARCHITECTURE & IMPLEMENTATION SPECIFICATION
## Dubai Create AI Agents Championship 2026–2027 | Best Business AI Agent
**System Role:** Autonomous Enterprise OS & Corporate Agency Mesh  
**Security Standard:** DESC ISR v3.1 & Tau-3 FinOps Financial Gatekeeper  
**Version:** 2.0 (Canonical Implementation Contract)  
**Author:** Mohamed Fathy, BPharm (Lead Systems Architect & Founder)

---

### 1. Architectural Philosophy: The Autonomous Agency Mesh
Qala operates as an autonomous, sovereign enterprise agency comprising a **Managing Director (Chief of Staff)** agent orchestrating **6 Departmental Specialist Agents** across Finance, Legal, Trade/Customs, HR, Procurement, and Operations.

To prevent prompt injection, hallucinated expenditures, and unauthorized data egress, **zero state mutation or execution occurs without passing through the inviolable C99 Policy Governor**.

```
                           ┌────────────────────────────────────────┐
                           │      MANAGING DIRECTOR / COS AGENT     │
                           │  - Executive intent decomposition      │
                           │  - Inter-departmental coordination    │
                           │  - CEO human escalation & approvals    │
                           └───────────────────┬────────────────────┘
                                               │
             ┌───────────────────┬─────────────┴─────┬───────────────────┬───────────────────┐
             ▼                   ▼                   ▼                   ▼                   ▼
     ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
     │   FINANCE    │    │    LEGAL     │    │TRADE/CUSTOMS │    │  HR & PEOPLE │    │ PROCUREMENT  │
     │  & FINOPS    │    │ & COMPLIANCE │    │ (DP WORLD)   │    │  (UAE LABOR) │    │  & OPS MESH  │
     └──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘    └──────┬───────┘
            │                   │                   │                   │                   │
            └───────────────────┴─────────────┬─────┴───────────────────┴───────────────────┘
                                              ▼
                             ┌───────────────────────────────────┐
                             │    QALA C99 GOVERNOR BRIDGE       │
                             │  (adapters/qala_adapters.so)      │
                             ├───────────────────────────────────┤
                             │ • Tau-3 Gate (> AED 50k ceiling)  │
                             │ • DESC ISR v3.1 Path Sandbox      │
                             │ • Scope-Bound Single-Use Tokens   │
                             │ • Append-Only SHA-256 HMAC Ledger │
                             └─────────────────┬─────────────────┘
                                               ▼
                             ┌───────────────────────────────────┐
                             │   MOCK / REAL ENTERPRISE APIS     │
                             │   (SAP, Dubai Customs, DEWA)      │
                             └───────────────────────────────────┘
```

---

### 2. Inter-Agent Communication Protocol (Agent Envelope)

All messages between the Managing Director and Departmental Specialists must follow this standard JSON-RPC 2.0 envelope:

```json
{
  "jsonrpc": "2.0",
  "id": "req-99214-aed",
  "trace_id": "tr-dxb-2026-00192",
  "from_department": "managing_director",
  "to_department": "trade_customs",
  "task": {
    "action": "clear_container_shipment",
    "parameters": {
      "bill_of_lading": "BL-DPW-88192",
      "container_id": "MSCU7729104",
      "cif_value_aed": 1250000.00,
      "hs_code": "8471.30.00"
    }
  },
  "governor_envelope": {
    "is_mutating": true,
    "financial_impact_aed": 62500.00,
    "target_workspace_path": "/workspace/customs_manifest.json",
    "approval_token": null
  }
}
```

---

### 3. Departmental Specialist Agents & Tool Catalogs

#### 3.1 Finance & FinOps Specialist (`qala_finance`)
1. `finops_reconcile_vat(invoice_no, trn, net_aed, vat_aed)`:
   - Validates commercial invoices against UAE FTA 5% VAT statutory rules.
   - Rejects discrepancies $> \text{AED 0.05}$ rounding tolerance.
2. `finops_request_wire(beneficiary, iban, amount_aed, memo, approval_token)`:
   - Gated by C99 Tau-3 Policy: If `amount_aed > 50000.00`, requires verified single-use token.
3. `finops_audit_payroll(month, total_wps_aed, employee_count)`:
   - Audits Wages Protection System (WPS) files pursuant to UAE Ministerial Resolution No. 43 of 2022.

#### 3.2 Legal & Compliance Specialist (`qala_legal`)
1. `legal_review_contract(contract_text, governing_law)`:
   - Validates jurisdiction clauses (DIFC Court, ADGM Court, or Dubai Civil Courts).
   - Flags missing limitation-of-liability or un-severable non-compete clauses.
2. `legal_check_desc_compliance(system_id, data_classification)`:
   - Enforces DESC ISR v3.1 classification (`Public`, `Restricted`, `Confidential`, `Secret`).
3. `legal_audit_nda(nda_text, confidentiality_period_years)`:
   - Ensures term conforms to standard UAE trade secrets protection rules.

#### 3.3 Trade & Customs Specialist (`qala_trade`)
1. `customs_calculate_duty(hs_code, cif_value_aed, origin_country, free_zone_exit)`:
   - Applies GCC Unified Customs Tariff (5% standard) + 5% UAE VAT.
   - Calculates Free Zone exit duties when moving cargo from JAFZA/DAFZA to Dubai mainland.
2. `customs_clear_shipment(bill_of_lading, container_id, importer_code, duty_amount_aed, approval_token)`:
   - Submits declaration to Dubai Customs Mirsal II.
   - Requires C99 cryptographic approval token if `duty_amount_aed > AED 50,000.00`.
3. `customs_inspect_bol(bill_of_lading, declared_weight_kg)`:
   - Cross-checks ocean bill of lading against container weight manifest.

#### 3.4 HR & People Specialist (`qala_hr`)
1. `hr_calculate_gratuity(basic_salary_aed, service_years, termination_type)`:
   - Codifies **UAE Federal Decree-Law No. 33 of 2021 (Article 51)**:
     * Less than 1 year: 0 gratuity.
     * 1 to 5 years: 21 days' basic salary per year of service.
     * More than 5 years: 30 days' basic salary per additional year of service.
     * Absolute cap: 2 years' total salary.
2. `hr_check_nafis_quota(total_skilled_workforce, emirati_skilled_employees)`:
   - Audits private-sector Emiratisation quota (mandatory 2% bi-annual increment).
   - Calculates statutory penalty exposure (AED 8,000/month per un-hired Emirati).
3. `hr_calculate_leave_settlement(basic_salary_aed, unused_annual_leave_days)`:
   - Computes cash-in-lieu of accrued annual leave upon separation.

#### 3.5 Procurement & Operations Specialists (`qala_procurement`, `qala_ops`)
1. `procurement_three_way_match(po_number, invoice_number, grn_number)`:
   - Matches Purchase Order, Commercial Invoice, and Goods Received Note.
2. `procurement_sanctions_check(vendor_name, uae_tax_id)`:
   - Screens vendor against UAE Executive Office for AML/CFT lists.
3. `ops_dewa_audit_commercial_bill(account_no, kwh_consumed, billed_aed)`:
   - Audits commercial DEWA electricity tariffs against statutory slab brackets.

---

### 4. C99 Governor Security Bridge (`adapters/qala_mcp_policy_adapter.c`)

The C99 Governor is non-bypassable. Every mutating tool call must link to `qala_verify_action`:

```c
typedef enum {
    QALA_VERDICT_SAFE            = 0,
    QALA_VERDICT_REVIEW_REQUIRED = 1,
    QALA_VERDICT_BLOCKED         = 2
} qala_verdict_t;

typedef struct {
    const char *action_type;
    double      amount_aed;
    const char *target_path;
    const char *approval_token;
    const char *required_scope;
} qala_action_context_t;

qala_verdict_t qala_verify_action(const qala_action_context_t *ctx, char *rationale_out, size_t rationale_cap);
```

#### Inviolable Rules:
1. **DESC ISR v3.1 Path Containment:** `target_path` must resolve strictly within designated `/workspace/`. Relative directory traversal (`..`, `\u002e\u002e`) and sensitive file targets (`.env`, `id_rsa`, `*.pem`, `*.sql`) return `QALA_VERDICT_BLOCKED`.
2. **Tau-3 FinOps Threshold:** Any transaction with `amount_aed > 50000.00` returns `QALA_VERDICT_REVIEW_REQUIRED` unless a valid, unconsumed cryptographic token is presented.
3. **Single-Use Scope Consumption:** Approval codes are strictly single-use and bound to the matching scope (`finops_remediate`, `customs_clear`). Replay attempts are rejected with `HTTP 409 Conflict`.
4. **Append-Only HMAC Ledger:** Every verdict is written to `qala-audit.jsonl` with SHA-256 HMAC cryptographic chaining.

---

### 5. Multi-Agent Cross-Departmental Scenario
Implementers must verify this complete autonomous scenario:
1. Container `MSCU8829104` arrives at DP World Jebel Ali Port with CIF value AED 1,200,000.
2. **Trade Specialist** calculates 5% customs duty (AED 60,000) + 5% VAT (AED 63,000) $\rightarrow$ total settlement AED 123,000.
3. **Trade Specialist** calls `customs_clear_shipment`.
4. **C99 Governor** halts execution with `QALA_VERDICT_REVIEW_REQUIRED` (amount exceeds AED 50,000).
5. **Managing Director** escalates request to Human CEO with cryptographic request ID.
6. **CEO** issues single-use token `QALA-CORP-8819-AUTH`.
7. **Governor** consumes token, validates scope, and completes clearance.
8. **Finance Specialist** reconciles VAT against FTA ledger.
9. **Operations Specialist** dispatches logistics fleet to container terminal.
10. All actions verified in `qala-audit.jsonl`.
