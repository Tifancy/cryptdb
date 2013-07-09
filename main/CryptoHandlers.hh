#pragma once

#include <util/util.hh>
#include <crypto/prng.hh>
#include <crypto/BasicCrypto.hh>
#include <crypto/paillier.hh>
#include <crypto/ope.hh>
#include <crypto/blowfish.hh>
#include <parser/sql_utils.hh>
#include <crypto/SWPSearch.hh>

#include <sql_select.h>
#include <sql_delete.h>
#include <sql_insert.h>
#include <sql_update.h>


/* Class hierarchy:
 * EncLayer:  
 * -  encrypts and decrypts data for a certain onion layer. It also
 *    knows how to transform the data type of some plain data to the data type of
 *    encrypted data in the DBMS.
 * 
 * HOM: a more specialized type of EncLayer 
 *
 * EncLayerFactory: creates EncLayer-s for SECLEVEL-s of interest 
 */


//TODO(burrows): need to implement cleanup & destructors


/*
 * Requirements & TODO:
 *  -- chain create fields
 *  -- anon name should not be in EncLayers
 *  -- remove unnecessary padding
 */

class EncLayer {
 public:
    EncLayer(Create_field * f) : cf(f) {}
    //  EncLayer(Create_field * f, std::string key) : cf(f) {}


    virtual SECLEVEL level() = 0;

    // returns a rewritten create field to include in rewritten query
    virtual Create_field * newCreateField(std::string anonname = "") = 0;

    virtual Item * encrypt(Item * ptext, uint64_t IV = 0) = 0;
    virtual Item * decrypt(Item * ctext, uint64_t IV = 0) = 0;

    // returns the decryptUDF to remove the onion layer
    virtual Item * decryptUDF(Item * col, Item * ivcol = NULL) {
        thrower() << "decryptUDF not supported";
    }

    // can be deserialized by corresponding factory
    virtual std::string serialize() = 0;

 protected:
    Create_field *cf;
};



class HOM : public EncLayer {
public:
    HOM(Create_field * cf, std::string seed_key);

    // serialize and deserialize
    std::string serialize() {return seed_key;}
    HOM(const std::string & serial);


    SECLEVEL level() {return SECLEVEL::HOM;}
    Create_field * newCreateField(std::string anonname = "");

    //TODO needs multi encrypt and decrypt
    Item * encrypt(Item * p, uint64_t IV = 0);
    Item * decrypt(Item * c, uint64_t IV = 0);

    //expr is the expression (e.g. a field) over which to sum
    Item * sumUDA(Item * expr);
    Item * sumUDF(Item * i1, Item * i2);

private:
    std::string seed_key;
    static const uint nbits = 1024;
    Paillier_priv * sk;

    ~HOM();
};

class Search : public EncLayer {
public:
    Search(Create_field * cf, std::string seed_key);

    // serialize and deserialize
    std::string serialize() {return key;}
    Search(const std::string & serial);


    SECLEVEL level() {return SECLEVEL::SEARCH;}
    Create_field * newCreateField(std::string anonname = "");

    Item * encrypt(Item * ptext, uint64_t IV = 0);
    Item * decrypt(Item * ctext, uint64_t IV = 0)__attribute__((noreturn));

    //expr is the expression (e.g. a field) over which to sum
    Item * searchUDF(Item * field, Item * expr);


private:
    static const uint key_bytes = 16;
    std::string key;

};



extern const std::vector<udf_func*> udf_list;

class EncLayerFactory {
public:
    static EncLayer * encLayer(onion o, SECLEVEL sl, Create_field *cf,
                               std::string key);

    // creates EncLayer from its serialization
    static EncLayer * encLayerFromSerial(onion o, SECLEVEL sl,
					 const std::string & serial);
};


