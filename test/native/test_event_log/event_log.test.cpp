#include <unity.h>

#include "codec.h"
#include "event_log.h"

namespace
{
    Event event(int64_t time, EventType type, GateLane lane)
    {
        Event e;
        e.time = time;
        e.type = type;
        e.lane = lane;
        return e;
    }
}

void setUp()
{
}

void tearDown()
{
}

void test_add_keeps_order_and_drops_oldest()
{
    EventLog log;
    for (int i = 0; i < static_cast<int>(MAX_EVENTS) + 5; ++i)
    {
        log.add(event(i, EventType::SafetyStop, GateLane::Entry));
    }
    TEST_ASSERT_EQUAL_UINT(MAX_EVENTS, log.all().size());
    TEST_ASSERT_EQUAL_INT64(5, log.all().front().time);
    TEST_ASSERT_EQUAL_INT64(MAX_EVENTS + 4, log.all().back().time);
    log.clear();
    TEST_ASSERT_EQUAL_UINT(0, log.all().size());
}

void test_labels_and_names()
{
    TEST_ASSERT_EQUAL_STRING("Sicherheitsabschaltung", eventTypeLabel(EventType::SafetyStop));
    TEST_ASSERT_EQUAL_STRING("Fernöffnung", eventTypeLabel(EventType::RemoteOpening));
    TEST_ASSERT_EQUAL_STRING("Kasse nicht erreichbar", eventTypeLabel(EventType::KasseUnreachable));
    TEST_ASSERT_EQUAL_STRING("Einfahrt", gateLaneLabel(GateLane::Entry));
    TEST_ASSERT_EQUAL_STRING("Ausfahrt", gateLaneLabel(GateLane::Exit));
    EventType t;
    TEST_ASSERT_TRUE(eventTypeFromName("loop_fault", t));
    TEST_ASSERT_EQUAL(EventType::LoopFault, t);
    TEST_ASSERT_FALSE(eventTypeFromName("nope", t));
    GateLane l;
    TEST_ASSERT_TRUE(gateLaneFromName("exit", l));
    TEST_ASSERT_EQUAL(GateLane::Exit, l);
    TEST_ASSERT_FALSE(gateLaneFromName("links", l));
}

void test_codec_round_trip()
{
    EventLog log;
    Event e = event(1760530000, EventType::RemoteOpening, GateLane::Exit);
    e.plate = "ZH98765";
    e.user = "betrieb";
    e.reason = "Kulanz";
    e.note = "ohne Zahlung";
    log.add(e);
    log.add(event(1760530100, EventType::SafetyStop, GateLane::Entry));

    EventLog loaded;
    TEST_ASSERT_TRUE(eventsFromJson(eventsToJson(log), loaded));
    TEST_ASSERT_EQUAL_UINT(2, loaded.all().size());
    const Event& r = loaded.all()[0];
    TEST_ASSERT_EQUAL_INT64(1760530000, r.time);
    TEST_ASSERT_EQUAL(EventType::RemoteOpening, r.type);
    TEST_ASSERT_EQUAL(GateLane::Exit, r.lane);
    TEST_ASSERT_EQUAL_STRING("ZH98765", r.plate.c_str());
    TEST_ASSERT_EQUAL_STRING("betrieb", r.user.c_str());
    TEST_ASSERT_EQUAL_STRING("Kulanz", r.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("ohne Zahlung", r.note.c_str());
}

void test_codec_rejects_invalid_files()
{
    EventLog log;
    log.add(event(1, EventType::SafetyStop, GateLane::Entry));
    TEST_ASSERT_FALSE(eventsFromJson("kaputt", log));
    TEST_ASSERT_FALSE(eventsFromJson("{\"events\":[{\"t\":1,\"type\":\"party\",\"lane\":\"entry\",\"kz\":\"\","
                                     "\"user\":\"\",\"reason\":\"\",\"note\":\"\"}]}",
                                     log));
    TEST_ASSERT_FALSE(eventsFromJson("{\"events\":[{\"t\":1,\"type\":\"safety_stop\"}]}", log));
    TEST_ASSERT_EQUAL_UINT(1, log.all().size());
    TEST_ASSERT_TRUE(eventsFromJson("{\"events\":[]}", log));
    TEST_ASSERT_EQUAL_UINT(0, log.all().size());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_add_keeps_order_and_drops_oldest);
    RUN_TEST(test_labels_and_names);
    RUN_TEST(test_codec_round_trip);
    RUN_TEST(test_codec_rejects_invalid_files);
    return UNITY_END();
}
