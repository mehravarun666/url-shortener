# API Design (v0.1)

Base URL for management APIs: `/api/v1`  
Create uses `/shorten`; redirect uses `/{short_code}`.

Authenticated endpoints expect:

```http
Authorization: Bearer <jwt>
```

Success responses use JSON. Errors follow:

```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Human-readable description"
  }
}
```

---

## Health (public)

```http
GET /health
```

**Response** `200 OK` — `{ "status": "ok" }`

---

## Authentication

### Register

```http
POST /api/v1/auth/register
```

```json
{ "email": "user@example.com", "password": "strong-password", "name": "Ada" }
```

Password must be at least 8 characters.

**Response** `201 Created`

```json
{ "id": 1, "email": "user@example.com", "name": "Ada", "created_at": "..." }
```

**Errors:** `400`, `409` email taken

### Login

```http
POST /api/v1/auth/login
```

```json
{ "email": "user@example.com", "password": "strong-password" }
```

**Response** `200 OK`

```json
{
  "access_token": "<jwt>",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

**Errors:** `401` invalid credentials

### Me (JWT required)

```http
GET /api/v1/auth/me
```

Returns the current user profile.

---

## Create short URL (JWT required)

```http
POST /shorten
```

```json
{
  "url": "https://github.com",
  "custom_alias": "docs",
  "expires_at": "2026-12-31T23:59:59Z"
}
```

**Response** `200 OK` — `{ "short_code": "docs" }`  
**Errors:** `400`, `401`, `409`

---

## URL Management (JWT required)

All list/get/update/delete operations are scoped to the authenticated user.

### List

```http
GET /api/v1/urls?page=1&limit=20&q=example
```

### Get

```http
GET /api/v1/urls/{id}
```

### Update

```http
PATCH /api/v1/urls/{id}
```

```json
{
  "original_url": "https://example.com/updated",
  "custom_alias": "newalias",
  "expires_at": "2027-01-01T00:00:00Z"
}
```

### Delete

```http
DELETE /api/v1/urls/{id}
```

**Response** `204 No Content`

---

## Redirect (public)

```http
GET /{short_code}
```

**Response** `302 Found` with `Location` header.  
`404` if unknown, `410` if expired.

---

## Status codes

| Code | Meaning |
| ---- | ------- |
| 200 / 201 / 204 | Success |
| 302 | Redirect |
| 400 | Validation |
| 401 | Missing/invalid token or credentials |
| 404 | Not found |
| 409 | Conflict |
| 410 | Expired |
| 500 | Server error |
