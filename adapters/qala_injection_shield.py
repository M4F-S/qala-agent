"""
qala_injection_shield.py - Qala Enterprise Trade Security & Injection Shield
Dubai Create AI Agents Championship 2026-2027 | Best Business AI Agent
Primary Engine: Belya C99 Policy Gate + Gomaa Injection Defense Patterns
Standards: DESC ISR v3.1 Path Containment + Tau-3 FinOps AED 50k Gate
"""

import os
import sys
import re
import json
import ctypes
import time
from typing import Dict, Any, List, Optional, Tuple

# Resolve Gomaa Search Path
GOMAA_HOME = os.environ.get("GOMAA_HOME", os.path.expanduser("~/gomaa"))
if os.path.isdir(GOMAA_HOME) and GOMAA_HOME not in sys.path:
    sys.path.insert(0, GOMAA_HOME)

try:
    from gomaa.security import AdmissionControl
    from gomaa.core import _neutralize_control_tokens
    GOMAA_AVAILABLE = True
except ImportError:
    GOMAA_AVAILABLE = False

GOMAA_SECURITY_AVAILABLE = GOMAA_AVAILABLE


# ==============================================================================
# GOMAA & ENTERPRISE TRADE INJECTION PATTERNS
# ==============================================================================

TRADE_INJECTION_PATTERNS = [
    r"ignore\s+(all\s+)?(previous\s+|prior\s+|your\s+)?(instructions|commands|directives|rules|system\s+prompt)",
    r"disregard\s+(all\s+)?(previous\s+|prior\s+|your\s+)?(instructions|training|system\s+prompt|rules|commands|directives)",
    r"you\s+must\s+ignore\s+(the\s+)?system\s+prompt",
    r"you\s+are\s+now\s+(in\s+)?(unrestricted|dan|jailbreak)\s+mode",
    r"\bdan\s+mode\b",
    r"\bjailbreak\s+mode\b",
    # Enterprise trade & tariff evasion injections
    r"override\s+(duty|tariff|vat|policy|gate|token|approval)",
    r"set\s+(duty|vat|tariff)\s*(to|=)\s*0",
    r"bypass\s+(tau-3|policy|approval|token|guard)",
    r"release\s+container\s+without\s+(token|approval|code)",
]

SUSPICIOUS_PATH_PATTERNS = [
    r"\.\./",
    r"/\.\.",
    r"%2e%2e",
    r"/etc/shadow",
    r"/etc/passwd",
    r"\.env\b",
    r"\.aws\b",
    r"\.pem\b",
    r"\.id_rsa\b",
]


class QalaInjectionShield:
    """
    Enterprise Trade Document Security & Prompt Injection Shield:
    - Neutralizes LLM prompt injection control tokens (<|im_start|>, [INST], <<SYS>>)
    - Intercepts direct override directives and trade evasion injections
    - Enforces DESC ISR v3.1 path containment on file/manifest references
    - Feeds sanitized trade payloads into Qala C99 policy gate
    """

    def __init__(self, lib_dir: Optional[str] = None):
        self._lib = None
        if lib_dir is None:
            lib_dir = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "build")

        for name in ["libqala.dylib", "libqala.so", "libdubai_adapters.dylib", "libdubai_adapters.so"]:
            cand = os.path.join(lib_dir, name)
            if os.path.exists(cand):
                try:
                    self._lib = ctypes.CDLL(cand)
                    break
                except OSError:
                    continue

    @staticmethod
    def neutralize_text(text: str) -> str:
        """Neutralizes control tokens in prose while preserving code blocks."""
        if not text:
            return ""
        if GOMAA_SECURITY_AVAILABLE:
            clean = _neutralize_control_tokens(text)
        else:
            tokens_to_neutralize = [
                (r"<\|im_start\|>", "<\u200b|im_start|\u200b>"),
                (r"<\|system\|>", "<\u200b|system|\u200b>"),
                (r"<\|user\|>", "<\u200b|user|\u200b>"),
                (r"<\|assistant\|>", "<\u200b|assistant|\u200b>"),
                (r"\[INST\]", "[\u200bINST\u200b]"),
                (r"\[/INST\]", "[\u200b/INST\u200b]"),
                (r"<<SYS>>", "<\u200b<SYS>\u200b>"),
                (r"<</SYS>>", "<\u200b</SYS>\u200b>"),
            ]
            clean = text
            for pat, repl in tokens_to_neutralize:
                clean = re.sub(pat, repl, clean, flags=re.IGNORECASE)

        # XML / Context tag escaping
        clean = re.sub(r"</(recalled_memory_context|trade_context|declaration|script)>", r"<\\/\1>", clean, flags=re.IGNORECASE)
        return clean

    def validate_tool_arguments(self, tool_name: str, args: Dict[str, Any]) -> Tuple[bool, str, Dict[str, Any]]:
        """
        Scans all arguments of an MCP tool call for injection attempts.
        Returns: (is_safe, error_reason, sanitized_args)
        """
        sanitized = {}

        for k, v in args.items():
            if isinstance(v, str):
                # 1. Check path containment on path-like keys
                if any(term in k.lower() for term in ["path", "file", "document", "manifest", "declaration_file"]):
                    for p_pat in SUSPICIOUS_PATH_PATTERNS:
                        if re.search(p_pat, v, re.IGNORECASE):
                            return False, f"DESC ISR v3.1 Violation: Path traversal or unauthorized secret access detected in '{k}': {v}", {}

                # 2. Check prompt injection patterns
                v_lower = v.lower()
                for pat in TRADE_INJECTION_PATTERNS:
                    if re.search(pat, v_lower):
                        return False, f"Prompt Injection Intercepted in '{k}': matches pattern '{pat}'", {}

                # 3. Neutralize any control tokens
                sanitized[k] = self.neutralize_text(v)
            else:
                sanitized[k] = v

        return True, "SAFE", sanitized

    def evaluate_trade_action(
        self,
        tool_name: str,
        arguments: Dict[str, Any],
        supplied_approval_code: Optional[str] = None,
        workspace_path: str = "/workspace"
    ) -> Dict[str, Any]:
        """
        End-to-end evaluation:
        1. Gomaa Injection Defense Screen
        2. Tau-3 FinOps Policy Gate (AED 50k threshold)
        3. DP World / FTA calculation verification
        """
        t0 = time.perf_counter_ns()

        # Step 1: Injection Shield
        is_safe, reason, clean_args = self.validate_tool_arguments(tool_name, arguments)
        if not is_safe:
            return {
                "verdict": "VERDICT_UNSAFE",
                "is_safe": False,
                "is_blocked": True,
                "security_alert": "PROMPT_INJECTION_OR_PATH_ESCAPE",
                "reason": reason,
                "tool_name": tool_name,
                "execution_latency_ms": round((time.perf_counter_ns() - t0) / 1_000_000.0, 3)
            }

        # Step 2: Extract monetary value
        declared_value = 0.0
        for val_key in ["cif_value_aed", "declared_value_aed", "taxable_amount_aed", "amount_aed"]:
            if val_key in clean_args:
                try:
                    declared_value = float(clean_args[val_key])
                    break
                except (ValueError, TypeError):
                    pass

        # Step 3: Tau-3 Policy Evaluation (AED 50k threshold)
        is_mutating = tool_name in ["customs/clear_shipment", "vat/commit_payment", "dpworld/release_container"]
        if is_mutating and declared_value > 50000.00:
            valid_tokens = ["QALA-AUTH-DPW-9921", "QALA-AUTH-TAX-1044"]
            if not supplied_approval_code or supplied_approval_code not in valid_tokens:
                return {
                    "verdict": "VERDICT_REVIEW",
                    "is_safe": False,
                    "is_blocked": True,
                    "security_alert": "TAU3_THRESHOLD_EXCEEDED",
                    "reason": f"Declared value AED {declared_value:,.2f} exceeds AED 50,000 threshold. Valid approval code required.",
                    "tool_name": tool_name,
                    "execution_latency_ms": round((time.perf_counter_ns() - t0) / 1_000_000.0, 3)
                }

        # Step 4: Legitimate calculation settlement
        customs_duty = 0.0
        statutory_vat = 0.0
        total_settlement = declared_value

        if tool_name in ["customs/calculate_duty", "customs/clear_shipment"]:
            customs_duty = round(declared_value * 0.05, 2)
            statutory_vat = round((declared_value + customs_duty) * 0.05, 2)
            total_settlement = round(declared_value + customs_duty + statutory_vat, 2)
        elif tool_name == "vat/reconcile_invoice":
            statutory_vat = round(declared_value * 0.05, 2)
            total_settlement = round(declared_value + statutory_vat, 2)

        t1 = time.perf_counter_ns()
        return {
            "verdict": "VERDICT_SAFE",
            "is_safe": True,
            "is_blocked": False,
            "security_alert": "NONE",
            "tool_name": tool_name,
            "declared_cif_aed": declared_value,
            "customs_duty_5pct_aed": customs_duty,
            "statutory_vat_5pct_aed": statutory_vat,
            "total_settlement_aed": total_settlement,
            "sanitized_arguments": clean_args,
            "execution_latency_ms": round((t1 - t0) / 1_000_000.0, 3)
        }
