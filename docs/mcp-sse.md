# Streamable HTTP MCP Endpoint Implementation (No SSE)

This document describes **Option 1**: implement **MCP Streamable HTTP** using a single JSON-RPC endpoint (`POST /mcp`) and **do not implement SSE**. This is intended to be compatible with clients like `mcporter` that expect MCP-over-HTTP.

Note: Despite the filename, this is **not** the legacy "MCP-SSE handshake" transport. Streamable HTTP can work without SSE by returning `application/json` directly.

## Goals

1. Add MCP endpoint: `POST /mcp` (JSON-RPC 2.0 request/notification/response).
2. Support minimum methods typically required by MCP clients:
   - `initialize`
   - `notifications/initialized` (notification)
   - `ping`
   - `tools/list`
   - `tools/call`
3. Keep existing REST-ish endpoints (`/mcp/initialize`, `/mcp/tools/list`, `/mcp/tools/call`) untouched for backward compatibility, but prefer `/mcp` for new clients.

## Preconditions: Fix HTTP Request Reading

Current server code reads only a single `recv()` chunk (4KB) and immediately parses/handles it. This will break MCP requests because JSON bodies can be larger than 4KB and TCP can split packets arbitrarily.

### Required change

Implement `ReadHttpRequestFully(SOCKET)` and use it in the accept loop:

1. Read until headers are complete (`\r\n\r\n`).
2. Parse headers and extract `Content-Length`.
3. Read until body length equals `Content-Length`.
4. Enforce a max body size (recommend: 1MB for MCP).
5. If `Transfer-Encoding: chunked`, return 400 for now (do not attempt partial support).

Only after a full request is assembled should you call `HandleHttpRequest(fullRequest)`.

## Routing: Add `/mcp`

### `POST /mcp`

Treat the body as a single JSON-RPC message:

- If body is invalid JSON: return HTTP 400 and a JSON-RPC error object (with `id: null`).
- Otherwise dispatch based on JSON-RPC shape:
  - Request: has `"method"` and an `"id"`.
  - Notification: has `"method"` and **no** `"id"`.
  - Response: has `"result"` or `"error"` (clients may send; server can accept and ignore).

### `GET /mcp`

If you are not implementing SSE streams, return `405 Method Not Allowed`.

### `DELETE /mcp`

Return `405 Method Not Allowed` initially. (Optional future work: session close.)

## Security

### Authorization

Continue using Bearer token:

- All requests except `OPTIONS` and `/health` must include:
  - `Authorization: Bearer <auth_token>`

### Origin check (recommended)

If an `Origin` header is present:

- Allow only configured origins (recommend: add `security.allowed_origins` array).
- If not allowed, return HTTP 403.

Default policy suggestion for local tools:

- Allow `http://127.0.0.1` and `http://localhost` (optionally with any port).

### MCP protocol version header

If `MCP-Protocol-Version` header is present:

- Accept only versions you support.
- If unsupported, return HTTP 400.

If missing:

- Do not reject (assume a default version).

## Sessions (Strongly Recommended)

Without sessions, multiple clients can interfere with each other. Implement the `MCP-Session-Id` mechanism.

### Session rules

1. On successful `initialize` response:
   - Generate `session_id` (UUIDv4 or 32+ random hex).
   - Include HTTP response header: `MCP-Session-Id: <session_id>`.
   - Store in memory: `session_id -> { protocolVersion, initialized=false, createdAt }`.
2. For all subsequent `POST /mcp` requests (except `initialize`):
   - Require header `MCP-Session-Id`.
   - If missing: HTTP 400.
   - If unknown/expired: HTTP 404 (client should re-initialize).
3. When receiving notification `notifications/initialized`:
   - Mark session as `initialized=true`.

## JSON-RPC Implementation Details

### Common validation

For `POST /mcp` body parsed as JSON object:

- Require `"jsonrpc": "2.0"`.
- Do not support batch in v1 (reject arrays with HTTP 400).

### Notification / Response handling

If the incoming JSON-RPC message is a **notification** (no `id`) or a **response** (has `result/error`):

- Return HTTP `202 Accepted` with empty body.
- For `notifications/initialized`, update session state.

### Methods to support

#### 1) `initialize` (request)

Input: `{ "method": "initialize", "params": {...} }`

Behavior:

- Negotiate protocol version (pick the best supported version; typically return the same one the client asked for if supported).
- Return JSON-RPC result with:
  - `protocolVersion`
  - `capabilities` (at least `tools`)
  - `serverInfo` (`name`, `version`)
- Return `MCP-Session-Id` header.

#### 2) `ping` (request)

Return `result: {}`.

#### 3) `tools/list` (request)

Return JSON-RPC `result`:

```json
{
  "tools": [
    { "name": "read_file", "description": "...", "inputSchema": { } }
  ]
}
```

Implementation mapping:

- Use existing `ToolRegistry::getAllTools()` which already contains `name`, `description`, `inputSchema`.

#### 4) `tools/call` (request)

Input `params` should include:

- `name` (string)
- `arguments` (object, optional)

Behavior:

- Look up tool in `ToolRegistry`.
- Enforce `PolicyGuard::evaluateToolCall(toolName, args)` before calling tool.
- Call tool handler and return JSON-RPC `result` as MCP tool result:
  - `{ "content": [...], "isError": false }` (or `true`)

### Error mapping

Return HTTP 200 with JSON-RPC error for typical RPC-level issues:

- Parse error: `-32700`
- Invalid Request: `-32600`
- Method not found: `-32601`
- Invalid params: `-32602`
- Server error / policy denied / not initialized: use `-32000` with clear message

For transport-level errors (too large body, unsupported transfer encoding), prefer HTTP 4xx.

## Integration Notes for This Repo

Recommended minimal-diff structure:

1. Keep current endpoints as-is.
2. Add new handler:
   - `HandleMcpStreamableHttp(request)` for `POST /mcp`.
3. Keep tool implementations in the existing `ToolRegistry` and reuse:
   - `RegisterMcpTools()`
   - `ToolRegistry::getAllTools()`
   - `ToolRegistry::getTool().handler(args)`
4. Fix request reading in `HttpServerThread` first; otherwise MCP will be flaky.

## Acceptance Tests (Manual)

Use curl (replace token/port):

1. `initialize`:
   - `POST /mcp` with JSON-RPC initialize payload
   - Expect HTTP 200 JSON body + `MCP-Session-Id` header
2. `tools/list`:
   - `POST /mcp` with `MCP-Session-Id` header and `tools/list`
   - Expect tools array
3. `tools/call`:
   - Call a low-risk tool and validate result shape `{content,isError}`

