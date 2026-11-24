CREATE TABLE `control_commands` (
	`id` text PRIMARY KEY NOT NULL,
	`kind` text NOT NULL,
	`revision` integer NOT NULL,
	`desired` text NOT NULL,
	`status` text NOT NULL,
	`message` text NOT NULL,
	`created_at` integer NOT NULL,
	`updated_at` integer NOT NULL
);
--> statement-breakpoint
CREATE TABLE `control_locks` (
	`id` text PRIMARY KEY NOT NULL,
	`revision` integer NOT NULL,
	`busy_until` integer NOT NULL
);
