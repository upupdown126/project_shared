CREATE TABLE IF NOT EXISTS recordings (
    id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
    camera_id BIGINT UNSIGNED NOT NULL,
    channel INT UNSIGNED NOT NULL,
    path VARCHAR(1024) NOT NULL,
    start_ms BIGINT NOT NULL,
    end_ms BIGINT NOT NULL,
    size_bytes BIGINT UNSIGNED NOT NULL,
    checksum CHAR(32) NOT NULL,
    discontinuity BOOLEAN NOT NULL DEFAULT FALSE,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    KEY idx_recordings_lookup (camera_id, channel, start_ms, end_ms)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
