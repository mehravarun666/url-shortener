# API Design (v0.1)

Base URL: `/api/v1`

All authenticated endpoints expect:

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

## Authentication

### Register

```http
POST /api/v1/auth/register
```

**Body**

```json
{
  "email": "user@example.com",
  "password": "strong-password",
  "name": "Ada Lovelace"
}
```

**Response** `201 Created`

```json
{
  "id": "uuid",
  "email": "user@example.com",
  "name": "Ada Lovelace",
  "created_at": "2026-07-14T12:00:00Z"
}
```

---

### Login

```http
POST /api/v1/auth/login
```

**Body**

```json
{
  "email": "user@example.com",
  "password": "strong-password"
}
```

**Response** `200 OK`

```json
{
  "access_token": "<jwt>",
  "token_type": "Bearer",
  "expires_in": 3600
}
```

---

### Current user

```http
GET /api/v1/auth/me
```

Auth required.

**Response** `200 OK`

```json
{
  "id": "uuid",
  "email": "user@example.com",
  "name": "Ada Lovelace",
  "created_at": "2026-07-14T12:00:00Z"
}
```

---

## URL Management

### Create short URL

```http
POST /api/v1/urls
```

Auth required.

**Body**

```json
{
  "original_url": "https://example.com/very/long/path",
  "custom_alias": "docs",
  "expires_at": "2026-12-31T23:59:59Z"
}
```

`custom_alias` and `expires_at` are optional.

**Response** `201 Created`

```json
{
  "id": "uuid",
  "original_url": "https://example.com/very/long/path",
  "short_code": "docs",
  "short_url": "https://short.ly/docs",
  "expires_at": "2026-12-31T23:59:59Z",
  "click_count": 0,
  "created_at": "2026-07-14T12:00:00Z"
}
```

---

### List URLs

```http
GET /api/v1/urls?page=1&limit=20&q=example
```

Auth required. Supports pagination and optional search (`q`).

**Response** `200 OK`

```json
{
  "data": [],
  "pagination": {
    "page": 1,
    "limit": 20,
    "total": 0
  }
}
```

---

### Get URL by id

```http
GET /api/v1/urls/{id}
```

Auth required.

**Response** `200 OK` — same shape as create.

---

### Update URL

```http
PATCH /api/v1/urls/{id}
```

Auth required.

**Body** (all fields optional)

```json
{
  "original_url": "https://example.com/updated",
  "custom_alias": "new-alias",
  "expires_at": "2027-01-01T00:00:00Z"
}
```

**Response** `200 OK`

---

### Delete URL

```http
DELETE /api/v1/urls/{id}
```

Auth required.

**Response** `204 No Content`

---

## Redirect (public)

### Resolve short code

```http
GET /{short_code}
```

No auth. Records a click, then redirects.

**Response** `302 Found`

```http
Location: https://example.com/very/long/path
```

Returns `404` if the code is unknown, or `410 Gone` if expired.

---

## Analytics

### Summary for a URL

```http
GET /api/v1/urls/{id}/analytics
```

Auth required.

**Response** `200 OK`

```json
{
  "url_id": "uuid",
  "short_code": "docs",
  "total_clicks": 128,
  "unique_visitors": 94,
  "browsers": [
    { "name": "Chrome", "count": 70 },
    { "name": "Safari", "count": 30 }
  ],
  "devices": [
    { "type": "desktop", "count": 80 },
    { "type": "mobile", "count": 48 }
  ],
  "countries": [
    { "code": "IN", "count": 60 },
    { "code": "US", "count": 40 }
  ],
  "referrers": [
    { "source": "twitter.com", "count": 25 },
    { "source": "direct", "count": 50 }
  ]
}
```

---

### Click history (paginated)

```http
GET /api/v1/urls/{id}/analytics/clicks?page=1&limit=50
```

Auth required.

**Response** `200 OK`

```json
{
  "data": [
    {
      "id": "uuid",
      "clicked_at": "2026-07-14T12:05:00Z",
      "ip_hash": "…",
      "user_agent": "Mozilla/5.0 …",
      "browser": "Chrome",
      "device": "desktop",
      "country": "IN",
      "referrer": "https://twitter.com"
    }
  ],
  "pagination": {
    "page": 1,
    "limit": 50,
    "total": 128
  }
}
```

---

## Status codes (common)

| Code | Meaning                          |
| ---- | -------------------------------- |
| 200  | Success                          |
| 201  | Created                          |
| 204  | Deleted / no body                |
| 302  | Redirect                         |
| 400  | Bad request / validation failure |
| 401  | Missing or invalid token         |
| 403  | Authenticated but not allowed    |
| 404  | Resource not found               |
| 409  | Conflict (e.g. alias taken)      |
| 410  | Short URL expired                |
| 429  | Rate limited                     |
| 500  | Server error                      |
