# Makefile - Qala Sovereign Business AI Agent
# Track: Best Business AI Agent | Dubai AI Agents Championship 2026-2027

CC ?= cc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2 -Iadapters
LDFLAGS ?= -lm -lcurl

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LIB_EXT = dylib
    SHARED_FLAGS = -dynamiclib
else
    LIB_EXT = so
    SHARED_FLAGS = -shared
endif

BUILD_DIR = build
BIN = $(BUILD_DIR)/qala_agent
LIB = $(BUILD_DIR)/libqala.$(LIB_EXT)

all: $(BUILD_DIR) $(BIN) $(LIB) $(BUILD_DIR)/test_qala $(BUILD_DIR)/test_grounding $(BUILD_DIR)/test_mcp_client $(BUILD_DIR)/test_mcp_interop

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN): src/qala_agent.c adapters/qala_mcp_policy_adapter.c adapters/grounding_shield.c adapters/mcp_curl_client.c adapters/cJSON.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(LIB): adapters/qala_mcp_policy_adapter.c adapters/grounding_shield.c adapters/mcp_curl_client.c adapters/cJSON.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -fPIC $(SHARED_FLAGS) $^ $(LDFLAGS) -o $(LIB)

$(BUILD_DIR)/test_qala: adapters/qala_mcp_policy_adapter.c adapters/grounding_shield.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DQALA_STANDALONE_TEST $^ -lm -o $@

$(BUILD_DIR)/test_grounding: adapters/grounding_shield.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DGROUNDING_STANDALONE_TEST $^ -lm -o $@

$(BUILD_DIR)/test_mcp_client: adapters/mcp_curl_client.c adapters/cJSON.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DMCP_CURL_STANDALONE_TEST $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_mcp_interop: tests/test_mcp_interop.c adapters/mcp_curl_client.c adapters/cJSON.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

test: all
	@echo "=================================================================="
	@echo " RUNNING QALA NATIVE CLI ACCEPTANCE BATTERY"
	@echo "=================================================================="
	./$(BIN) --test
	@echo "=================================================================="
	@echo " RUNNING QALA POLICY GATE & GROUNDING SHIELD TEST SUITE"
	@echo "=================================================================="
	./$(BUILD_DIR)/test_qala
	./$(BUILD_DIR)/test_grounding
	./$(BUILD_DIR)/test_mcp_client
	@echo "=================================================================="
	@echo " RUNNING STATELESS MCP INTEROP TEST SUITE"
	@echo "=================================================================="
	@python3 tests/mcp_mock_server.py & \
	SERVER_PID=$$!; \
	sleep 0.5; \
	./$(BUILD_DIR)/test_mcp_interop; \
	STATUS=$$?; \
	kill $$SERVER_PID 2>/dev/null || true; \
	wait $$SERVER_PID 2>/dev/null || true; \
	exit $$STATUS
	@echo "[SUCCESS] All Qala Agent verification suites passed deterministically!"

clean:
	rm -rf $(BUILD_DIR)
	rm -f qala-audit.jsonl *.audit.jsonl qala-tokens.jsonl *.tokens.jsonl *.jsonl data/*.jsonl

.PHONY: all test clean
