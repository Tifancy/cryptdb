#pragma once

#include <algorithm>
#include <util/onions.hh>
#include <util/cryptdb_log.hh>
#include <main/schema.hh>
#include <main/rewrite_ds.hh>
#include <parser/embedmysql.hh>
#include <parser/stringify.hh>

/***************************************************/

extern "C" void *create_embedded_thd(int client_flag);

class ReturnField {
public:
    ReturnField(bool is_salt, const std::string &field_called,
                const OLK &olk, int salt_pos)
        : is_salt(is_salt), field_called(field_called), olk(olk),
          salt_pos(salt_pos) {}

    bool getIsSalt() const {return is_salt;}
    std::string fieldCalled() const {return field_called;}
    const OLK getOLK() const {return olk;}
    int getSaltPosition() const {return salt_pos;}
    std::string stringify();

private:
    const bool is_salt;
    const std::string field_called;
    const OLK olk;      // if !olk.key, field is not encrypted
    const int salt_pos; // position of salt of this field in
                        // the query results, or -1 if such
                        // salt was not requested
};

typedef struct ReturnMeta {
    std::map<int, ReturnField> rfmeta;
    std::string stringify();
} ReturnMeta;


class OnionAdjustExcept {
public:
    OnionAdjustExcept(const TableMeta &tm, const FieldMeta &fm, onion o,
                      SECLEVEL l)
        : tm(tm), fm(fm), o(o), tolevel(l) {}

    const TableMeta &tm;
    const FieldMeta &fm;
    const onion o;
    const SECLEVEL tolevel;
};

// TODO: Maybe we want a database name argument/member.
typedef class ConnectionInfo {
public:
    std::string server;
    uint port;
    std::string user;
    std::string passwd;

    ConnectionInfo(const std::string &s, const std::string &u,
                   const std::string &p, uint port = 0)
        : server(s), port(port), user(u), passwd(p) {};
    ConnectionInfo() : server(""), port(0), user(""), passwd("") {};
} ConnectionInfo;

class ProxyState;

// state maintained at the proxy
typedef struct SharedProxyState {
    SharedProxyState(ConnectionInfo ci, const std::string &embed_dir,
                     const std::string &master_key,
                     SECURITY_RATING default_sec_rating);
    ~SharedProxyState();
    SECURITY_RATING defaultSecurityRating() const
    {
        return default_sec_rating;
    }

    const std::unique_ptr<AES_KEY> &getMasterKey() const
    {
        return masterKey;
    }
    const std::unique_ptr<Connect> &getConn() const {return conn;}
    SchemaCache &getSchemaCache() {return cache;}
    static int db_init(const std::string &embed_dir);

    friend class ProxyState;

private:
    const std::unique_ptr<AES_KEY> masterKey;
    const std::string &embed_dir;
    const int mysql_dummy;
    const std::unique_ptr<Connect> conn;
    const SECURITY_RATING default_sec_rating;
    SchemaCache cache;
} SharedProxyState;

class ProxyState {
public:
    ProxyState(SharedProxyState &shared)
        : shared(shared),
          e_conn(Connect::getEmbedded(shared.embed_dir)) {}
    ~ProxyState();

    SECURITY_RATING defaultSecurityRating() const;
    const std::unique_ptr<AES_KEY> &getMasterKey() const;
    const std::unique_ptr<Connect> &getConn() const;
    const std::unique_ptr<Connect> &getEConn() const;
    void safeCreateEmbeddedTHD();
    void dumpTHDs();
    SchemaCache &getSchemaCache() {return shared.cache;}
    std::shared_ptr<const SchemaInfo> getSchemaInfo()
        {return shared.cache.getSchema(this->getConn(), this->getEConn());}

private:
    SharedProxyState &shared;
    const std::unique_ptr<Connect> e_conn;
    std::vector<std::unique_ptr<THD, void (*)(THD *)> > thds;
};

extern __thread ProxyState *thread_ps;

// For REPLACE and DELETE we are duplicating the MetaKey information.
class Delta {
public:
    enum TableType {REGULAR_TABLE, BLEEDING_TABLE};

    Delta(const DBMeta &parent_meta) : parent_meta(parent_meta) {}
    virtual ~Delta() {}

    /*
     * Take the update action against the database. Contains high level
     * serialization semantics.
     */
    virtual bool apply(const std::unique_ptr<Connect> &e_conn,
                       TableType table_type) = 0;

protected:
    const DBMeta &parent_meta;

    std::string tableNameFromType(TableType table_type) const;
};

// CreateDelta calls must provide the key.  meta and
// parent_meta have not yet been associated such that the key can be
// functionally derived.
template <typename KeyType>
class AbstractCreateDelta : public Delta {
public:
    AbstractCreateDelta(const DBMeta &parent_meta,
                        const KeyType &key)
        : Delta(parent_meta), key(key) {}

protected:
    const KeyType key;
};

class CreateDelta : public AbstractCreateDelta<IdentityMetaKey> {
public:
    CreateDelta(std::unique_ptr<DBMeta> &&meta,
                const DBMeta &parent_meta,
                IdentityMetaKey key)
        : AbstractCreateDelta(parent_meta, key), meta(std::move(meta)) {}

    bool apply(const std::unique_ptr<Connect> &e_conn,
               TableType table_type);

private:
    const std::unique_ptr<DBMeta> meta;
};

class DerivedKeyDelta : public Delta {
public:
    DerivedKeyDelta(const DBMeta &meta,
                    const DBMeta &parent_meta)
        : Delta(parent_meta), meta(meta),
          key(parent_meta.getKey(meta))
    {}

protected:
    const DBMeta &meta;
    const AbstractMetaKey &key;
};

class ReplaceDelta : public DerivedKeyDelta {
public:
    ReplaceDelta(const DBMeta &meta, const DBMeta &parent_meta)
        : DerivedKeyDelta(meta, parent_meta) {}

    bool apply(const std::unique_ptr<Connect> &e_conn,
               TableType table_type);
};

class DeleteDelta : public DerivedKeyDelta {
public:
    DeleteDelta(const DBMeta &meta, const DBMeta &parent_meta)
        : DerivedKeyDelta(meta, parent_meta) {}

    bool apply(const std::unique_ptr<Connect> &e_conn,
               TableType table_type);
};

class Rewriter;

enum class CompletionType {DDLCompletion, AdjustOnionCompletion};

bool
writeDeltas(const std::unique_ptr<Connect> &e_conn,
            const std::vector<std::unique_ptr<Delta> > &deltas,
            Delta::TableType table_type);
void
deltaOutputBeforeQuery(const std::unique_ptr<Connect> &e_conn,
                       const std::string &original_query,
                       const std::vector<std::unique_ptr<Delta> > &deltas,
                       CompletionType completion_type,
                       uint64_t *const embedded_completion_id);

void
deltaOutputAfterQuery(const std::unique_ptr<Connect> &e_conn,
                      const std::vector<std::unique_ptr<Delta> > &deltas,
                      uint64_t embedded_completion_id);

bool setRegularTableToBleedingTable(const std::unique_ptr<Connect> &e_conn);
bool setBleedingTableToRegularTable(const std::unique_ptr<Connect> &e_conn);

class RewritePlan;
class Analysis {
    Analysis() = delete;
    Analysis(const Analysis &a) = delete;
    Analysis(Analysis &&a) = delete;
    Analysis &operator=(const Analysis &a) = delete;
    Analysis &operator=(Analysis &&a) = delete;

public:
    Analysis(const std::string &default_db, const SchemaInfo &schema)
        : pos(0), inject_alias(false),
          db_name(default_db), schema(schema) {}

    unsigned int pos; // > a counter indicating how many projection
                      // fields have been analyzed so far
    std::map<const FieldMeta *, const salt_type> salts;
    std::map<const Item *, std::unique_ptr<RewritePlan> > rewritePlans;
    std::map<std::string, std::map<const std::string, const std::string>>
        table_aliases;
    std::map<const Item_field *, std::pair<Item_field *, OLK>> item_cache;

    // information for decrypting results
    ReturnMeta rmeta;

    bool inject_alias;

    // These functions are prefered to their lower level counterparts.
    bool addAlias(const std::string &alias, const std::string &db,
                  const std::string &table);
    OnionMeta &getOnionMeta(const std::string &db,
                            const std::string &table,
                            const std::string &field, onion o) const;
    OnionMeta &getOnionMeta(const FieldMeta &fm, onion o) const;
    FieldMeta &getFieldMeta(const std::string &db,
                            const std::string &table,
                            const std::string &field) const;
    FieldMeta &getFieldMeta(const TableMeta &tm,
                            const std::string &field) const;
    TableMeta &getTableMeta(const std::string &db,
                            const std::string &table) const;
    DatabaseMeta &getDatabaseMeta(const std::string &db) const;
    bool tableMetaExists(const std::string &db,
                         const std::string &table) const;
    bool nonAliasTableMetaExists(const std::string &db,
                                 const std::string &table) const;
    bool databaseMetaExists(const std::string &db) const;
    std::string getAnonTableName(const std::string &db,
                                 const std::string &table,
                                 bool *const is_alias=NULL) const;
    std::string
        translateNonAliasPlainToAnonTableName(const std::string &db,
                                              const std::string &table)
        const;
    std::string getAnonIndexName(const std::string &db,
                                 const std::string &table,
                                 const std::string &index_name,
                                 onion o) const;
    std::string getAnonIndexName(const TableMeta &tm,
                                 const std::string &index_name,
                                 onion o) const;
    static const EncLayer &getBackEncLayer(const OnionMeta &om);
    static SECLEVEL getOnionLevel(const OnionMeta &om);
    SECLEVEL getOnionLevel(const FieldMeta &fm, onion o);
    static const std::vector<std::unique_ptr<EncLayer> > &
        getEncLayers(const OnionMeta &om);
    const SchemaInfo &getSchema() {return schema;}

    std::vector<std::unique_ptr<Delta> > deltas;

    std::string getDatabaseName() const {return db_name;}

    // access to isAlias(...)
    friend class MultiDeleteHandler;

private:
    const std::string db_name;
    const SchemaInfo &schema;

    bool isAlias(const std::string &db,
                 const std::string &table) const;
    std::string unAliasTable(const std::string &db,
                             const std::string &table) const;
};

bool
lowLevelGetCurrentDatabase(const std::unique_ptr<Connect> &c,
                           std::string *const out_db);

bool
lowLevelSetCurrentDatabase(const std::unique_ptr<Connect> &c,
                           const std::string &db);

std::vector<std::string>
getAllUDFs();

std::string
lexToQuery(const LEX &lex);

