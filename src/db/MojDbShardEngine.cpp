/* @@@LICENSE
*
* Copyright (c) 2013 LG Electronics, Inc.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* LICENSE@@@ */

#include "db/MojDbShardEngine.h"
#include "db/MojDb.h"
#include "db/MojDbServiceDefs.h"
#include <boost/crc.hpp>
#include <string>

using namespace std;

static const MojChar* const TestKind1Str =
    _T("{\"id\":\"ShardInfo:1\",")
    _T("\"owner\":\"mojodb.admin\",")
    _T("\"indexes\":[{\"name\":\"Info\",\"props\":[{\"name\":\"ShardId\"},{\"name\":\"Path\"},{\"name\":\"Active\"},{\"name\":\"Transient\"},{\"name\":\"Media\"},{\"name\":\"IdBase64\"}]}, \
    ]}");

MojLogger MojDbShardEngine::s_log(_T("db.shardEngine"));

MojDbShardEngine::MojDbShardEngine(void)
{
    MojLogTrace(s_log);
#ifdef __GXX_EXPERIMENTAL_CXX0X__
    mp_db = nullptr;
#else
    mp_db = NULL;
#endif
}

MojDbShardEngine::~MojDbShardEngine(void)
{
    MojLogTrace(s_log);
}

/**
 *
 */
MojErr MojDbShardEngine::init (MojDb* ip_db)
{
    mp_db = ip_db;
    return (MojErrNone);
}

/**
 * put a new shard description to db
 */
MojErr MojDbShardEngine::put (MojUInt32 i_id, bool i_active, bool i_transient, MojString& i_path, MojString& i_media, MojString& i_id_base64)
{
#ifdef __GXX_EXPERIMENTAL_CXX0X__
    if(mp_db == nullptr)
        return MojErrNotInitialized;
#else
    if(mp_db == NULL)
        return MojErrNotInitialized;
#endif

    MojObject obj;
    MojString tmp_string;

    MojErr err = obj.putString(_T("_kind"), _T("ShardInfo:1"));
    MojErrCheck(err);

    MojInt32 val = static_cast<MojInt32>(i_id);
    MojObject obj1(val); //keep numeric
    err = obj.put(_T("ShardId"), obj1);
    MojErrCheck(err);

    MojObject obj2(i_path);
    err = obj.put(_T("Path"), obj2);
    MojErrCheck(err);

    MojObject obj3(i_active);
    err = obj.put(_T("Active"), obj3);
    MojErrCheck(err);

    MojObject obj4(i_transient);
    err = obj.put(_T("Transient"), obj4);
    MojErrCheck(err);

    MojObject obj5(i_media);
    err = obj.put(_T("Media"), obj5);
    MojErrCheck(err);

    MojObject obj6(i_id_base64);
    err = obj.put(_T("IdBase64"), obj6);
    MojErrCheck(err);

    err = mp_db->put(obj);
    MojErrCheck(err);
    obj.clear();

    return err;
}

/**
 * get shard description by id
 */
MojErr MojDbShardEngine::get (MojUInt32 i_id, ShardInfo& o_info)
{
    MojErr err = MojErrNone;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    if(mp_db == nullptr)
        return MojErrNotInitialized;
#else
    if(mp_db == NULL)
        return MojErrNotInitialized;
#endif

    MojDbQuery query;
    MojDbCursor cursor;
    MojInt32 id = static_cast<MojInt32>(i_id);
    MojObject obj(id);
    MojObject dbObj;

    err = query.from(_T("ShardInfo:1"));
    MojErrCheck(err);
    err = query.where(_T("ShardId"), MojDbQuery::OpEq, obj);
    MojErrCheck(err);

    err = mp_db->find(query, cursor);

    if (err == MojErrNone)
    {
        bool found;
        err = cursor.get(dbObj, found);
        MojErrCheck(err);
        if (found)
        {
            bool flag;
            MojString value;
            //Path
            err = dbObj.get(_T("Path"), value, found);
            MojErrCheck(err);

            if (found)
                o_info.path = value;
            else
            {
                MojLogWarning(MojDbShardEngine::s_log, _T("not found field 'Path' for record with id [%x]\n"), i_id);
            }

            //Active
            found = dbObj.get(_T("Active"), flag);
            MojErrCheck(err);

            if (found)
                o_info.active = flag;
            else
            {
                MojLogWarning(MojDbShardEngine::s_log, _T("not found field 'Active' for record with id [%x]\n"), i_id);
            }

            //Transient
            found = dbObj.get(_T("Transient"), flag);
            MojErrCheck(err);

            if (found)
                o_info.active = flag;
            else
            {
                MojLogWarning(MojDbShardEngine::s_log, _T("not found field 'Transient' for record with id [%x]\n"), i_id);
            }

            //Media
            err = dbObj.get(_T("Media"), value, found);
            MojErrCheck(err);

            if (found)
                o_info.media = value;
            else
            {
                MojLogWarning(MojDbShardEngine::s_log, _T("not found field 'Media' for record with id [%x]\n"), i_id);
            }

            //IdBase64
            err = dbObj.get(_T("IdBase64"), value, found);
            MojErrCheck(err);

            if (found)
                o_info.id_base64 = value;
            else
            {
                MojLogWarning(MojDbShardEngine::s_log, _T("not found field 'IdBase64' for record with id [%x]\n"), i_id);
            }
        }

        cursor.close();
        o_info.id = i_id;
    }

    return err;
}

/**
 * get correspond shard id using path to device
 */
MojErr MojDbShardEngine::getIdForPath (MojString& i_path, MojUInt32& o_id)
{
    MojErr err = MojErrNone;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    if(mp_db == nullptr)
        return MojErrNotInitialized;
#else
    if(mp_db == NULL)
        return MojErrNotInitialized;
#endif

    if ((i_path.length() == 1) && (i_path.last() == '/' ))
    {
        o_id = 0; //main db id
        return MojErrNone;
    }

    MojString path = i_path;
    //trim tailing symbol '/' if present
    if ( (i_path.length() > 2) && (i_path.last() == '/' ) )
    {
        i_path.substring(0, i_path.length() - 2, path);
    }

    //get record from db, extract id and return
    MojDbQuery query;
    MojDbCursor cursor;
    MojObject obj(path);
    MojObject dbObj;

    err = query.from(_T("ShardInfo:1"));
    MojErrCheck(err);
    err = query.where(_T("Path"), MojDbQuery::OpEq, obj);
    MojErrCheck(err);

    err = mp_db->find(query, cursor);

    if (err == MojErrNone)
    {
        bool found;
        err = cursor.get(dbObj, found);
        MojErrCheck(err);
        if (found)
        {
            MojInt32 value;
            err = dbObj.get(_T("ShardId"), value, found);
            MojErrCheck(err);

            if (found)
                o_id = static_cast<MojUInt32>(value);
            else
                err = MojErrNotFound;
        }

        cursor.close();
    }

    return err;
}

/**
 * set shard activity
 */
MojErr MojDbShardEngine::setActivity (MojUInt32 i_id, bool i_isActive)
{
    MojErr err = MojErrNone;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    if(mp_db == nullptr)
        return MojErrNotInitialized;
#else
    if(mp_db == NULL)
        return MojErrNotInitialized;
#endif

    MojDbQuery query;
    MojDbCursor cursor;
    MojInt32 val = static_cast<MojInt32>(i_id);
    MojObject obj(val);
    MojObject dbObj;
    MojUInt32 count = 0;

    err = query.from(_T("ShardInfo:1"));
    MojErrCheck(err);
    err = query.where(_T("ShardId"), MojDbQuery::OpEq, obj);
    MojErrCheck(err);

    err = mp_db->find(query, cursor);

    if (err == MojErrNone)
    {
        bool found;
        err = cursor.get(dbObj, found);
        MojErrCheck(err);
        cursor.close();

        if (found)
        {
            MojObject update;
            err = update.put(_T("Active"), i_isActive);
            MojErrCheck(err);
            err = mp_db->merge(query, update, count);
            MojErrCheck(err);
        }
    }

    return err;
}

/*
 * compute a new shard id
 */
MojErr MojDbShardEngine::computeShardId (MojString& i_media, MojUInt32& o_id)
{
    MojErr err = MojErrNone;
    MojUInt32 id;
    MojUInt32 calc_id;
    MojUInt32 prefix = 1;
    bool found = false;

    if (!_computeId(i_media, calc_id))
        return MojErrInvalidArg;

    do
    {
        id = calc_id | (prefix * 0x01000000);

        //check the table to see if this ID already exists
        err = isIdExist(id);
        if (err == MojErrExists)
        {
            MojLogWarning(MojDbShardEngine::s_log, _T("id generation -> [%x] exist already, prefix = %u\n"), id, prefix);
            prefix++;
        }
        else
        {
            if (err == MojErrNotFound)
            {
                o_id = id;
                found = true;
                break;
            }
            else
                return err;
        }

        if (prefix == 128)
        {
            MojLogWarning(MojDbShardEngine::s_log, _T("id generation -> next iteration\n"));
            prefix = 1;
            _computeId(i_media, calc_id); //next iteration
        }
    }
    while (!found);

    return err;
}

bool MojDbShardEngine::_computeId (MojString& i_media, MojUInt32& o_id)
{
    if(i_media.length() == 0)
        return false;

    std::string str = i_media.data();

    //Create a 24 bit hash of the string
    static boost::crc_32_type result;
    result.process_bytes(str.data(), str.length());
    MojInt32 code = result.checksum();

    //Prefix the 24 bit hash with 0x01 to create a 32 bit unique shard ID
    o_id = code & 0xFFFFFF;

    return true;
}

MojErr MojDbShardEngine::isIdExist (MojUInt32 i_id)
{
    MojErr err = MojErrNone;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    if(mp_db == nullptr)
        return MojErrNotInitialized;
#else
    if(mp_db == NULL)
        return MojErrNotInitialized;
#endif

    MojDbQuery query;
    MojDbCursor cursor;
    MojInt32 val = static_cast<MojInt32>(i_id);
    MojObject obj(val);
    MojObject dbObj;

    err = query.from(_T("ShardInfo:1"));
    MojErrCheck(err);
    err = query.where(_T("ShardId"), MojDbQuery::OpEq, obj);
    MojErrCheck(err);

    err = mp_db->find(query, cursor);

    if (err == MojErrNone)
    {
        bool found;
        err = cursor.get(dbObj, found);
        MojErrCheck(err);
        cursor.close();
        return (found ? MojErrExists : MojErrNotFound);
    }

    return err;
}

MojErr MojDbShardEngine::isIdExist (MojString i_id_base64)
{
    MojErr err = MojErrNone;

#ifdef __GXX_EXPERIMENTAL_CXX0X__
    if(mp_db == nullptr)
        return MojErrNotInitialized;
#else
    if(mp_db == NULL)
        return MojErrNotInitialized;
#endif

    MojDbQuery query;
    MojDbCursor cursor;
    MojObject obj(i_id_base64);
    MojObject dbObj;

    err = query.from(_T("ShardInfo:1"));
    MojErrCheck(err);
    err = query.where(_T("IdBase64"), MojDbQuery::OpEq, obj);
    MojErrCheck(err);

    err = mp_db->find(query, cursor);

    if (err == MojErrNone)
    {
        bool found;
        err = cursor.get(dbObj, found);
        MojErrCheck(err);
        cursor.close();
        return (found ? MojErrExists : MojErrNotFound);
    }

    return err;
}