#include <parser/sql_utils.hh>



using namespace std;

static bool lib_initialized = false;

void
init_mysql(const string & embed_db)
{
    if (lib_initialized) {
	return;
    }
    char dir_arg[1024];
    snprintf(dir_arg, sizeof(dir_arg), "--datadir=%s", embed_db.c_str());

    const char *mysql_av[] =
    { "progname",
            "--skip-grant-tables",
            dir_arg,
            /* "--skip-innodb", */
            /* "--default-storage-engine=MEMORY", */
            "--character-set-server=utf8",
            "--language=" MYSQL_BUILD_DIR "/sql/share/"
    };

    assert(0 == mysql_library_init(sizeof(mysql_av) / sizeof(mysql_av[0]),
				   (char**) mysql_av, 0));

    assert(0 == mysql_thread_init());

    lib_initialized = true;
}

bool
isTableField(string token)
{
    size_t pos = token.find(".");

    if (pos == string::npos) {
        return false;
    } else {
        return true;
    }
}

// NOTE: Use FieldMeta::fullName if you know what onion's full name you
// need.
string
fullName(string field, string table)
{
    if (isTableField(field)) {
        return field;
    } else {
        return table + "." + field;
    }
}

char *
make_thd_string(const string &s, size_t *lenp)
{
    THD *thd = current_thd;
    assert(thd);
    if (lenp)
        *lenp = s.size();
    return thd->strmake(s.data(), s.size());
}

string
ItemToString(Item * i) {
    assert(i);
    String s;
    String *s0 = i->val_str(&s);
    std::string ret;
    if (NULL == s0) {
        assert(i->is_null());
        ret = std::string("NULL");
    } else {
        ret = string(s0->ptr(), s0->length());
    }
    return ret;
}
