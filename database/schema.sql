-- URL Shortener schema (PostgreSQL)
-- Current: urls table (shortening core)

CREATE TABLE urls (
    id SERIAL PRIMARY KEY,
    original_url TEXT NOT NULL,
    short_code VARCHAR(10) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    click_count INTEGER DEFAULT 0
);

CREATE INDEX idx_urls_short_code ON urls (short_code);
