# API Design (v0.1)

Base URL for management APIs: `/api/v1`  
Create/redirect use root paths for the MVP.

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

## Health

```http
GET /health
```

**Response** `200 OK`

```json
{ "status": "ok" }
```

---

## Create short URL

```http
POST /shorten
```

**Body**

```json
{
  "url": "https://github.com",
  "custom_alias": "docs",
  "expires_at": "2026-12-31T23:59:59Z"
}
```

- `url` — required; must start with `http://` or `https://`
- `custom_alias` — optional; 1–10 alphanumeric characters
- `expires_at` — optional ISO-8601 timestamp in the future

**Response** `200 OK`

```json
{ "short_code": "docs" }
```

**Errors:** `400` validation, `409` alias taken

---

## URL Management

### List URLs

```http
GET /api/v1/urls?page=1&limit=20&q=example
```

Supports pagination and optional search (`q` matches `original_url`).

**Response** `200 OK`

```json
{
  "data": [
    {
      "id": 1,
      "original_url": "https://github.com",
      "short_code": "docs",
      "click_count": 3,
      "created_at": "2026-09-02 12:00:00",
      "expires_at": null
    }
  ],
  "pagination": {
    "page": 1,
    "limit": 20,
    "total": 1
  }
}
```

---

### Get URL by id

```http
GET /api/v1/urls/{id}
```

**Response** `200 OK` — same object shape as list items.  
**Errors:** `404` if missing

---

### Update URL

```http
PATCH /api/v1/urls/{id}
```

**Body** (all fields optional)

```json
{
  "original_url": "https://example.com/updated",
  "custom_alias": "newalias",
  "expires_at": "2027-01-01T00:00:00Z"
}
```

Set `"expires_at": null` to clear expiration.

**Response** `200 OK` — updated object  
**Errors:** `400`, `404`, `409` (alias taken)

---

### Delete URL

```http
DELETE /api/v1/urls/{id}
```

**Response** `204 No Content`  
**Errors:** `404` if missing

---

## Redirect (public)

```http
GET /{short_code}
```

Records a click, then redirects.

**Response** `302 Found`

```http
Location: https://example.com/very/long/path
```

Returns `404` if unknown, or `410 Gone` if expired.

---

## Status codes (common)

| Code | Meaning |
| ---- | ------- |
| 200  | Success |
| 204  | Deleted / no body |
| 302  | Redirect |
| 400  | Bad request / validation failure |
| 404  | Resource not found |
| 409  | Conflict (e.g. alias taken) |
| 410  | Short URL expired |
| 500  | Server error |
