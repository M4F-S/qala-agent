# DP WORLD & DUBAI CUSTOMS STATELESS MCP TOOL SCHEMAS
## Standard JSON-RPC 2.0 Specifications (Protocol Version 2026-07-28)
### Track: Best Business AI Agent | Dubai AI Agents Championship 2026-2027

---

### 1. `customs/calculate_duty`
Calculates GCC Unified Customs Tariff (5% standard) and UAE VAT (5%) on Cost, Insurance and Freight (CIF) value for shipments arriving at DP World terminals (Jebel Ali Port / Dubai Cargo Village).

#### Schema:
```json
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
}
```

---

### 2. `customs/clear_shipment`
Submits a container clearance declaration to Dubai Customs / DP World Mirsal II system. High-value transactions (> AED 50,000.00) require a verified, single-use cryptographic approval token.

#### Schema:
```json
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
}
```

---

### 3. `vat/reconcile_invoice`
Reconciles corporate commercial invoice against UAE Federal Tax Authority (FTA) 5% VAT rules and customs entry bill.

#### Schema:
```json
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
}
```
