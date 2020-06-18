// Copyright (c) 2017-2019 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "cbtx.h"
#include "core_io.h"
#include "deterministicmns.h"
#include "simplifiedmns.h"
#include "specialtx.h"

#include "base58.h"
#include "chainparams.h"
#include "consensus/merkle.h"
#include "univalue.h"
#include "validation.h"

CSimplifiedMNListEntry::CSimplifiedMNListEntry(const CDeterministicMN& dmn) :
    proRegTxHash(dmn.proTxHash),
    confirmedHash(dmn.pdmnState->confirmedHash),
    service(dmn.pdmnState->addr),
    pubKeyOperator(dmn.pdmnState->pubKeyOperator),
    keyIDVoting(dmn.pdmnState->keyIDVoting),
    isValid(dmn.pdmnState->nPoSeBanHeight == -1)
{
}

uint256 CSimplifiedMNListEntry::CalcHash() const
{
    CHashWriter hw(SER_GETHASH, CLIENT_VERSION);
    hw << *this;
    return hw.GetHash();
}

std::string CSimplifiedMNListEntry::ToString() const
{
    return strprintf("CSimplifiedMNListEntry(proRegTxHash=%s, confirmedHash=%s, service=%s, pubKeyOperator=%s, votingAddress=%s, isValid=%d)",
        proRegTxHash.ToString(), confirmedHash.ToString(), service.ToString(false), EncodeDestination(CTxDestination(pubKeyOperator)), EncodeDestination(CTxDestination(keyIDVoting)), isValid);
}

void CSimplifiedMNListEntry::ToJson(UniValue& obj) const
{
    obj.clear();
    obj.setObject();
    obj.push_back(Pair("proRegTxHash", proRegTxHash.ToString()));
    obj.push_back(Pair("confirmedHash", confirmedHash.ToString()));
    obj.push_back(Pair("service", service.ToString(false)));
    obj.push_back(Pair("pubKeyOperator", EncodeDestination(CTxDestination(pubKeyOperator))));
    obj.push_back(Pair("votingAddress", EncodeDestination(CTxDestination(keyIDVoting))));
    obj.push_back(Pair("isValid", isValid));
}

CSimplifiedMNList::CSimplifiedMNList(const std::vector<CSimplifiedMNListEntry>& smlEntries)
{
    mnList.resize(smlEntries.size());
    for (size_t i = 0; i < smlEntries.size(); i++) {
        mnList[i] = std::make_unique<CSimplifiedMNListEntry>(smlEntries[i]);
    }

    std::sort(mnList.begin(), mnList.end(), [&](const std::unique_ptr<CSimplifiedMNListEntry>& a, const std::unique_ptr<CSimplifiedMNListEntry>& b) {
        return a->proRegTxHash.Compare(b->proRegTxHash) < 0;
    });
}

CSimplifiedMNList::CSimplifiedMNList(const CDeterministicMNList& dmnList)
{
    mnList.resize(dmnList.GetAllMNsCount());

    size_t i = 0;
    dmnList.ForEachMN(false, [this, &i](const CDeterministicMNCPtr& dmn) {
        mnList[i++] = std::make_unique<CSimplifiedMNListEntry>(*dmn);
    });

    std::sort(mnList.begin(), mnList.end(), [&](const std::unique_ptr<CSimplifiedMNListEntry>& a, const std::unique_ptr<CSimplifiedMNListEntry>& b) {
        return a->proRegTxHash.Compare(b->proRegTxHash) < 0;
    });
}

uint256 CSimplifiedMNList::CalcMerkleRoot(bool* pmutated) const
{
    std::vector<uint256> leaves;
    leaves.reserve(mnList.size());
    for (const auto& e : mnList) {
        leaves.emplace_back(e->CalcHash());
    }
    return ComputeMerkleRoot(leaves, pmutated);
}

CSimplifiedMNListDiff::CSimplifiedMNListDiff()
{
}

CSimplifiedMNListDiff::~CSimplifiedMNListDiff()
{
}

void CSimplifiedMNListDiff::ToJson(UniValue& obj) const
{
    obj.setObject();

    obj.push_back(Pair("baseBlockHash", baseBlockHash.ToString()));
    obj.push_back(Pair("blockHash", blockHash.ToString()));

    CDataStream ssCbTxMerkleTree(SER_NETWORK, PROTOCOL_VERSION);
    ssCbTxMerkleTree << cbTxMerkleTree;
    obj.push_back(Pair("cbTxMerkleTree", HexStr(ssCbTxMerkleTree.begin(), ssCbTxMerkleTree.end())));

    obj.push_back(Pair("cbTx", EncodeHexTx(*cbTx)));

    UniValue deletedMNsArr(UniValue::VARR);
    for (const auto& h : deletedMNs) {
        deletedMNsArr.push_back(h.ToString());
    }
    obj.push_back(Pair("deletedMNs", deletedMNsArr));

    UniValue mnListArr(UniValue::VARR);
    for (const auto& e : mnList) {
        UniValue eObj;
        e.ToJson(eObj);
        mnListArr.push_back(eObj);
    }
    obj.push_back(Pair("mnList", mnListArr));

    CCbTx cbTxPayload;
    if (GetTxPayload(*cbTx, cbTxPayload)) {
        obj.push_back(Pair("merkleRootMNList", cbTxPayload.merkleRootMNList.ToString()));
    }
}

bool BuildSimplifiedMNListDiff(const uint256& baseBlockHash, const uint256& blockHash, CSimplifiedMNListDiff& mnListDiffRet, std::string& errorRet)
{
    AssertLockHeld(cs_main);
    mnListDiffRet = CSimplifiedMNListDiff();

    const CBlockIndex* baseBlockIndex = ::ChainActive().Genesis();
    if (!baseBlockHash.IsNull()) {
        auto it = mapBlockIndex.find(baseBlockHash);
        if (it == mapBlockIndex.end()) {
            errorRet = strprintf("block %s not found", baseBlockHash.ToString());
            return false;
        }
        baseBlockIndex = it->second;
    }
    auto blockIt = mapBlockIndex.find(blockHash);
    if (blockIt == mapBlockIndex.end()) {
        errorRet = strprintf("block %s not found", blockHash.ToString());
        return false;
    }
    const CBlockIndex* blockIndex = blockIt->second;

    if (!::ChainActive().Contains(baseBlockIndex) || !::ChainActive().Contains(blockIndex)) {
        errorRet = strprintf("block %s and %s are not in the same chain", baseBlockHash.ToString(), blockHash.ToString());
        return false;
    }
    if (baseBlockIndex->nHeight > blockIndex->nHeight) {
        errorRet = strprintf("base block %s is higher then block %s", baseBlockHash.ToString(), blockHash.ToString());
        return false;
    }

    LOCK(deterministicMNManager->cs);

    auto baseDmnList = deterministicMNManager->GetListForBlock(baseBlockIndex);
    auto dmnList = deterministicMNManager->GetListForBlock(blockIndex);
    mnListDiffRet = baseDmnList.BuildSimplifiedDiff(dmnList);

    // We need to return the value that was provided by the other peer as it otherwise won't be able to recognize the
    // response. This will usually be identical to the block found in baseBlockIndex. The only difference is when a
    // null block hash was provided to get the diff from the genesis block.
    mnListDiffRet.baseBlockHash = baseBlockHash;

    // TODO store coinbase TX in CBlockIndex
    CBlock block;
    if (!ReadBlockFromDisk(block, blockIndex, Params().GetConsensus())) {
        errorRet = strprintf("failed to read block %s from disk", blockHash.ToString());
        return false;
    }

    mnListDiffRet.cbTx = block.vtx[0];

    std::vector<uint256> vHashes;
    std::vector<bool> vMatch(block.vtx.size(), false);
    for (const auto& tx : block.vtx) {
        vHashes.emplace_back(tx->GetHash());
    }
    vMatch[0] = true; // only coinbase matches
    mnListDiffRet.cbTxMerkleTree = CPartialMerkleTree(vHashes, vMatch);

    return true;
}
