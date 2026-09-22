CREATE DATABASE IF NOT EXISTS `ctf_server` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE ctf_server;

CREATE TABLE IF NOT EXISTS `challenges` (
    `id` INTEGER PRIMARY KEY AUTO_INCREMENT,
    `creator_id` VARCHAR(255) NOT NULL,
    `name` VARCHAR(255) NOT NULL,
    `description` TEXT NOT NULL,
    `flag` VARCHAR(255) NOT NULL,
    `genre` INTEGER NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `answers` (
    `id` INTEGER PRIMARY KEY AUTO_INCREMENT,
    `challenge_id` INTEGER NOT NULL,
    `user_id` VARCHAR(255) NOT NULL,
    `answer` VARCHAR(255) NOT NULL,
    `is_correct` BOOLEAN NOT NULL,
    `created_at` DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (challenge_id) REFERENCES challenges(id) ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `users` (
    `id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,
    `username` VARCHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `password_hash` VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `created_at` DATETIME NOT NULL,
    UNIQUE KEY `users_username_unique` (`username`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

CREATE TABLE IF NOT EXISTS `auth_sessions` (
    `id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin PRIMARY KEY,
    `user_id` CHAR(32) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,
    `issued_at` DATETIME NOT NULL,
    `expires_at` DATETIME NOT NULL,
    KEY `auth_sessions_expires_at` (`expires_at`),
    CONSTRAINT `auth_sessions_user_fk` FOREIGN KEY (`user_id`) REFERENCES `users` (`id`),
    CONSTRAINT `auth_sessions_time_order` CHECK (`expires_at` > `issued_at`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
