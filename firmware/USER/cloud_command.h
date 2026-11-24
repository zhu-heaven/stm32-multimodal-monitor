#ifndef CLOUD_COMMAND_H
#define CLOUD_COMMAND_H
/* Pure parser: 1 = valid switch command, 0 = not a command/invalid JSON,
 * -1 = valid request ID but unsupported/invalid params. No GPIO side effects.
 * id needs 14 bytes; only OneNET decimal string IDs of 1..13 digits accepted. */
int CloudCommand_Parse(const char *json,char id[14],unsigned char *on,char config[192]);
#endif
