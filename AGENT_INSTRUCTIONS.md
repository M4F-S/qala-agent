# QALA (القلعة) AGENT SESSION INSTRUCTIONS & OPERATOR GUIDELINES
## Best Business AI Agent | Dubai Create AI Agents Championship 2026–2027
### Target Workspace: `/Users/mohamedfathy/Desktop/qala-agent`
### Remote Origin: `https://github.com/M4F-S/qala-agent.git`
### Google Drive Mirror: `My Drive/Dubai_AI_Championship_2027/qala-agent/`

---

### 1. OPERATOR IDENTITY & COMMUNICATION RULES
1. **Who the Operator is:** Mohamed Fathy, BPharm (Lead Systems Architect & Founder).
2. **Clear & Simple Language:** Always discuss problems, trade-offs, and architecture in **clear, simple, everyday English**. Never overwhelm the operator with dense jargon or complex multiple-choice interrogations.
3. **Research-First Protocol:** **NEVER guess, assume, or invent policy rules or trade architectures.** Always ask Mohamed for domain research or enterprise guidelines before making major decisions.
4. **Engineering First (Strictly No Hype):** Focus 100% on **building, hardening, and testing the software code**. Do NOT talk about pitch decks, demo videos, or peripheral marketing until the agent code is 100% complete and passing tests.
5. **Tri-Mirror Synchronization:** The local folder, Google Drive mirror, and GitHub repo **must remain 100% identical mirrors at all times**. Whenever code or documentation is created or modified, run `./sync_mirror.sh` to update both Google Drive and GitHub immediately.

---

### 2. UNDERLYING ENGINE FOUNDATION: BELYA
- **Primary Foundation Engine:** **`Belya` (`M4F-S/Belya`)**
  - Qala is built directly on **Belya's deterministic C99 architecture**.
  - *Why:* Corporate treasury, customs settlements (DP World / Dubai Customs), and DESC ISR v3.1 security demand hard, unbypassable policy compilation, zero-allocation memory safety, and microsecond determinism.

---

### 3. ARCHITECTURAL SCOPE: BUSINESS & TRADE GATEKEEPER
Qala is an autonomous enterprise policy gatekeeper and trade settlement agent:
1. **DESC ISR v3.1 Path Sandbox:** Enforces strict containment within designated `/workspace/` directories, completely blocking path traversal (`..`, `%2e%2e`) and secret access (`.env`, `.aws`, `*.pem`).
2. **Tau-3 FinOps Policy Gate:** Enforces the strict **AED 50,000.00** threshold. Any mutation or transaction exceeding AED 50k requires an unconsumed, scope-bound single-use cryptographic approval code.
3. **Stateless MCP Client (2026-07-28 Spec):** Standard `libcurl` and `cJSON` transport communicating with enterprise ERPs and customs APIs.
4. **Factual Grounding Shield:** Cross-checks trade and regulatory claims against statutory corpora to prevent LLM hallucinations.

---

### 4. IMMEDIATE ENGINEERING ROADMAP
When continuing development in this session, focus on these concrete coding tasks:
- [ ] **Task 1: Dynamic `/workspace` Containment:** Upgrade `adapters/qala_mcp_policy_adapter.c` from static token denylisting to strict canonical path prefix matching using `realpath()`.
- [ ] **Task 2: Persistent Dual-Key Token Lifecycle:** Implement persistent on-disk state (JSONL or SQLite) for single-use approval codes so restarts do not re-arm consumed tokens.
- [ ] **Task 3: Real Customs & Logistics Tool Definitions:** Add realistic JSON-RPC tool schemas for DP World container clearance (`customs/clear_shipment`, `customs/calculate_duty`, `vat/reconcile_invoice`).
- [ ] **Task 4: Build & Test Verification:** Run `make clean && make test` and ensure all suites pass with zero warnings.

---

### 5. HOW TO VERIFY & TEST
```bash
# Compile and run the complete test suite
make clean && make test

# Synchronize with Google Drive and GitHub
./sync_mirror.sh
```
