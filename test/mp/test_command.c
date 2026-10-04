// Unit tests of the command layer (doc/mp/ROADMAP.md M2.1)
#include "mp/command.h"

#include <stdio.h>
#include <string.h>

static int failures;
static int executed[16];
static int num_executed;

#define CHECK(cond) do { if (!(cond)) { printf("FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static mp_command make(int tick, int player, int sequence, int arg)
{
    mp_command c;
    memset(&c, 0, sizeof(c));
    c.type = MP_COMMAND_TEST;
    c.tick = tick;
    c.player_id = player;
    c.sequence = sequence;
    c.args[0] = arg;
    return c;
}

static void record(const mp_command *c)
{
    executed[num_executed++] = c->args[0];
}

static void test_round_trip(void)
{
    mp_command in = make(123456, 3, 42, -7);
    for (int i = 1; i < MP_COMMAND_MAX_ARGS; i++) {
        in.args[i] = i * 1000 - 3000;
    }
    uint8_t data[MP_COMMAND_SERIALIZED_SIZE];
    buffer buf;
    buffer_init(&buf, data, sizeof(data));
    mp_command_write(&in, &buf);
    CHECK(buf.index == MP_COMMAND_SERIALIZED_SIZE);
    CHECK(data[0] == MP_COMMAND_TEST && data[1] == 0); // little endian

    mp_command out;
    buffer_init(&buf, data, sizeof(data));
    CHECK(mp_command_read(&out, &buf));
    CHECK(memcmp(&in, &out, sizeof(in)) == 0);

    data[0] = 0;
    buffer_init(&buf, data, sizeof(data));
    CHECK(!mp_command_read(&out, &buf));
}

static void test_queue_order(void)
{
    mp_command_queue_clear();
    // added out of order: must run by tick, then player, then sequence
    mp_command cs[] = { make(10, 1, 0, 4), make(5, 2, 0, 2), make(10, 0, 1, 3), make(5, 0, 0, 1),
        make(10, 0, 0, 30), make(20, 0, 0, 5) };
    for (int i = 0; i < 6; i++) {
        CHECK(mp_command_queue_add(&cs[i]));
    }
    num_executed = 0;
    CHECK(mp_command_queue_run_due(4, record) == 0);
    CHECK(mp_command_queue_run_due(5, record) == 2);
    CHECK(executed[0] == 1 && executed[1] == 2);
    CHECK(mp_command_queue_run_due(15, record) == 3);
    CHECK(executed[2] == 30 && executed[3] == 3 && executed[4] == 4);
    CHECK(mp_command_queue_size() == 1);
    CHECK(mp_command_queue_run_due(100, record) == 1);
    CHECK(executed[5] == 5 && mp_command_queue_size() == 0);
}

static void test_queue_full(void)
{
    mp_command_queue_clear();
    mp_command c = make(1, 0, 0, 0);
    for (int i = 0; i < MP_COMMAND_QUEUE_SIZE; i++) {
        CHECK(mp_command_queue_add(&c));
    }
    CHECK(!mp_command_queue_add(&c));
    mp_command_queue_clear();
}

int main(void)
{
    test_round_trip();
    test_queue_order();
    test_queue_full();
    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("All command tests passed\n");
    return 0;
}
