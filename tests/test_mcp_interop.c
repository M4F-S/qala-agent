/* test_mcp_interop.c
 * End-to-End Interoperability Test Suite for Stateless MCP 2026-07-28
 * Validates C99 libcurl client against live Python Mock Server over HTTP/SSE
 * Compiler Flags: -std=c99 -Wall -Wextra -Werror -O2 -Iadapters
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "mcp_curl_client.h"
#include "cJSON.h"

#define MCP_TEST_ENDPOINT "http://127.0.0.1:8765/mcp"
#define MCP_AUTH_ENDPOINT "http://127.0.0.1:8765/mcp/auth-test"
#define MCP_FORBIDDEN_ENDPOINT "http://127.0.0.1:8765/mcp/forbidden-test"

static void test_1_initialize(void) {
    printf("[TEST 1] mcp_client_initialize() -> MCP_SUCCESS & non-null payload...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_initialize(&cfg, 1001, &resp);

    assert(st == MCP_SUCCESS);
    assert(resp.status == MCP_SUCCESS);
    assert(resp.http_status_code == 200);
    assert(!resp.is_rpc_error);
    assert(resp.result_text_payload != NULL);
    assert(resp.raw_json_response != NULL);
    assert(strstr(resp.result_text_payload, "2026-07-28") != NULL);

    /* Verify JSON schema contains protocolVersion and serverInfo */
    cJSON *root = cJSON_Parse(resp.raw_json_response);
    assert(root != NULL);
    cJSON *res = cJSON_GetObjectItem(root, "result");
    assert(res != NULL);
    cJSON *proto = cJSON_GetObjectItem(res, "protocolVersion");
    assert(proto != NULL && strcmp(proto->valuestring, "2026-07-28") == 0);
    cJSON *server_info = cJSON_GetObjectItem(res, "serverInfo");
    assert(server_info != NULL);
    cJSON_Delete(root);

    mcp_response_free(&resp);
    printf("         [PASS] Handshake succeeded with valid protocolVersion.\n");
}

static void test_2_list_tools(void) {
    printf("[TEST 2] mcp_client_list_tools() -> MCP_SUCCESS & tools array...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_list_tools(&cfg, 1002, &resp);

    assert(st == MCP_SUCCESS);
    assert(resp.status == MCP_SUCCESS);
    assert(resp.http_status_code == 200);
    assert(!resp.is_rpc_error);
    assert(resp.result_text_payload != NULL);
    assert(resp.raw_json_response != NULL);

    /* Verify result has tools array with valid items */
    cJSON *root = cJSON_Parse(resp.raw_json_response);
    assert(root != NULL);
    cJSON *res = cJSON_GetObjectItem(root, "result");
    assert(res != NULL);
    cJSON *tools = cJSON_GetObjectItem(res, "tools");
    assert(tools != NULL && cJSON_IsArray(tools));
    int tool_count = cJSON_GetArraySize(tools);
    assert(tool_count >= 4);

    bool has_calc_duty = false;
    bool has_clear_shipment = false;
    bool has_vat_reconcile = false;

    for (int i = 0; i < tool_count; ++i) {
        cJSON *t = cJSON_GetArrayItem(tools, i);
        cJSON *n = cJSON_GetObjectItem(t, "name");
        if (n && n->valuestring) {
            if (strcmp(n->valuestring, "customs/calculate_duty") == 0) has_calc_duty = true;
            if (strcmp(n->valuestring, "customs/clear_shipment") == 0) has_clear_shipment = true;
            if (strcmp(n->valuestring, "vat/reconcile_invoice") == 0) has_vat_reconcile = true;
        }
    }
    assert(has_calc_duty);
    assert(has_clear_shipment);
    assert(has_vat_reconcile);

    cJSON *first_tool = cJSON_GetArrayItem(tools, 0);
    assert(first_tool != NULL);
    cJSON *name = cJSON_GetObjectItem(first_tool, "name");
    assert(name != NULL && name->valuestring != NULL);
    assert(strcmp(name->valuestring, "customs_duty_calculator") == 0);
    cJSON_Delete(root);

    mcp_response_free(&resp);
    printf("         [PASS] Discovered %d tools in array payload.\n", tool_count);
}

static void test_3_call_tool(void) {
    printf("[TEST 3] mcp_client_call_tool() -> MCP_SUCCESS & execution result...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_req_t req = {
        .tool_name = "customs_duty_calculator",
        .arguments_json = "{\"hs_code\":\"8471.30\",\"declared_value_aed\":100000.0}",
        .request_id = 1003
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_call_tool(&cfg, &req, &resp);

    assert(st == MCP_SUCCESS);
    assert(resp.status == MCP_SUCCESS);
    assert(resp.http_status_code == 200);
    assert(!resp.is_rpc_error);
    assert(resp.result_text_payload != NULL);
    assert(strstr(resp.result_text_payload, "AED 5000.00") != NULL);

    mcp_response_free(&resp);
    printf("         [PASS] Tool execution completed with payload: 'AED 5000.00'.\n");
}

static void test_4_unauthorized(void) {
    printf("[TEST 4] Missing Auth Header -> MCP_ERR_UNAUTHORIZED (HTTP 401)...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_AUTH_ENDPOINT,
        .bearer_token = NULL, /* Omitting token triggers 401 */
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_initialize(&cfg, 1004, &resp);

    assert(st == MCP_ERR_UNAUTHORIZED);
    assert(resp.status == MCP_ERR_UNAUTHORIZED);
    assert(resp.http_status_code == 401);

    mcp_response_free(&resp);
    printf("         [PASS] Verified HTTP 401 correctly mapped to MCP_ERR_UNAUTHORIZED.\n");
}

static void test_5_forbidden(void) {
    printf("[TEST 5] Policy Rejection -> MCP_ERR_FORBIDDEN (HTTP 403)...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_FORBIDDEN_ENDPOINT,
        .bearer_token = "valid-token-insufficient-acl",
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_initialize(&cfg, 1005, &resp);

    assert(st == MCP_ERR_FORBIDDEN);
    assert(resp.status == MCP_ERR_FORBIDDEN);
    assert(resp.http_status_code == 403);

    mcp_response_free(&resp);
    printf("         [PASS] Verified HTTP 403 correctly mapped to MCP_ERR_FORBIDDEN.\n");
}

static void test_6_dpworld_calculate_duty(void) {
    printf("[TEST 6] DP World Customs Duty & VAT Calculation (customs/calculate_duty)...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_req_t req = {
        .tool_name = "customs/calculate_duty",
        .arguments_json = "{\"hs_code\":\"8471.30.00\",\"cif_value_aed\":100000.0,\"origin_country\":\"DE\"}",
        .request_id = 1006
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_call_tool(&cfg, &req, &resp);

    assert(st == MCP_SUCCESS);
    assert(resp.status == MCP_SUCCESS);
    assert(resp.http_status_code == 200);
    assert(!resp.is_rpc_error);
    assert(resp.result_text_payload != NULL);
    assert(strstr(resp.result_text_payload, "DP World Customs Tariff Assessment") != NULL);
    assert(strstr(resp.result_text_payload, "Customs Duty (5%): AED 5000.00") != NULL);
    assert(strstr(resp.result_text_payload, "UAE VAT (5% on CIF+Duty): AED 5250.00") != NULL);
    assert(strstr(resp.result_text_payload, "Total Settlement: AED 10250.00") != NULL);

    mcp_response_free(&resp);
    printf("         [PASS] 5%% Customs Tariff & 5%% UAE VAT settled on CIF base.\n");
}

static void test_7_dpworld_clear_shipment_unapproved(void) {
    printf("[TEST 7] DP World Container Clearance > 50k without Token -> Gated...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_req_t req = {
        .tool_name = "customs/clear_shipment",
        .arguments_json = "{\"bill_of_lading\":\"BL-DPW-99214\",\"container_id\":\"MSCU8291043\","
                          "\"importer_code\":\"DXB-IMP-77102\",\"duty_amount_aed\":125000.00}",
        .request_id = 1007
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_call_tool(&cfg, &req, &resp);

    assert(st == MCP_ERR_RPC_FAULT);
    assert(resp.status == MCP_ERR_RPC_FAULT);
    assert(resp.is_rpc_error);
    assert(resp.http_status_code == 200);
    assert(resp.result_text_payload != NULL);
    assert(strstr(resp.result_text_payload, "exceeds AED 50,000 threshold and requires approval_token") != NULL);

    mcp_response_free(&resp);
    printf("         [PASS] High-value clearance correctly rejected without human approval token.\n");
}

static void test_8_dpworld_clear_shipment_approved(void) {
    printf("[TEST 8] DP World Container Clearance > 50k with Token -> Cleared...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_req_t req = {
        .tool_name = "customs/clear_shipment",
        .arguments_json = "{\"bill_of_lading\":\"BL-DPW-99214\",\"container_id\":\"MSCU8291043\","
                          "\"importer_code\":\"DXB-IMP-77102\",\"duty_amount_aed\":125000.00,"
                          "\"approval_token\":\"DUBAI-APPROVE-9941-X\"}",
        .request_id = 1008
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_call_tool(&cfg, &req, &resp);

    assert(st == MCP_SUCCESS);
    assert(resp.status == MCP_SUCCESS);
    assert(resp.http_status_code == 200);
    assert(!resp.is_rpc_error);
    assert(resp.result_text_payload != NULL);
    assert(strstr(resp.result_text_payload, "DP World Mirsal II Electronic Release Order Issued") != NULL);
    assert(strstr(resp.result_text_payload, "Status: CLEARED (Gate Pass Valid 72 Hours)") != NULL);
    assert(strstr(resp.result_text_payload, "RO-DPW-2026-") != NULL);

    mcp_response_free(&resp);
    printf("         [PASS] Valid single-use token triggered DP World Mirsal II Release Order.\n");
}

static void test_9_vat_reconcile_invoice(void) {
    printf("[TEST 9] FTA 5%% Statutory VAT Reconciliation (vat/reconcile_invoice)...\n");
    mcp_client_config_t cfg = {
        .endpoint_url = MCP_TEST_ENDPOINT,
        .bearer_token = NULL,
        .client_name = "qala-interop-test",
        .timeout_ms = 5000
    };

    mcp_tool_call_req_t req = {
        .tool_name = "vat/reconcile_invoice",
        .arguments_json = "{\"invoice_number\":\"INV-2026-9041\",\"tax_registration_number\":\"100234567800003\","
                          "\"net_amount_aed\":50000.00,\"vat_amount_aed\":2500.00}",
        .request_id = 1009
    };

    mcp_tool_call_resp_t resp;
    mcp_status_t st = mcp_client_call_tool(&cfg, &req, &resp);

    assert(st == MCP_SUCCESS);
    assert(resp.status == MCP_SUCCESS);
    assert(resp.http_status_code == 200);
    assert(!resp.is_rpc_error);
    assert(resp.result_text_payload != NULL);
    assert(strstr(resp.result_text_payload, "FTA VAT RECONCILED") != NULL);

    mcp_response_free(&resp);
    printf("         [PASS] FTA 5%% VAT reconciliation passed deterministic audit.\n");
}

int main(void) {
    printf("==================================================================\n");
    printf(" MCP 2026-07-28 INTEROPERABILITY & CONFORMANCE TEST SUITE\n");
    printf(" Target Server: %s\n", MCP_TEST_ENDPOINT);
    printf("==================================================================\n");

    if (mcp_client_global_init() != 0) {
        fprintf(stderr, "Fatal: mcp_client_global_init() failed\n");
        return 1;
    }

    test_1_initialize();
    test_2_list_tools();
    test_3_call_tool();
    test_4_unauthorized();
    test_5_forbidden();
    test_6_dpworld_calculate_duty();
    test_7_dpworld_clear_shipment_unapproved();
    test_8_dpworld_clear_shipment_approved();
    test_9_vat_reconcile_invoice();

    mcp_client_global_cleanup();

    printf("==================================================================\n");
    printf(" [SUCCESS] All 9 MCP Interoperability Tests Passed Deterministically!\n");
    printf("==================================================================\n");
    return 0;
}
