#include "testing_dsl.h"

// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//                Unit Tests
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

TEST(test_pushvalue)
    const char *s0 = "hello";
    const char *s1 = "world\0w0rld"; unsigned int s1_len = 11;
    const char *s2 = "paradise";
    pushvalue(L, s0, strlen(s0));
    pushvalue(L, NULL, -1);
    pushvalue(L, s1, s1_len);
    pushvalue(L, s1, strlen(s1));
    pushvalue(L, NULL, 1);
    pushvalue(L, s2, strlen(s2));

    TEST_ASSERT(!strcmp(s2, lua_tolstring(L, -1, NULL)));
    TEST_ASSERT(NULL == lua_tolstring(L, -2, NULL));
    TEST_ASSERT(!strcmp(s1, lua_tolstring(L, -3, NULL)));
    TEST_ASSERT(!memcmp(s1, lua_tolstring(L, -4, NULL), s1_len));
    TEST_ASSERT(NULL == lua_tolstring(L, -5, NULL));
    TEST_ASSERT(!strcmp(s0, lua_tolstring(L, -6, NULL)));
END_TEST

TEST(test_luaToCharp)
    const unsigned int number = 10;
    const char *const s0      = "red";
    const char *const s1      = "green\0purple"; unsigned s1_len = 11;
    const char *const s2      = "blue";

    lua_pushnil(L);
    lua_pushstring(L, s0);
    lua_pushlstring(L, s1, s1_len);
    lua_pushstring(L, s1);
    lua_pushnumber(L, number);

    lua_pushstring(L, s2);
    TEST_ASSERT(!strcmp(luaToCharp(L, -1), s2));

    lua_pushstring(L, s0);
    TEST_ASSERT(!strcmp(luaToCharp(L, -1), s0));

    TEST_ASSERT(number == lua_tointeger(L, -3));
    TEST_ASSERT(!strcmp(luaToCharp(L, -4), s1));
    TEST_ASSERT(!memcmp(luaToCharp(L, -5), s1, s1_len));
    TEST_ASSERT(!strcmp(luaToCharp(L, -6), s0));
    TEST_ASSERT(!strcmp(luaToCharp(L, -7), ""));
END_TEST

TEST(test_waitForCommand)
    struct LuaQuery lua_query;
    lua_query.persist.ell = (void *)0x01;
    lua_query.command_ready = true;
    waitForCommand(&lua_query);
END_TEST

TEST(test_completeLuaQuery)
    const unsigned int init_command     = RESULTS;
    const pthread_t init_thread         = (pthread_t)0x1;
    const void *const init_host_data    = (void *)0x2;
    const unsigned int init_restarts    = 0x3;
    const bool init_mysql_connected     = false;

    struct LuaQuery lua_query = {
        .command            = init_command,
        .command_ready      = true,
        .completion_signal  = false,
        .thread             = (pthread_t)init_thread,
        .persist            = {
            .restarts           = init_restarts,
            .host_data          = init_host_data,
            .ell                = L,
            .wait               = 1
        },
        .sql                = NULL,
        .output_count       = -1,
        .mysql_connected    = init_mysql_connected
    };

    completeLuaQuery(&lua_query, 0);
    TEST_ASSERT(init_command         == lua_query.command);
    TEST_ASSERT(false                == lua_query.command_ready);
    TEST_ASSERT(true                 == lua_query.completion_signal);
    TEST_ASSERT(init_thread          == lua_query.thread);
    TEST_ASSERT(init_restarts        == lua_query.persist.restarts);
    TEST_ASSERT(init_host_data       == lua_query.persist.host_data);
    TEST_ASSERT(L                    == lua_query.persist.ell);
    TEST_ASSERT(NULL                 == lua_query.sql);
    TEST_ASSERT(COMMAND_OUTPUT_COUNT == lua_query.output_count);
    TEST_ASSERT(init_mysql_connected == lua_query.mysql_connected);
END_TEST

TEST(test_newLuaQuery)
    struct LuaQuery *const lua_query = malloc(sizeof(struct LuaQuery));
    memset(lua_query, 0xDEADBEEF, sizeof(struct LuaQuery));

    const char *const init_host     = strdup(fast_bad_host);
    const char *const init_user     = strdup("martianinvader");
    const char *const init_passwd   = strdup("lophtcrak");
    const unsigned int init_port    = 0x1111;

    struct HostData *host_data = malloc(sizeof(struct HostData));
    TEST_ASSERT(host_data);
    *host_data = (struct HostData){
        .host       = init_host,
        .user       = init_user,
        .passwd     = init_passwd,
        .port       = init_port
    };

    const unsigned int init_wait    = 1;

    TEST_ASSERT(newLuaQuery(lua_query, host_data, init_wait));

    TEST_ASSERT(-1                   == lua_query->command);
    TEST_ASSERT(false                == lua_query->command_ready);
    TEST_ASSERT(false                == lua_query->completion_signal);
    TEST_ASSERT(NO_THREAD            != lua_query->thread);
    TEST_ASSERT(0                    == lua_query->persist.restarts);
    TEST_ASSERT(host_data            == lua_query->persist.host_data);
    TEST_ASSERT(NULL                 == lua_query->persist.ell);
    TEST_ASSERT(init_wait            == lua_query->persist.wait);
    TEST_ASSERT(NULL                 == lua_query->sql);
    TEST_ASSERT(-1                   == lua_query->output_count);
    TEST_ASSERT(false                == lua_query->mysql_connected);

    POSSIBLE_SELF_OWNING_THREAD(lua_query->thread);

    // the thread is using data allocated from this thread and will
    // segfault and shutdown if we do not manually kill.
    TEST_ASSERT(FAILURE != stopLuaQueryThread(lua_query));
END_TEST

TEST(test_createLuaQuery)
    const char *const init_host     = strdup(fast_bad_host);
    const char *const init_user     = strdup("rainbow");
    const char *const init_passwd   = strdup("jetplane");
    const unsigned int init_port    = 0x2222;

    struct HostData *host_data = malloc(sizeof(struct HostData));
    TEST_ASSERT(host_data);
    *host_data = (struct HostData){
        .host       = init_host,
        .user       = init_user,
        .passwd     = init_passwd,
        .port       = init_port
    };
    struct HostData *const orig_host_data = host_data;

    const unsigned int init_wait    = 1;

    struct LuaQuery **const p_lua_query =
        createLuaQuery((struct HostData **)&host_data, init_wait);
    TEST_ASSERT(p_lua_query);
    struct LuaQuery *const lua_query = *p_lua_query;

    TEST_ASSERT(-1                   == lua_query->command);
    TEST_ASSERT(false                == lua_query->command_ready);
    TEST_ASSERT(false                == lua_query->completion_signal);
    TEST_ASSERT(NO_THREAD            != lua_query->thread);
    TEST_ASSERT(0                    == lua_query->persist.restarts);
    TEST_ASSERT(orig_host_data       == lua_query->persist.host_data);
    TEST_ASSERT(NULL                 == lua_query->persist.ell);
    TEST_ASSERT(init_wait            == lua_query->persist.wait);
    TEST_ASSERT(NULL                 == lua_query->sql);
    TEST_ASSERT(-1                   == lua_query->output_count);
    TEST_ASSERT(false                == lua_query->mysql_connected);

    TEST_ASSERT(NULL                == host_data);
    TEST_ASSERT(orig_host_data->host== lua_query->persist.host_data->host);
    TEST_ASSERT(orig_host_data->user== lua_query->persist.host_data->user);
    TEST_ASSERT(orig_host_data->passwd== lua_query->persist.host_data->passwd);
    TEST_ASSERT(orig_host_data->port ==lua_query->persist.host_data->port);

    POSSIBLE_SELF_OWNING_THREAD(lua_query->thread);

    TEST_ASSERT(FAILURE != stopLuaQueryThread(lua_query));
END_TEST

TEST(test_stopLuaQueryThread)
    struct LuaQuery lua_query, test_lua_query;
    memset(&lua_query, 0xFEFEFEFE, sizeof(struct LuaQuery));
    memcpy(&test_lua_query, &lua_query, sizeof(struct LuaQuery));

    test_lua_query.mysql_connected = lua_query.mysql_connected = true;
    test_lua_query.thread = lua_query.thread = (pthread_t)malloc(20);
    test_lua_query.persist.wait = lua_query.persist.wait = 1;

    TEST_ASSERT(FAILURE == stopLuaQueryThread(&lua_query));
    TEST_ASSERT(!memcmp(&lua_query, &test_lua_query,
                        sizeof(struct LuaQuery)));
END_TEST

TEST(test_stopAndStartLuaQueryThread)
    struct LuaQuery *lua_query = malloc(sizeof(struct LuaQuery));
    TEST_ASSERT(lua_query);
    memset(lua_query, 0xDEADBEEF, sizeof(struct LuaQuery));

    struct LuaQuery *const test_lua_query =
        malloc(sizeof(struct LuaQuery));
    TEST_ASSERT(test_lua_query);
    memcpy(test_lua_query, lua_query, sizeof(struct LuaQuery));

    const char *const init_host     = strdup(fast_bad_host);
    const char *const init_user     = strdup("buildings");
    const char *const init_passwd   = strdup("cars");
    const unsigned int init_port    = 0x3333;

    struct HostData *host_data = malloc(sizeof(struct HostData));
    *host_data = (struct HostData){
        .host       = init_host,
        .user       = init_user,
        .passwd     = init_passwd,
        .port       = init_port
    };

    const unsigned int init_wait    = 1;

    lua_query->thread = NO_THREAD;
    test_lua_query->persist.host_data =
        lua_query->persist.host_data         = host_data;
    test_lua_query->persist.wait =
        lua_query->persist.wait              = init_wait;

    TEST_ASSERT(startLuaQueryThread(lua_query));

    POSSIBLE_SELF_OWNING_THREAD(lua_query->thread);

    TEST_ASSERT(FAILURE != stopLuaQueryThread(lua_query));
    test_lua_query->thread = lua_query->thread;
    TEST_ASSERT(!memcmp(lua_query, test_lua_query,
                        sizeof(struct LuaQuery)));
END_TEST

TEST(test_destroyLuaQuery)
    struct HostData *host_data =
        createHostData(fast_bad_host, "else", "isunderthebed!", 199);
    TEST_ASSERT(host_data);

    const unsigned int init_wait = 1;
    struct LuaQuery **p_lua_query = createLuaQuery(&host_data, init_wait);
    TEST_ASSERT(p_lua_query && *p_lua_query);

    POSSIBLE_SELF_OWNING_THREAD((*p_lua_query)->thread);

    destroyLuaQuery(&p_lua_query);

    TEST_ASSERT(NULL == p_lua_query);
END_TEST

TEST(test_restartLuaQuery)
    TEST_ASSERT(MAX_RESTARTS > 0);

    const char *const init_host     = strdup(fast_bad_host);
    const char *const init_user     = strdup("where");
    const char *const init_passwd   = strdup("vacation?");
    const unsigned int init_port    = 0x9999;

    struct HostData **p_host_data = malloc(sizeof(struct HostData *));
    TEST_ASSERT(p_host_data);
    struct HostData *host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(host_data);
    p_host_data = &host_data;

    const unsigned init_wait        = 1;
    struct LuaQuery **p_lua_query = createLuaQuery(p_host_data, init_wait);
    TEST_ASSERT(p_lua_query && *p_lua_query);

    TEST_ASSERT(-1                  == (*p_lua_query)->command);
    TEST_ASSERT(NO_THREAD           != (*p_lua_query)->thread);

    POSSIBLE_SELF_OWNING_THREAD((*p_lua_query)->thread);

    TEST_ASSERT(RESTARTED == restartLuaQueryThread(p_lua_query));
    TEST_ASSERT(false            == (*p_lua_query)->command_ready);
    TEST_ASSERT(false            == (*p_lua_query)->completion_signal);
    TEST_ASSERT(-1               == (*p_lua_query)->command);
    TEST_ASSERT(NULL             == (*p_lua_query)->sql);
    TEST_ASSERT(-1               == (*p_lua_query)->output_count);
    TEST_ASSERT(false            == (*p_lua_query)->mysql_connected);
    TEST_ASSERT(NO_THREAD        != (*p_lua_query)->thread);
    TEST_ASSERT(!strcmp(init_host,
                        (*p_lua_query)->persist.host_data->host));
    TEST_ASSERT(!strcmp(init_user,
                        (*p_lua_query)->persist.host_data->user));
    TEST_ASSERT(!strcmp(init_passwd,
                        (*p_lua_query)->persist.host_data->passwd));
    TEST_ASSERT(init_port      == (*p_lua_query)->persist.host_data->port);
    TEST_ASSERT(1                == (*p_lua_query)->persist.restarts);
    TEST_ASSERT(NULL             == (*p_lua_query)->persist.ell);
    TEST_ASSERT(init_wait       == (*p_lua_query)->persist.wait);

    POSSIBLE_SELF_OWNING_THREAD((*p_lua_query)->thread);

    TEST_ASSERT(FAILURE != stopLuaQueryThread(*p_lua_query));
END_TEST

TEST(test_restartBadLuaQuery)
    TEST_ASSERT(MAX_RESTARTS > 0)

    const char *const init_host     = strdup(fast_bad_host);
    const char *const init_user     = strdup("saysomething");
    const char *const init_passwd   = strdup("quickly?");
    const unsigned int init_port    = 0xFFFF;

    struct HostData **p_host_data = malloc(sizeof(struct HostData *));
    TEST_ASSERT(p_host_data);
    struct HostData *host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(host_data);
    p_host_data = &host_data;

    const unsigned int init_wait    = 1;
    struct LuaQuery **p_lua_query = createLuaQuery(p_host_data, init_wait);
    TEST_ASSERT(p_lua_query && *p_lua_query);
    struct LuaQuery *lua_query = *p_lua_query;

    POSSIBLE_SELF_OWNING_THREAD(lua_query->thread);

    // This should cause stopLuaQueryThread to fail.
    pthread_cancel(lua_query->thread);
    lua_query->thread = (pthread_t)malloc(20);
    memset((void *)lua_query->thread, 0x12345678, 4);

    TEST_ASSERT(RESTARTED == restartLuaQueryThread(&lua_query));

    TEST_ASSERT(FAILURE != stopLuaQueryThread(lua_query));
    // POSSIBLE_SELF_OWNING_THREAD(lua_query->thread);
END_TEST

void *
dummyThread(void *const unused)
{
    assert(!pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
    assert(!pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL));

    while (1);
}

TEST(test_badIssueCommand)
    TEST_ASSERT(MAX_RESTARTS > 0)

    const char *const init_host     = fast_bad_host;
    const char *const init_user     = "going";
    const char *const init_passwd   = "tophail?";
    const unsigned int init_port    = 0xEEEE;

    struct HostData *host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(host_data);

    const unsigned int init_wait    = 1;
    struct LuaQuery **p_lua_query = createLuaQuery(&host_data, init_wait);
    TEST_ASSERT(p_lua_query && *p_lua_query);

    // soft kill should fail, then hard kill should succeed
    TEST_ASSERT(!pthread_create(&(*p_lua_query)->thread, NULL, dummyThread,
                                NULL));
    issueCommand(L, KILL, p_lua_query);
    TEST_ASSERT(true           == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
    TEST_ASSERT(false                   == (*p_lua_query)->command_ready);
    TEST_ASSERT(false                == (*p_lua_query)->completion_signal);
    TEST_ASSERT(COMMAND_OUTPUT_COUNT    == (*p_lua_query)->output_count);
    TEST_ASSERT(false                  == (*p_lua_query)->mysql_connected);
    TEST_ASSERT(NULL                    == (*p_lua_query)->persist.ell);
    TEST_ASSERT(init_wait               == (*p_lua_query)->persist.wait);

    // test restart attempts
    size_t i = 0;
    for (; i < MAX_RESTARTS; ++i) {
        issueQuery(L, QUERY, "SELECT SOMETHING", p_lua_query);
        TEST_ASSERT(false      == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
        TEST_ASSERT(NO_THREAD               != (*p_lua_query)->thread);
        TEST_ASSERT(COMMAND_OUTPUT_COUNT  == (*p_lua_query)->output_count);
        TEST_ASSERT(false              == (*p_lua_query)->mysql_connected);
        TEST_ASSERT(NULL                   == (*p_lua_query)->persist.ell);
        TEST_ASSERT(init_wait             == (*p_lua_query)->persist.wait);

        TEST_ASSERT(FAILURE != stopLuaQueryThread(*p_lua_query));
        TEST_ASSERT(NO_THREAD               == (*p_lua_query)->thread);
        TEST_ASSERT(COMMAND_OUTPUT_COUNT  == (*p_lua_query)->output_count);
        TEST_ASSERT(false             == (*p_lua_query)->mysql_connected);

        TEST_ASSERT(i+1               ==(*p_lua_query)->persist.restarts);
    }

    // test for sane zombie
    issueQuery(L, QUERY, strdup("worldly"), p_lua_query);
    TEST_ASSERT(false          == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
    TEST_ASSERT(NO_THREAD               == (*p_lua_query)->thread)
    TEST_ASSERT(COMMAND_OUTPUT_COUNT    == (*p_lua_query)->output_count);
    TEST_ASSERT(false                  == (*p_lua_query)->mysql_connected);
    TEST_ASSERT(MAX_RESTARTS          == (*p_lua_query)->persist.restarts);
    TEST_ASSERT(NULL                    == (*p_lua_query)->persist.ell);
    TEST_ASSERT(init_wait               == (*p_lua_query)->persist.wait);

    TEST_ASSERT(NOTHING_TO_STOP == stopLuaQueryThread((*p_lua_query)));
    TEST_ASSERT(NO_THREAD               == (*p_lua_query)->thread)
    TEST_ASSERT(COMMAND_OUTPUT_COUNT    == (*p_lua_query)->output_count);
    TEST_ASSERT(false                  == (*p_lua_query)->mysql_connected);
    TEST_ASSERT(MAX_RESTARTS          == (*p_lua_query)->persist.restarts);

    issueCommand(L, KILL, p_lua_query);
    TEST_ASSERT(true           == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
    TEST_ASSERT(NO_THREAD               == (*p_lua_query)->thread);
    TEST_ASSERT(COMMAND_OUTPUT_COUNT    == (*p_lua_query)->output_count);
    TEST_ASSERT(false                  == (*p_lua_query)->mysql_connected);
    TEST_ASSERT(MAX_RESTARTS          == (*p_lua_query)->persist.restarts);
    TEST_ASSERT(NULL                    == (*p_lua_query)->persist.ell);
    TEST_ASSERT(init_wait               == (*p_lua_query)->persist.wait);

    TEST_ASSERT(!strcmp(init_host,
                        (*p_lua_query)->persist.host_data->host));
    TEST_ASSERT(!strcmp(init_user,
                        (*p_lua_query)->persist.host_data->user));
    TEST_ASSERT(!strcmp(init_passwd,
                        (*p_lua_query)->persist.host_data->passwd));
    TEST_ASSERT(init_port == (*p_lua_query)->persist.host_data->port);
END_TEST

TEST(test_handleKilledQuery)
    TEST_ASSERT(MAX_RESTARTS > 0)

    const char *const init_host     = slow_bad_host;
    const char *const init_user     = real_user;
    const char *const init_passwd   = real_passwd;
    const unsigned int init_port    = real_port;

    struct HostData *bad_host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(bad_host_data);

    const unsigned int init_wait    = 1;
    struct LuaQuery **p_bad_lua_query =
        createLuaQuery(&bad_host_data, init_wait);
    TEST_ASSERT(p_bad_lua_query && *p_bad_lua_query);

    issueQuery(L, QUERY, strdup("do 0"), p_bad_lua_query);
    TEST_ASSERT(false         == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
    TEST_ASSERT(NO_THREAD            != (*p_bad_lua_query)->thread);
    TEST_ASSERT(COMMAND_OUTPUT_COUNT == (*p_bad_lua_query)->output_count);
    TEST_ASSERT(false         == (*p_bad_lua_query)->mysql_connected);
    TEST_ASSERT(1             == (*p_bad_lua_query)->persist.restarts);
    TEST_ASSERT(NULL                 == (*p_bad_lua_query)->persist.ell);
    TEST_ASSERT(init_wait            == (*p_bad_lua_query)->persist.wait);

    POSSIBLE_SELF_OWNING_THREAD((*p_bad_lua_query)->thread);

    struct HostData *good_host_data =
        createHostData(real_host, real_user, real_passwd, real_port);
    TEST_ASSERT(good_host_data);

    struct LuaQuery *good_lua_query = malloc(sizeof(struct LuaQuery));
    TEST_ASSERT(good_lua_query);
    memcpy(good_lua_query, *p_bad_lua_query, sizeof(struct LuaQuery));
    good_lua_query->persist.host_data = good_host_data;

    issueCommand(L, RESULTS, &good_lua_query);
    UGLY_SLEEP

    TEST_ASSERT(false          == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
    TEST_ASSERT(NO_THREAD            != good_lua_query->thread);
    TEST_ASSERT(COMMAND_OUTPUT_COUNT == good_lua_query->output_count);
    TEST_ASSERT(true                 == good_lua_query->mysql_connected);
    TEST_ASSERT(2                    == good_lua_query->persist.restarts);
    TEST_ASSERT(NULL                 == good_lua_query->persist.ell);
    TEST_ASSERT(init_wait            == good_lua_query->persist.wait);

    issueCommand(L, KILL, &good_lua_query);
    TEST_ASSERT(true          == lua_toboolean(L, -COMMAND_OUTPUT_COUNT));
    TEST_ASSERT(NO_THREAD            != good_lua_query->thread);
    TEST_ASSERT(COMMAND_OUTPUT_COUNT == good_lua_query->output_count);
    TEST_ASSERT(true                 == good_lua_query->mysql_connected);
    TEST_ASSERT(2                    == good_lua_query->persist.restarts);
    TEST_ASSERT(NULL                 == good_lua_query->persist.ell);
    TEST_ASSERT(init_wait            == good_lua_query->persist.wait);
END_TEST

TEST(test_zombie)
    struct LuaQuery lua_query, test_lua_query;
    memset(&lua_query, 0x11111111, sizeof(struct LuaQuery));
    memcpy(&test_lua_query, &lua_query, sizeof(struct LuaQuery));

    TEST_ASSERT(MAX_RESTARTS >= 0);
    if (MAX_RESTARTS > 0) {
        test_lua_query.persist.restarts =
            lua_query.persist.restarts =        MAX_RESTARTS - 1;
        TEST_ASSERT(false == zombie(&lua_query));
        TEST_ASSERT(!memcmp(&lua_query, &test_lua_query,
                       sizeof(struct LuaQuery)));
    }

    test_lua_query.persist.restarts =
        lua_query.persist.restarts =            MAX_RESTARTS;
    TEST_ASSERT(true == zombie(&lua_query));
    TEST_ASSERT(!memcmp(&lua_query, &test_lua_query, sizeof(struct LuaQuery)));

    // will cause abort
    /*
    test_lua_query.persist.restarts =
        lua_query.persist.restarts =            MAX_RESTARTS + 1;
    TEST_ASSERT(true == zombie(&lua_query));
    TEST_ASSERT(!memcmp(&lua_query, &test_lua_query, sizeof(struct LuaQuery)));
    */
END_TEST

TEST(test_createHostData)
    const char *const init_host     = "birdsandplains";
    const char *const init_user     = "automobiles";
    const char *const init_passwd   = "sendandreceive";
    const unsigned int init_port    = 0x9898;

    const struct HostData test_host_data = {
        .host           = init_host,
        .user           = init_user,
        .passwd         = init_passwd,
        .port           = init_port
    };

    struct HostData *const host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(host_data);

    TEST_ASSERT(!strcmp(test_host_data.host, host_data->host));
    TEST_ASSERT(!strcmp(test_host_data.user, host_data->user));
    TEST_ASSERT(!strcmp(test_host_data.passwd, host_data->passwd));
    TEST_ASSERT(test_host_data.port == host_data->port);
    TEST_ASSERT(init_port           == host_data->port);

    // make sure createHostData didn't mutate values it doesn't own
    TEST_ASSERT(init_host    != host_data->host);
    TEST_ASSERT(init_user    != host_data->user);
    TEST_ASSERT(init_passwd  != host_data->passwd);

    TEST_ASSERT(!strcmp(test_host_data.host, init_host));
    TEST_ASSERT(!strcmp(test_host_data.user, init_user));
    TEST_ASSERT(!strcmp(test_host_data.passwd, init_passwd));
END_TEST

TEST(test_destroyHostData)
    struct HostData *host_data =
        createHostData("some", "data", "willdo", 1005);
    TEST_ASSERT(host_data);

    destroyHostData(&host_data);
    TEST_ASSERT(NULL == host_data);
END_TEST

TEST(test_deepCopyLuaQuery)
    const char *const init_host         = "some-host";
    const char *const init_user         = "a-user";
    const char *const init_passwd       = "dat-passwd";
    const unsigned int init_port        = 0x7654;

    struct HostData *host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(host_data);

    const unsigned int init_wait        = 1;
    struct LuaQuery **p_lua_query = createLuaQuery(&host_data, init_wait);
    TEST_ASSERT(p_lua_query && *p_lua_query);
    TEST_ASSERT(NULL == host_data);
    struct LuaQuery *const lua_query = *p_lua_query;
    POSSIBLE_SELF_OWNING_THREAD(lua_query->thread);

    struct LuaQuery *const cp_lua_query = deepCopyLuaQuery(*p_lua_query);
    TEST_ASSERT(cp_lua_query);

    TEST_ASSERT(cp_lua_query->command           == lua_query->command);
    TEST_ASSERT(cp_lua_query->command_ready   == lua_query->command_ready);
    TEST_ASSERT(cp_lua_query->completion_signal
                    == lua_query->completion_signal);
    TEST_ASSERT(cp_lua_query->thread            == lua_query->thread);
    TEST_ASSERT(cp_lua_query->persist.restarts
                    == lua_query->persist.restarts);
    // deep copied
    TEST_ASSERT(cp_lua_query->persist.host_data
                    != lua_query->persist.host_data);
    TEST_ASSERT(cp_lua_query->persist.host_data->host
                    != lua_query->persist.host_data->host);
    TEST_ASSERT(cp_lua_query->persist.host_data->user
                    != lua_query->persist.host_data->user);
    TEST_ASSERT(cp_lua_query->persist.host_data->passwd
                    != lua_query->persist.host_data->passwd);
    TEST_ASSERT(!strcmp(cp_lua_query->persist.host_data->host,
                        lua_query->persist.host_data->host));
    TEST_ASSERT(!strcmp(cp_lua_query->persist.host_data->user,
                        lua_query->persist.host_data->user));
    TEST_ASSERT(!strcmp(cp_lua_query->persist.host_data->passwd,
                        lua_query->persist.host_data->passwd));
    TEST_ASSERT(cp_lua_query->persist.host_data->port
                    == lua_query->persist.host_data->port);

    TEST_ASSERT(cp_lua_query->persist.ell == lua_query->persist.ell);
    TEST_ASSERT(NULL == cp_lua_query->persist.ell);
    TEST_ASSERT(cp_lua_query->persist.wait == lua_query->persist.wait);

    // deep copied
    TEST_ASSERT(cp_lua_query->sql != lua_query->sql
                || NULL == cp_lua_query->sql);
    if (cp_lua_query->sql) {
        TEST_ASSERT(!strcmp(cp_lua_query->sql, lua_query->sql));
    }

    TEST_ASSERT(cp_lua_query->output_count == lua_query->output_count);

    destroyLuaQuery(&p_lua_query);
END_TEST

TEST(test_undoDeepCopyLuaQuery)
    const char *const init_host         = fast_bad_host;
    const char *const init_user         = "kansisc9tymichigan";
    const char *const init_passwd       = "uknow";
    const unsigned int init_port        = 0x4589;

    struct HostData *host_data =
        createHostData(init_host, init_user, init_passwd, init_port);
    TEST_ASSERT(host_data);

    const unsigned int init_wait        = 1;
    struct LuaQuery **p_lua_query = createLuaQuery(&host_data, init_wait);
    TEST_ASSERT(p_lua_query && *p_lua_query);
    TEST_ASSERT(NULL == host_data);
    POSSIBLE_SELF_OWNING_THREAD((*p_lua_query)->thread);
    struct LuaQuery *save_lua_query = malloc(sizeof(struct LuaQuery));
    TEST_ASSERT(save_lua_query);
    memcpy(save_lua_query, *p_lua_query, sizeof(struct LuaQuery));

    struct LuaQuery *cp_lua_query = deepCopyLuaQuery(*p_lua_query);
    TEST_ASSERT(cp_lua_query);

    struct LuaQuery *const save_cp_lua_query = cp_lua_query;
    undoDeepCopyLuaQuery(&cp_lua_query);
    // deleted the copy
    TEST_ASSERT(NULL == cp_lua_query);
    TEST_ASSERT(NULL == save_cp_lua_query->sql);
    TEST_ASSERT(NULL == save_cp_lua_query->persist.host_data);

    // left the original intact
    TEST_ASSERT(p_lua_query && *p_lua_query);
    TEST_ASSERT((*p_lua_query)->persist.host_data);

    NON_DETERMINISM
    save_lua_query->mysql_connected = (*p_lua_query)->mysql_connected;

    TEST_ASSERT(!memcmp(*p_lua_query, save_lua_query,
                        sizeof(struct LuaQuery)));
    TEST_ASSERT(!strcmp((*p_lua_query)->persist.host_data->host,
                        save_lua_query->persist.host_data->host));
    TEST_ASSERT(!strcmp((*p_lua_query)->persist.host_data->user,
                        save_lua_query->persist.host_data->user));
    TEST_ASSERT(!strcmp((*p_lua_query)->persist.host_data->passwd,
                        save_lua_query->persist.host_data->passwd));
    TEST_ASSERT((*p_lua_query)->persist.host_data->port
                    == save_lua_query->persist.host_data->port);
END_TEST

// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//                 Helpers
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
static int
all(struct lua_State *const L)
{
    test_pushvalue(L);
    test_luaToCharp(L);
    test_waitForCommand(L);
    test_completeLuaQuery(L);
    test_newLuaQuery(L);
    test_createLuaQuery(L);
    test_stopLuaQueryThread(L);
    test_stopAndStartLuaQueryThread(L);
    test_destroyLuaQuery(L);
    test_restartLuaQuery(L);
    test_restartBadLuaQuery(L);
    test_badIssueCommand(L);
    test_handleKilledQuery(L);
    test_zombie(L);
    test_createHostData(L);
    test_destroyHostData(L);
    test_deepCopyLuaQuery(L);
    test_undoDeepCopyLuaQuery(L);

    printTestStats();
    waitForSelfOwningThreads();
}


// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
//                 Exports
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// ;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

static const struct luaL_reg
test_lib[] = {
#define F(n) { #n, n }
    F(test_pushvalue),
    F(all),
    {0, 0},
};

extern int
lua_test_init(lua_State *const L)
{
    luaL_openlib(L, "TestThreadedQuery", test_lib, 0);

    return 1;
}


