// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "contractdb.h"

#include "accounts/account.h"
#include "accounts/id.h"
#include "accounts/key.h"
#include "commons/uint256.h"
#include "util.h"
#include "vm/vmrunenv.h"

#include <stdint.h>

using namespace std;

bool CContractDB::BatchWrite(const map<vector<unsigned char>, vector<unsigned char> > &mapContractDb) {
    CLevelDBBatch batch;
    for (auto &item : mapContractDb) {
        if (item.second.empty()) {
            batch.Erase(item.first);
        } else {
            batch.Write(item.first, item.second);
        }
    }
    return db.WriteBatch(batch, true);
}

bool CContractDB::EraseKey(const vector<unsigned char> &vKey) {
    return db.Erase(vKey);
}

bool CContractDB::HaveData(const vector<unsigned char> &vKey) {
    return db.Exists(vKey);
}

bool CContractDB::GetScript(const int nIndex, vector<unsigned char> &vScriptId, vector<unsigned char> &vValue) {
    assert(nIndex >= 0 && nIndex <= 1);
    leveldb::Iterator *pcursor = db.NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    string strPrefixTemp("def");
    //ssKeySet.insert(ssKeySet.end(), 9);
    ssKeySet.insert(ssKeySet.end(), &strPrefixTemp[0], &strPrefixTemp[3]);
    int i(0);
    if (1 == nIndex) {
        if (vScriptId.empty()) {
            return ERRORMSG("GetScript() : nIndex is 1, and vScriptId is empty");
        }
        vector<char> vId(vScriptId.begin(), vScriptId.end());
        ssKeySet.insert(ssKeySet.end(), vId.begin(), vId.end());
        vector<unsigned char> vKey(ssKeySet.begin(), ssKeySet.end());
        if (HaveData(vKey)) {  //判断传过来的key,数据库中是否已经存在
            pcursor->Seek(ssKeySet.str());
            i = nIndex;
        } else {
            pcursor->Seek(ssKeySet.str());
        }
    } else {
        pcursor->Seek(ssKeySet.str());
    }
    while (pcursor->Valid() && i-- >= 0) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            //			CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            string strScriptKey(slKey.data(), 0, slKey.size());
            //			ssKey >> strScriptKey;
            string strPrefix = strScriptKey.substr(0, 3);
            if (strPrefix == "def") {
                if (-1 == i) {
                    leveldb::Slice slValue = pcursor->value();
                    CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                    ssValue >> vValue;
                    vScriptId.clear();
                    vScriptId.insert(vScriptId.end(), slKey.data() + 3, slKey.data() + slKey.size());
                }
                pcursor->Next();
            } else {
                delete pcursor;
                return false;
            }
        } catch (std::exception &e) {
            return ERRORMSG("%s : Deserialize or I/O error - %s", __func__, e.what());
        }
    }
    delete pcursor;
    if (i >= 0)
        return false;
    return true;
}
bool CContractDB::GetContractData(const int curBlockHeight, const vector<unsigned char> &vScriptId, const int &nIndex,
                                vector<unsigned char> &vScriptKey, vector<unsigned char> &vScriptData) {
    const int iPrefixLen   = 4;
    const int iScriptIdLen = 6;
    const int iSpaceLen    = 1;
    assert(nIndex >= 0 && nIndex <= 1);
    leveldb::Iterator *pcursor = db.NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);

    string strPrefixTemp("data");
    ssKeySet.insert(ssKeySet.end(), &strPrefixTemp[0], &strPrefixTemp[4]);
    vector<char> vId(vScriptId.begin(), vScriptId.end());
    ssKeySet.insert(ssKeySet.end(), vId.begin(), vId.end());
    ssKeySet.insert(ssKeySet.end(), '_');
    int i(0);
    if (1 == nIndex) {
        if (vScriptKey.empty()) {
            return ERRORMSG("GetContractData() : nIndex is 1, and vScriptKey is empty");
        }
        vector<char> vsKey(vScriptKey.begin(), vScriptKey.end());
        ssKeySet.insert(ssKeySet.end(), vsKey.begin(), vsKey.end());
        vector<unsigned char> vKey(ssKeySet.begin(), ssKeySet.end());
        if (HaveData(vKey)) {  //判断传过来的key,数据库中是否已经存在
            pcursor->Seek(ssKeySet.str());
            i = nIndex;
        } else {
            pcursor->Seek(ssKeySet.str());
        }
    } else {
        pcursor->Seek(ssKeySet.str());
    }
    while (pcursor->Valid() && i-- >= 0) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            if (0 == memcmp((char *)&ssKey[0], (char *)&ssKeySet[0], 11)) {
                if (-1 == i) {
                    leveldb::Slice slValue = pcursor->value();
                    CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                    ssValue >> vScriptData;
                    vScriptKey.clear();
                    vScriptKey.insert(vScriptKey.end(), slKey.data() + iPrefixLen + iScriptIdLen + iSpaceLen,
                                      slKey.data() + slKey.size());
                }
                pcursor->Next();
            } else {
                delete pcursor;
                return false;
            }
        } catch (std::exception &e) {
            return ERRORMSG("%s : Deserialize or I/O error - %s\n", __func__, e.what());
        }
    }
    delete pcursor;
    if (i >= 0)
        return false;
    return true;
}

bool CContractDB::GetTxHashByAddress(const CKeyID &keyId, int nHeight, map<vector<unsigned char>, vector<unsigned char> > &mapTxHash) {
    leveldb::Iterator *pcursor = db.NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);

    string strPrefixTemp("ADDR");
    ssKeySet.insert(ssKeySet.end(), &strPrefixTemp[0], &strPrefixTemp[4]);
    ssKeySet << keyId;
    ssKeySet << nHeight;
    pcursor->Seek(ssKeySet.str());

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey   = pcursor->key();
            leveldb::Slice slValue = pcursor->value();
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            if (0 == memcmp((char *)&ssKey[0], (char *)&ssKeySet[0], 28)) {
                vector<unsigned char> vValue;
                vector<unsigned char> vKey;
                CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                ssValue >> vValue;
                vKey.insert(vKey.end(), slKey.data(), slKey.data() + slKey.size());
                mapTxHash.insert(make_pair(vKey, vValue));
                pcursor->Next();
            } else {
                break;
            }
        } catch (std::exception &e) {
            return ERRORMSG("%s : Deserialize or I/O error - %s\n", __func__, e.what());
        }
    }
    delete pcursor;
    return true;
}

bool CContractDB::GetAllContractAcc(const CRegID &scriptId, map<vector<unsigned char>, vector<unsigned char> > &mapAcc) {
    leveldb::Iterator *pcursor = db.NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);

    string strPrefixTemp("acct");
    ssKeySet.insert(ssKeySet.end(), &strPrefixTemp[0], &strPrefixTemp[4]);
    vector<unsigned char> vRegId = scriptId.GetRegIdRaw();
    vector<char> vId(vRegId.begin(), vRegId.end());
    ssKeySet.insert(ssKeySet.end(), vId.begin(), vId.end());
    ssKeySet.insert(ssKeySet.end(), '_');

    pcursor->Seek(ssKeySet.str());
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey   = pcursor->key();
            leveldb::Slice slValue = pcursor->value();
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            if (0 == memcmp((char *)&ssKey[0], (char *)&ssKeySet[0], 11)) {
                vector<unsigned char> vValue;
                vector<unsigned char> vKey;
                CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
                ssValue >> vValue;
                vKey.insert(vKey.end(), slKey.data(), slKey.data() + slKey.size());
                mapAcc.insert(make_pair(vKey, vValue));
                pcursor->Next();
            } else {
                break;
            }
        } catch (std::exception &e) {
            return ERRORMSG("%s : Deserialize or I/O error - %s\n", __func__, e.what());
        }
    }
    delete pcursor;
    return true;
}

Object CContractDB::ToJsonObj(string Prefix) {
    Object obj;
    Array arrayObj;

    leveldb::Iterator *pcursor = db.NewIterator();
    CDataStream ssKeySet(SER_DISK, CLIENT_VERSION);
    ssKeySet.insert(ssKeySet.end(), &Prefix[0], &Prefix[Prefix.length()]);
    pcursor->Seek(ssKeySet.str());

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        try {
            leveldb::Slice slKey = pcursor->key();
            CDataStream ssKey(slKey.data(), slKey.data() + slKey.size(), SER_DISK, CLIENT_VERSION);
            string strScriptKey(slKey.data(), 0, slKey.size());
            string strPrefix       = strScriptKey.substr(0, Prefix.length());
            leveldb::Slice slValue = pcursor->value();
            CDataStream ssValue(slValue.data(), slValue.data() + slValue.size(), SER_DISK, CLIENT_VERSION);
            Object obj;
            if (strPrefix == Prefix) {
                if (Prefix == "def") {
                    obj.push_back(Pair("contract_id", HexStr(ssKey)));
                    obj.push_back(Pair("value", HexStr(ssValue)));
                } else if (Prefix == "data") {
                    obj.push_back(Pair("key", HexStr(ssKey)));
                    obj.push_back(Pair("value", HexStr(ssValue)));
                } else if (Prefix == "acct") {
                    obj.push_back(Pair("acct_key", HexStr(ssKey)));
                    obj.push_back(Pair("acct_value", HexStr(ssValue)));
                }

            } else {
                obj.push_back(Pair("unkown_key", HexStr(ssKey)));
                obj.push_back(Pair("unkown_value", HexStr(ssValue)));
            }
            arrayObj.push_back(obj);
            pcursor->Next();
        } catch (std::exception &e) {
            if (pcursor)
                delete pcursor;
            LogPrint("ERROR", "line:%d,%s : Deserialize or I/O error - %s\n", __LINE__, __func__, e.what());
        }
    }
    delete pcursor;
    obj.push_back(Pair("contract_db", arrayObj));
    return obj;
}

bool CContractCache::SetData(const vector<unsigned char> &vKey, const vector<unsigned char> &vValue) {
    mapContractDb[vKey] = vValue;
    return true;
}

bool CContractCache::UndoScriptData(const vector<unsigned char> &vKey, const vector<unsigned char> &vValue) {
    vector<unsigned char> vPrefix(vKey.begin(), vKey.begin() + 4);
    vector<unsigned char> vScriptDataPrefix = {'d', 'a', 't', 'a'};
    if (vPrefix == vScriptDataPrefix) {
        assert(vKey.size() > 10);
        if (vKey.size() < 10) {
            return ERRORMSG("UndoScriptData() : vKey=%s error!\n", HexStr(vKey));
        }
        vector<unsigned char> vScriptCountKey = {'s', 'd', 'n', 'u', 'm'};
        vector<unsigned char> vScriptId(vKey.begin() + 4, vKey.begin() + 10);
        vector<unsigned char> vOldValue;
        if (mapContractDb.count(vKey)) {
            vOldValue = mapContractDb[vKey];
        } else {
            GetData(vKey, vOldValue);
        }
        vScriptCountKey.insert(vScriptCountKey.end(), vScriptId.begin(), vScriptId.end());
        CDataStream ds(SER_DISK, CLIENT_VERSION);

        int nCount(0);
        if (vValue.empty()) {  //key所对应的值由非空设置为空，计数减1
            if (!vOldValue.empty()) {
                if (!GetContractItemCount(vScriptId, nCount))
                    return false;
                --nCount;
                if (!SetContractItemCount(vScriptId, nCount))
                    return false;
            }
        } else {  //key所对应的值由空设置为非空，计数加1
            if (vOldValue.empty()) {
                GetContractItemCount(vScriptId, nCount);
                ++nCount;
                if (!SetContractItemCount(vScriptId, nCount))
                    return false;
            }
        }
    }
    mapContractDb[vKey] = vValue;
    return true;
}

bool CContractCache::BatchWrite(const map<vector<unsigned char>, vector<unsigned char> > &mapData) {
    for (auto &items : mapData) {
        mapContractDb[items.first] = items.second;
    }
    return true;
}

bool CContractCache::EraseKey(const vector<unsigned char> &vKey) {
    if (mapContractDb.count(vKey) > 0) {
        mapContractDb[vKey].clear();
    } else {
        vector<unsigned char> vValue;
        if (pBase->GetData(vKey, vValue)) {
            vValue.clear();
            mapContractDb[vKey] = vValue;
        } else {
            return false;
        }
    }
    return true;
}

bool CContractCache::HaveData(const vector<unsigned char> &vKey) {
    if (mapContractDb.count(vKey) > 0) {
        if (!mapContractDb[vKey].empty())
            return true;
        else
            return false;
    }
    return pBase->HaveData(vKey);
}

bool CContractCache::GetScript(const int nIndex, vector<unsigned char> &vScriptId, vector<unsigned char> &vValue) {
    if (0 == nIndex) {
        vector<unsigned char> scriptKey = {'d', 'e', 'f'};
        vector<unsigned char> vDataKey;
        vector<unsigned char> vDataValue;
        vDataKey.clear();
        vDataValue.clear();
        for (auto &item : mapContractDb) {  //遍历本级缓存数据，找出合法的最小的key值
            vector<unsigned char> vTemp(item.first.begin(), item.first.begin() + 3);
            if (scriptKey == vTemp) {
                if (item.second.empty()) {
                    continue;
                }
                vDataKey   = item.first;
                vDataValue = item.second;
                break;
            }
        }
        if (!pBase->GetScript(nIndex, vScriptId, vValue)) {  //上级没有获取符合条件的key值
            if (vDataKey.empty())
                return false;
            else {  //返回本级缓存的查询结果
                vScriptId.clear();
                vValue.clear();
                vScriptId.assign(vDataKey.begin() + 3, vDataKey.end());
                vValue = vDataValue;
                return true;
            }
        } else {                     //上级获取到符合条件的key值
            if (vDataKey.empty()) {  //缓存中没有符合条件的key，直接返回上级的查询结果
                return true;
            }
            vector<unsigned char> dataKeyTemp = {'d', 'e', 'f'};
            dataKeyTemp.insert(dataKeyTemp.end(), vScriptId.begin(), vScriptId.end());  //上级得到的key值
            if (dataKeyTemp < vDataKey) {                                               //若上级查询的key小于本级缓存的key,且此key在缓存中没有，则直接返回数据库中查询的结果
                if (mapContractDb.count(dataKeyTemp) == 0)
                    return true;
                else {
                    mapContractDb[dataKeyTemp].clear();           //在缓存中dataKeyTemp已经被删除过了，重新将此key对应的value清除
                    return GetScript(nIndex, vScriptId, vValue);  //重新从数据库中获取下一条数据
                }
            } else {  //若上级查询的key大于等于本级缓存的key,返回本级的数据
                vScriptId.clear();
                vValue.clear();
                vScriptId.assign(vDataKey.begin() + 3, vDataKey.end());
                vValue = vDataValue;
                return true;
            }
        }
    } else if (1 == nIndex) {
        vector<unsigned char> vKey = {'d', 'e', 'f'};
        vKey.insert(vKey.end(), vScriptId.begin(), vScriptId.end());
        vector<unsigned char> vPreKey(vKey);
        map<vector<unsigned char>, vector<unsigned char> >::iterator iterFindKey = mapContractDb.upper_bound(vPreKey);
        vector<unsigned char> vDataKey;
        vector<unsigned char> vDataValue;
        vDataKey.clear();
        vDataValue.clear();
        vector<unsigned char> vKeyTemp = {'d', 'e', 'f'};
        while (iterFindKey != mapContractDb.end()) {
            vector<unsigned char> vTemp(iterFindKey->first.begin(), iterFindKey->first.begin() + 3);
            if (vKeyTemp == vTemp) {
                if (iterFindKey->second.empty()) {
                    ++iterFindKey;
                    continue;
                } else {
                    vDataKey   = iterFindKey->first;
                    vDataValue = iterFindKey->second;
                    break;
                }
            } else {
                ++iterFindKey;
            }
        }
        if (!pBase->GetScript(nIndex, vScriptId, vValue)) {  //从BASE获取指定键值之后的下一个值
            if (vDataKey.empty())
                return false;
            else {
                vScriptId.clear();
                vValue.clear();
                vScriptId.assign(vDataKey.begin() + 3, vDataKey.end());
                vValue = vDataValue;
                return true;
            }
        } else {
            if (vDataKey.empty())  //缓存中没有符合条件的key，直接返回上级的查询结果
                return true;
            vector<unsigned char> dataKeyTemp = {'d', 'e', 'f'};
            dataKeyTemp.insert(dataKeyTemp.end(), vScriptId.begin(), vScriptId.end());  //上级得到的key值
            if (dataKeyTemp < vDataKey) {
                if (mapContractDb.count(dataKeyTemp) == 0)
                    return true;
                else {
                    mapContractDb[dataKeyTemp].clear();           //在缓存中dataKeyTemp已经被删除过了，重新将此key对应的value清除
                    return GetScript(nIndex, vScriptId, vValue);  //重新从数据库中获取下一条数据
                }
            } else {  //若上级查询的key大于等于本级缓存的key,返回本级的数据
                vScriptId.clear();
                vValue.clear();
                vScriptId.assign(vDataKey.begin() + 3, vDataKey.end());
                vValue = vDataValue;
                return true;
            }
        }
    }
    return true;
}

bool CContractCache::SetScript(const vector<unsigned char> &vScriptId, const vector<unsigned char> &vScriptContent) {
    vector<unsigned char> scriptKey = {'d', 'e', 'f'};
    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());

    if (!HaveScript(vScriptId)) {
        int nCount(0);
        GetScriptCount(nCount);
        ++nCount;
        if (!SetScriptCount(nCount))
            return false;
    }

    return SetData(scriptKey, vScriptContent);
}

bool CContractCache::Flush() {
    bool ok = pBase->BatchWrite(mapContractDb);
    if (ok) {
        mapContractDb.clear();
    }
    return ok;
}

bool CContractCache::Flush(IContractView *pView) {
    bool ok = pView->BatchWrite(mapContractDb);
    if (ok) {
        mapContractDb.clear();
    }
    return ok;
}

unsigned int CContractCache::GetCacheSize() {
    return ::GetSerializeSize(mapContractDb, SER_DISK, CLIENT_VERSION);
}

bool CContractCache::WriteTxOutPut(const uint256 &txid, const vector<CVmOperate> &vOutput, CContractDBOperLog &operLog) {
    vector<unsigned char> vKey = {'o', 'u', 't', 'p', 'u', 't'};
    CDataStream ds1(SER_DISK, CLIENT_VERSION);
    ds1 << txid;
    vKey.insert(vKey.end(), ds1.begin(), ds1.end());

    vector<unsigned char> vValue;
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << vOutput;
    vValue.assign(ds.begin(), ds.end());

    vector<unsigned char> oldValue;
    oldValue.clear();
    GetData(vKey, oldValue);
    operLog = CContractDBOperLog(vKey, oldValue);
    return SetData(vKey, vValue);
}

bool CContractCache::SetTxHashByAddress(const CKeyID &keyId, int nHeight, int nIndex,
                                            const string &strTxHash, CContractDBOperLog &operLog) {
    vector<unsigned char> vKey = {'A', 'D', 'D', 'R'};

    CDataStream ds1(SER_DISK, CLIENT_VERSION);
    ds1 << keyId;
    ds1 << nHeight;
    vKey.insert(vKey.end(), ds1.begin(), ds1.end());
    vector<unsigned char> vValue(strTxHash.begin(), strTxHash.end());

    vector<unsigned char> oldValue;
    oldValue.clear();
    GetData(vKey, oldValue);
    operLog = CContractDBOperLog(vKey, oldValue);
    return SetData(vKey, vValue);
}

bool CContractCache::GetTxHashByAddress(
    const CKeyID &keyId, int nHeight, map<vector<unsigned char>, vector<unsigned char> > &mapTxHash) {
    pBase->GetTxHashByAddress(keyId, nHeight, mapTxHash);

    vector<unsigned char> vPreKey = {'A', 'D', 'D', 'R'};
    CDataStream ds1(SER_DISK, CLIENT_VERSION);
    ds1 << keyId;
    ds1 << nHeight;
    vPreKey.insert(vPreKey.end(), ds1.begin(), ds1.end());

    map<vector<unsigned char>, vector<unsigned char> >::iterator iterFindKey =
        mapContractDb.upper_bound(vPreKey);
    while (iterFindKey != mapContractDb.end()) {
        if (0 == memcmp((char *)&iterFindKey->first[0], (char *)&vPreKey[0], 28)) {
            if (iterFindKey->second.empty())
                mapTxHash.erase(iterFindKey->first);
            else {
                mapTxHash.insert(make_pair(iterFindKey->first, iterFindKey->second));
            }
        } else {
            break;
        }
    }
    return true;
}

bool CContractCache::GetAllContractAcc(
    const CRegID &scriptId, map<vector<unsigned char>, vector<unsigned char> > &mapAcc) {
    return pBase->GetAllContractAcc(scriptId, mapAcc);
}

bool CContractCache::ReadTxOutPut(const uint256 &txid, vector<CVmOperate> &vOutput) {
    vector<unsigned char> vKey = {'o', 'u', 't', 'p', 'u', 't'};
    CDataStream ds1(SER_DISK, CLIENT_VERSION);
    ds1 << txid;
    vKey.insert(vKey.end(), ds1.begin(), ds1.end());
    vector<unsigned char> vValue;
    if (!GetData(vKey, vValue))
        return false;
    CDataStream ds(vValue, SER_DISK, CLIENT_VERSION);
    ds >> vOutput;
    return true;
}

bool CContractCache::ReadTxIndex(const uint256 &txid, CDiskTxPos &pos) {
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << txid;
    vector<unsigned char> vTxHash = {'T'};
    vTxHash.insert(vTxHash.end(), ds.begin(), ds.end());
    vector<unsigned char> vTxPos;

    if (mapContractDb.count(vTxHash)) {
        if (mapContractDb[vTxHash].empty()) {
            return false;
        }
        vTxPos = mapContractDb[vTxHash];
        CDataStream dsPos(vTxPos, SER_DISK, CLIENT_VERSION);
        dsPos >> pos;
    } else {
        if (!GetData(vTxHash, vTxPos))
            return false;
        CDataStream dsPos(vTxPos, SER_DISK, CLIENT_VERSION);
        dsPos >> pos;
    }
    return true;
}
bool CContractCache::WriteTxIndex(const vector<pair<uint256, CDiskTxPos> > &list, vector<CContractDBOperLog> &vTxIndexOperDB) {
    for (vector<pair<uint256, CDiskTxPos> >::const_iterator it = list.begin(); it != list.end(); it++) {
        LogPrint("txindex", "txhash:%s dispos: nFile=%d, nPos=%d nTxOffset=%d\n", it->first.GetHex(), it->second.nFile, it->second.nPos, it->second.nTxOffset);
        CDataStream ds(SER_DISK, CLIENT_VERSION);
        ds << it->first;
        vector<unsigned char> vTxHash = {'T'};
        vTxHash.insert(vTxHash.end(), ds.begin(), ds.end());
        vector<unsigned char> vTxPos;
        CDataStream dsPos(SER_DISK, CLIENT_VERSION);
        dsPos << it->second;
        vTxPos.insert(vTxPos.end(), dsPos.begin(), dsPos.end());
        CContractDBOperLog txIndexOper;
        txIndexOper.vKey = vTxHash;
        GetData(vTxHash, txIndexOper.vValue);
        vTxIndexOperDB.push_back(txIndexOper);
        if (!SetData(vTxHash, vTxPos))
            return false;
    }
    return true;
}

bool CContractCache::GetScript(const vector<unsigned char> &vScriptId, vector<unsigned char> &vValue) {
    vector<unsigned char> scriptKey = {'d', 'e', 'f'};

    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());
    return GetData(scriptKey, vValue);
}

bool CContractCache::GetScript(const CRegID &scriptId, vector<unsigned char> &vValue) {
    return GetScript(scriptId.GetRegIdRaw(), vValue);
}

bool CContractCache::GetContractData(const int nCurBlockHeight, const vector<unsigned char> &vScriptId,
                                         const vector<unsigned char> &vScriptKey, vector<unsigned char> &vScriptData) {
    // assert(vScriptKey.size() == 8);
    vector<unsigned char> vKey = {'d', 'a', 't', 'a'};

    vKey.insert(vKey.end(), vScriptId.begin(), vScriptId.end());
    vKey.push_back('_');
    vKey.insert(vKey.end(), vScriptKey.begin(), vScriptKey.end());
    vector<unsigned char> vValue;
    if (!GetData(vKey, vValue))
        return false;
    if (vValue.empty())
        return false;
    vScriptData = vValue;

    return true;
}

bool CContractCache::GetContractData(const int nCurBlockHeight, const vector<unsigned char> &vScriptId,
                                         const int &nIndex, vector<unsigned char> &vScriptKey,
                                         vector<unsigned char> &vScriptData) {
    if (0 == nIndex) {
        vector<unsigned char> vKey = {'d', 'a', 't', 'a'};
        vKey.insert(vKey.end(), vScriptId.begin(), vScriptId.end());
        vKey.push_back('_');
        vector<unsigned char> vDataKey;
        vector<unsigned char> vDataValue;
        vDataKey.clear();
        vDataValue.clear();
        map<vector<unsigned char>, vector<unsigned char> >::iterator iterFindKey = mapContractDb.upper_bound(vKey);
        while (iterFindKey != mapContractDb.end()) {
            vector<unsigned char> vKeyTemp(vKey.begin(), vKey.begin() + vScriptId.size() + 5);
            vector<unsigned char> vTemp(iterFindKey->first.begin(), iterFindKey->first.begin() + vScriptId.size() + 5);
            if (vKeyTemp == vTemp) {
                if (iterFindKey->second.empty()) {
                    ++iterFindKey;
                    continue;
                } else {
                    vDataKey   = iterFindKey->first;
                    vDataValue = iterFindKey->second;
                    break;
                }
            } else {
                break;
            }
        }
        bool bUpLevelRet(false);
        int nIndexTemp = nIndex;
        while ((bUpLevelRet = pBase->GetContractData(nCurBlockHeight, vScriptId, nIndexTemp, vScriptKey, vScriptData))) {
            nIndexTemp = 1;
            vector<unsigned char> dataKeyTemp(vKey.begin(), vKey.end());
            dataKeyTemp.insert(dataKeyTemp.end(), vScriptKey.begin(), vScriptKey.end());
            if (vDataKey.empty()) {  //缓存中没有符合条件的key，直接返回上级的查询结果
                if (!mapContractDb.count(dataKeyTemp)) {
                    return true;
                } else {
                    continue;  //重新从数据库中获取下一条数据
                }
            } else {
                if (dataKeyTemp < vDataKey) {
                    if (!mapContractDb.count(dataKeyTemp)) {
                        return true;
                    } else {
                        continue;  //重新从数据库中获取下一条数据
                    }
                } else {
                    if (vDataValue.empty()) {  //本级和上级数据key相同,且本级数据已经删除，重新从上级获取下一条数据
                        continue;
                    }
                    vScriptKey.clear();
                    vScriptData.clear();
                    vScriptKey.insert(vScriptKey.end(), vDataKey.begin() + 11, vDataKey.end());
                    vScriptData = vDataValue;
                    return true;
                }
            }
        }
        if (!bUpLevelRet) {
            if (vDataKey.empty())
                return false;
            else {
                vScriptKey.clear();
                vScriptData.clear();
                vScriptKey.insert(vScriptKey.end(), vDataKey.begin() + 11, vDataKey.end());
                if (vDataValue.empty()) {
                    return false;
                }
                vScriptData = vDataValue;
                return true;
            }
        }
    } else if (1 == nIndex) {
        vector<unsigned char> vKey = {'d', 'a', 't', 'a'};
        vKey.insert(vKey.end(), vScriptId.begin(), vScriptId.end());
        vKey.push_back('_');
        vector<unsigned char> vPreKey(vKey);
        vPreKey.insert(vPreKey.end(), vScriptKey.begin(), vScriptKey.end());
        map<vector<unsigned char>, vector<unsigned char> >::iterator iterFindKey = mapContractDb.upper_bound(vPreKey);
        vector<unsigned char> vDataKey;
        vector<unsigned char> vDataValue;
        vDataValue.clear();
        vDataKey.clear();
        while (iterFindKey != mapContractDb.end()) {
            vector<unsigned char> vKeyTemp(vKey.begin(), vKey.begin() + vScriptId.size() + 5);
            vector<unsigned char> vTemp(iterFindKey->first.begin(), iterFindKey->first.begin() + vScriptId.size() + 5);
            if (vKeyTemp == vTemp) {
                if (iterFindKey->second.empty()) {
                    ++iterFindKey;
                    continue;
                } else {
                    vDataKey   = iterFindKey->first;
                    vDataValue = iterFindKey->second;
                    break;
                }
            } else {
                break;
            }
        }
        bool bUpLevelRet(false);
        while ((bUpLevelRet = pBase->GetContractData(nCurBlockHeight, vScriptId, nIndex, vScriptKey, vScriptData))) {
            vector<unsigned char> dataKeyTemp(vKey.begin(), vKey.end());
            dataKeyTemp.insert(dataKeyTemp.end(), vScriptKey.begin(), vScriptKey.end());
            if (vDataKey.empty()) {  //缓存中没有符合条件的key，直接返回上级的查询结果
                if (!mapContractDb.count(dataKeyTemp)) {
                    return true;
                } else {
                    continue;  //重新从数据库中获取下一条数据
                }
            } else {
                if (dataKeyTemp < vDataKey) {
                    if (!mapContractDb.count(dataKeyTemp))
                        return true;
                    else {
                        continue;  //在缓存中dataKeyTemp已经被删除过了，重新从数据库中获取下一条数据
                    }
                } else {
                    if (vDataValue.empty()) {  //本级和上级数据key相同,且本级数据已经删除，重新从上级获取下一条数据
                        continue;
                    }
                    vScriptKey.clear();
                    vScriptData.clear();
                    vScriptKey.insert(vScriptKey.end(), vDataKey.begin() + 11, vDataKey.end());
                    vScriptData = vDataValue;
                    return true;
                }
            }
        }
        if (!bUpLevelRet) {
            if (vDataKey.empty())
                return false;
            else {
                vScriptKey.clear();
                vScriptData.clear();
                vScriptKey.insert(vScriptKey.end(), vDataKey.begin() + 11, vDataKey.end());
                if (vDataValue.empty()) {
                    return false;
                }
                vScriptData = vDataValue;
                return true;
            }
        }
    } else {
        return ERRORMSG("GetContractData, nIndex > 1 error");
    }

    return true;
}
bool CContractCache::SetContractData(const vector<unsigned char> &vScriptId, const vector<unsigned char> &vScriptKey,
                                         const vector<unsigned char> &vScriptData, CContractDBOperLog &operLog) {
    vector<unsigned char> vKey = {'d', 'a', 't', 'a'};
    vKey.insert(vKey.end(), vScriptId.begin(), vScriptId.end());
    vKey.push_back('_');
    vKey.insert(vKey.end(), vScriptKey.begin(), vScriptKey.end());
    vector<unsigned char> vNewValue(vScriptData.begin(), vScriptData.end());
    if (!HaveData(vKey)) {
        int nCount(0);
        GetContractItemCount(vScriptId, nCount);
        ++nCount;
        if (!SetContractItemCount(vScriptId, nCount))
            return false;
    }

    vector<unsigned char> oldValue;
    oldValue.clear();
    GetData(vKey, oldValue);
    operLog  = CContractDBOperLog(vKey, oldValue);
    bool ret = SetData(vKey, vNewValue);
    return ret;
}

bool CContractCache::HaveScript(const vector<unsigned char> &vScriptId) {
    vector<unsigned char> scriptKey = {'d', 'e', 'f'};
    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());
    return HaveData(scriptKey);
}

bool CContractCache::GetScriptCount(int &nCount) {
    vector<unsigned char> scriptKey = {'s', 'n', 'u', 'm'};
    vector<unsigned char> vValue;
    if (!GetData(scriptKey, vValue))
        return false;

    CDataStream ds(vValue, SER_DISK, CLIENT_VERSION);
    ds >> nCount;
    return true;
}

bool CContractCache::SetScriptCount(const int nCount) {
    vector<unsigned char> scriptKey = {'s', 'n', 'u', 'm'};
    vector<unsigned char> vValue;
    vValue.clear();
    if (nCount > 0) {
        CDataStream ds(SER_DISK, CLIENT_VERSION);
        ds << nCount;
        vValue.insert(vValue.end(), ds.begin(), ds.end());
    }  else if (nCount < 0) {
        return false;
    }
    // If nCount = 0, set an empty value to trigger deleting it in level DB.

    if (!SetData(scriptKey, vValue))
        return false;

    return true;
}
bool CContractCache::EraseScript(const vector<unsigned char> &vScriptId) {
    vector<unsigned char> scriptKey = {'d', 'e', 'f'};
    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());
    if (HaveScript(vScriptId)) {
        int nCount(0);
        if (!GetScriptCount(nCount))
            return false;
        if (!SetScriptCount(--nCount))
            return false;
    }
    return EraseKey(scriptKey);
}
bool CContractCache::GetContractItemCount(const vector<unsigned char> &vScriptId, int &nCount) {
    vector<unsigned char> scriptKey = {'s', 'd', 'n', 'u', 'm'};
    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());
    vector<unsigned char> vValue;
    if (!GetData(scriptKey, vValue)) {
        nCount = 0;
        return true;
    }

    CDataStream ds(vValue, SER_DISK, CLIENT_VERSION);
    ds >> nCount;
    return true;
}

bool CContractCache::SetContractItemCount(const vector<unsigned char> &vScriptId, int nCount) {
    if (nCount < 0)
        return false;

    vector<unsigned char> scriptKey = {'s', 'd', 'n', 'u', 'm'};
    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());

    vector<unsigned char> vValue;
    vValue.clear();

    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << nCount;
    vValue.insert(vValue.end(), ds.begin(), ds.end());

    if (!SetData(scriptKey, vValue))
        return false;

    return true;
}

bool CContractCache::EraseAppData(const vector<unsigned char> &vScriptId,
                                      const vector<unsigned char> &vScriptKey, CContractDBOperLog &operLog) {
    vector<unsigned char> vKey = {'d', 'a', 't', 'a'};
    vKey.insert(vKey.end(), vScriptId.begin(), vScriptId.end());
    vKey.push_back('_');
    vKey.insert(vKey.end(), vScriptKey.begin(), vScriptKey.end());

    if (HaveData(vKey)) {
        int nCount(0);
        if (!GetContractItemCount(vScriptId, nCount))
            return false;

        if (!SetContractItemCount(vScriptId, --nCount))
            return false;

        vector<unsigned char> vValue;
        if (!GetData(vKey, vValue))
            return false;

        operLog = CContractDBOperLog(vKey, vValue);

        if (!EraseKey(vKey))
            return false;
    }

    return true;
}

bool CContractCache::EraseAppData(const vector<unsigned char> &vKey) {
    if (vKey.size() < 12) {
        return ERRORMSG("EraseAppData delete script data key value error!");
    }
    vector<unsigned char> vScriptId(vKey.begin() + 4, vKey.begin() + 10);
    vector<unsigned char> vScriptKey(vKey.begin() + 11, vKey.end());
    CContractDBOperLog operLog;
    return EraseAppData(vScriptId, vScriptKey, operLog);
}

bool CContractCache::HaveScriptData(const vector<unsigned char> &vScriptId, const vector<unsigned char> &vScriptKey) {
    vector<unsigned char> scriptKey = {'d', 'a', 't', 'a'};
    scriptKey.insert(scriptKey.end(), vScriptId.begin(), vScriptId.end());
    scriptKey.push_back('_');
    scriptKey.insert(scriptKey.end(), vScriptKey.begin(), vScriptKey.end());
    return HaveData(scriptKey);
}

bool CContractCache::GetScript(const int nIndex, CRegID &scriptId, vector<unsigned char> &vValue) {
    vector<unsigned char> tem;
    if (nIndex != 0) {
        tem = scriptId.GetRegIdRaw();
    }
    if (GetScript(nIndex, tem, vValue)) {
        scriptId.SetRegID(tem);
        return true;
    }

    return false;
}

bool CContractCache::SetScript(const CRegID &scriptId, const vector<unsigned char> &vValue) {
    return SetScript(scriptId.GetRegIdRaw(), vValue);
}

bool CContractCache::HaveScript(const CRegID &scriptId) {
    return HaveScript(scriptId.GetRegIdRaw());
}

bool CContractCache::EraseScript(const CRegID &scriptId) {
    return EraseScript(scriptId.GetRegIdRaw());
}

bool CContractCache::GetContractItemCount(const CRegID &scriptId, int &nCount) {
    return GetContractItemCount(scriptId.GetRegIdRaw(), nCount);
}

bool CContractCache::EraseAppData(const CRegID &scriptId, const vector<unsigned char> &vScriptKey, CContractDBOperLog &operLog) {
    return EraseAppData(scriptId.GetRegIdRaw(), vScriptKey, operLog);
}

bool CContractCache::HaveScriptData(const CRegID &scriptId, const vector<unsigned char> &vScriptKey) {
    return HaveScriptData(scriptId.GetRegIdRaw(), vScriptKey);
}

bool CContractCache::GetContractData(const int nCurBlockHeight, const CRegID &scriptId, const vector<unsigned char> &vScriptKey,
                                         vector<unsigned char> &vScriptData) {
    return GetContractData(nCurBlockHeight, scriptId.GetRegIdRaw(), vScriptKey, vScriptData);
}

bool CContractCache::GetContractData(const int nCurBlockHeight, const CRegID &scriptId, const int &nIndex,
                                         vector<unsigned char> &vScriptKey, vector<unsigned char> &vScriptData) {
    return GetContractData(nCurBlockHeight, scriptId.GetRegIdRaw(), nIndex, vScriptKey, vScriptData);
}

bool CContractCache::SetContractData(const CRegID &scriptId, const vector<unsigned char> &vScriptKey,
                                         const vector<unsigned char> &vScriptData, CContractDBOperLog &operLog) {
    return SetContractData(scriptId.GetRegIdRaw(), vScriptKey, vScriptData, operLog);
}

bool CContractCache::SetTxRelAccout(const uint256 &txHash, const set<CKeyID> &relAccount) {
    vector<unsigned char> vKey = {'t', 'x'};
    vector<unsigned char> vValue;
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << txHash;
    vKey.insert(vKey.end(), ds.begin(), ds.end());
    ds.clear();
    ds << relAccount;
    vValue.assign(ds.begin(), ds.end());
    return SetData(vKey, vValue);
}
bool CContractCache::GetTxRelAccount(const uint256 &txHash, set<CKeyID> &relAccount) {
    vector<unsigned char> vKey = {'t', 'x'};
    vector<unsigned char> vValue;
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << txHash;
    vKey.insert(vKey.end(), ds.begin(), ds.end());
    if (!GetData(vKey, vValue))
        return false;
    ds.clear();
    vector<char> temp;
    temp.assign(vValue.begin(), vValue.end());
    ds.insert(ds.end(), temp.begin(), temp.end());
    ds >> relAccount;
    return true;
}

bool CContractCache::EraseTxRelAccout(const uint256 &txHash) {
    vector<unsigned char> vKey = {'t', 'x'};
    vector<unsigned char> vValue;
    vValue.clear();
    CDataStream ds(SER_DISK, CLIENT_VERSION);
    ds << txHash;
    vKey.insert(vKey.end(), ds.begin(), ds.end());
    SetData(vKey, vValue);
    return true;
}

Object CContractCache::ToJsonObj() const {
    Object obj;
    Array arrayObj;
    for (auto& item : mapContractDb) {
        Object obj;
        obj.push_back(Pair("key", HexStr(item.first)));
        obj.push_back(Pair("value", HexStr(item.second)));
        arrayObj.push_back(obj);
    }
    // obj.push_back(Pair("mapContractDb", arrayObj));
    arrayObj.push_back(pBase->ToJsonObj("def"));
    arrayObj.push_back(pBase->ToJsonObj("data"));
    arrayObj.push_back(pBase->ToJsonObj("author"));

    obj.push_back(Pair("mapContractDb", arrayObj));
    return obj;
}

string CContractCache::ToString() {
    string str("");
    vector<unsigned char> vPrefix = {'d', 'a', 't', 'a'};
    for (auto &item : mapContractDb) {
        vector<unsigned char> vTemp(item.first.begin(), item.first.begin() + 4);
        if (vTemp == vPrefix) {
            str = strprintf("vKey=%s\n vData=%s\n", HexStr(item.first), HexStr(item.second));
        }
    }
    return str;
}

bool CContractCache::SetDelegateData(const CAccount &delegateAcct, CContractDBOperLog &operLog) {
    CRegID regId(0, 0);
    vector<unsigned char> vVoteKey = {'d', 'e', 'l', 'e', 'g', 'a', 't', 'e', '_'};
    uint64_t nMaxNumber            = 0xFFFFFFFFFFFFFFFF;
    string strVotes                = strprintf("%016x", nMaxNumber - delegateAcct.receivedVotes);
    vVoteKey.insert(vVoteKey.end(), strVotes.begin(), strVotes.end());
    vVoteKey.push_back('_');
    vVoteKey.insert(vVoteKey.end(), delegateAcct.regID.GetRegIdRaw().begin(), delegateAcct.regID.GetRegIdRaw().end());
    vector<unsigned char> vVoteValue;
    vVoteValue.push_back(1);
    if (!SetContractData(regId, vVoteKey, vVoteValue, operLog))
        return false;

    return true;
}

bool CContractCache::SetDelegateData(const vector<unsigned char> &vKey) {
    if (vKey.empty()) {
        return true;
    }
    vector<unsigned char> vValue;
    vValue.push_back(1);
    if (!SetData(vKey, vValue)) {
        return false;
    }
    return true;
}

bool CContractCache::EraseDelegateData(const CAccountLog &delegateAcct, CContractDBOperLog &operLog) {
    CRegID regId(0, 0);
    vector<unsigned char> vVoteOldKey = {'d', 'e', 'l', 'e', 'g', 'a', 't', 'e', '_'};
    uint64_t nMaxNumber               = 0xFFFFFFFFFFFFFFFF;
    string strOldVoltes               = strprintf("%016x", nMaxNumber - delegateAcct.receivedVotes);
    vVoteOldKey.insert(vVoteOldKey.end(), strOldVoltes.begin(), strOldVoltes.end());
    vVoteOldKey.push_back('_');
    vVoteOldKey.insert(vVoteOldKey.end(), delegateAcct.regID.GetRegIdRaw().begin(), delegateAcct.regID.GetRegIdRaw().end());
    if (!EraseAppData(regId, vVoteOldKey, operLog))
        return false;

    return true;
}

bool CContractCache::EraseDelegateData(const vector<unsigned char> &vKey) {
    if (!EraseKey(vKey))
        return false;

    return true;
}

bool CContractCache::GetScriptAcc(const CRegID &scriptId, const vector<unsigned char> &vAccKey,
                                      CAppUserAccount &appAccOut) {
    vector<unsigned char> scriptKey = {'a', 'c', 'c', 't'};
    vector<unsigned char> vRegId    = scriptId.GetRegIdRaw();
    scriptKey.insert(scriptKey.end(), vRegId.begin(), vRegId.end());
    scriptKey.push_back('_');
    scriptKey.insert(scriptKey.end(), vAccKey.begin(), vAccKey.end());
    vector<unsigned char> vValue;

    //LogPrint("vm","%s",HexStr(scriptKey));
    if (!GetData(scriptKey, vValue))
        return false;
    CDataStream ds(vValue, SER_DISK, CLIENT_VERSION);
    ds >> appAccOut;
    return true;
}

bool CContractCache::SetScriptAcc(const CRegID &scriptId, const CAppUserAccount &appAccOut,
                                      CContractDBOperLog &operlog) {
    vector<unsigned char> scriptKey = {'a', 'c', 'c', 't'};
    vector<unsigned char> vRegId    = scriptId.GetRegIdRaw();
    vector<unsigned char> vAccKey   = appAccOut.GetAccUserId();
    scriptKey.insert(scriptKey.end(), vRegId.begin(), vRegId.end());
    scriptKey.push_back('_');
    scriptKey.insert(scriptKey.end(), vAccKey.begin(), vAccKey.end());
    vector<unsigned char> vValue;
    operlog.vKey = scriptKey;
    if (GetData(scriptKey, vValue)) {
        operlog.vValue = vValue;
    }
    CDataStream ds(SER_DISK, CLIENT_VERSION);

    ds << appAccOut;
    //LogPrint("vm","%s",HexStr(scriptKey));
    vValue.clear();
    vValue.insert(vValue.end(), ds.begin(), ds.end());
    return SetData(scriptKey, vValue);
}

bool CContractCache::EraseScriptAcc(const CRegID &scriptId, const vector<unsigned char> &vKey) {
    vector<unsigned char> scriptKey = {'a', 'c', 'c', 't'};
    vector<unsigned char> vRegId    = scriptId.GetRegIdRaw();
    scriptKey.insert(scriptKey.end(), vRegId.begin(), vRegId.end());
    scriptKey.push_back('_');
    scriptKey.insert(scriptKey.end(), vKey.begin(), vKey.end());
    vector<unsigned char> vValue;

    //LogPrint("vm","%s",HexStr(scriptKey));
    if (!GetData(scriptKey, vValue)) {
        return false;
    }

    return EraseKey(scriptKey);
}

bool CContractCache::GetData(const vector<unsigned char> &vKey, vector<unsigned char> &vValue) {
    if (mapContractDb.count(vKey) > 0) {
        if (!mapContractDb[vKey].empty()) {
            vValue = mapContractDb[vKey];
            return true;
        } else {
            return false;
        }
    }

    if (!pBase->GetData(vKey, vValue)) {
        return false;
    }
    mapContractDb[vKey] = vValue;  //cache it here for speed in-mem access
    return true;
}