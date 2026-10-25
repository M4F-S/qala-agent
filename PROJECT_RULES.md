# GLOBAL PROJECT ENGINEERING RULES & ARCHITECTURAL INVARIANTS
## Dubai Create AI Agents Championship 2026–2027
### Autonomous Systems Engineering & Regulatory Governance Mandate

---

### PREAMBLE: THE MANDATE FOR ENGINEERING EXCELLENCE
This repository and its decoupled agent workspaces exist to build world-class, sovereign AI agents competing for the Grand Prize (AED 1,000,000+) in the Dubai Create AI Agents Championship (Dubai Future Foundation / DCDE). 

To ensure our work survives the most rigorous technical and regulatory scrutiny by panels from MBZUAI, TII, G42, and the Dubai Health Authority (DHA), all development across all agents, sessions, and team members must strictly conform to the following non-negotiable Global Rules.

---

### RULE 1: THE DATA & PROVENANCE INVARIANT
> **"No unmeasured number leaves this repository without genuine hardware provenance."**
1. Never narrate, publish, or present a simulation, projection, or mathematical model as an empirical measurement.
2. If a metric is derived from theoretical specifications (e.g. DDR5-5600 bus physics: $89.6\text{ GB/s} / 4.5\text{ GB} = 19.91\text{ tok/s}$), it MUST be prominently and unambiguously labelled **`[MODELLED / PROJECTED]`**.
3. Only metrics accompanied by raw, reproducible machine execution logs (e.g. `llama-bench` CSVs with hardware timestamps, CPU/GPU flags, and commit hashes) may be labelled **`[EMPIRICAL / MEASURED]`**.
4. Test suites must never assert synthetic constants or hardcoded pass rates as if they were live model evaluations.

---

### RULE 2: DEFENSE-IN-DEPTH & FAIL-CLOSED SAFETY
> **"When in doubt, fail closed. Never emit false assurance."**
1. **Clinical Safety (Taryaq):**
   - The clinical engine must **never** return `CLINICAL_SAFE` for unmodeled drugs, unknown interactions, or missing lab parameters.
   - If a medication or combination is not in the verified interaction database, the engine MUST return `UNVERIFIED_DRUG_REQUIRES_PHARMACIST_REVIEW` or `CLINICAL_CLAIM_CYCLE_WARN`.
   - All numerical lab values (eGFR, serum creatinine, dosages) must be strictly checked with `isfinite()` and bounded within physiological limits.
2. **Policy & Sandboxing (Qala):**
   - Path verification must enforce strict containment within authorized workspace boundaries.
   - Null or unassigned scopes (`required_scope == NULL`) must never act as wildcards to bypass scope verification.
   - High-value transactions (Tau-3 threshold: AED 50,000) strictly require verified single-use cryptographic tokens.
3. **Personal Privacy (Nibras):**
   - Zero-allocation memory guarantees.
   - All personally identifiable information (PII) must be scrubbed or masked in accordance with UAE Federal Decree-Law No. 45 of 2021 (PDPL) before any external processing.

---

### RULE 3: DO NOT REINVENT THE WHEEL
> **"Rely on established, battle-tested standards and robust libraries."**
1. Use standard ISO C99 (`-std=c99 -Wall -Wextra -Werror -O2`) with POSIX feature test macros (`#define _POSIX_C_SOURCE 200809L` and `#define _XOPEN_SOURCE 700`) to guarantee deterministic compilation on Linux glibc and macOS clang.
2. Use established standard libraries:
   - Networking & HTTP: Standard `libcurl`.
   - Serialization: Standard `cJSON`.
   - Cryptographic primitives: Standard NIST SHA-256 / HMAC.
   - Healthcare Interoperability: Fast Healthcare Interoperability Resources (FHIR R4) and SMART on FHIR conventions.
3. Avoid ad-hoc, brittle custom protocols where standard HTTP/JSON-RPC 2.0 or WebSocket (RFC 6455) interfaces exist.

---

### RULE 4: STATUTORY & RESEARCH-DRIVEN GROUNDING
> **"Every clinical rule and policy gate must cite primary statutory or peer-reviewed literature."**
1. **Primary UAE & GCC Baselines:**
   - **UAE Federal Law No. 2 of 2019 (Health ICT Law):** Article 13 data localization and zero cross-border PHI egress.
   - **DHA Standards Notice SN 03/2019:** 
     * §3.7.1: Outpatient claims submission by 15th of the subsequent month.
     * §3.7.2: 0.03% per calendar day delay penalty on Net Claimed amount for late initial submission.
     * §3.8.1: Resubmission within 21 calendar days from Remittance Advice.
     * §3.8.4: 0.03% per calendar day delay penalty for late resubmissions.
     * §3.9.3: 0.03% per calendar day late payment penalty owed by Payers to Healthcare Providers.
   - **DESC Information Security Regulations (ISR v3.1):** Strict path containment, least-privilege role-based access, and audit ledger integrity.
   - **DoH Abu Dhabi Responsible AI Standard & Saudi SFDA MDS-G53:** Human-in-the-loop oversight, explainability, and post-market tracking.
2. **Medical Intelligence (Per Comparative Healthcare Directory 2026):**
   - Adopt the "agent drafts, clinician signs" framework to prevent high-risk SaMD classification barriers.
   - Defeat alert fatigue by utilizing longitudinal pharmaco-clinical risk trajectories (identifying compound deterioration such as ACE-inhibitor + NSAID in declining eGFR) rather than firing noisy binary pop-ups.
   - Bridge regional diglossia: process colloquial spoken Arabic dialects (15+ variants) and auto-generate structured English clinical summaries (SNOMED-CT / ICD-10).

---

### RULE 5: PURE ENGINEERING FIRST (NO HYPE, NO DISTRACTIONS)
> **"The software is the pitch. Without an impeccable agent, presentations are meaningless."**
1. Do not spend time editing pitch decks, slides, marketing copy, or video scripts until the underlying code, test batteries, and local agents are 100% functional, warning-clean, and green.
2. Every feature, rule, or claim must be backed by an automated test case that runs in CI.
3. Work defensively, commit frequently with clear conventional commit messages, and verify all builds after every change.
