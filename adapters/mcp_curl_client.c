/* mcp_curl_client.c
 * Production C99 Driver for Stateless MCP 2026-07-28 over Streamable HTTP
 * Powered by libcurl (transport) + cJSON (JSON-RPC 2.0 serialization)
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <curl/curl.h>

#include "cJSON.h"
#include "mcp_curl_client.h"

/* Memory buffer for accumulating HTTP response body */
typedef struct {
    char  *data;
    size_t size;
} http_memory_buf_t;

#define MCP_MAX_RESPONSE_BYTES (16 * 1024 * 1024)

static size_t curl_write_memory_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    http_memory_buf_t *mem = (http_memory_buf_t *)userp;

    /* F-14 Fix: Cap response buffer at 16MB to prevent unbounded OOM */
    if (mem->size + realsize > MCP_MAX_RESPONSE_BYTES) {
        return 0;
    }

    char *ptr = (char *)realloc(mem->data, mem->size + realsize + 1);
    if (!ptr) {
        return 0; /* Out of memory */
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = '\0';

    return realsize;
}

int mcp_client_global_init(void) {
    return (curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) ? 0 : -1;
}

void mcp_client_global_cleanup(void) {
    curl_global_cleanup();
}

void mcp_response_free(mcp_tool_call_resp_t *resp) {
    if (!resp) return;
    if (resp->result_text_payload) {
        free(resp->result_text_payload);
        resp->result_text_payload = NULL;
    }
    if (resp->raw_json_response) {
        free(resp->raw_json_response);
        resp->raw_json_response = NULL;
    }
}

/* Internal routine: builds JSON-RPC 2.0 payload matching MCP 2026-07-28 spec */
static char *build_mcp_jsonrpc_body(
    const char *method,
    const char *tool_name,
    const char *arguments_json,
    const char *client_name,
    uint64_t    request_id
) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;

    cJSON_AddStringToObject(root, "jsonrpc", "2.0");
    /* AF-10 Fix: Mask request_id to safe 53-bit integer range to prevent double precision loss */
    cJSON_AddNumberToObject(root, "id", (double)(request_id & 0x1FFFFFFFFFFFFFULL));
    cJSON_AddStringToObject(root, "method", method);

    cJSON *params = cJSON_CreateObject();
    if (params) {
        const char *cname = (client_name && client_name[0] != '\0')
                            ? client_name : "qala-native-c99";

        if (strcmp(method, "initialize") == 0) {
            cJSON_AddStringToObject(params, "protocolVersion", "2026-07-28");
            cJSON *cinfo = cJSON_CreateObject();
            if (cinfo) {
                cJSON_AddStringToObject(cinfo, "name", cname);
                cJSON_AddStringToObject(cinfo, "version", "1.0.0");
                cJSON_AddItemToObject(params, "clientInfo", cinfo);
            }
            cJSON *caps = cJSON_CreateObject();
            if (caps) {
                cJSON *roots = cJSON_CreateObject();
                if (roots) {
                    cJSON_AddBoolToObject(roots, "listChanged", false);
                    cJSON_AddItemToObject(caps, "roots", roots);
                }
                cJSON_AddItemToObject(params, "capabilities", caps);
            }
        } else if (strcmp(method, "tools/call") == 0) {
            if (tool_name) {
                cJSON_AddStringToObject(params, "name", tool_name);
            }
            if (arguments_json && arguments_json[0] != '\0') {
                cJSON *args_parsed = cJSON_Parse(arguments_json);
                if (args_parsed) {
                    cJSON_AddItemToObject(params, "arguments", args_parsed);
                } else {
                    /* F-14 Fix: Reject unparseable arguments rather than silently dropping */
                    cJSON_Delete(root);
                    return NULL;
                }
            } else {
                cJSON_AddObjectToObject(params, "arguments");
            }
        }

        /* MCP 2026-07-28 protocol metadata placed inside params._meta */
        cJSON *meta = cJSON_CreateObject();
        if (meta) {
            cJSON_AddStringToObject(meta, "io.modelcontextprotocol/protocolVersion", "2026-07-28");
            cJSON *client_caps = cJSON_CreateObject();
            if (client_caps) {
                cJSON *roots = cJSON_CreateObject();
                if (roots) {
                    cJSON_AddBoolToObject(roots, "listChanged", false);
                    cJSON_AddItemToObject(client_caps, "roots", roots);
                }
                cJSON_AddItemToObject(meta, "io.modelcontextprotocol/clientCapabilities", client_caps);
            }
            cJSON_AddItemToObject(params, "_meta", meta);
        }

        cJSON_AddItemToObject(root, "params", params);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return json_str;
}

/* Extracts JSON payload from either pure JSON or text/event-stream (SSE) */
static char *extract_mcp_payload(const char *raw_body) {
    if (!raw_body) return NULL;

    const char *p = raw_body;
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;

    /* Fast path: pure JSON */
    if (*p == '{' || *p == '[') {
        return strdup(p);
    }

    /* SSE Stream framing: accumulate 'data:' lines */
    size_t cap = 2048, len = 0;
    char *acc = (char *)malloc(cap);
    if (!acc) return NULL;
    acc[0] = '\0';

    char *candidate_json = NULL;
    const char *line = p;

    while (*line) {
        const char *next_line = line;
        while (*next_line && *next_line != '\n') next_line++;
        size_t line_len = (size_t)(next_line - line);
        if (line_len > 0 && line[line_len - 1] == '\r') line_len--;

        if (line_len == 0) {
            /* Empty line denotes end of an SSE frame event */
            if (len > 0) {
                cJSON *test = cJSON_Parse(acc);
                if (test) {
                    bool is_pref = (cJSON_GetObjectItemCaseSensitive(test, "result") != NULL ||
                                    cJSON_GetObjectItemCaseSensitive(test, "error") != NULL ||
                                    cJSON_GetObjectItemCaseSensitive(test, "jsonrpc") != NULL);
                    if (is_pref || !candidate_json) {
                        if (candidate_json) free(candidate_json);
                        candidate_json = strdup(acc);
                    }
                    cJSON_Delete(test);
                }
                len = 0;
                acc[0] = '\0';
            }
        } else if (line_len >= 5 && strncmp(line, "data:", 5) == 0) {
            const char *val = line + 5;
            size_t val_len = line_len - 5;
            if (val_len > 0 && *val == ' ') { val++; val_len--; }

            if (len + val_len + 2 > cap) {
                size_t new_cap = (len + val_len + 2 > cap * 2) ? (len + val_len + 2048) : (cap * 2);
                char *new_acc = (char *)realloc(acc, new_cap);
                if (!new_acc) {
                    free(acc);
                    if (candidate_json) free(candidate_json);
                    return NULL;
                }
                acc = new_acc;
                cap = new_cap;
            }
            if (len > 0) acc[len++] = '\n';
            memcpy(acc + len, val, val_len);
            len += val_len;
            acc[len] = '\0';
        }
        line = *next_line ? next_line + 1 : next_line;
    }

    if (len > 0) {
        cJSON *test = cJSON_Parse(acc);
        if (test) {
            if (candidate_json) free(candidate_json);
            candidate_json = strdup(acc);
            cJSON_Delete(test);
        }
    }
    free(acc);
    return candidate_json ? candidate_json : strdup(raw_body);
}

/* Parse JSON-RPC response from MCP Server */
static void parse_mcp_response(
    const char           *response_body,
    mcp_tool_call_resp_t *out_resp
) {
    if (!response_body || !out_resp) return;

    char *clean_payload = extract_mcp_payload(response_body);
    if (!clean_payload) {
        out_resp->status = MCP_ERR_JSON_PARSE;
        return;
    }

    out_resp->raw_json_response = clean_payload;

    cJSON *root = cJSON_Parse(clean_payload);
    if (!root) {
        out_resp->status = MCP_ERR_JSON_PARSE;
        return;
    }

    /* Check for JSON-RPC error */
    cJSON *err_obj = cJSON_GetObjectItemCaseSensitive(root, "error");
    if (err_obj && cJSON_IsObject(err_obj)) {
        out_resp->is_rpc_error = true;
        out_resp->status = MCP_ERR_RPC_FAULT;

        cJSON *code = cJSON_GetObjectItemCaseSensitive(err_obj, "code");
        if (code && cJSON_IsNumber(code)) {
            out_resp->rpc_error_code = code->valueint;
        }

        cJSON *msg = cJSON_GetObjectItemCaseSensitive(err_obj, "message");
        if (msg && cJSON_IsString(msg) && msg->valuestring) {
            snprintf(out_resp->rpc_error_message, sizeof(out_resp->rpc_error_message), "%s", msg->valuestring);
        }
        cJSON_Delete(root);
        return;
    }

    /* Extract content from "result" */
    cJSON *res_obj = cJSON_GetObjectItemCaseSensitive(root, "result");
    if (res_obj) {
        cJSON *content_arr = cJSON_GetObjectItemCaseSensitive(res_obj, "content");
        if (content_arr && cJSON_IsArray(content_arr)) {
            cJSON *first_elem = cJSON_GetArrayItem(content_arr, 0);
            if (first_elem) {
                cJSON *text_node = cJSON_GetObjectItemCaseSensitive(first_elem, "text");
                if (text_node && cJSON_IsString(text_node) && text_node->valuestring) {
                    out_resp->result_text_payload = strdup(text_node->valuestring);
                }
            }
        }
        if (!out_resp->result_text_payload) {
            /* Fallback to printing unformatted result object */
            out_resp->result_text_payload = cJSON_PrintUnformatted(res_obj);
        }
        /* F-14 Fix: Check isError flag in tools/call */
        cJSON *is_err = cJSON_GetObjectItemCaseSensitive(res_obj, "isError");
        if (is_err && cJSON_IsTrue(is_err)) {
            out_resp->status = MCP_ERR_RPC_FAULT;
            out_resp->is_rpc_error = true;
        } else {
            out_resp->status = MCP_SUCCESS;
        }
    } else {
        /* F-14 Fix: Missing both result and error is an invalid JSON-RPC response */
        out_resp->status = MCP_ERR_JSON_PARSE;
    }

    cJSON_Delete(root);
}

/* Execute HTTP POST request using libcurl */
static mcp_status_t execute_mcp_http_post(
    const mcp_client_config_t *config,
    const char                *mcp_method_header,
    const char                *post_body,
    mcp_tool_call_resp_t      *out_resp
) {
    if (!config || !config->endpoint_url || !post_body || !out_resp) {
        return MCP_ERR_INVALID_PARAM;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        out_resp->status = MCP_ERR_CONNECT;
        return MCP_ERR_CONNECT;
    }

    http_memory_buf_t chunk = { .data = malloc(1), .size = 0 };
    if (!chunk.data) {
        curl_easy_cleanup(curl);
        out_resp->status = MCP_ERR_CONNECT;
        return MCP_ERR_CONNECT;
    }
    chunk.data[0] = '\0';

    /* Build HTTP headers adhering to Stateless MCP 2026-07-28 */
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json, text/event-stream");
    headers = curl_slist_append(headers, "MCP-Protocol-Version: 2026-07-28");

    const char *cname = (config->client_name && config->client_name[0] != '\0')
                        ? config->client_name : "qala-native-c99";
    char name_hdr[128];
    snprintf(name_hdr, sizeof(name_hdr), "Mcp-Name: %s", cname);
    headers = curl_slist_append(headers, name_hdr);

    char method_hdr[128];
    snprintf(method_hdr, sizeof(method_hdr), "Mcp-Method: %s", mcp_method_header);
    headers = curl_slist_append(headers, method_hdr);

    if (config->bearer_token && config->bearer_token[0] != '\0') {
        /* F-14 Fix: Reject CRLF in bearer token and dynamically allocate header buffer */
        if (strpbrk(config->bearer_token, "\r\n") != NULL) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            free(chunk.data);
            out_resp->status = MCP_ERR_INVALID_PARAM;
            return MCP_ERR_INVALID_PARAM;
        }
        size_t b_len = strlen(config->bearer_token);
        char *auth_hdr = (char *)malloc(b_len + 32);
        if (auth_hdr) {
            snprintf(auth_hdr, b_len + 32, "Authorization: Bearer %s", config->bearer_token);
            headers = curl_slist_append(headers, auth_hdr);
            free(auth_hdr);
        }
    }

    curl_easy_setopt(curl, CURLOPT_URL, config->endpoint_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_memory_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, (long)(config->timeout_ms ? config->timeout_ms : 5000));
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Qala-StatelessMCP-Client/1.0 (Dubai 2027)");

    CURLcode res = curl_easy_perform(curl);
    mcp_status_t final_status = MCP_SUCCESS;

    if (res != CURLE_OK) {
        if (res == CURLE_OPERATION_TIMEDOUT) {
            final_status = MCP_ERR_TIMEOUT;
        } else {
            final_status = MCP_ERR_CONNECT;
        }
        out_resp->status = final_status;
    } else {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &out_resp->http_status_code);

        /* F-14 Fix: Parse JSON-RPC payload if available even on HTTP errors to capture error message/code */
        if (chunk.size > 0 && chunk.data[0] != '\0') {
            parse_mcp_response(chunk.data, out_resp);
        }

        if (out_resp->http_status_code == 401) {
            final_status = MCP_ERR_UNAUTHORIZED;
            out_resp->status = final_status;
        } else if (out_resp->http_status_code == 403) {
            final_status = MCP_ERR_FORBIDDEN;
            out_resp->status = final_status;
        } else if (out_resp->http_status_code >= 400) {
            final_status = MCP_ERR_HTTP_STATUS;
            out_resp->status = final_status;
        } else {
            final_status = out_resp->status;
        }
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    free(chunk.data);

    return final_status;
}

mcp_status_t mcp_client_initialize(
    const mcp_client_config_t *config,
    uint64_t                   request_id,
    mcp_tool_call_resp_t      *out_resp
) {
    if (!out_resp) return MCP_ERR_INVALID_PARAM;
    memset(out_resp, 0, sizeof(*out_resp));

    if (!config) {
        out_resp->status = MCP_ERR_INVALID_PARAM;
        return MCP_ERR_INVALID_PARAM;
    }

    char *post_body = build_mcp_jsonrpc_body("initialize", NULL, NULL, config->client_name, request_id);
    if (!post_body) {
        out_resp->status = MCP_ERR_JSON_PARSE;
        return MCP_ERR_JSON_PARSE;
    }

    mcp_status_t st = execute_mcp_http_post(config, "initialize", post_body, out_resp);
    free(post_body);
    return st;
}

mcp_status_t mcp_client_call_tool(
    const mcp_client_config_t *config,
    const mcp_tool_call_req_t *req,
    mcp_tool_call_resp_t      *out_resp
) {
    if (!out_resp) return MCP_ERR_INVALID_PARAM;
    memset(out_resp, 0, sizeof(*out_resp));

    if (!config || !req || !req->tool_name) {
        out_resp->status = MCP_ERR_INVALID_PARAM;
        return MCP_ERR_INVALID_PARAM;
    }

    char *post_body = build_mcp_jsonrpc_body("tools/call", req->tool_name, req->arguments_json, config->client_name, req->request_id);
    if (!post_body) {
        out_resp->status = MCP_ERR_JSON_PARSE;
        return MCP_ERR_JSON_PARSE;
    }

    mcp_status_t st = execute_mcp_http_post(config, "tools/call", post_body, out_resp);
    free(post_body);
    return st;
}

mcp_status_t mcp_client_list_tools(
    const mcp_client_config_t *config,
    uint64_t                   request_id,
    mcp_tool_call_resp_t      *out_resp
) {
    if (!out_resp) return MCP_ERR_INVALID_PARAM;
    memset(out_resp, 0, sizeof(*out_resp));

    if (!config) {
        out_resp->status = MCP_ERR_INVALID_PARAM;
        return MCP_ERR_INVALID_PARAM;
    }

    char *post_body = build_mcp_jsonrpc_body("tools/list", NULL, NULL, config->client_name, request_id);
    if (!post_body) {
        out_resp->status = MCP_ERR_JSON_PARSE;
        return MCP_ERR_JSON_PARSE;
    }

    mcp_status_t st = execute_mcp_http_post(config, "tools/list", post_body, out_resp);
    free(post_body);
    return st;
}

#ifdef MCP_CURL_STANDALONE_TEST
#include <assert.h>

int main(void) {
    /* Test 1: Verify tools/call JSON-RPC body structure (params._meta and clientCapabilities) */
    char *body1 = build_mcp_jsonrpc_body("tools/call", "erp_customs_settlement", "{\"batch_id\":882,\"aed\":125000}", "qala-test-suite", 42);
    assert(body1 != NULL);
    assert(strstr(body1, "\"jsonrpc\":\"2.0\"") != NULL);
    assert(strstr(body1, "\"id\":42") != NULL);
    assert(strstr(body1, "\"method\":\"tools/call\"") != NULL);
    assert(strstr(body1, "\"name\":\"erp_customs_settlement\"") != NULL);
    assert(strstr(body1, "\"batch_id\":882") != NULL);

    /* Verify _meta is inside params, NOT at root */
    cJSON *parsed1 = cJSON_Parse(body1);
    assert(parsed1 != NULL);
    assert(cJSON_GetObjectItem(parsed1, "_meta") == NULL);
    cJSON *p1 = cJSON_GetObjectItem(parsed1, "params");
    assert(p1 != NULL);
    cJSON *meta1 = cJSON_GetObjectItem(p1, "_meta");
    assert(meta1 != NULL);
    assert(cJSON_GetObjectItem(meta1, "io.modelcontextprotocol/protocolVersion") != NULL);
    assert(cJSON_GetObjectItem(meta1, "io.modelcontextprotocol/clientCapabilities") != NULL);
    cJSON_Delete(parsed1);
    free(body1);

    /* Test 2: Verify tools/list JSON-RPC body schema (no dummy arguments or name fields) */
    char *body2 = build_mcp_jsonrpc_body("tools/list", NULL, NULL, "qala-test-suite", 43);
    assert(body2 != NULL);
    cJSON *parsed2 = cJSON_Parse(body2);
    assert(parsed2 != NULL);
    cJSON *p2 = cJSON_GetObjectItem(parsed2, "params");
    assert(p2 != NULL);
    assert(cJSON_GetObjectItem(p2, "name") == NULL);
    assert(cJSON_GetObjectItem(p2, "arguments") == NULL);
    assert(cJSON_GetObjectItem(p2, "_meta") != NULL);
    cJSON_Delete(parsed2);
    free(body2);

    /* Test 3: Verify initialize Handshake payload structure */
    char *body3 = build_mcp_jsonrpc_body("initialize", NULL, NULL, "qala-enterprise", 44);
    assert(body3 != NULL);
    cJSON *parsed3 = cJSON_Parse(body3);
    assert(parsed3 != NULL);
    cJSON *p3 = cJSON_GetObjectItem(parsed3, "params");
    assert(p3 != NULL);
    cJSON *proto = cJSON_GetObjectItem(p3, "protocolVersion");
    assert(proto != NULL && strcmp(proto->valuestring, "2026-07-28") == 0);
    cJSON *cinfo = cJSON_GetObjectItem(p3, "clientInfo");
    assert(cinfo != NULL);
    cJSON *cname = cJSON_GetObjectItem(cinfo, "name");
    assert(cname != NULL && strcmp(cname->valuestring, "qala-enterprise") == 0);
    cJSON *caps = cJSON_GetObjectItem(p3, "capabilities");
    assert(caps != NULL && cJSON_GetObjectItem(caps, "roots") != NULL);
    cJSON_Delete(parsed3);
    free(body3);

    /* Test 4: Verify pure JSON response parsing */
    const char *mock_success_json =
        "{\"jsonrpc\":\"2.0\",\"id\":42,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"Settlement #882 executed for AED 125,000.00\"}]}}";
    mcp_tool_call_resp_t r4;
    memset(&r4, 0, sizeof(r4));
    parse_mcp_response(mock_success_json, &r4);
    assert(r4.status == MCP_SUCCESS);
    assert(r4.is_rpc_error == false);
    assert(r4.result_text_payload != NULL);
    assert(strstr(r4.result_text_payload, "AED 125,000.00") != NULL);
    mcp_response_free(&r4);

    /* Test 5: Verify SSE single frame parsing (event: message\ndata: {...}\n\n) */
    const char *mock_sse_frame =
        "event: message\r\n"
        "data: {\"jsonrpc\":\"2.0\",\"id\":45,\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"SSE Settlement #883 Verified\"}]}}\r\n\r\n";
    mcp_tool_call_resp_t r5;
    memset(&r5, 0, sizeof(r5));
    parse_mcp_response(mock_sse_frame, &r5);
    assert(r5.status == MCP_SUCCESS);
    assert(r5.is_rpc_error == false);
    assert(r5.result_text_payload != NULL);
    assert(strstr(r5.result_text_payload, "SSE Settlement #883 Verified") != NULL);
    mcp_response_free(&r5);

    /* Test 6: Verify SSE multi-line chunk assembly */
    const char *mock_sse_multiline =
        "event: message\n"
        "data: {\"jsonrpc\":\"2.0\",\n"
        "data: \"id\":46,\n"
        "data: \"result\":{\"content\":[{\"type\":\"text\",\"text\":\"Multi-chunk SSE payload\"}]}}\n\n";
    mcp_tool_call_resp_t r6;
    memset(&r6, 0, sizeof(r6));
    parse_mcp_response(mock_sse_multiline, &r6);
    assert(r6.status == MCP_SUCCESS);
    assert(r6.result_text_payload != NULL);
    assert(strstr(r6.result_text_payload, "Multi-chunk SSE payload") != NULL);
    mcp_response_free(&r6);

    /* Test 7: Verify SSE stream with leading ping/heartbeat frames */
    const char *mock_sse_with_pings =
        ": heartbeat keepalive\r\n\r\n"
        "event: ping\r\n"
        "data: {}\r\n\r\n"
        "event: message\r\n"
        "data: {\"jsonrpc\":\"2.0\",\"id\":47,\"result\":{\"tools\":[{\"name\":\"customs_query\",\"description\":\"Check tariff rates\"}]}}\r\n\r\n";
    mcp_tool_call_resp_t r7;
    memset(&r7, 0, sizeof(r7));
    parse_mcp_response(mock_sse_with_pings, &r7);
    assert(r7.status == MCP_SUCCESS);
    assert(r7.result_text_payload != NULL);
    assert(strstr(r7.result_text_payload, "customs_query") != NULL);
    mcp_response_free(&r7);

    /* Test 8: Verify JSON-RPC error extraction from SSE stream */
    const char *mock_sse_error =
        "event: message\r\n"
        "data: {\"jsonrpc\":\"2.0\",\"id\":48,\"error\":{\"code\":-32602,\"message\":\"Invalid params: amount exceeds limit\"}}\r\n\r\n";
    mcp_tool_call_resp_t r8;
    memset(&r8, 0, sizeof(r8));
    parse_mcp_response(mock_sse_error, &r8);
    assert(r8.status == MCP_ERR_RPC_FAULT);
    assert(r8.is_rpc_error == true);
    assert(r8.rpc_error_code == -32602);
    assert(strstr(r8.rpc_error_message, "exceeds limit") != NULL);
    mcp_response_free(&r8);

    /* Test 9: Verify honest HTTP error classification & network transport failure */
    mcp_client_global_init();

    mcp_client_config_t offline_cfg = {
        .endpoint_url = "http://127.0.0.1:59999/mcp",
        .bearer_token = NULL,
        .client_name = "qala-test-suite",
        .timeout_ms = 500
    };
    mcp_tool_call_req_t req = {
        .tool_name = "test_tool",
        .arguments_json = "{}",
        .request_id = 1
    };
    mcp_tool_call_resp_t r9;
    mcp_status_t st = mcp_client_call_tool(&offline_cfg, &req, &r9);
    assert(st == MCP_ERR_CONNECT);
    assert(r9.status == MCP_ERR_CONNECT || r9.status == MCP_ERR_NETWORK);
    mcp_response_free(&r9);

    /* Verify initialize call to offline port also returns connection error */
    mcp_tool_call_resp_t r10;
    mcp_status_t st_init = mcp_client_initialize(&offline_cfg, 10, &r10);
    assert(st_init == MCP_ERR_CONNECT);
    assert(r10.status == MCP_ERR_CONNECT);
    mcp_response_free(&r10);

    mcp_client_global_cleanup();

    printf("[PASS] All libcurl + cJSON Stateless MCP 2026-07-28 client tests passed successfully! (9/9 suites)\n");
    return 0;
}
#endif
