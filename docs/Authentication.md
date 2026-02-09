# Authentication Guide

ClawDesk MCP Server uses Bearer token authentication to secure all API endpoints.

## Overview

Starting from version 0.2.0, all HTTP requests (except CORS preflight OPTIONS requests) require a valid Bearer token in the Authorization header.

## Configuration

### Auth Token Generation

When the server starts for the first time, it automatically generates a random 64-character hexadecimal auth token and saves it to `config.json`:

```json
{
  "auth_token": "a1b2c3d4e5f6...64-character-hex-string",
  "server_port": 35182,
  "listen_address": "0.0.0.0",
  ...
}
```

### Token Regeneration

If the auth token is missing or invalid (less than 32 characters), the server will automatically regenerate a new token on startup.

## Using the Auth Token

### HTTP Requests

Include the auth token in the `Authorization` header with the `Bearer` scheme:

```bash
curl -H "Authorization: Bearer YOUR_AUTH_TOKEN_HERE" http://localhost:35182/status
```

### Example with Different Endpoints

**Get Status:**

```bash
curl -H "Authorization: Bearer a1b2c3d4..." http://192.168.31.3:35182/status
```

**List Disks:**

```bash
curl -H "Authorization: Bearer a1b2c3d4..." http://192.168.31.3:35182/disks
```

**Read File:**

```bash
curl -H "Authorization: Bearer a1b2c3d4..." "http://192.168.31.3:35182/read?path=C:\test.txt"
```

**Execute Command:**

```bash
curl -X POST \
  -H "Authorization: Bearer a1b2c3d4..." \
  -H "Content-Type: application/json" \
  -d '{"command":"dir"}' \
  http://192.168.31.3:35182/execute
```

## Error Responses

### 401 Unauthorized

If the token is missing or invalid, the server returns:

```json
{
    "error": "unauthorized"
}
```

HTTP Status: `401 Unauthorized`

### CORS Preflight (OPTIONS)

CORS preflight requests (OPTIONS method) do NOT require authentication and will always return 200 OK:

```bash
curl -X OPTIONS http://localhost:35182/status
# Returns 200 OK without requiring Authorization header
```

## Security Best Practices

1. **Keep the token secret**: Never commit `config.json` to version control
2. **Use HTTPS in production**: When exposing the server over the network, use a reverse proxy with HTTPS
3. **Rotate tokens regularly**: Manually edit `config.json` to change the token, or delete it and restart the server to generate a new one
4. **Restrict network access**: Use `listen_address: "127.0.0.1"` if you only need local access
5. **Use firewall rules**: Configure Windows Firewall to restrict which machines can connect

## MCP Client Configuration

When configuring an MCP client (like Claude Desktop), you'll need to pass the auth token. Example configuration:

```json
{
    "mcpServers": {
        "clawdesk": {
            "command": "node",
            "args": ["path/to/mcp-client.js"],
            "env": {
                "CLAWDESK_URL": "http://192.168.31.3:35182",
                "CLAWDESK_TOKEN": "your-auth-token-here"
            }
        }
    }
}
```

Your MCP client implementation should read the token from the environment variable and include it in all HTTP requests.

## Testing Authentication

Use the provided test script to verify authentication is working:

```bash
../scripts/test_auth.sh
```

This script will:

1. Test requests without a token (should fail with 401)
2. Test requests with an invalid token (should fail with 401)
3. Test requests with a valid token (should succeed with 200)
4. Test CORS preflight requests (should succeed without token)
5. Test multiple endpoints with valid token

## Troubleshooting

### "unauthorized" error on all requests

1. Check that `config.json` exists and contains a valid `auth_token`
2. Verify you're using the correct token in the Authorization header
3. Ensure the header format is exactly: `Authorization: Bearer <token>`
4. Check for extra spaces or newlines in the token

### Token not being generated

1. Check file permissions on the directory
2. Ensure the server has write access to create/modify `config.json`
3. Check the Dashboard or logs for error messages

### CORS issues in browser

1. CORS preflight (OPTIONS) requests work without authentication
2. The actual request (GET, POST, etc.) requires the Authorization header
3. Ensure your browser/client includes the Authorization header in the actual request

## Token Format

- Length: 64 characters
- Format: Hexadecimal (0-9, a-f)
- Example: `a1b2c3d4e5f6789012345678901234567890abcdefabcdefabcdefabcdefabcd`

The token is generated using a cryptographically secure random number generator.
