#include "command.h"

#include <string.h>

static struct {
    mp_command items[MP_COMMAND_QUEUE_SIZE];
    int size;
} queue;

void mp_command_write(const mp_command *command, buffer *buf)
{
    buffer_write_i32(buf, command->type);
    buffer_write_i32(buf, command->player_id);
    buffer_write_i32(buf, command->sequence);
    for (int i = 0; i < MP_COMMAND_MAX_ARGS; i++) {
        buffer_write_i32(buf, command->args[i]);
    }
    // the tick is written last so that a command can be serialized before it is scheduled
    buffer_write_i32(buf, command->tick);
}

int mp_command_read(mp_command *command, buffer *buf)
{
    command->type = buffer_read_i32(buf);
    command->player_id = buffer_read_i32(buf);
    command->sequence = buffer_read_i32(buf);
    for (int i = 0; i < MP_COMMAND_MAX_ARGS; i++) {
        command->args[i] = buffer_read_i32(buf);
    }
    command->tick = buffer_read_i32(buf);
    return !buf->overflow && command->type > MP_COMMAND_NONE && command->type < MP_COMMAND_MAX;
}

static int compare(const mp_command *a, const mp_command *b)
{
    if (a->tick != b->tick) {
        return a->tick < b->tick ? -1 : 1;
    }
    if (a->player_id != b->player_id) {
        return a->player_id < b->player_id ? -1 : 1;
    }
    if (a->sequence != b->sequence) {
        return a->sequence < b->sequence ? -1 : 1;
    }
    return 0;
}

void mp_command_queue_clear(void)
{
    queue.size = 0;
}

int mp_command_queue_add(const mp_command *command)
{
    if (queue.size >= MP_COMMAND_QUEUE_SIZE) {
        return 0;
    }
    // insertion sort keeps the queue ordered; it is short and mostly appended to
    int i = queue.size;
    while (i > 0 && compare(&queue.items[i - 1], command) > 0) {
        queue.items[i] = queue.items[i - 1];
        i--;
    }
    queue.items[i] = *command;
    queue.size++;
    return 1;
}

int mp_command_queue_size(void)
{
    return queue.size;
}

int mp_command_queue_run_due(int tick, mp_command_executor executor)
{
    int count = 0;
    while (count < queue.size && queue.items[count].tick <= tick) {
        count++;
    }
    if (!count) {
        return 0;
    }
    // copy first: an executor may schedule new commands
    mp_command due[MP_COMMAND_QUEUE_SIZE];
    memcpy(due, queue.items, count * sizeof(mp_command));
    memmove(queue.items, &queue.items[count], (queue.size - count) * sizeof(mp_command));
    queue.size -= count;
    for (int i = 0; i < count; i++) {
        executor(&due[i]);
    }
    return count;
}
