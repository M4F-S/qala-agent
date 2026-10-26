# Qala — implementation plan (repo-local)

**Owner:** the implementation agent assigned to `qala-agent`. This file is the complete brief; you do not need any external document.
**Category:** Best Business AI Agent.
**Rounds:** 1 = blocking, do now. 2 = memory. 3 = RAG. Do not start a later round before the earlier one is green.

---

## Ground rules

- **You own this repo only.** Never edit `nibras-agent`, `taryaq-agent`, or the umbrella `dubai-ai-championship-2027`.
- **Vendored files you must never edit** (other repos copy them; you are the canonical source):
  `adapters/qala_mcp_policy_adapter.c`, `adapters/qala_adapters.h`, `adapters/grounding_shield.c/.h`, `adapters/mcp_curl_client.c/.h`, `adapters/cJSON.*`.
- **Green on macOS is not evidence.** `-Wformat-truncation` is a GCC diagnostic; clang may not emit it. Every acceptance run must include:
  `docker run --rm -v "$PWD":/w -w /w gcc:13 sh -c "apt-get update -qq && apt-get install -y -qq libcurl4-openssl-dev >/dev/null && make clean && make test"`
- **Definition of done** (every round): `make clean && make test` exits 0 **twice consecutively**, the container run passes, `git status` shows no `build/`, `*.o`, `*.so`, `*.jsonl` or `__pycache__`.
- **Claims policy.** Every README claim needs a command that proves it. Banned until proven: `tamper-evident`, `cryptographic chaining`, `AF_UNIX`, `Laya 322M`, `Jais 3`, `< 0.02 ms`, `100 % on-device`, `zero dependencies`.

---

# Round 1 — make it runnable and honest (~4 h)

## Q-1 · Build fix (blocking, 30 min)

`adapters/qala_mcp_policy_adapter.c` fails under the repo's own `-O2 -Werror` at two sites.

`:356` — replace:
```c
    } else {
        snprintf(full, sizeof(full), "%s/%s", ws_root, input_path);
    }
```
with:
```c
    } else {
        size_t a = strlen(ws_root), b = strlen(input_path);
        if (a + 1 + b >= sizeof(full)) return false;   /* fail-closed: caller denies */
        memcpy(full, ws_root, a);
        full[a] = '/';
        memcpy(full + a + 1, input_path, b + 1);
    }
```

`:381` — same shape:
```c
            } else {
                size_t ra = strlen(resolved), rb = strlen(bname);
                if (ra + 1 + rb >= sizeof(combined)) return false;
                memcpy(combined, resolved, ra);
                combined[ra] = '/';
                memcpy(combined + ra + 1, bname, rb + 1);
            }
```

`return false` is the correct direction: `resolve_canonical_path()` false reaches `if (!resolve_canonical_path(...)) return true; /* Fail-closed */`, so an over-long path is **denied**, not allowed.

**Accept:** `make clean && make all` exit 0; `make test` exit 0 with 18 × `[PASS]`.

## Q-2 · The token ledger must survive `make clean` (45 min)

Your `Makefile` clean target deletes `qala-tokens.jsonl` and `*.jsonl`. That is the single-use ledger — deleting it re-arms every spent token.

1. Narrow `clean` to `build/` and audit logs only. Never `qala-tokens.jsonl`, never a blanket `*.jsonl`.
2. In `src/qala_agent.c`, resolve `QALA_TOKEN_STORE_PATH` (already honoured by `qala_get_token_store_path()`), else `~/.qala/tokens.jsonl`; `mkdir -p` the parent at startup; call `qala_set_token_store_path()` with the absolute path.
3. Add `qala-tokens.jsonl` and `*.tokens.jsonl` to `.gitignore`.
4. Keep the existing self-test isolation at `src/qala_agent.c:220`.
5. In `qala_adapters.h`, next to the setter, document: "callers should call `qala_set_token_store_path()` at startup; otherwise the ledger is written relative to the working directory."

**Accept:** run the binary twice — the second run still rejects the replayed token **and** the ledger file is present after `make clean`. Both, not one.

## Q-3 · Stop calling it landlock (15 min)

The verdict string `"landlock_fs_denied"` (`:539`, asserted at `:767` and `:856`) names a kernel feature you do not use; enforcement is `realpath` + prefix containment. Rename to `"workspace_containment_denied"` and update the assertions. Naming only — the mechanism is sound (every out-of-workspace path is denied; verified).

## Q-4 · Claims table (45 min)

Rewrite `README.md` so each claim has a proof command. Keep: deterministic C99 gate, AED 50,000 threshold + mutation gate, single-use scope-bound tokens (drop "cryptographic" — they are literals), path containment, stateless MCP client, grounding shield. Reword "sub-millisecond cold start" as function latency, not process start.

**Accept:** `grep -rniE "tamper-evident|cryptographic chaining|AF_UNIX|Laya 322M|Jais 3|< ?0\.02 ?ms" README.md` → no hits.

## Q-5 · Licence + attribution (30 min)

Add `LICENSE` (Apache-2.0), `NOTICE`:

```
This product includes software derived from:
  - Belya (C99 autonomous SE core)   — © Mohamed Fathy — <repo URL>
  - gomaa (agent memory OS, optional) — © Mohamed Fathy — <repo URL>
Components: adapters/qala_mcp_policy_adapter.c, src/qala_agent.c,
adapters/qala_injection_shield.py
```
and a README "Built on" section: what is new (policy gate rules, sandbox, MCP transport) vs reused (Belya engine, gomaa optional).

## Q-6 · CI + one command (30 min)

`.github/workflows/ci.yml`: `ubuntu-latest`, install `gcc make libcurl4-openssl-dev`, run `make clean && make test`. Add `make verify` (clean + all + test + banner).

## Q-7 · Mirror script and personal data (30 min)

`sync_mirror.sh` contains a personal Google Drive path and a Gmail address, and runs `git add .` plus `rsync --delete`.
- `GDRIVE_ROOT="${GDRIVE_ROOT:-…}"`; remove the email from `AGENT_INSTRUCTIONS.md`.
- Replace `git add .` with explicit paths.
- Abort if staged files match `build/|*.o|*.so|*.jsonl|__pycache__`.
- Drop `--delete` (or gate it behind `--force`).

## Q-8 · gomaa guard (30 min)

`adapters/qala_injection_shield.py` searches `~/.gemini/antigravity/scratch/mnemosyne`. Replace with `GOMAA_HOME` (default `~/gomaa`); guard `tests/test_qala_injection_defense.py` with `@unittest.skipUnless(GOMAA_AVAILABLE, "optional: gomaa Memory OS not installed (set GOMAA_HOME)")`; add an "Optional integration: gomaa" README section.

**Accept:** `GOMAA_HOME=/nonexistent make test` exits 0 with skips reported.

---

# Round 2 — case memory (2–3 days, after Round 1)

Qala's "memory" is **decision history**, not personal memory. The audit ledger already records every tool call and verdict; make it useful.

- [x] Index the audit ledger with SQLite FTS5 (`libsqlite3` is already installed by your CI; add it to `LDFLAGS`).
- [x] Answer: "has this vendor/amount/path been approved before, by whom, and did it go wrong?" — returned as a ranked list of prior cases.
- [x] Surface it in `qala_agent.c` as a `history <query>` command; every answer cites the audit line's `timestamp_utc`.
- [x] Test: three cases, one approval, one rejection, one replay attempt → query returns the right two.

**Do not** add embeddings or gomaa to the runtime path in this round.

---

# Round 3 — policy RAG (5–8 days, after Round 2)

- [ ] Corpus: DESC ISR v3.1, Dubai Customs tariff schedule, the AED 50,000 policy text — as small, versioned files with a SHA-256 manifest.
- [ ] Chunk by section, index in FTS5 (BM25). No model, no network.
- [ ] Upgrade `grounding_shield` (you own it): add `doc_id`, `section`, `page`, `passage_sha256` to `grounding_source_t`; return the citation in `grounding_result_t`.
- [ ] Gate: a tool call that cites a policy clause must carry the citation into the audit record. A citation whose `passage_sha256` no longer matches the corpus is a hard block.
- [ ] Demo question a judge can type: "what is the approval threshold and where is it defined?" → retrieved clause + citation + verifier verdict.

**Do not** claim RAG over any model that is not in the repo.

---

## Report back (each round)

1. Exit codes: `make clean && make test` ×2, container run.
2. For Round 1: `GOMAA_HOME=/nonexistent make test`, and the two token-ledger behaviours.
3. `git diff --stat` and the claims-table commands with output.
4. Confirmation that no file outside `qala-agent/` was touched, and that vendored canonical files are unmodified except where this plan says.

**Do not start Round 2/3 work in Round 1's PR.** One round, one PR, one review.
