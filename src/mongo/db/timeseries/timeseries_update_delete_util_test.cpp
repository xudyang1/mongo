/**
 *    Copyright (C) 2021-present MongoDB, Inc.
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 *
 *    You should have received a copy of the Server Side Public License
 *    along with this program. If not, see
 *    <http://www.mongodb.com/licensing/server-side-public-license>.
 *
 *    As a special exception, the copyright holders give permission to link the
 *    code of portions of this program with the OpenSSL library under certain
 *    conditions as described in each individual source file and distribute
 *    linked combinations including the program with the OpenSSL library. You
 *    must comply with the Server Side Public License in all respects for
 *    all of the code used other than as permitted herein. If you modify file(s)
 *    with this exception, you may extend this exception to your version of the
 *    file(s), but you are not obligated to do so. If you do not wish to do so,
 *    delete this exception statement from your version. If you delete this
 *    exception statement from all source files in the program, then also delete
 *    it in the license file.
 */


#include "mongo/bson/bsonobj.h"
#include "mongo/db/pipeline/document_source_merge_gen.h"
#include "mongo/db/pipeline/legacy_runtime_constants_gen.h"
#include "mongo/db/service_context_d_test_fixture.h"
#include "mongo/db/timeseries/timeseries_update_delete_util.h"
#include "mongo/unittest/unittest.h"
#include "mongo/util/assert_util.h"

namespace mongo {
namespace {

class TimeseriesUpdateDeleteUtilTest : public ServiceContextMongoDTest {
protected:
    void setUp() {
        ServiceContextMongoDTest::setUp();
        _opCtx = cc().makeOperationContext();
    }

    ServiceContext::UniqueOperationContext _opCtx;
    StringData _metaField = "tag";
    NamespaceString _ns{"timeseries_update_delete_util_test", "system.buckets.t"};
};

TEST_F(TimeseriesUpdateDeleteUtilTest, QueryOnlyDependsOnMetaFieldNoMetaField) {
    // Empty query.
    ASSERT_TRUE(timeseries::queryOnlyDependsOnMetaField(_opCtx.get(), _ns, BSONObj(), boost::none));

    // Query on the "meta" field.
    ASSERT_FALSE(timeseries::queryOnlyDependsOnMetaField(_opCtx.get(),
                                                         _ns,
                                                         BSON("meta"
                                                              << "A"),
                                                         boost::none));
}

TEST_F(TimeseriesUpdateDeleteUtilTest, QueryOnlyDependsOnMetaField) {
    // Empty query.
    ASSERT_TRUE(timeseries::queryOnlyDependsOnMetaField(_opCtx.get(), _ns, BSONObj(), _metaField));

    // Query on the metaField using dot notation.
    ASSERT_TRUE(
        timeseries::queryOnlyDependsOnMetaField(_opCtx.get(),
                                                _ns,
                                                BSON("$and" << BSON_ARRAY(BSON(_metaField + ".a"
                                                                               << "A")
                                                                          << BSON(_metaField + ".b"
                                                                                  << "B"))),
                                                _metaField));

    // Query on a nested field of the metaField.
    ASSERT_TRUE(timeseries::queryOnlyDependsOnMetaField(
        _opCtx.get(),
        _ns,
        BSON("$and" << BSON_ARRAY(BSON(_metaField << BSON("a"
                                                          << "A"))
                                  << BSON(_metaField << BSON("b"
                                                             << "B")))),
        _metaField));


    // Query on a field that is a prefix of the metaField.
    ASSERT_FALSE(timeseries::queryOnlyDependsOnMetaField(_opCtx.get(),
                                                         _ns,
                                                         BSON(_metaField + "a"
                                                              << "A"),
                                                         _metaField));

    // Query using $jsonSchema with the metaField required.
    ASSERT_TRUE(timeseries::queryOnlyDependsOnMetaField(
        _opCtx.get(),
        _ns,
        BSON("$jsonSchema" << BSON("required" << BSON_ARRAY(_metaField))),
        _metaField));

    // Query using $jsonSchema with a field that is not the metaField required.
    ASSERT_FALSE(timeseries::queryOnlyDependsOnMetaField(
        _opCtx.get(),
        _ns,
        BSON("$jsonSchema" << BSON("required" << BSON_ARRAY("measurement"))),
        _metaField));
}

TEST_F(TimeseriesUpdateDeleteUtilTest, QueryOnlyDependsOnMetaFieldLet) {
    // Query on the metaField.
    ASSERT_TRUE(timeseries::queryOnlyDependsOnMetaField(
        _opCtx.get(),
        _ns,
        BSON("$expr" << BSON("$eq" << BSON_ARRAY("$" + _metaField << "$$" + _metaField))),
        _metaField,
        LegacyRuntimeConstants(),
        BSON(_metaField << "A")));

    // Query on a field that is not the metaField.
    ASSERT_FALSE(timeseries::queryOnlyDependsOnMetaField(
        _opCtx.get(),
        _ns,
        BSON("$expr" << BSON("$eq" << BSON_ARRAY("$meta"
                                                 << "$$" + _metaField))),
        _metaField,
        LegacyRuntimeConstants(),
        BSON(_metaField << "A")));
}

TEST_F(TimeseriesUpdateDeleteUtilTest, TranslateQuery) {
    // Translate empty query.
    ASSERT_BSONOBJ_EQ(timeseries::translateQuery(BSONObj(), _metaField), BSONObj());

    // Translate query using dot notation.
    ASSERT_BSONOBJ_EQ(timeseries::translateQuery(BSON("$and" << BSON_ARRAY(BSON(_metaField + ".a"
                                                                                << "A")
                                                                           << BSON(_metaField + ".b"
                                                                                   << "B"))),
                                                 _metaField),
                      BSON("$and" << BSON_ARRAY(BSON("meta.a"
                                                     << "A")
                                                << BSON("meta.b"
                                                        << "B"))));

    // Translate query on a nested field.
    ASSERT_BSONOBJ_EQ(
        timeseries::translateQuery(BSON("$and" << BSON_ARRAY(BSON(_metaField << BSON("a"
                                                                                     << "A"))
                                                             << BSON(_metaField << BSON("b"
                                                                                        << "B")))),
                                   _metaField),
        BSON("$and" << BSON_ARRAY(BSON("meta" << BSON("a"
                                                      << "A"))
                                  << BSON("meta" << BSON("b"
                                                         << "B")))));

    // Translate query on a field that is the prefix of the metaField.
    ASSERT_BSONOBJ_EQ(timeseries::translateQuery(BSON(_metaField + "a"
                                                      << "A"),
                                                 _metaField),
                      BSON(_metaField + "a"
                           << "A"));

    // Translate query using let.
    ASSERT_BSONOBJ_EQ(
        timeseries::translateQuery(
            BSON("$expr" << BSON("$eq" << BSON_ARRAY("$" + _metaField << "$$" + _metaField))),
            _metaField),
        BSON("$expr" << BSON("$eq" << BSON_ARRAY("$meta"
                                                 << "$$" + _metaField))));

    // Translate query using $literal.
    ASSERT_BSONOBJ_EQ(
        timeseries::translateQuery(
            BSON("$expr" << BSON(
                     "$eq" << BSON_ARRAY("$" + _metaField + ".b" << BSON("$literal"
                                                                         << "$" + _metaField)))),
            _metaField),
        BSON("$expr" << BSON("$eq" << BSON_ARRAY("$meta.b" << BSON("$literal"
                                                                   << "$" + _metaField)))));

    // TODO: SERVER-59092 Uncomment this test.
    // // Translate query using $jsonSchema with the metaField required.
    // ASSERT_BSONOBJ_EQ(
    //     timeseries::translateQuery(
    //         BSON("$jsonSchema" << BSON("required" << BSON_ARRAY(_metaField))), _metaField),
    //     BSON("$jsonSchema" << BSON("required" << BSON_ARRAY("meta"))));

    // TODO: SERVER-59092 Uncomment this test.
    // // Translate query using $jsonSchema with a field that is not the metaField required.
    // ASSERT_BSONOBJ_EQ(
    //     timeseries::translateQuery(
    //         BSON("$jsonSchema" << BSON("required" << BSON_ARRAY(_metaField << _metaField +
    //         "a"))), _metaField),
    //     BSON("$jsonSchema" << BSON("required" << BSON_ARRAY("meta" << _metaField + "a"))));
}
}  // namespace
}  // namespace mongo