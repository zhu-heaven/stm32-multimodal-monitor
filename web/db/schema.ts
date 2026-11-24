import {sqliteTable,text,integer} from 'drizzle-orm/sqlite-core';
export const sitePassword=sqliteTable('site_password',{
 id:integer('id').primaryKey(),salt:text('salt').notNull(),hash:text('hash').notNull(),createdAt:integer('created_at').notNull()
});
export const passwordSessions=sqliteTable('password_sessions',{
 tokenHash:text('token_hash').primaryKey(),expires:integer('expires').notNull()
});
export const passwordAttempts=sqliteTable('password_attempts',{
 bucket:text('bucket').primaryKey(),count:integer('count').notNull(),expires:integer('expires').notNull()
});
export const controlLocks=sqliteTable('control_locks',{
 id:text('id').primaryKey(),revision:integer('revision').notNull(),busyUntil:integer('busy_until').notNull()
});
export const controlCommands=sqliteTable('control_commands',{
 id:text('id').primaryKey(),kind:text('kind').notNull(),revision:integer('revision').notNull(),desired:text('desired').notNull(),status:text('status').notNull(),message:text('message').notNull(),createdAt:integer('created_at').notNull(),updatedAt:integer('updated_at').notNull()
});
export const forecastCache=sqliteTable('forecast_cache',{
 id:text('id').primaryKey(),payload:text('payload').notNull(),fetchedAt:integer('fetched_at').notNull()
});
