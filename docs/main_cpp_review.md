# src/main.cpp Strict Review

Context: This review treats `src/main.cpp` as production-critical entrypoint code (UI + HTTP + MCP + OS capabilities). Findings are ordered by severity and focus on bugs, security risks, and reliability issues.

## Executive Summary

- The current implementation contains multiple high-impact security issues (notably a direct remote command execution path) and a fragile, non-HTTP-compliant socket server.
- The current architecture (a 5k+ line monolith) makes correctness and testing difficult and increases regression risk.
- The observed symptom `curl http://127.0.0.1:35182/... -> Connection refused` is consistent with “HTTP server thread not actually listening or already exited”, and the program currently provides poor observability for that failure (UI can claim started even if `bind/listen` failed).

## Recent Changes Noted (Claude Code)

- `g_running` was changed from `bool` to `std::atomic<bool>`, reducing a real data race risk.
- Added `logs/http_server.log` and additional bind/listen error logging for the HTTP server thread.
- Added a startup handshake (`g_httpServerStartedEvent`/`g_httpServerStartedOK`) to avoid showing “Server started” when `bind/listen` failed.
- Switched several `sprintf/strcpy` usages to `snprintf/strncpy` (partial hardening).
- Moved `/health` handling before auth so it can be used for diagnostics without a token.

## Critical

- HTTP request parsing is not reliable: the server calls `recv()` once and assumes it received a full HTTP request (headers + body). Any fragmentation or larger requests can be truncated, causing random failures (MCP JSON parse errors, missing body, etc.).
  - Location: `src/main.cpp:4388-4437` (single `recv` then `HandleHttpRequest`)
  - Also: routes that do `request.find("\r\n\r\n")` to find body depend on full request being present. For example:
    - `src/main.cpp:3987-4004` (`/mcp/tools/call`)
    - `src/main.cpp:4021-4088` (`/execute`)

- Remote command execution (RCE) endpoint: `/execute` parses JSON via substring search and executes `cmd.exe /c ...` directly, bypassing the intended security model (`allowed_commands`, `PolicyGuard`, `CommandService`).
  - Location: `src/main.cpp:4019-4099` (`POST /execute`)
  - Command runner: `src/main.cpp:2656-2768` (`ExecuteCommand`)
  - Impact: any party with a token can execute arbitrary commands.

- Data race on shutdown flag: `g_running` is a plain `bool` read/written by multiple threads (UI thread + HTTP thread). This is undefined behavior under C++ memory model.
  - Location: `src/main.cpp:76` (definition), `src/main.cpp:4366`, `src/main.cpp:5419`, `src/main.cpp:2841-2853` (write on `/exit`).

## High

- Single-threaded blocking server: accepts and handles requests synchronously. Any slow operation blocks all other clients and can make the service appear “down”.
  - Location: `src/main.cpp:4366-4437` (server loop)
  - Blocking handlers: `/search`, `/read`, `/execute` (various heavy I/O and CPU work).

- Unsafe C string APIs and fixed buffers: extensive use of `strcpy`/`sprintf` etc. with fixed buffers and struct fields; future changes can easily create buffer overflows.
  - Examples:
    - `src/main.cpp:806-808` (screenshot filename via `sprintf` into `char filename[128]`)
    - `src/main.cpp:881-889` (401 header via `sprintf`)
    - `src/main.cpp:5319-5320` (tray balloon text via `strcpy`)

- “Download file” endpoints read the whole file into memory and return it as a single string response; no size limits and easy OOM.
  - Examples:
    - `src/main.cpp:3030-3047` (`/clipboard/image/*`)
    - `src/main.cpp:3084-3110` (`/screenshot/file/*`)
    - `src/main.cpp:3146-3164` (`/clipboard/file/*`)

- File/dir access endpoints appear to bypass `allowed_dirs` / policy enforcement: `/list`, `/search`, `/read` operate directly on caller-provided paths (only token-gated). If the token leaks, attacker can read arbitrary files.
  - Examples:
    - `src/main.cpp:3166+` (`/list`)
    - `src/main.cpp:3238+` (`/search`)
    - `src/main.cpp:3409+` (`/read`)

- Poor startup observability: UI claims “server started” after thread creation, even if `bind/listen` fails and the thread exits immediately.
  - Location: thread created at `src/main.cpp:5299-5315`; tray notification at `src/main.cpp:5317-5325`.

## Medium

- `/health` is now unauthenticated: this improves diagnosability and allows deployment scripts to check liveness without a token, but it also leaks service presence/port to the LAN (design tradeoff).
  - Location: `src/main.cpp:2829+` (health handler placed before `IsAuthorizedRequest`).

- Routing uses substring matching on request line: `find("GET /status")` etc. can produce false positives and makes parsing fragile. There is no strict method/path/query parsing.
  - Location: throughout `HandleHttpRequest` (e.g. `src/main.cpp:2863-2866`, `src/main.cpp:2919-2921`).

- Inconsistent JSON construction: mix of `nlohmann::json` with manual string concatenation and custom escaping. This is error-prone (double-escaping, injection bugs) and hard to test.
  - Examples:
    - `src/main.cpp:3356-3406` (manual JSON building for `/search` results)
    - `src/main.cpp:2748-2767` (manual escaping for command output)

- Inconsistent log directories: some logs go to relative `logs/...`, while other helpers write to `%LocalAppData%\\ClawDeskMCP\\logs`. This complicates support and debugging.
  - Examples:
    - Helper chooses LocalAppData: `src/main.cpp:181-205`
    - WinMain creates `logs` and uses relative paths: `src/main.cpp:5111-5127`

## Low / Maintainability

- Monolithic file: `src/main.cpp` is ~5,467 lines and contains UI, HTTP server, MCP, and multiple OS capability endpoints. This reduces testability and increases regression risk.
  - Size: `src/main.cpp` (5k+ LOC).

## Recommended Refactor Priorities

1. Replace the hand-rolled socket HTTP server with a mature HTTP library (the repo already vendors `cpp-httplib`), or at minimum implement a correct request reader (looping `recv`, parse headers, respect `Content-Length`).
2. Remove or lock down `/execute`; route all privileged operations through `PolicyGuard` and enforce `allowed_dirs/allowed_apps/allowed_commands` consistently.
3. Make shutdown/thread coordination correct: use `std::atomic<bool>` for `g_running` (or a stop-token pattern), and add a definitive “server is listening” signal before claiming “started”.
4. Replace `strcpy/sprintf` with safe alternatives (`snprintf`, `StringCchCopyA`, etc.) and centralize HTTP response building to avoid fixed buffers.
5. Stream file responses with size limits, and return proper HTTP error codes for oversized payloads.
