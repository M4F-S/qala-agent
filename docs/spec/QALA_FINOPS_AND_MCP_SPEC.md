# QALA FINOPS & STATELESS MCP ARCHITECTURE SPECIFICATION
## Sovereign Enterprise Gatekeeper for Dubai Trade & Logistics
### Best Business AI Agent | Dubai Create AI Agents Championship 2026–2027

---

### 1. Executive Mission
Qala (القلعة) is a bare-metal C99 policy gatekeeper and trade settlement agent designed for Dubai enterprise logistics (DP World, Dubai Customs, Jebel Ali Free Zone) and corporate treasury operations. It enforces zero-trust boundaries between autonomous LLMs and enterprise execution backends.

```
                           [LLM Action Proposal]
                                     │
                                     ▼
                   ┌───────────────────────────────────┐
                   │       QALA C99 POLICY GATE        │
                   │  - DESC ISR v3.1 Path Containment │
                   │  - Tau-3 FinOps Threshold (50k)   │
                   │  - Single-Use Scope Verification  │
                   └─────────────────┬─────────────────┘
                                     │
                      ┌──────────────┴──────────────┐
                      ▼                             ▼
              [SAFE / VERIFIED]             [REVIEW REQUIRED]
                      │                             │
                      ▼                             ▼
        ┌───────────────────────────┐ ┌───────────────────────────┐
        │  Stateless MCP Client     │ │  Human Approval Gate      │
        │  - libcurl + cJSON        │ │  - Dual-Key Token         │
        │  - JSON-RPC 2.0 over HTTP │ │  - Scope: finops_remediate│
        │  - Connects to SAP / ERP  │ │  - Single-use consumption │
        └───────────────────────────┘ └───────────────────────────┘
```

---

### 2. Core Architectural Pillars

#### 2.1 DESC ISR v3.1 Path Isolation & Sandboxing
- In accordance with Dubai Electronic Security Center (DESC) Information Security Regulations v3.1:
  * Restricts all filesystem access strictly to designated runtime directories (`/workspace/`).
  * Rejects relative directory traversal sequences (`..`, `\u002e\u002e`, `%2e%2e`).
  * Rejects access to sensitive credential and secret paths (`.env`, `.aws/credentials`, `id_rsa`, `*.pem`, `*.sql`).
  * Enforces parent-directory canonicalization via `realpath()` with fail-closed semantics.

#### 2.2 Tau-3 FinOps Policy Gate
- Enforces an automated financial threshold of **AED 50,000.00**:
  * Transactions $\le\text{AED 50,000.00}$ with read-only operations are marked `VERDICT_SAFE`.
  * Transactions $>\text{AED 50,000.00}$ or operations performing state mutation (`is_mutating = true`) are gated as `VERDICT_REVIEW`.
  * Requires a verified, unconsumed, scope-bound single-use cryptographic approval code before execution.
  * Rejects wildcard scope matching: if `required_scope` is provided, it must strictly match the slot scope in constant time (`constant_time_streq`).

#### 2.3 Stateless MCP Transport (2026-07-28 Specification)
- Production C99 driver communicating over streamable HTTP with Server-Sent Events (SSE) framing.
- Built strictly with standard `libcurl` and `cJSON`.
- Implements standard JSON-RPC 2.0 handshake, tool enumeration (`tools/list`), and execution (`tools/call`).
- Standardized error mapping: HTTP 401 $\rightarrow$ `MCP_ERR_UNAUTHORIZED`; HTTP 403 $\rightarrow$ `MCP_ERR_FORBIDDEN`.

#### 2.4 Grounding Shield
- Audits numerical claims and trade amounts against verified statutory corpora before permitting mutations, preventing LLM-generated hallucinated settlements.
