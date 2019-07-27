// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2017-2019 The WaykiChain Developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TX_CONTRACT_H
#define TX_CONTRACT_H

#include "tx.h"
#include "entities/contract.h"

class CLuaContractDeployTx : public CBaseTx {
public:
    CLuaContract contract;  // contract script content

public:
    CLuaContractDeployTx(const CBaseTx *pBaseTx): CBaseTx(LCONTRACT_DEPLOY_TX) {
        assert(LCONTRACT_DEPLOY_TX == pBaseTx->nTxType);
        *this = *(CLuaContractDeployTx *)pBaseTx;
    }
    CLuaContractDeployTx(): CBaseTx(LCONTRACT_DEPLOY_TX) {}
    ~CLuaContractDeployTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        nVersion = this->nVersion;
        READWRITE(VARINT(nValidHeight));
        READWRITE(txUid);

        READWRITE(contract);
        READWRITE(VARINT(llFees));
        READWRITE(signature);
    )

    TxID ComputeSignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << uint8_t(nTxType) << VARINT(nValidHeight) << txUid << contract
               << VARINT(llFees);
            sigHash = ss.GetHash();
        }

        return sigHash;
    }

    virtual uint256 GetHash() const { return ComputeSignatureHash(); }
    virtual std::shared_ptr<CBaseTx> GetNewInstance() { return std::make_shared<CLuaContractDeployTx>(this); }
    virtual uint64_t GetFuel(int32_t nFuelRate);
    virtual map<TokenSymbol, uint64_t> GetValues() const { return map<TokenSymbol, uint64_t>{{SYMB::WICC, 0}}; }
    virtual string ToString(CAccountDBCache &view);
    virtual Object ToJson(const CAccountDBCache &AccountView) const;
    virtual bool GetInvolvedKeyIds(CCacheWrapper &cw, set<CKeyID> &keyIds);

    virtual bool CheckTx(int height, CCacheWrapper &cw, CValidationState &state);
    virtual bool ExecuteTx(int height, int index, CCacheWrapper &cw, CValidationState &state);
};

lass CUniversalContractDeployTx : public CBaseTx {
public:
    CUniversalContract contract;  // contract script content
    TokenSymbol coin_symbol;
    uint64_t coin_amount;
    TokenSymbol fee_symbol;

public:
    CUniversalContractDeployTx(const CBaseTx *pBaseTx): CBaseTx(LCONTRACT_DEPLOY_TX) {
        assert(LCONTRACT_DEPLOY_TX == pBaseTx->nTxType);
        *this = *(CUniversalContractDeployTx *)pBaseTx;
    }
    CUniversalContractDeployTx(): CBaseTx(LCONTRACT_DEPLOY_TX) {}
    ~CUniversalContractDeployTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        nVersion = this->nVersion;
        READWRITE(VARINT(nValidHeight));
        READWRITE(txUid);

        READWRITE(contract);
        READWRITE(VARINT(llFees));
        READWRITE(signature);
    )

    TxID ComputeSignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << uint8_t(nTxType) << VARINT(nValidHeight) << txUid << contract
               << VARINT(llFees);
            sigHash = ss.GetHash();
        }

        return sigHash;
    }

    virtual uint256 GetHash() const { return ComputeSignatureHash(); }
    virtual std::shared_ptr<CBaseTx> GetNewInstance() { return std::make_shared<CLuaContractDeployTx>(this); }
    virtual uint64_t GetFuel(int32_t nFuelRate);
    virtual map<TokenSymbol, uint64_t> GetValues() const { return map<TokenSymbol, uint64_t>{{SYMB::WICC, 0}}; }
    virtual string ToString(CAccountDBCache &view);
    virtual Object ToJson(const CAccountDBCache &AccountView) const;
    virtual bool GetInvolvedKeyIds(CCacheWrapper &cw, set<CKeyID> &keyIds);

    virtual bool CheckTx(int height, CCacheWrapper &cw, CValidationState &state);
    virtual bool ExecuteTx(int height, int index, CCacheWrapper &cw, CValidationState &state);
};

class CContractInvokeTx : public CBaseTx {
public:
    mutable CUserID appUid; // app regid or address
    uint64_t bcoins;        // transfer amount
    string arguments;       // arguments to invoke a contract method

public:
    CContractInvokeTx() : CBaseTx(LCONTRACT_INVOKE_TX) {}

    CContractInvokeTx(const CBaseTx *pBaseTx): CBaseTx(LCONTRACT_INVOKE_TX) {
        assert(LCONTRACT_INVOKE_TX == pBaseTx->nTxType);
        *this = *(CContractInvokeTx *)pBaseTx;
    }

    CContractInvokeTx(const CUserID &txUidIn, CUserID appUidIn, uint64_t feesIn,
                uint64_t bcoinsIn, int validHeightIn, string &argumentsIn):
                CBaseTx(LCONTRACT_INVOKE_TX, txUidIn, validHeightIn, feesIn) {
        if (txUidIn.type() == typeid(CRegID))
            assert(!txUidIn.get<CRegID>().IsEmpty()); //FIXME: shouldnot be using assert here, throw an error instead.

        if (appUidIn.type() == typeid(CRegID))
            assert(!appUidIn.get<CRegID>().IsEmpty());

        appUid = appUidIn;
        bcoins = bcoinsIn;
        arguments = argumentsIn;
    }

    CContractInvokeTx(const CUserID &txUidIn, CUserID appUidIn, uint64_t feesIn, uint64_t bcoinsIn, int validHeightIn):
                CBaseTx(LCONTRACT_INVOKE_TX, txUidIn, validHeightIn, feesIn) {
        if (txUidIn.type() == typeid(CRegID))
            assert(!txUidIn.get<CRegID>().IsEmpty());
        else if (txUidIn.type() == typeid(CPubKey))
            assert(txUidIn.get<CPubKey>().IsFullyValid());

        if (appUidIn.type() == typeid(CRegID))
            assert(!appUidIn.get<CRegID>().IsEmpty());

        appUid = appUidIn;
        bcoins = bcoinsIn;
    }

    ~CContractInvokeTx() {}

    IMPLEMENT_SERIALIZE(
        READWRITE(VARINT(this->nVersion));
        nVersion = this->nVersion;
        READWRITE(VARINT(nValidHeight));
        READWRITE(txUid);
        READWRITE(appUid);
        READWRITE(VARINT(llFees));
        READWRITE(VARINT(bcoins));
        READWRITE(arguments);
        READWRITE(signature);
    )

    TxID ComputeSignatureHash(bool recalculate = false) const {
        if (recalculate || sigHash.IsNull()) {
            CHashWriter ss(SER_GETHASH, 0);
            ss << VARINT(nVersion) << uint8_t(nTxType) << VARINT(nValidHeight) << txUid << appUid
               << VARINT(llFees) << VARINT(bcoins) << arguments;
            sigHash = ss.GetHash();
        }
        return sigHash;
    }

    virtual map<TokenSymbol, uint64_t> GetValues() const { return map<TokenSymbol, uint64_t>{{SYMB::WICC, bcoins}}; }
    virtual uint256 GetHash() const { return ComputeSignatureHash(); }
    virtual std::shared_ptr<CBaseTx> GetNewInstance() { return std::make_shared<CContractInvokeTx>(this); }
    virtual string ToString(CAccountDBCache &view);
    virtual Object ToJson(const CAccountDBCache &AccountView) const;
    virtual bool GetInvolvedKeyIds(CCacheWrapper &cw, set<CKeyID> &keyIds);

    virtual bool CheckTx(int height, CCacheWrapper &cw, CValidationState &state);
    virtual bool ExecuteTx(int height, int index, CCacheWrapper &cw, CValidationState &state);
};

#endif //TX_CONTRACT_H