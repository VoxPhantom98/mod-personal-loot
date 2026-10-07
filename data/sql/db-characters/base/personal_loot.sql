-- Personal loot persistence. Install manually into the character database after backup.
-- Additive migration: does not alter or remove existing character tables.

CREATE TABLE IF NOT EXISTS `personal_loot_event` (
  `event_id` BINARY(16) NOT NULL,
  `source_key` VARBINARY(191) NOT NULL,
  `source_kind` TINYINT UNSIGNED NOT NULL,
  `source_entry` INT UNSIGNED NOT NULL,
  `source_spawn_id` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  `map_id` SMALLINT UNSIGNED NOT NULL,
  `instance_id` INT UNSIGNED NOT NULL DEFAULT 0,
  `difficulty` TINYINT UNSIGNED NOT NULL,
  `loot_mode` SMALLINT UNSIGNED NOT NULL,
  `encounter_id` INT UNSIGNED DEFAULT NULL,
  `state` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `created_at` BIGINT UNSIGNED NOT NULL,
  `ready_at` BIGINT UNSIGNED DEFAULT NULL,
  PRIMARY KEY (`event_id`),
  KEY `idx_pl_event_state` (`state`, `event_id`),
  UNIQUE KEY `uq_personal_loot_source` (`source_key`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='0 preparing, 1 ready, 2 recovery; committed before exposure';

CREATE TABLE IF NOT EXISTS `personal_loot_recipient` (
  `event_id` BINARY(16) NOT NULL,
  `character_guid` BIGINT UNSIGNED NOT NULL,
  `account_id` INT UNSIGNED NOT NULL,
  `eligibility_snapshot` JSON NOT NULL,
  `personal_gold` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`event_id`, `character_guid`),
  KEY `idx_personal_loot_character` (`character_guid`),
  CONSTRAINT `fk_pl_recipient_event` FOREIGN KEY (`event_id`)
    REFERENCES `personal_loot_event` (`event_id`) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `personal_loot_reward` (
  `event_id` BINARY(16) NOT NULL,
  `character_guid` BIGINT UNSIGNED NOT NULL,
  `reward_index` INT UNSIGNED NOT NULL,
  `reward_kind` TINYINT UNSIGNED NOT NULL,
  `item_entry` INT UNSIGNED DEFAULT NULL,
  `quantity` BIGINT UNSIGNED NOT NULL,
  `random_property` INT NOT NULL DEFAULT 0,
  `random_suffix` INT UNSIGNED NOT NULL DEFAULT 0,
  `item_flags` INT UNSIGNED NOT NULL DEFAULT 0,
  `eligibility_metadata` JSON NOT NULL,
  PRIMARY KEY (`event_id`, `character_guid`, `reward_index`),
  KEY `idx_pl_reward_character` (`character_guid`, `event_id`, `reward_index`),
  CONSTRAINT `fk_pl_reward_recipient` FOREIGN KEY (`event_id`, `character_guid`)
    REFERENCES `personal_loot_recipient` (`event_id`, `character_guid`) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Immutable outcome: kind 0 item, 1 gold';

CREATE TABLE IF NOT EXISTS `personal_loot_mail_delivery` (
  `delivery_id` BINARY(16) NOT NULL,
  `character_guid` BIGINT UNSIGNED NOT NULL,
  `state` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `mail_id` INT UNSIGNED DEFAULT NULL,
  `created_at` BIGINT UNSIGNED NOT NULL,
  `delivered_at` BIGINT UNSIGNED DEFAULT NULL,
  PRIMARY KEY (`delivery_id`),
  UNIQUE KEY `uq_pl_mail_id` (`mail_id`),
  UNIQUE KEY `uq_pl_delivery_owner` (`delivery_id`, `character_guid`),
  KEY `idx_pl_pending_mail` (`state`, `character_guid`),
  CONSTRAINT `fk_pl_delivery_mail` FOREIGN KEY (`mail_id`) REFERENCES `mail` (`id`) ON DELETE SET NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='0 queued, 1 committed mail; retries reuse delivery_id';

CREATE TABLE IF NOT EXISTS `personal_loot_claim` (
  `event_id` BINARY(16) NOT NULL,
  `character_guid` BIGINT UNSIGNED NOT NULL,
  `reward_index` INT UNSIGNED NOT NULL,
  `delivery_kind` TINYINT UNSIGNED NOT NULL,
  `delivery_id` BINARY(16) DEFAULT NULL,
  `inventory_item_guid` BIGINT UNSIGNED DEFAULT NULL,
  `created_at` BIGINT UNSIGNED NOT NULL,
  PRIMARY KEY (`event_id`, `character_guid`, `reward_index`),
  KEY `idx_pl_claim_delivery` (`delivery_id`, `character_guid`),
  CONSTRAINT `fk_pl_claim_reward` FOREIGN KEY (`event_id`, `character_guid`, `reward_index`)
    REFERENCES `personal_loot_reward` (`event_id`, `character_guid`, `reward_index`) ON DELETE RESTRICT,
  CONSTRAINT `fk_pl_claim_mail` FOREIGN KEY (`delivery_id`, `character_guid`)
    REFERENCES `personal_loot_mail_delivery` (`delivery_id`, `character_guid`) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Insert-only claim guard: kind 0 inventory/gold, 1 mail outbox';

CREATE TABLE IF NOT EXISTS `personal_loot_snapshot` (
  `event_id` BINARY(16) NOT NULL,
  `format_version` INT UNSIGNED NOT NULL,
  `payload` JSON NOT NULL,
  PRIMARY KEY (`event_id`),
  CONSTRAINT `fk_pl_snapshot_event` FOREIGN KEY (`event_id`)
    REFERENCES `personal_loot_event` (`event_id`) ON DELETE RESTRICT
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Versioned exact generation snapshot for restoration';

CREATE TABLE IF NOT EXISTS `personal_loot_instance_cycle` (
  `instance_id` INT UNSIGNED NOT NULL,
  `cycle_id` BINARY(16) NOT NULL,
  `map_id` INT UNSIGNED NOT NULL,
  `difficulty` TINYINT UNSIGNED NOT NULL,
  PRIMARY KEY (`instance_id`),
  UNIQUE KEY `uq_pl_instance_cycle` (`cycle_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='Current instance lifetime pointer; old rewards retain their cycle in source_key';
