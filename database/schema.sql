-- URL Shortener initial schema (PostgreSQL)
-- Tables: users, urls, clicks (analytics)

CREATE EXTENSION IF NOT EXISTS "pgcrypto";

-- ---------------------------------------------------------------------------
-- Users
-- ---------------------------------------------------------------------------
CREATE TABLE users (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    email         VARCHAR(255) NOT NULL,
    password_hash TEXT         NOT NULL,
    name          VARCHAR(120) NOT NULL,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT users_email_unique UNIQUE (email)
);

CREATE INDEX idx_users_email ON users (email);

-- ---------------------------------------------------------------------------
-- URLs
-- ---------------------------------------------------------------------------
CREATE TABLE urls (
    id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id       UUID         NOT NULL REFERENCES users (id) ON DELETE CASCADE,
    original_url  TEXT         NOT NULL,
    short_code    VARCHAR(64)  NOT NULL,
    expires_at    TIMESTAMPTZ,
    click_count   BIGINT       NOT NULL DEFAULT 0,
    is_active     BOOLEAN      NOT NULL DEFAULT TRUE,
    created_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    updated_at    TIMESTAMPTZ  NOT NULL DEFAULT NOW(),

    CONSTRAINT urls_short_code_unique UNIQUE (short_code),
    CONSTRAINT urls_original_url_not_empty CHECK (length(trim(original_url)) > 0),
    CONSTRAINT urls_short_code_not_empty CHECK (length(trim(short_code)) > 0)
);

CREATE INDEX idx_urls_user_id ON urls (user_id);
CREATE INDEX idx_urls_short_code ON urls (short_code);
CREATE INDEX idx_urls_expires_at ON urls (expires_at)
    WHERE expires_at IS NOT NULL;

-- ---------------------------------------------------------------------------
-- Analytics (per-click events)
-- ---------------------------------------------------------------------------
CREATE TABLE clicks (
    id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    url_id      UUID         NOT NULL REFERENCES urls (id) ON DELETE CASCADE,
    clicked_at  TIMESTAMPTZ  NOT NULL DEFAULT NOW(),
    ip_hash     VARCHAR(128),
    user_agent  TEXT,
    browser     VARCHAR(64),
    device      VARCHAR(32),
    country     CHAR(2),
    referrer    TEXT
);

CREATE INDEX idx_clicks_url_id ON clicks (url_id);
CREATE INDEX idx_clicks_clicked_at ON clicks (clicked_at);
CREATE INDEX idx_clicks_url_id_clicked_at ON clicks (url_id, clicked_at DESC);
