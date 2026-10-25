/* mcp_curl_client.h - Production libcurl + cJSON Driver for Stateless MCP 2026-07-28
 * Target: Universal Enterprise Connectivity (SAP, ERPNext, Dubai Customs APIs)
 * Protocol: Streamable HTTP over JSON-RPC 2.0 with L7 Header Routing
 */
#ifndef MCP_CURL_CLIENT_H
#define MCP_CURL_CLIENT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MCP_SUCCESS            = 0,
    MCP_ERR_CONNECT        = 1,
    MCP_ERR_NETWORK        = 1,   /* Alias for transport/connection failure */
    MCP_ERR_HTTP_STATUS    = 2,   /* Generic HTTP 4xx/5xx status */
    MCP_ERR_AUTH_BLOCKED   = 3,   /* Generic auth failure fallback */
    MCP_ERR_UNAUTHORIZED   = 4,   /* HTTP 401: Missing or invalid token */
    MCP_ERR_FORBIDDEN      = 5,   /* HTTP 403: Policy gate or ACL denied */
    MCP_ERR_JSON_PARSE     = 6,   /* Malformed JSON or SSE payload */
    MCP_ERR_PARSE          = 6,   /* Alias for parse failure */
    MCP_ERR_RPC_FAULT      = 7,   /* MCP / JSON-RPC error returned by server */
    MCP_ERR_TIMEOUT        = 8,   /* Network or socket timeout */
    MCP_ERR_INVALID_PARAM  = 9    /* Missing or invalid input arguments */
} mcp_status_t;

/* Client configuration for MCP connection */
typedef struct {
    const char *endpoint_url;     /* e.g., "http://127.0.0.1:8765/mcp" or "https://api.dubaicustoms.ae/mcp" */
    const char *bearer_token;     /* Optional auth token (never echoed to logs) */
    const char *client_name;      /* Optional client name (default: "qala-native-c99") */
    uint32_t    timeout_ms;       /* Timeout in milliseconds (default 5000) */
} mcp_client_config_t;

/* Tool execution request context */
typedef struct {
    const char *tool_name;        /* e.g., "erp_customs_settlement", "query_tariffs" */
    const char *arguments_json;   /* JSON object string, e.g. "{\"batch_id\":882}" */
    uint64_t    request_id;       /* Monotonic JSON-RPC request identifier */
} mcp_tool_call_req_t;

/* Response envelope */
typedef struct {
    mcp_status_t status;
    long         http_status_code;
    bool         is_rpc_error;
    int32_t      rpc_error_code;
    char         rpc_error_message[256];
    char        *result_text_payload;  /* Caller must free with mcp_response_free() */
    char        *raw_json_response;    /* Caller must free with mcp_response_free() */
} mcp_tool_call_resp_t;

/* Lifecycle functions */
int mcp_client_global_init(void);
void mcp_client_global_cleanup(void);

/* Core execution functions */
mcp_status_t mcp_client_initialize(
    const mcp_client_config_t *config,
    uint64_t                   request_id,
    mcp_tool_call_resp_t      *out_resp
);

mcp_status_t mcp_client_call_tool(
    const mcp_client_config_t *config,
    const mcp_tool_call_req_t *req,
    mcp_tool_call_resp_t      *out_resp
);

mcp_status_t mcp_client_list_tools(
    const mcp_client_config_t *config,
    uint64_t                   request_id,
    mcp_tool_call_resp_t      *out_resp
);

/* Frees dynamic buffers inside response envelope */
void mcp_response_free(mcp_tool_call_resp_t *resp);

#ifdef __cplusplus
}
#endif

#endif /* MCP_CURL_CLIENT_H */
