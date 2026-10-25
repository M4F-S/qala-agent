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
    assert(tool_count >= 1);

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

    mcp_client_global_cleanup();

    printf("==================================================================\n");
    printf(" [SUCCESS] All 5 MCP Interoperability Tests Passed Deterministically!\n");
    printf("==================================================================\n");
    return 0;
}
