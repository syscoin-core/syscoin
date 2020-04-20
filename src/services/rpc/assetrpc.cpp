﻿// Copyright (c) 2013-2019 The Syscoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <validation.h>
#include <boost/algorithm/string.hpp>
#include <rpc/util.h>
#include <services/assetconsensus.h>
#include <services/rpc/assetrpc.h>
#include <chainparams.h>
#include <rpc/server.h>
#include <validationinterface.h>
#include <thread>
#include <policy/rbf.h>
#include <policy/policy.h>
#include <chrono>
#include <consensus/validation.h>
#include <index/txindex.h>
using namespace std;
extern std::string exePath;
extern std::string EncodeDestination(const CTxDestination& dest);
extern CTxDestination DecodeDestination(const std::string& str);
extern UniValue ValueFromAmount(const CAmount& amount);
extern std::string EncodeHexTx(const CTransaction& tx, const int serializeFlags = 0);
extern bool DecodeHexTx(CMutableTransaction& tx, const std::string& hex_tx, bool try_no_witness = false, bool try_witness = true);
extern RecursiveMutex cs_setethstatus;
using namespace std;
UniValue convertaddress(const JSONRPCRequest& request)	
{	

    RPCHelpMan{"convertaddress",	
        "\nConvert between Syscoin 3 and Syscoin 4 formats. This should only be used with addressed based on compressed private keys only. P2WPKH can be shown as P2PKH in Syscoin 3.\n",	
        {	
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The syscoin address to get the information of."}	
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "v3address", "The syscoin 3 address validated"},
                {RPCResult::Type::STR, "v4address", "The syscoin 4 address validated"},
            }},	
        RPCExamples{	
            HelpExampleCli("convertaddress", "\"sys1qw40fdue7g7r5ugw0epzk7xy24tywncm26hu4a7\"")	
            + HelpExampleRpc("convertaddress", "\"sys1qw40fdue7g7r5ugw0epzk7xy24tywncm26hu4a7\"")	
        }	
    }.Check(request);	

    UniValue ret(UniValue::VOBJ);	
    CTxDestination dest = DecodeDestination(request.params[0].get_str());	
    // Make sure the destination is valid	
    if (!IsValidDestination(dest)) {	
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");	
    }	
    std::string currentV4Address = "";	
    std::string currentV3Address = "";	
    CTxDestination v4Dest;	
    if (auto witness_id = boost::get<WitnessV0KeyHash>(&dest)) {	
        v4Dest = dest;	
        currentV4Address =  EncodeDestination(v4Dest);	
        currentV3Address =  EncodeDestination(PKHash(*witness_id));	
    }	
    else if (auto key_id = boost::get<PKHash>(&dest)) {	
        v4Dest = WitnessV0KeyHash(*key_id);	
        currentV4Address =  EncodeDestination(v4Dest);	
        currentV3Address =  EncodeDestination(*key_id);	
    }	
    else if (auto script_id = boost::get<ScriptHash>(&dest)) {	
        v4Dest = *script_id;	
        currentV4Address =  EncodeDestination(v4Dest);	
        currentV3Address =  currentV4Address;	
    }	
    else if (boost::get<WitnessV0ScriptHash>(&dest)) {	
        v4Dest = dest;	
        currentV4Address =  EncodeDestination(v4Dest);	
        currentV3Address =  currentV4Address;	
    } 	


    ret.pushKV("v3address", currentV3Address);	
    ret.pushKV("v4address", currentV4Address); 	
    return ret;	
}

int CheckActorsInTransactionGraph(const uint256& lookForTxHash){
    LOCK(cs_main);
    LOCK(mempool.cs);
    {
        CTxMemPool::setEntries setAncestors;
        const CTransactionRef &txRef = mempool.get(lookForTxHash);
        if (!txRef)
            return ZDAG_NOT_FOUND;
        if(!IsZdagTx(txRef->nVersion))
            return ZDAG_WARNING_NOT_ZDAG_TX;
        // the zdag tx should be under MTU of IP packet
        if(txRef->GetTotalSize() > MAX_STANDARD_ZDAG_TX_SIZE)
            return ZDAG_WARNING_SIZE_OVER_POLICY;
        // check if any inputs are dbl spent, reject if so
        if(mempool.existsConflicts(*txRef))
            return ZDAG_MAJOR_CONFLICT;        

        // check this transaction isn't RBF enabled
        RBFTransactionState rbfState = IsRBFOptIn(*txRef, mempool, setAncestors);
        if (rbfState == RBFTransactionState::UNKNOWN)
            return ZDAG_NOT_FOUND;
        else if (rbfState == RBFTransactionState::REPLACEABLE_BIP125)
            return ZDAG_WARNING_RBF;
        for (CTxMemPool::txiter it : setAncestors) {
            const CTransactionRef& ancestorTxRef = it->GetSharedTx();
            // should be under MTU of IP packet
            if(ancestorTxRef->GetTotalSize() > MAX_STANDARD_ZDAG_TX_SIZE)
                return ZDAG_WARNING_SIZE_OVER_POLICY;
            // check if any ancestor inputs are dbl spent, reject if so
            if(mempool.existsConflicts(*ancestorTxRef))
                return ZDAG_MAJOR_CONFLICT;
            if(!IsZdagTx(ancestorTxRef->nVersion))
                return ZDAG_WARNING_NOT_ZDAG_TX;
        }  
    }
    return ZDAG_STATUS_OK;
}
int VerifyTransactionGraph(const uint256& lookForTxHash) {  
    int status = CheckActorsInTransactionGraph(lookForTxHash);
    if(status != ZDAG_STATUS_OK){
        return status;
    }
	return ZDAG_STATUS_OK;
}

UniValue assetallocationverifyzdag(const JSONRPCRequest& request) {
	const UniValue &params = request.params;
    RPCHelpMan{"assetallocationverifyzdag",
        "\nShow status as it pertains to any current Z-DAG conflicts or warnings related to a ZDAG transaction.\n"
        "Return value is in the status field and can represent 3 levels(0, 1 or 2)\n"
        "Level -1 means not found, not a ZDAG transaction, perhaps it is already confirmed.\n"
        "Level 0 means OK.\n"
        "Level 1 means warning (checked that in the mempool there are more spending balances than current POW sender balance). An active stance should be taken and perhaps a deeper analysis as to potential conflicts related to the sender.\n"
        "Level 2 means an active double spend was found and any depending asset allocation sends are also flagged as dangerous and should wait for POW confirmation before proceeding.\n",
        {
            {"txid", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction id of the ZDAG transaction."}
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "status", "The status level of the transaction"},
            }}, 
        RPCExamples{
            HelpExampleCli("assetallocationverifyzdag", "\"txid\"")
            + HelpExampleRpc("assetallocationverifyzdag", "\"txid\"")
        }
    }.Check(request);

	uint256 txid;
	txid.SetHex(params[0].get_str());
	UniValue oAssetAllocationStatus(UniValue::VOBJ);
    oAssetAllocationStatus.__pushKV("status", VerifyTransactionGraph(txid));
	return oAssetAllocationStatus;
}



UniValue syscoindecoderawtransaction(const JSONRPCRequest& request) {
    const UniValue &params = request.params;
    RPCHelpMan{"syscoindecoderawtransaction",
    "\nDecode raw syscoin transaction (serialized, hex-encoded) and display information pertaining to the service that is included in the transactiion data output(OP_RETURN)\n",
    {
        {"hexstring", RPCArg::Type::STR_HEX, RPCArg::Optional::NO, "The transaction hex string."}
    },
    RPCResult{
        RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "txtype", "The syscoin transaction type"},
            {RPCResult::Type::NUM, "asset_guid", "The guid of the asset"},
            {RPCResult::Type::STR, "symbol", "The asset symbol"},
            {RPCResult::Type::STR_HEX, "txid", "The transaction id"},
            {RPCResult::Type::NUM, "height", "The blockheight of the transaction"},
            {RPCResult::Type::STR, "sender", "The address of the sender"},
            {RPCResult::Type::ARR, "allocations", "(array of json receiver objects)",
                {
                    {RPCResult::Type::OBJ, "", "",
                    {
                            {RPCResult::Type::STR, "address", "The address of the receiver"},
                            {RPCResult::Type::NUM, "amount", "The amount of the transaction"},
                    }},
                }},
            {RPCResult::Type::NUM, "total", "The total amount in this transaction"},
            {RPCResult::Type::BOOL, "confirmed", "If the transaction is confirmed"},
        }}, 
    RPCExamples{
        HelpExampleCli("syscoindecoderawtransaction", "\"hexstring\"")
        + HelpExampleRpc("syscoindecoderawtransaction", "\"hexstring\"")
    }
    }.Check(request);

    string hexstring = params[0].get_str();
    CMutableTransaction tx;
    if(!DecodeHexTx(tx, hexstring, false, true))
        DecodeHexTx(tx, hexstring, true, true);
    CTransaction rawTx(tx);
    if (rawTx.IsNull())
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Could not decode transaction");
    
    UniValue output(UniValue::VOBJ);
    if(!DecodeSyscoinRawtransaction(rawTx, output))
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Not a Syscoin transaction");
    return output;
}

UniValue assetinfo(const JSONRPCRequest& request) {
    const UniValue &params = request.params;
    RPCHelpMan{"assetinfo",
        "\nShow stored values of a single asset and its.\n",
        {
            {"asset_guid", RPCArg::Type::NUM, RPCArg::Optional::NO, "The asset guid"}
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "asset_guid", "The guid of the asset"},
                {RPCResult::Type::STR, "symbol", "The asset symbol"},
                {RPCResult::Type::STR_HEX, "txid", "The transaction id that created this asset"},
                {RPCResult::Type::STR, "public_value", "The public value attached to this asset"},
                {RPCResult::Type::STR, "address", "The address that controls this asset"},
                {RPCResult::Type::STR_HEX, "contract", "The ethereum contract address"},
                {RPCResult::Type::NUM, "balance", "The current balance"},
                {RPCResult::Type::NUM, "total_supply", "The total supply of this asset"},
                {RPCResult::Type::NUM, "max_supply", "The maximum supply of this asset"},
                {RPCResult::Type::NUM, "update_flag", "The flag in decimal"},
                {RPCResult::Type::NUM, "precision", "The precision of this asset"},
            }},
        RPCExamples{
            HelpExampleCli("assetinfo", "\"assetguid\"")
            + HelpExampleRpc("assetinfo", "\"assetguid\"")
        }
    }.Check(request);

    const int &nAsset = params[0].get_uint();
    UniValue oAsset(UniValue::VOBJ);

    CAsset txPos;
    if (passetdb == nullptr || !passetdb->ReadAsset(nAsset, txPos))
        throw JSONRPCError(RPC_DATABASE_ERROR, "Failed to read from asset DB");
    
    if(!BuildAssetJson(txPos, nAsset, oAsset))
        oAsset.clear();
    return oAsset;
}
UniValue listassets(const JSONRPCRequest& request) {
    const UniValue &params = request.params;
    RPCHelpMan{"listassets",
        "\nScan through all assets.\n",
        {
            {"count", RPCArg::Type::NUM, "10", "The number of results to return."},
            {"from", RPCArg::Type::NUM, "0", "The number of results to skip."},
            {"options", RPCArg::Type::OBJ, RPCArg::Optional::OMITTED, "A json object with options to filter results.",
                {
                    {"asset_guid", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "Asset GUID to filter"},
                    {"addresses", RPCArg::Type::ARR, RPCArg::Optional::OMITTED, "A json array with owners",  
                        {
                            {"address", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Address to filter"},
                        },
                        "[addressobjects,...]"
                    }
                }
                }
            },
            RPCResult{
                RPCResult::Type::ARR, "", "",
                {
                    {RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::NUM, "asset_guid", "The guid of the asset"},
                        {RPCResult::Type::STR, "symbol", "The asset symbol"},
                        {RPCResult::Type::STR, "public_value", "The public value attached to this asset"},
                        {RPCResult::Type::STR, "address", "The address that controls this asset"},
                        {RPCResult::Type::STR_HEX, "contract", "The ethereum contract address"},
                        {RPCResult::Type::NUM, "balance", "The current balance"},
                        {RPCResult::Type::NUM, "burn_balance", "The current burn balance"},
                        {RPCResult::Type::NUM, "total_supply", "The total supply of this asset"},
                        {RPCResult::Type::NUM, "max_supply", "The maximum supply of this asset"},
                        {RPCResult::Type::NUM, "update_flag", "The flag in decimal"},
                        {RPCResult::Type::NUM, "precision", "The precision of this asset"},
                    }},
                }
            },
            RPCExamples{
            HelpExampleCli("listassets", "0")
            + HelpExampleCli("listassets", "10 10")
            + HelpExampleCli("listassets", "0 0 '{\"addresses\":[{\"address\":\"sys1qw40fdue7g7r5ugw0epzk7xy24tywncm26hu4a7\"},{\"address\":\"sys1qw40fdue7g7r5ugw0epzk7xy24tywncm26hu4a7\"}]}'")
            + HelpExampleCli("listassets", "0 0 '{\"asset_guid\":3473733}'")
            + HelpExampleRpc("listassets", "0, 0, '{\"addresses\":[{\"address\":\"sys1qw40fdue7g7r5ugw0epzk7xy24tywncm26hu4a7\"},{\"address\":\"sys1qw40fdue7g7r5ugw0epzk7xy24tywncm26hu4a7\"}]}'")
            + HelpExampleRpc("listassets", "0, 0, '{\"asset_guid\":3473733}'")
            }
    }.Check(request);
    UniValue options;
    uint32_t count = 10;
    uint32_t from = 0;
    if (params.size() > 0) {
        count = params[0].get_uint();
        if (count == 0) {
            count = 10;
        }
    }
    if (params.size() > 1) {
        from = params[1].get_uint();
    }
    if (params.size() > 2) {
        options = params[2];
    }
    UniValue oRes(UniValue::VARR);
    if (!passetdb->ScanAssets(count, from, options, oRes))
        throw JSONRPCError(RPC_MISC_ERROR, "Scan failed");
    return oRes;
}
UniValue syscoingetspvproof(const JSONRPCRequest& request)
{
    RPCHelpMan{"syscoingetspvproof",
    "\nReturns SPV proof for use with inter-chain transfers.\n",
    {
        {"txid", RPCArg::Type::STR, RPCArg::Optional::NO, "A transaction that is in the block"},
        {"blockhash", RPCArg::Type::STR_HEX, RPCArg::Optional::OMITTED_NAMED_ARG, "If specified, looks for txid in the block with this hash"}
    },
    RPCResult{
        RPCResult::Type::STR, "proof", "JSON representation of merkle proof (transaction index, siblings and block header and some other information useful for moving coins/assets to another chain)"},
    RPCExamples{""},
    }.Check(request);
    LOCK(cs_main);
    UniValue res(UniValue::VOBJ);
    uint256 txhash = ParseHashV(request.params[0], "parameter 1");
    CBlockIndex* pblockindex = nullptr;
    uint256 hashBlock;
    if (!request.params[1].isNull()) {
        hashBlock = ParseHashV(request.params[1], "blockhash");
        pblockindex = LookupBlockIndex(hashBlock);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Block not found");
        }
    }


    // Allow txindex to catch up if we need to query it and before we acquire cs_main.
    if (g_txindex && !pblockindex) {
        g_txindex->BlockUntilSyncedToCurrentChain();
    }
    CTransactionRef tx;
    if (pblockindex == nullptr)
    {
        if (!GetTransaction(txhash, tx, Params().GetConsensus(), hashBlock) || hashBlock.IsNull())
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Transaction not yet in block");
        pblockindex = LookupBlockIndex(hashBlock);
        if (!pblockindex) {
            throw JSONRPCError(RPC_INTERNAL_ERROR, "Transaction index corrupt");
        }
    }

    CBlock block;
    if (IsBlockPruned(pblockindex)) {
        throw JSONRPCError(RPC_MISC_ERROR, "Block not available (pruned data)");
    }

    if (!ReadBlockFromDisk(block, pblockindex, Params().GetConsensus())) {
        // Block not found on disk. This could be because we have the block
        // header in our index but don't have the block (for example if a
        // non-whitelisted node sends us an unrequested long chain of valid
        // blocks, we add the headers to our index, but don't accept the
        // block).
        throw JSONRPCError(RPC_MISC_ERROR, "Block not found on disk");
    }   
    CDataStream ssBlock(SER_NETWORK, PROTOCOL_VERSION);
    ssBlock << pblockindex->GetBlockHeader(Params().GetConsensus());
    const std::string &rawTx = EncodeHexTx(*tx, PROTOCOL_VERSION | SERIALIZE_TRANSACTION_NO_WITNESS);
    res.__pushKV("transaction",rawTx);
    res.__pushKV("blockhash", hashBlock.GetHex());
    // get first 80 bytes of header (non auxpow part)
    res.__pushKV("header", HexStr(ssBlock.begin(), ssBlock.begin()+80));
    UniValue siblings(UniValue::VARR);
    // store the index of the transaction we are looking for within the block
    int nIndex = 0;
    for (unsigned int i = 0;i < block.vtx.size();i++) {
        const uint256 &txHashFromBlock = block.vtx[i]->GetHash();
        if(txhash == txHashFromBlock)
            nIndex = i;
        siblings.push_back(txHashFromBlock.GetHex());
    }
    res.__pushKV("siblings", siblings);
    res.__pushKV("index", nIndex);    
    return res;
}

UniValue syscoinstopgeth(const JSONRPCRequest& request) {
    RPCHelpMan{"syscoinstopgeth",
    "\nStops Geth and the relayer from running.\n",
    {},       
    RPCResult{
        RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "status", "Result"},
        }},
    RPCExamples{
        HelpExampleCli("syscoinstopgeth", "")
        + HelpExampleRpc("syscoinstopgeth", "")
    }
    }.Check(request);
    if(!StopRelayerNode(relayerPID))
        throw JSONRPCError(RPC_MISC_ERROR, "Could not stop relayer");
    if(!StopGethNode(gethPID))
        throw JSONRPCError(RPC_MISC_ERROR, "Could not stop Geth");
    UniValue ret(UniValue::VOBJ);
    ret.__pushKV("status", "success");
    return ret;
}
UniValue syscoinstartgeth(const JSONRPCRequest& request) {
    RPCHelpMan{"syscoinstartgeth",
    "\nStarts Geth and the relayer.\n",
    {},
    RPCResult{
        RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR, "status", "Result"},
        }},
    RPCExamples{
        HelpExampleCli("syscoinstartgeth", "")
        + HelpExampleRpc("syscoinstartgeth", "")
    }
    }.Check(request);
    
    StopRelayerNode(relayerPID);
    StopGethNode(gethPID);
    int wsport = gArgs.GetArg("-gethwebsocketport", 8646);
    int ethrpcport = gArgs.GetArg("-gethrpcport", 8645);
    int rpcport = gArgs.GetArg("-rpcport", BaseParams().RPCPort());
    const std::string mode = gArgs.GetArg("-gethsyncmode", "light");
    if(!StartGethNode(exePath, gethPID, wsport, ethrpcport, mode))
        throw JSONRPCError(RPC_MISC_ERROR, "Could not start Geth");
    if(!StartRelayerNode(exePath, relayerPID, rpcport, wsport, ethrpcport))
        throw JSONRPCError(RPC_MISC_ERROR, "Could not stop relayer");
    
    UniValue ret(UniValue::VOBJ);
    ret.__pushKV("status", "success");
    return ret;
}
UniValue syscoinsetethstatus(const JSONRPCRequest& request) {
    const UniValue &params = request.params;
    RPCHelpMan{"syscoinsetethstatus",
        "\nSets ethereum syncing and network status for indication status of network sync.\n",
        {
            {"syncing_status", RPCArg::Type::STR, RPCArg::Optional::NO, "Syncing status ether 'syncing' or 'synced'"},
            {"highest_block", RPCArg::Type::NUM, RPCArg::Optional::NO, "What the highest block height on Ethereum is found to be.  Usually coupled with syncing_status of 'syncing'.  Set to 0 if sync_status is 'synced'"}
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "status", "Result"},
            }},
        RPCExamples{
            HelpExampleCli("syscoinsetethstatus", "\"syncing\" 7000000")
            + HelpExampleCli("syscoinsetethstatus", "\"synced\" 0")
            + HelpExampleRpc("syscoinsetethstatus", "\"syncing\", 7000000")
            + HelpExampleRpc("syscoinsetethstatus", "\"synced\", 0")
        }
        }.Check(request);
    UniValue ret(UniValue::VOBJ);
    UniValue retArray(UniValue::VARR);
    static uint64_t nLastExecTime = GetSystemTimeInSeconds();
    if(!fRegTest && GetSystemTimeInSeconds() - nLastExecTime <= 60){
        LogPrint(BCLog::SYS, "Please wait at least 1 minute between status calls\n");
        ret.__pushKV("missing_blocks", retArray);
        return ret;
    }
    string status = params[0].get_str();
    uint32_t highestBlock = params[1].get_uint();
    const uint32_t nGethOldHeight = fGethCurrentHeight;
    
    if(highestBlock > 0){
        if(!pethereumtxrootsdb->PruneTxRoots(highestBlock))
        {
            LogPrintf("Failed to write to prune Ethereum TX Roots database!\n");
            ret.__pushKV("missing_blocks", retArray);
            return ret;
        }
    }
    std::vector<std::pair<uint32_t, uint32_t> > vecMissingBlockRanges;
    pethereumtxrootsdb->AuditTxRootDB(vecMissingBlockRanges);
    fGethSyncStatus = status; 
    if(!fGethSynced && fGethSyncStatus == "synced" && vecMissingBlockRanges.empty())  {     
        fGethSynced = true;
    }
    if(fGethSyncStatus == "synced"){
        for(const auto& range: vecMissingBlockRanges){
            UniValue retRange(UniValue::VOBJ);
            retRange.__pushKV("from", range.first);
            retRange.__pushKV("to", range.second);
            retArray.push_back(retRange);
        }
    }
    LogPrint(BCLog::SYS, "syscoinsetethstatus old height %d new height %d\n", nGethOldHeight, fGethCurrentHeight);
    ret.__pushKV("missing_blocks", retArray);
    if(fZMQEthStatus){
        UniValue oEthStatus(UniValue::VOBJ);
        oEthStatus.__pushKV("geth_sync_status",  fGethSyncStatus);
        oEthStatus.__pushKV("geth_total_blocks",  fGethSyncHeight);
        oEthStatus.__pushKV("geth_current_block",  fGethCurrentHeight);
        oEthStatus.push_back(ret);
        GetMainSignals().NotifySyscoinUpdate(oEthStatus.write().c_str(), "ethstatus");
    }
    nLastExecTime = GetSystemTimeInSeconds();
    return ret;
}
UniValue syscoinsetethheaders(const JSONRPCRequest& request) {
    const UniValue &params = request.params;
    RPCHelpMan{"syscoinsetethheaders",
        "\nSets Ethereum headers in Syscoin to validate transactions through the SYSX bridge.\n",
        {
            {"headers", RPCArg::Type::ARR, RPCArg::Optional::NO, "An array of arrays (block number, tx root) from Ethereum blockchain", 
                {
                    {"", RPCArg::Type::ARR, RPCArg::Optional::OMITTED, "An array of [block number, tx root] ",
                        {
                            {"block_number", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "The block height number"},
                            {"block_hash", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Hash of the block"},
                            {"previous_hash", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "Hash of the previous block"},
                            {"tx_root", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "The Ethereum TX root of the block height"},
                            {"receipt_root", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "The Ethereum TX Receipt root of the block height"},
                            {"timestamp", RPCArg::Type::NUM, RPCArg::Optional::OMITTED, "The Ethereum block timestamp"},
                        }
                    }
                },
                "[blocknumber, blockhash, previoushash, txroot, txreceiptroot, timestamp] ..."
            }
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "status", "Result"},
            }},
        RPCExamples{
            HelpExampleCli("syscoinsetethheaders", "\"[[7043888,\\\"0xd8ac75c7b4084c85a89d6e28219ff162661efb8b794d4b66e6e9ea52b4139b10\\\",\\\"0xd8ac75c7b4084c85a89d6e28219ff162661efb8b794d4b66e6e9ea52b4139b10\\\",\\\"0xd8ac75c7b4084c85a89d6e28219ff162661efb8b794d4b66e6e9ea52b4139b10\\\"],...]\"")
            + HelpExampleRpc("syscoinsetethheaders", "\"[[7043888,\\\"0xd8ac75c7b4084c85a89d6e28219ff162661efb8b794d4b66e6e9ea52b4139b10\\\",\\\"0xd8ac75c7b4084c85a89d6e28219ff162661efb8b794d4b66e6e9ea52b4139b10\\\",\\\"0xd8ac75c7b4084c85a89d6e28219ff162661efb8b794d4b66e6e9ea52b4139b10\\\"],...]\"")
        }
    }.Check(request);
    LOCK(cs_setethstatus);
    EthereumTxRootMap txRootMap;       
    const UniValue &headerArray = params[0].get_array();
    
    for(size_t i =0;i<headerArray.size();i++){
        EthereumTxRoot txRoot;
        const UniValue &tupleArray = headerArray[i].get_array();
        if(tupleArray.size() != 6)
            throw JSONRPCError(RPC_INVALID_PARAMS, "Invalid size in a Ethereum header input, should be size of 6");
        const uint32_t &nHeight = tupleArray[0].get_uint();
        string blockHash = tupleArray[1].get_str();
        boost::erase_all(blockHash, "0x");  // strip 0x
        txRoot.vchBlockHash = ParseHex(blockHash);
        string prevHash = tupleArray[2].get_str();
        boost::erase_all(prevHash, "0x");  // strip 0x
        txRoot.vchPrevHash = ParseHex(prevHash);
        string txRootStr = tupleArray[3].get_str();
        boost::erase_all(txRootStr, "0x");  // strip 0x
        txRoot.vchTxRoot = ParseHex(txRootStr);
        string txReceiptRoot = tupleArray[4].get_str();
        boost::erase_all(txReceiptRoot, "0x");  // strip 0x
        txRoot.vchReceiptRoot = ParseHex(txReceiptRoot);
        const int64_t &nTimestamp = tupleArray[5].get_int64();
        txRoot.nTimestamp = nTimestamp;
        txRootMap.emplace(std::piecewise_construct,  std::forward_as_tuple(nHeight),  std::forward_as_tuple(txRoot));
    } 
    bool res = pethereumtxrootsdb->FlushWrite(txRootMap);
    UniValue ret(UniValue::VOBJ);
    ret.__pushKV("status", res? "success": "fail");
    return ret;
}
UniValue syscoinclearethheaders(const JSONRPCRequest& request) {
    RPCHelpMan{"syscoinclearethheaders",
        "\nClears Ethereum headers in Syscoin.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR, "status", "Result"},
            }}, 
        RPCExamples{
            HelpExampleCli("syscoinclearethheaders", "")
            + HelpExampleRpc("syscoinclearethheaders", "")
        }
    }.Check(request);
    bool res = pethereumtxrootsdb->Clear();
    UniValue ret(UniValue::VOBJ);
    ret.__pushKV("status", res? "success": "fail");
    return ret;
}
UniValue syscoingettxroots(const JSONRPCRequest& request)
{
    RPCHelpMan{"syscoingettxroot",
    "\nGet Ethereum transaction and receipt roots based on block height.\n",
    {
        {"height", RPCArg::Type::NUM, RPCArg::Optional::NO, "The block height to lookup."}
    },
    RPCResult{
        RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR_HEX, "txroot", "The transaction merkle root"},
            {RPCResult::Type::STR_HEX, "receiptroot", "The receipt merkle root"},
        }},
    RPCExamples{
        HelpExampleCli("syscoingettxroots", "23232322")
        + HelpExampleRpc("syscoingettxroots", "23232322")
    }
    }.Check(request);
    LOCK(cs_setethstatus);
    uint32_t nHeight = request.params[0].get_uint();
    std::pair<std::vector<unsigned char>,std::vector<unsigned char>> vchTxRoots;
    EthereumTxRoot txRootDB;
    if(!pethereumtxrootsdb || !pethereumtxrootsdb->ReadTxRoots(nHeight, txRootDB)){
       throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Could not read transaction roots");
    }
      
    UniValue ret(UniValue::VOBJ);  
    ret.pushKV("blockhash", HexStr(txRootDB.vchBlockHash));
    ret.pushKV("prevhash", HexStr(txRootDB.vchPrevHash)); 
    ret.pushKV("txroot", HexStr(txRootDB.vchTxRoot));
    ret.pushKV("receiptroot", HexStr(txRootDB.vchReceiptRoot));
    ret.pushKV("timestamp", txRootDB.nTimestamp);
    
    return ret;
} 
UniValue syscoincheckmint(const JSONRPCRequest& request)
{
    RPCHelpMan{"syscoincheckmint",
    "\nGet the Syscoin mint transaction by looking up using Bridge Transfer ID.\n",
    {
        {"bridge_transfer_id", RPCArg::Type::NUM, RPCArg::Optional::NO, "Ethereum Bridge Transfer ID used to burn funds to move to Syscoin."}
    },
    RPCResult{
        RPCResult::Type::OBJ, "", "",
        {
            {RPCResult::Type::STR_HEX, "txid", "The transaction id"},
        }}, 
    RPCExamples{
        HelpExampleCli("syscoincheckmint", "1221")
        + HelpExampleRpc("syscoincheckmint", "1221")
    }
    }.Check(request);
    const uint32_t nBridgeTransferID = request.params[0].get_uint();
    uint256 sysTxid;
    if(!pethereumtxmintdb || !pethereumtxmintdb->Read(nBridgeTransferID, sysTxid)){
       throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Could not read Syscoin transaction using Bridge Transfer ID");
    }
    UniValue output(UniValue::VOBJ);
    output.pushKV("txid", sysTxid.GetHex());
    return output;
} 
CAmount getAuxFee(const std::string &public_data, const CAmount& nAmount, const uint8_t &nPrecision, CTxDestination & address) {
    UniValue publicObj;
    if(!publicObj.read(public_data))
        return -1;
    const UniValue &auxFeesObj = find_value(publicObj, "aux_fees");
    if(!auxFeesObj.isObject())
        return -1;
    const UniValue &addressObj = find_value(auxFeesObj, "address");
    if(!addressObj.isStr())
        return -1;
    address = DecodeDestination(addressObj.get_str());
    const UniValue &feeStructObj = find_value(auxFeesObj, "fee_struct");
    if(!feeStructObj.isArray())
        return -1;
    const UniValue &feeStructArray = feeStructObj.get_array();
    if(feeStructArray.size() == 0)
        return -1;
     
    CAmount nAccumulatedFee = 0;
    CAmount nBoundAmount = 0;
    CAmount nNextBoundAmount = 0;
    double nRate = 0;
    for(unsigned int i =0;i<feeStructArray.size();i++){
        if(!feeStructArray[i].isArray())
            return -1;
        const UniValue &feeStruct = feeStructArray[i].get_array();
        const UniValue &feeStructNext = feeStructArray[i < feeStructArray.size()-1? i+1:i].get_array();
        if(!feeStruct[0].isStr() && !feeStruct[0].isNum())
            return -1;
        if(!feeStructNext[0].isStr() && !feeStructNext[0].isNum())
                return -1;   
        UniValue boundValue = feeStruct[0]; 
        UniValue nextBoundValue = feeStructNext[0]; 
        nBoundAmount = AssetAmountFromValue(boundValue, nPrecision);
        nNextBoundAmount = AssetAmountFromValue(nextBoundValue, nPrecision);
        if(!feeStruct[1].isStr())
            return -1;
        if(!ParseDouble(feeStruct[1].get_str(), &nRate))
            return -1;
        // case where amount is in between the bounds
        if(nAmount >= nBoundAmount && nAmount < nNextBoundAmount){
            break;    
        }
        nBoundAmount = nNextBoundAmount - nBoundAmount;
        // must be last bound
        if(nBoundAmount <= 0){
            return (nAmount - nNextBoundAmount) * nRate + nAccumulatedFee;
        }
        nAccumulatedFee += (nBoundAmount * nRate);
    }
    return (nAmount - nBoundAmount) * nRate + nAccumulatedFee;    
}
// clang-format off
static const CRPCCommand commands[] =
{ //  category              name                                actor (function)                argNames
    //  --------------------- ------------------------          -----------------------         ----------
    { "syscoin",            "syscoingettxroots",                &syscoingettxroots,             {"height"} },
    { "syscoin",            "syscoingetspvproof",               &syscoingetspvproof,            {"txid"} },
    { "syscoin",            "convertaddress",                   &convertaddress,                {"address"} },
    { "syscoin",            "syscoindecoderawtransaction",      &syscoindecoderawtransaction,   {}},
    { "syscoin",            "assetinfo",                        &assetinfo,                     {"asset_guid"}},
    { "syscoin",            "listassets",                       &listassets,                    {"count","from","options"} },
    { "syscoin",            "assetallocationverifyzdag",        &assetallocationverifyzdag,     {"txid"} },
    { "syscoin",            "syscoinsetethstatus",              &syscoinsetethstatus,           {"syncing_status","highestBlock"} },
    { "syscoin",            "syscoinsetethheaders",             &syscoinsetethheaders,          {"headers"} },
    { "syscoin",            "syscoinclearethheaders",           &syscoinclearethheaders,        {} },
    { "syscoin",            "syscoinstopgeth",                  &syscoinstopgeth,               {} },
    { "syscoin",            "syscoinstartgeth",                 &syscoinstartgeth,              {} },
    { "syscoin",            "syscoincheckmint",                 &syscoincheckmint,              {"ethtxid"} },
};
// clang-format on
void RegisterAssetRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
