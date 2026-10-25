#!/usr/bin/env python3
"""
mcp_mock_server.py - Stateless MCP 2026-07-28 Mock HTTP/SSE Server
Specification: Streamable HTTP over JSON-RPC 2.0 with L7 Header Routing
Supports: initialize, tools/list, tools/call, auth/forbidden tests, shutdown
Pure Python 3 standard library: http.server, json, sys, signal, time, threading
"""

import sys
import json
import time
import signal
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
import socketserver

HOST = "127.0.0.1"
PORT = 8765

class MCPMockHandler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, fmt, *args):
        # Suppress routine request logs to keep test output pristine
        pass

    def send_error_response(self, code, message):
        body = f"{message}\n".encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()

    def send_sse_frame(self, data_dict):
        json_str = json.dumps(data_dict)
        sse_body = f"event: message\r\ndata: {json_str}\r\n\r\n".encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Content-Length", str(len(sse_body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(sse_body)
        self.wfile.flush()

    def handle_shutdown(self):
        body = b"MCP Mock Server shutting down\n"
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)
        self.wfile.flush()

        def _do_shutdown(srv):
            time.sleep(0.05)
            srv.shutdown()

        threading.Thread(target=_do_shutdown, args=(self.server,), daemon=True).start()

    def do_GET(self):
        if self.path == "/shutdown":
            self.handle_shutdown()
            return
        elif self.path in ("/health", "/", "/mcp"):
            body = b"OK - MCP Mock Server Active\n"
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(body)
            self.wfile.flush()
            return
        else:
            self.send_error_response(404, "Not Found")

    def do_POST(self):
        if self.path == "/shutdown":
            self.handle_shutdown()
            return

        # 1. Path-specific negative auth & policy tests
        if self.path == "/mcp/auth-test":
            auth = self.headers.get("Authorization")
            if not auth or not auth.strip():
                self.send_error_response(401, "MCP_ERR_UNAUTHORIZED: Missing or invalid token")
                return

        if self.path == "/mcp/forbidden-test":
            self.send_error_response(403, "MCP_ERR_FORBIDDEN: Policy gate denied access")
            return

        if not self.path.startswith("/mcp"):
            self.send_error_response(404, "Not Found: Invalid endpoint path")
            return

        # 2. MCP 2026-07-28 stateless HTTP header validation
        proto_ver = self.headers.get("MCP-Protocol-Version")
        if proto_ver != "2026-07-28":
            self.send_error_response(400, "HeaderMismatch: MCP-Protocol-Version must be 2026-07-28")
            return

        mcp_name = self.headers.get("Mcp-Name")
        if not mcp_name or not mcp_name.strip():
            self.send_error_response(400, "HeaderMismatch: Missing Mcp-Name header")
            return

        mcp_method = self.headers.get("Mcp-Method")
        if not mcp_method or not mcp_method.strip():
            self.send_error_response(400, "HeaderMismatch: Missing Mcp-Method header")
            return

        # 3. Read & validate JSON-RPC payload
        try:
            content_len = int(self.headers.get("Content-Length", 0))
            raw_body = self.rfile.read(content_len).decode("utf-8")
            rpc_payload = json.loads(raw_body)
        except Exception as e:
            self.send_error_response(400, f"HeaderMismatch: Malformed JSON body - {e}")
            return

        # Invariant: Rejects request if _meta is at JSON-RPC root
        if "_meta" in rpc_payload:
            self.send_error_response(400, "HeaderMismatch: _meta must be inside params._meta, not at root")
            return

        # Verify header method matches body method
        body_method = rpc_payload.get("method")
        if body_method != mcp_method:
            self.send_error_response(400, f"HeaderMismatch: Mcp-Method header '{mcp_method}' != payload '{body_method}'")
            return

        req_id = rpc_payload.get("id", 1)
        params = rpc_payload.get("params", {})

        # 4. Route methods to SSE responses
        if mcp_method == "initialize":
            res = {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "protocolVersion": "2026-07-28",
                    "serverInfo": {
                        "name": "dubai-mcp-mock-server",
                        "version": "1.0.0"
                    },
                    "capabilities": {
                        "tools": {"listChanged": False},
                        "logging": {}
                    }
                }
            }
            self.send_sse_frame(res)
            return

        elif mcp_method == "tools/list":
            res = {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "tools": [
                        {
                            "name": "customs_duty_calculator",
                            "description": "Calculates Dubai Customs import tariffs and 5% VAT",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "hs_code": {"type": "string"},
                                    "declared_value_aed": {"type": "number"}
                                },
                                "required": ["declared_value_aed"]
                            }
                        },
                        {
                            "name": "customs/calculate_duty",
                            "description": "Calculates GCC Unified Customs Tariff (5% standard) and UAE VAT (5%) on Cost, Insurance and Freight (CIF) value for DP World container arrivals",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "hs_code": {
                                        "type": "string",
                                        "description": "Harmonized System 6-to-8 digit tariff classification code (e.g. 8471.30.00)"
                                    },
                                    "cif_value_aed": {
                                        "type": "number",
                                        "description": "Cost, Insurance, and Freight (CIF) declared shipment value in AED"
                                    },
                                    "origin_country": {
                                        "type": "string",
                                        "description": "ISO 3166-1 alpha-2 origin country code (e.g. AE, US, CN, DE)"
                                    },
                                    "free_zone_exit": {
                                        "type": "boolean",
                                        "description": "True if cargo is exiting JAFZA / DAFZA into UAE mainland domestic market"
                                    }
                                },
                                "required": ["hs_code", "cif_value_aed"]
                            }
                        },
                        {
                            "name": "customs/clear_shipment",
                            "description": "Submits Dubai Customs Mirsal II / DP World container import declaration and generates electronic release order (RO)",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "bill_of_lading": {
                                        "type": "string",
                                        "description": "Ocean carrier master or house bill of lading number (e.g. BL-DPW-99214)"
                                    },
                                    "container_id": {
                                        "type": "string",
                                        "description": "ISO 6346 compliant container identifier (e.g. MSCU8291043)"
                                    },
                                    "importer_code": {
                                        "type": "string",
                                        "description": "Dubai Customs Client / Importer Registration Code (e.g. DXB-IMP-77102)"
                                    },
                                    "duty_amount_aed": {
                                        "type": "number",
                                        "description": "Total customs tariff and VAT settlement amount in AED"
                                    },
                                    "port_code": {
                                        "type": "string",
                                        "description": "UN/LOCODE port code ('AEJEA' for Jebel Ali, 'AEDXB' for Port Rashid)"
                                    },
                                    "declaration_type": {
                                        "type": "string",
                                        "description": "Import declaration type ('IMPORT_LOCAL', 'IMPORT_FREEZONE', 'TRANSIT')"
                                    },
                                    "approval_token": {
                                        "type": "string",
                                        "description": "Single-use cryptographic approval code required if duty_amount_aed > AED 50,000"
                                    }
                                },
                                "required": ["bill_of_lading", "container_id", "importer_code", "duty_amount_aed"]
                            }
                        },
                        {
                            "name": "vat/reconcile_invoice",
                            "description": "Reconciles corporate commercial invoice against UAE Federal Tax Authority (FTA) 5% VAT rules and customs entry bill",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "invoice_number": {
                                        "type": "string",
                                        "description": "Commercial tax invoice reference number"
                                    },
                                    "tax_registration_number": {
                                        "type": "string",
                                        "description": "15-digit UAE FTA Tax Registration Number (TRN)"
                                    },
                                    "net_amount_aed": {
                                        "type": "number",
                                        "description": "Net taxable invoice amount before VAT in AED"
                                    },
                                    "vat_amount_aed": {
                                        "type": "number",
                                        "description": "Claimed VAT amount in AED (must match 5% of net)"
                                    }
                                },
                                "required": ["invoice_number", "tax_registration_number", "net_amount_aed", "vat_amount_aed"]
                            }
                        },
                        {
                            "name": "dha_delay_penalty_calculator",
                            "description": "Calculates DHA SN 03/2019 0.03%/day late claim penalties",
                            "inputSchema": {
                                "type": "object",
                                "properties": {
                                    "claim_id": {"type": "string"},
                                    "delay_days": {"type": "integer"}
                                },
                                "required": ["claim_id", "delay_days"]
                            }
                        }
                    ]
                }
            }
            self.send_sse_frame(res)
            return

        elif mcp_method == "tools/call":
            tool_name = params.get("name")
            arguments = params.get("arguments", {})
            is_error = False

            if tool_name == "customs_duty_calculator":
                val = float(arguments.get("declared_value_aed", 0.0))
                duty = val * 0.05
                text_out = f"Calculated Duty: AED {duty:.2f} (5% standard tariff on AED {val:.2f})"
            elif tool_name == "customs/calculate_duty":
                cif = float(arguments.get("cif_value_aed", arguments.get("declared_value_aed", 0.0)))
                hs = arguments.get("hs_code", "8471.30.00")
                origin = arguments.get("origin_country", "AE")
                duty = cif * 0.05
                vat = (cif + duty) * 0.05
                total = duty + vat
                text_out = (
                    f"DP World Customs Tariff Assessment (HS {hs}, Origin {origin}): "
                    f"CIF: AED {cif:.2f} | Customs Duty (5%): AED {duty:.2f} | "
                    f"UAE VAT (5% on CIF+Duty): AED {vat:.2f} | Total Settlement: AED {total:.2f}"
                )
            elif tool_name == "customs/clear_shipment":
                bol = arguments.get("bill_of_lading", "BL-UNKNOWN")
                container = arguments.get("container_id", "CONT-UNKNOWN")
                imp_code = arguments.get("importer_code", "IMP-UNKNOWN")
                duty = float(arguments.get("duty_amount_aed", 0.0))
                port = arguments.get("port_code", "AEJEA")
                token = arguments.get("approval_token")

                if duty > 50000.0 and (not token or not str(token).strip()):
                    text_out = f"REJECTED: Shipment clearance for container {container} (AED {duty:.2f}) exceeds AED 50,000 threshold and requires approval_token"
                    is_error = True
                else:
                    text_out = (
                        f"DP World Mirsal II Electronic Release Order Issued: "
                        f"Release Order: RO-DPW-2026-88192 | Container: {container} | Port: {port} | "
                        f"Importer: {imp_code} | Customs Duty Settled: AED {duty:.2f} | "
                        f"Status: CLEARED (Gate Pass Valid 72 Hours)"
                    )
            elif tool_name == "vat/reconcile_invoice":
                inv = arguments.get("invoice_number", "INV-UNKNOWN")
                trn = arguments.get("tax_registration_number", "000000000000000")
                net = float(arguments.get("net_amount_aed", 0.0))
                vat = float(arguments.get("vat_amount_aed", 0.0))
                expected_vat = round(net * 0.05, 2)
                if abs(vat - expected_vat) > 0.05:
                    text_out = f"FTA VAT MISMATCH: Invoice {inv} TRN {trn} expected VAT AED {expected_vat:.2f} but claimed AED {vat:.2f}"
                    is_error = True
                else:
                    text_out = f"FTA VAT RECONCILED: Invoice {inv} TRN {trn} net AED {net:.2f} matches 5% statutory VAT AED {vat:.2f}"
            elif tool_name == "dha_delay_penalty_calculator":
                claim_id = arguments.get("claim_id", "CLM-UNKNOWN")
                days = int(arguments.get("delay_days", 0))
                penalty_pct = days * 0.03
                text_out = f"Claim {claim_id}: DHA SN 03/2019 penalty is {penalty_pct:.2f}% across {days} days"
            else:
                text_out = f"Executed mock tool '{tool_name}' with args {json.dumps(arguments)}"

            res = {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [
                        {
                            "type": "text",
                            "text": text_out
                        }
                    ],
                    "isError": is_error
                }
            }
            self.send_sse_frame(res)
            return

        else:
            self.send_error_response(400, f"UnsupportedMethod: {mcp_method}")


class ReusableThreadingServer(socketserver.ThreadingMixIn, HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


def main():
    port = PORT
    if len(sys.argv) > 1:
        try:
            port = int(sys.argv[1])
        except ValueError:
            pass

    server = ReusableThreadingServer((HOST, port), MCPMockHandler)

    def handle_signal(sig, frame):
        threading.Thread(target=server.shutdown, daemon=True).start()

    signal.signal(signal.SIGINT, handle_signal)
    signal.signal(signal.SIGTERM, handle_signal)

    print(f"[mcp_mock_server] Listening on http://{HOST}:{port}/mcp", flush=True)

    try:
        server.serve_forever()
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
