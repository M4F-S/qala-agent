# Qala (القلعة) — Sovereign Autonomous Business & FinOps Policy Gatekeeper
## Best Business AI Agent | Dubai Create AI Agents Championship 2026–2027

---

### 1. Executive Summary
In enterprise trade, logistics (e.g. DP World, Dubai Customs), and government operations across Dubai, enterprise AI assistants face severe operational and compliance risks:
1. **Uncontrolled Payouts & Autonomous Budget Drift:** Unbounded agentic workflows can execute catastrophic mutations without human oversight.
2. **Sovereign Path Traversal & Data Exfiltration:** DESC ISR v3.1 requires strict path containment and least-privilege isolation.
3. **Enterprise Protocol Friction:** Integrating modern agents into legacy ERPs requires stateless, robust transport.

**Qala** provides a bare-metal C99 policy gate enforcing:
- **Tau-3 FinOps Threshold:** Any transaction exceeding **AED 50,000.00** or modifying state strictly requires a valid, scope-bound, single-use cryptographic approval token.
- **DESC ISR v3.1 Path Containment:** Hardened path isolation preventing filesystem traversal.
- **Stateless MCP Transport (2026-07-28):** Streamable HTTP/JSON-RPC 2.0 client powered by libcurl and cJSON.
- **Grounding Shield:** Factual audit preventing regulatory hallucinations.

---

### 2. Build & Test Instructions
```bash
# Build standalone test binaries and shared library
make all

# Run complete verification battery (Policy gate, grounding shield, MCP client & interop)
make test

# Clean build artifacts
make clean
```
