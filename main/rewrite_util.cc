#include <memory>

#include <main/rewrite_util.hh>
#include <main/enum_text.hh>
#include <main/rewrite_main.hh>
#include <main/List_helpers.hh>
#include <main/macro_util.hh>
#include <parser/lex_util.hh>
#include <parser/stringify.hh>

extern CItemTypesDir itemTypes;

void
optimize(Item ** const i, Analysis &a) {
   //TODO
/*Item *i0 = itemTypes.do_optimize(*i, a);
    if (i0 != *i) {
        // item i was optimized (replaced) by i0
        if (a.itemRewritePlans.find(*i) != a.itemRewritePlans.end()) {
            a.itemRewritePlans[i0] = a.itemRewritePlans[*i];
            a.itemRewritePlans.erase(*i);
        }
        *i = i0;
    } */
}


// this function should be called at the root of a tree of items
// that should be rewritten
// @item_cache defaults to NULL
Item *
rewrite(Item *const i, const EncSet &req_enc, Analysis &a)
{
    RewritePlan *const rp = getAssert(a.rewritePlans, i);
    assert(rp);
    const EncSet solution = rp->es_out.intersect(req_enc);
    // FIXME: Use version that takes reason, expects 0 children,
    // and lets us indicate what our EncSet does have.
    TEST_NoAvailableEncSet(solution, i->type(), req_enc, rp->r.why_t,
                           NULL, 0);

    return itemTypes.do_rewrite(i, solution.chooseOne(), rp, a);
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t, const Analysis &a)
{
    // Table name can only be empty when grouping a nested join.
    assert(t->table_name || t->nested_join);
    if (t->table_name) {
        const std::string anon_name =
            a.getAnonTableName(std::string(t->table_name,
                               t->table_name_length));
        return rewrite_table_list(t, anon_name);
    } else {
        return copy(t);
    }
}

TABLE_LIST *
rewrite_table_list(const TABLE_LIST * const t,
                   const std::string &anon_name)
{
    TABLE_LIST * const new_t = copy(t);
    new_t->table_name =
        make_thd_string(anon_name, &new_t->table_name_length);
    new_t->alias = make_thd_string(anon_name);
    new_t->next_local = NULL;

    return new_t;
}

// @if_exists: defaults to false.
SQL_I_List<TABLE_LIST>
rewrite_table_list(SQL_I_List<TABLE_LIST> tlist, Analysis &a,
                   bool if_exists)
{
    if (!tlist.elements) {
        return SQL_I_List<TABLE_LIST>();
    }

    TABLE_LIST * tl;
    if (if_exists && (false == a.tableMetaExists(tlist.first->table_name))) {
       tl = copy(tlist.first);
    } else {
       tl = rewrite_table_list(tlist.first, a);
    }

    const SQL_I_List<TABLE_LIST> * const new_tlist =
        oneElemList<TABLE_LIST>(tl);

    TABLE_LIST * prev = tl;
    for (TABLE_LIST *tbl = tlist.first->next_local; tbl;
         tbl = tbl->next_local) {
        TABLE_LIST * new_tbl;
        if (if_exists && (false == a.tableMetaExists(tbl->table_name))) {
            new_tbl = copy(tbl);
        } else {
            new_tbl = rewrite_table_list(tbl, a);
        }

        prev->next_local = new_tbl;
        prev = new_tbl;
    }
    prev->next_local = NULL;

    return *new_tlist;
}

List<TABLE_LIST>
rewrite_table_list(List<TABLE_LIST> tll, Analysis &a)
{
    List<TABLE_LIST> * const new_tll = new List<TABLE_LIST>();

    List_iterator<TABLE_LIST> join_it(tll);

    for (;;) {
        const TABLE_LIST * const t = join_it++;
        if (!t) {
            break;
        }

        TABLE_LIST * const new_t = rewrite_table_list(t, a);
        new_tll->push_back(new_t);

        if (t->nested_join) {
            new_t->nested_join->join_list =
                rewrite_table_list(t->nested_join->join_list, a);
            return *new_tll;
        }

        if (t->on_expr) {
            new_t->on_expr = rewrite(t->on_expr, PLAIN_EncSet, a);
        }

	/* TODO: derived tables
        if (t->derived) {
            st_select_lex_unit *u = t->derived;
            rewrite_select_lex(u->first_select(), a);
        }
	*/
    }

    return *new_tll;
}

/*
 * Helper functions to look up via directory & invoke method.
 */
RewritePlan *
gather(Item * const i, reason &tr, Analysis &a)
{
    return itemTypes.do_gather(i, tr, a);
}

//TODO: need to check somewhere that plain is returned
//TODO: Put in gather helpers file.
void
analyze(Item * const i, Analysis &a)
{
    assert(i != NULL);
    LOG(cdb_v) << "calling gather for item " << *i;
    reason r;
    a.rewritePlans[i] = gather(i, r, a);
}

LEX *
begin_transaction_lex(const std::string &dbname) {
    static const std::string query = "START TRANSACTION;";
    query_parse *begin_parse = new query_parse(dbname, query);
    return begin_parse->lex();
}

LEX *
commit_transaction_lex(const std::string &dbname) {
    static const std::string query = "COMMIT;";
    query_parse *commit_parse = new query_parse(dbname, query);
    return commit_parse->lex();
}

//TODO(raluca) : figure out how to create Create_field from scratch
// and avoid this chaining and passing f as an argument
static Create_field *
get_create_field(const Analysis &a, Create_field * const f,
                 OnionMeta * const om, const std::string &name)
{
    Create_field *new_cf = f;

    // Default value is handled during INSERTion.
    auto save_default = f->def;
    f->def = NULL;

    auto enc_layers = a.getEncLayers(om);
    assert(enc_layers.size() > 0);
    for (auto l : enc_layers) {
        const Create_field * const old_cf = new_cf;
        new_cf = l->newCreateField(old_cf, name);

        if (old_cf != f) {
            delete old_cf;
        }
    }

    // Restore the default so we don't memleak it.
    f->def = save_default;
    return new_cf;
}

// NOTE: The fields created here should have NULL default pointers
// as such is handled during INSERTion.
std::vector<Create_field *>
rewrite_create_field(const FieldMeta * const fm,
                     Create_field * const f, const Analysis &a)
{
    LOG(cdb_v) << "in rewrite create field for " << *f;

    std::vector<Create_field *> output_cfields;

    //check if field is not encrypted
    if (fm->children.empty()) {
        Create_field *const new_f = f->clone(current_thd->mem_root);
        new_f->def = NULL;
        output_cfields.push_back(new_f);
        return output_cfields;
    }

    // create each onion column
    for (auto oit : fm->orderedOnionMetas()) {
        OnionMeta * const om = oit.second;
        Create_field * const new_cf =
            get_create_field(a, f, om, om->getAnonOnionName());

        output_cfields.push_back(new_cf);
    }

    // create salt column
    if (fm->has_salt) {
        THD * const thd         = current_thd;
        Create_field * const f0 = f->clone(thd->mem_root);
        f0->field_name          = thd->strdup(fm->getSaltName().c_str());
        f0->flags               = f0->flags | UNSIGNED_FLAG; // salt is
                                                             // unsigned
        f0->sql_type            = MYSQL_TYPE_LONGLONG;
        f0->length              = 8;
        f0->def                 = NULL;

        output_cfields.push_back(f0);
    }

    return output_cfields;
}

static
const OnionMeta *
getKeyOnionMeta(const FieldMeta * const fm)
{
    std::vector<onion> onions({oOPE, oDET, oPLAIN});
    for (auto it : onions) {
        const OnionMeta * const om = fm->getOnionMeta(it);
        if (NULL != om) {
            return om;
        }
    }

    assert(false);
}

// TODO: Add Key for oDET onion as well.
std::vector<Key *>
rewrite_key(const std::shared_ptr<TableMeta> &tm, Key *const key,
            const Analysis &a)
{
    std::vector<Key *> output_keys;
    Key *const new_key = key->clone(current_thd->mem_root);

    // Set anonymous name.
    const std::string new_name =
        a.getAnonIndexName(tm.get(), convert_lex_str(key->name));
    new_key->name = string_to_lex_str(new_name);

    // Set anonymous columns.
    const auto col_it =
        List_iterator<Key_part_spec>(key->columns);
    new_key->columns =
        reduceList<Key_part_spec>(col_it, List<Key_part_spec>(),
            [tm, a] (List<Key_part_spec> out_field_list,
                        Key_part_spec *const key_part) {
                const std::string field_name =
                    convert_lex_str(key_part->field_name);
                const FieldMeta *const fm =
                    a.getFieldMeta(tm.get(), field_name);
                // FIXME: Should return multiple onions.
                const OnionMeta *const om = getKeyOnionMeta(fm);
                key_part->field_name =
                    string_to_lex_str(om->getAnonOnionName());
                out_field_list.push_back(key_part);
                return out_field_list; /* lambda */
            });
    output_keys.push_back(new_key);

    return output_keys;
}

std::string
bool_to_string(bool b)
{
    if (true == b) {
        return "TRUE";
    } else {
        return "FALSE";
    }
}

bool
string_to_bool(const std::string &s)
{
    if (s == std::string("TRUE") || s == std::string("1")) {
        return true;
    } else if (s == std::string("FALSE") || s == std::string("0")) {
        return false;
    } else {
        throw "unrecognized string in string_to_bool!";
    }
}

List<Create_field>
createAndRewriteField(Analysis &a, const ProxyState &ps,
                      Create_field * const cf,
                      const std::shared_ptr<TableMeta> &tm,
                      bool new_table,
                      List<Create_field> &rewritten_cfield_list)
{
    // -----------------------------
    //         Update FIELD       
    // -----------------------------
    const std::string name = std::string(cf->field_name);
    auto buildFieldMeta =
        [] (const std::string name, Create_field * const cf,
            const ProxyState &ps, const std::shared_ptr<TableMeta> &tm)
    {
        if (Field::NEXT_NUMBER == cf->unireg_check) {
            return new FieldMeta(name, cf, NULL, SECURITY_RATING::PLAIN,
                                 tm.get()->leaseIncUniq());
        } else {
            return new FieldMeta(name, cf, ps.masterKey,
                                 ps.defaultSecurityRating(),
                                 tm.get()->leaseIncUniq());
        }
    };
    std::shared_ptr<FieldMeta> fm =
        std::shared_ptr<FieldMeta>(buildFieldMeta(name, cf, ps, tm));
    // Here we store the key name for the first time. It will be applied
    // after the Delta is read out of the database.
    if (true == new_table) {
        tm->addChild(new IdentityMetaKey(name), fm);
    } else {
        // FIXME: PTR.
        a.deltas.push_back(new CreateDelta(fm, tm.get(),
                                           new IdentityMetaKey(name)));
        a.deltas.push_back(new ReplaceDelta(tm, a.getSchema(),
                                            a.getSchema()->getKey(tm.get())));
    }

    // -----------------------------
    //         Rewrite FIELD       
    // -----------------------------
    // FIXME: PTR.
    const auto new_fields = rewrite_create_field(fm.get(), cf, a);
    rewritten_cfield_list.concat(vectorToList(new_fields));

    return rewritten_cfield_list;
}

//TODO: which encrypt/decrypt should handle null?
Item *
encrypt_item_layers(Item * const i, onion o, OnionMeta * const om,
                    const Analysis &a, uint64_t IV) {
    assert(!i->is_null());

    const auto enc_layers = a.getEncLayers(om);
    assert_s(enc_layers.size() > 0, "onion must have at least one layer");
    Item * enc = i;
    Item * prev_enc = NULL;
    for (auto layer : enc_layers) {
        LOG(encl) << "encrypt layer "
                  << TypeText<SECLEVEL>::toText(layer->level()) << "\n";
        enc = layer->encrypt(enc, IV);
        //need to free space for all enc
        //except the last one
        if (prev_enc) {
            delete prev_enc;
        }
        prev_enc = enc;
    }

    return enc;
}

std::string
rewriteAndGetSingleQuery(const ProxyState &ps, const std::string &q)
{
    Rewriter r;
    SchemaInfo *out_schema;
    QueryRewrite qr = r.rewrite(ps, q, &out_schema);
    assert(false == qr.output->stalesSchema());
    assert(false == qr.output->queryAgain());
    
    std::list<std::string> out_queryz;
    if (!qr.output->getQuery(&out_queryz)) {
        throw CryptDBError("Failed to retrieve query!");
    }
    assert(out_queryz.size() == 1);

    return out_queryz.back();
}

std::string
escapeString(const std::unique_ptr<Connect> &c,
             const std::string &escape_me)
{
    const unsigned int escaped_length = escape_me.size() * 2 + 1;
    std::unique_ptr<char, void (*)(void *)>
        escaped(new char[escaped_length], &operator delete []);
    c->real_escape_string(escaped.get(), escape_me.c_str(),
                          escape_me.size());

    const std::string out = std::string(escaped.get());

    return out;
}

void
encrypt_item_all_onions(Item *i, FieldMeta *fm,
                        uint64_t IV, std::vector<Item*> &l,
                        Analysis &a)
{
    for (auto it : fm->orderedOnionMetas()) {
        const onion o = it.first->getValue();
        OnionMeta * const om = it.second;
        l.push_back(encrypt_item_layers(i, o, om, a, IV));
    }
}

bool
mergeCompleteOLK(OLK olk1, OLK olk2, OLK *out_olk)
{
    if (olk1.o != olk2.o && olk1.l != olk2.l) {
        return false;
    } else if (olk1.key && olk2.key) {
        *out_olk = olk1;
        return olk1.key == olk2.key;
    } else if (olk1.key) {
        *out_olk = olk1;
        return true;
    } else if (olk2.key) {
        *out_olk = olk2;
        return true;
    } else {
        *out_olk = olk1;
        return olk1.l == SECLEVEL::PLAINVAL;
    }
}

