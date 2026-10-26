# Qala (القلعة) — Sovereign Enterprise Policy & Trade AI Agent
## Track: Best Business AI Agent | Dubai Create AI Agents Championship 2026–2027

Qala is a bare-metal C99 deterministic policy compiler and enterprise trade gatekeeper. Built on the Belya deterministic architecture, Qala enforces unbypassable boundaries between autonomous LLMs and enterprise execution backends (ERP systems, DP World / Dubai Customs APIs, and corporate treasury).

---

### 1. Verified Architectural Claims & Proof Commands

Every technical differentiator claimed by Qala is backed by a reproducible machine command.

| Architectural Feature | Specification & Standard | Deterministic Guarantee | Machine Proof Command |
| :--- | :--- | :--- | :--- |
| **Deterministic Policy Core** | ISO C99 (`-std=c99 -Wall -Wextra -Werror -O2`) | Zero dynamic heap allocation in core gate; microsecond function latency | `./build/qala_agent --test` |
| **Tau-3 FinOps Policy Gate** | Tau-Bench FinOps Standard | Strict AED 50,000.00 ceiling; mutations require valid scope-bound approval code | `./build/qala_agent --verify-tx 125000 QALA-AUTH-DPW-9921` |
| **Single-Use Token Ledger** | DESC ISR v3.1 Domain 7 (Access Control) | Single-use scope-bound approval codes; persistent disk ledger blocks replay across reboots | `./build/test_qala` |
| **Workspace Sandbox** | DESC ISR v3.1 Domain 14 (Containment) | Strict canonical prefix matching via `realpath()`; blocks traversal (`..`, `%2e%2e`) and secrets | `./build/test_qala` |
| **Stateless MCP Transport** | MCP Streamable HTTP Spec (2026-07-28) | RFC-compliant JSON-RPC 2.0 client via `libcurl` and `cJSON`; maps HTTP 401/403 to MCP error codes | `./build/test_mcp_interop` |
| **Factual Grounding Shield** | UAE Statutory & Regulatory Corpora | Cross-checks numerical penalties and tariffs against statutory texts to intercept hallucinations | `./build/test_grounding` |
| **Prompt Injection Defense** | Gomaa Defense Architecture (Optional) | Neutralizes control tokens (`<\|im_start\|>`, `[INST]`) and catches prompt override directives | `python3 tests/test_qala_injection_defense.py` |

---

### 2. Built On (Attribution & Lineage)

Qala builds upon proven foundations while introducing specialized sovereign trade and policy governance:

- **What is New in Qala:**
  - **DESC ISR v3.1 Path Sandbox:** Canonical prefix containment via `realpath()` with fail-closed bounds checking (`adapters/qala_mcp_policy_adapter.c`).
  - **Tau-3 FinOps Gate:** AED 50,000.00 ceiling and persistent on-disk single-use token lifecycle ledger (`qala-tokens.jsonl`).
  - **Stateless MCP 2026-07-28 Driver:** C99 `libcurl` + `cJSON` transport supporting Server-Sent Events (SSE) and L7 header routing (`adapters/mcp_curl_client.c`).
  - **DP World & Dubai Customs Tool Schemas:** Standard JSON-RPC 2.0 tools for container clearance (`customs/clear_shipment`), tariff calculations (`customs/calculate_duty`), and UAE FTA VAT reconciliation (`vat/reconcile_invoice`).
  - **Grounding Shield Verifier:** In-memory deterministic claim-versus-statutory-corpus cross-examiner (`adapters/grounding_shield.c`).

- **Derived & Reused Components:**
  - **Belya Engine:** Primary C99 deterministic execution and policy compilation core architecture (© Mohamed Fathy, Apache-2.0).
  - **Gomaa:** Optional agent memory and injection defense patterns (© Mohamed Fathy, Apache-2.0).
  - **cJSON:** Ultralightweight ANSI C JSON parser (Dave Gamble, MIT License).

See `NOTICE` and `LICENSE` for formal attribution details.

---

### 3. Optional Integration: Gomaa

Qala includes optional integration with the Gomaa Agent Memory OS for prompt injection defense:
- To enable Gomaa integration, set the `GOMAA_HOME` environment variable to your Gomaa installation directory (default: `~/gomaa`).
- If Gomaa is not installed or `GOMAA_HOME` points to a non-existent path, the Python test suite gracefully skips the Gomaa-dependent tests with exit code 0 (`OK (skipped=6)`).
- The core C99 policy gate, path sandbox, and stateless MCP transport operate completely standalone with zero dependencies on Python or Gomaa.

---

### 4. Build, Verify & Test

#### Prerequisites
- POSIX-compatible environment (Linux glibc or macOS clang)
- ISO C99 compiler (`gcc` or `clang`)
- GNU Make
- `libcurl` development headers (`libcurl4-openssl-dev` on Debian/Ubuntu)
- Python 3 (for MCP mock server and optional injection test suite)

#### Quick Start Commands
```bash
# Build all binaries and run all verification batteries
make verify

# Run test suites individually
make test

# Build the standalone shared library (libqala.so / libqala.dylib)
make all

# Clean build artifacts (preserves token ledger)
make clean
```
