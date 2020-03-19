// Copyright (c) 2016-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_syscoin_services.h>
#include <test/data/ethspv_valid.json.h>
#include <util/time.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <services/asset.h>
#include <base58.h>
#include <chainparams.h>
#include <boost/test/unit_test.hpp>
#include <iterator>
#include <core_io.h>
#include <key.h>
#include <math.h>
#include <key_io.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <services/rpc/assetrpc.h>
#include <services/assetconsensus.h>
using namespace std;
extern UniValue read_json(const std::string& jsondata);
BOOST_FIXTURE_TEST_SUITE(syscoin_asset_tests, BasicSyscoinTestingSetup)
BOOST_AUTO_TEST_CASE(generate_big_assetdata)
{
	tfm::format(std::cout,"Running generate_big_assetdata...\n");
	GenerateBlocks(5);
	string newaddress = GetNewFundedAddress("node1");
	// 480 bytes long
	string gooddata = "SfsddfdfsdsdffsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsDfdfddSfsddfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdssfsddSfdd";
	// 481 bytes long (makes 513 with the pubdata overhead)
	UniValue r;
	string baddata = gooddata + "a";
	string guid = AssetNew("node1", newaddress, gooddata);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "listassets"));
	UniValue rArray = r.get_array();
	BOOST_CHECK(rArray.size() > 0);
	BOOST_CHECK(itostr(find_value(rArray[0].get_obj(), "asset_guid").get_uint()) ==  guid || itostr(find_value(rArray[0].get_obj(), "asset_guid").get_uint()) ==  strSYSXAsset);
	string guid1 = AssetNew("node1", newaddress, gooddata);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo" , guid));
	BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_uint()) == guid);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo" , guid1));
    BOOST_CHECK(itostr(find_value(r.get_obj(), "asset_guid").get_uint()) == guid1);
    // data too big
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew" , "\"" + newaddress + "\",\"tmp\",\"" + baddata + "\",\"''\",3,2000,1000,31,{},\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
}
BOOST_AUTO_TEST_CASE(generate_map_test)
{
	tfm::format(std::cout,"Running generate_map_test...\n");
	AssetMap mapAssets;
    AssetAllocationMap mapAssetAllocations;
    for(int i =0;i<10000;i++){
        std::string recv = itostr(i);
        #if __cplusplus > 201402 
        auto result = mapAssets.try_emplace(i,  std::move(emptyAsset));
        auto result1 = mapAssetAllocations.try_emplace(recv,  std::move(emptyAllocation));
        #else
        auto result =  mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(i),  std::forward_as_tuple(std::move(emptyAsset)));
        auto result1 = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(recv),  std::forward_as_tuple(std::move(emptyAllocation)));
        #endif 
    }
    for(int i =0;i<10000;i++){
        std::string recv = itostr(i);
        #if __cplusplus > 201402 
        auto result = mapAssets.try_emplace(i,  std::move(emptyAsset));
        auto result1 = mapAssetAllocations.try_emplace(recv,  std::move(emptyAllocation));
        #else
        auto result =  mapAssets.emplace(std::piecewise_construct,  std::forward_as_tuple(i),  std::forward_as_tuple(std::move(emptyAsset)));
        auto result1 = mapAssetAllocations.emplace(std::piecewise_construct,  std::forward_as_tuple(recv),  std::forward_as_tuple(std::move(emptyAllocation)));
        #endif 
        if((i%2) == 0){
            CAsset tmpAsset;
            tmpAsset.nBalance = 1;
            result.first->second = std::move(tmpAsset);
            CAssetAllocationDBEntry tmpDbEntry;
            tmpDbEntry.nBalance = 1;
            result1.first->second = std::move(tmpDbEntry);
        } else {
            result.first->second.SetNull();
            result1.first->second.nBalance = 0;
        }
    }
  
   for(int i =0;i<10000;i++){
        if((i%2) != 0){
            auto &asset = mapAssets[i];
		    BOOST_CHECK (asset.IsNull());
            auto &assetallocation = mapAssetAllocations[itostr(i)];
		    BOOST_CHECK (assetallocation.nBalance <= 0);
        }
    }
  

}
BOOST_AUTO_TEST_CASE(generate_asset_spt_sysx)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_asset_spt_sysx...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");
    string updateFlags = itostr(ASSET_UPDATE_CONTRACT | ASSET_UPDATE_DATA);
    // cannot edit supply
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetupdate" , strSYSXAsset + ",\"pub\",\"''\",1," + updateFlags + ",{},\"''\""));

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" + find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

    // can update contract + pub data
    AssetUpdate("node1", strSYSXAsset, "pub", "''", updateFlags, "0x931d387731bbbc988b312206c74f77d004d6b84b");   
}
// keep two miner nodes on different chains, smaller chain gets assetnew and longer chain must be able to include it in blocks once longest chain rule plays out
BOOST_AUTO_TEST_CASE(generate_asset_reorg_activechain)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_asset_reorg_activechain...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");
    string assetaddress = GetNewFundedAddress("node3");
    StopNodes();
    // start node connecting to one another
    StartNode("node1", true, "", false, false);
    StartNode("node2", true, "", false, false);
    StartNode("node3", true, "", false, false);
    // send asset new to all the nodes so it is in mempool
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetnew" , "\"" +  assetaddress + "\",\"test\",\"''\",\"''\",8,250,250,31,{},\"''\""));
    string guid = itostr(find_value(r.get_obj(), "asset_guid").get_uint());
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "signrawtransactionwithwallet", "\"" + find_value(r.get_obj(), "hex").get_str() + "\""));
    string hex_str = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hex_str + "\""));
    // generate 11 blocks for node1 and 10 for node3
	string sBlocks = strprintf("%d", 1);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
	string newaddress = r.get_str();
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "generatetoaddress", sBlocks + ",\"" + newaddress + "\"", false));

	sBlocks = strprintf("%d", 1);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "getnewaddress"));
	newaddress = r.get_str();
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "generatetoaddress", sBlocks + ",\"" + newaddress + "\"", false));

    // save tip for longest chain, it should match this after we add our nodes
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getblockchaininfo"));
    string longestChain1 = find_value(r.get_obj(), "bestblockhash").get_str();
    // save tip for longest chain, it should match this after we add our nodes
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "getblockchaininfo"));
    string longestChain3 = find_value(r.get_obj(), "bestblockhash").get_str();

    // now connect the nodes, node 1 should see node 3 chain as orphan chain
    // then node 1 chain will become longest but it should include the asset new in its chain when we mine on it
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "addnode", "\"127.0.0.1:18379\", \"onetry\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "addnode", "\"127.0.0.1:18389\", \"onetry\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "addnode", "\"127.0.0.1:18369\", \"onetry\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "addnode", "\"127.0.0.1:18389\", \"onetry\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "addnode", "\"127.0.0.1:18369\", \"onetry\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "addnode", "\"127.0.0.1:18379\", \"onetry\""));
    sBlocks = strprintf("%d", 1);
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "getnewaddress"));
	newaddress = r.get_str();
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "generatetoaddress", sBlocks + ",\"" + newaddress + "\"", false));   

    // ensure the tips match after adding nodes, syncs to longest chain
    UninterruptibleSleep(std::chrono::milliseconds(1000));
    // shouldn't have asset in the chain because its not mined on the longest chain yet
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid + "\""), runtime_error);
    BOOST_CHECK_THROW(r = CallExtRPC("node2", "assetinfo", "\"" + guid + "\""), runtime_error);
    BOOST_CHECK_THROW(r = CallExtRPC("node3", "assetinfo", "\"" + guid + "\""), runtime_error);
    GenerateBlocks(1, "node3");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "sendrawtransaction" , "\"" + hex_str + "\""));
    GenerateBlocks(1, "node3");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid + "\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetinfo", "\"" + guid + "\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetinfo", "\"" + guid + "\""));
    // mine a few on the node with shorter chain initially for sanity and then on the other nodes
    GenerateBlocks(5, "node3");
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node1");
}
BOOST_AUTO_TEST_CASE(generate_auxfees)
{
	tfm::format(std::cout,"Running generate_auxfees...\n");
	string newaddress = GetNewFundedAddress("node1");
    string newaddressfee = GetNewFundedAddress("node1");
    UniValue pubDataObj(UniValue::VOBJ);
    pubDataObj.pushKV("description", "AGX silver backed token, licensed and operated by Interfix corporation");
    UniValue auxfeesObj(UniValue::VOBJ);
    auxfeesObj.pushKV("address", newaddressfee);
    UniValue feestructArr(UniValue::VARR);
    UniValue boundsArr(UniValue::VARR);
    boundsArr.push_back("0");
    boundsArr.push_back("0.01");
    feestructArr.push_back(boundsArr);
    UniValue boundsArr1(UniValue::VARR);
    boundsArr1.push_back("10");
    boundsArr1.push_back("0.004");
    feestructArr.push_back(boundsArr1);
    UniValue boundsArr2(UniValue::VARR);
    boundsArr2.push_back("250");
    boundsArr2.push_back("0.002");
    feestructArr.push_back(boundsArr2);
    UniValue boundsArr3(UniValue::VARR);
    boundsArr3.push_back("2500");
    boundsArr3.push_back("0.0007");
    feestructArr.push_back(boundsArr3);
    UniValue boundsArr4(UniValue::VARR);
    boundsArr4.push_back("25000");
    boundsArr4.push_back("0.00007");
    feestructArr.push_back(boundsArr4);
    UniValue boundsArr5(UniValue::VARR);
    boundsArr5.push_back("2500000");
    boundsArr5.push_back("0");
    feestructArr.push_back(boundsArr5);
    auxfeesObj.pushKV("fee_struct", feestructArr);
    pubDataObj.pushKV("aux_fees", auxfeesObj);
    CWitnessAddress address;
    const std::string& pubDataStr = pubDataObj.write();
 
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 5000000*COIN,8, address), 19456000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 25000000*COIN,8, address), 19456000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 10*COIN,8, address), 10000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 3000*COIN,8, address), 591000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 6*COIN,8, address), 6000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 250*COIN,8, address), 106000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 500*COIN,8, address), 156000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 1250*COIN,8, address), 306000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 2500*COIN,8, address), 556000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 7500*COIN,8, address), 906000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 20000*COIN,8, address), 1781000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 25000*COIN,8, address), 2131000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 60000*COIN,8, address), 2376000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 180000*COIN,8, address), 3216000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 700000*COIN,8, address), 6856000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 2500000*COIN,8, address), 19456000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 200.1005005*COIN,8, address), 86040200);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 2500001*COIN,8, address), 19456000000);
}
BOOST_AUTO_TEST_CASE(generate_auxfees1)
{
	tfm::format(std::cout,"Running generate_auxfees1...\n");
	string newaddress = GetNewFundedAddress("node1");
    string newaddressfee = GetNewFundedAddress("node1");
    UniValue pubDataObj(UniValue::VOBJ);
    pubDataObj.pushKV("description", "AGX silver backed token, licensed and operated by Interfix corporation");
    UniValue auxfeesObj(UniValue::VOBJ);
    auxfeesObj.pushKV("address", newaddressfee);
    UniValue feestructArr(UniValue::VARR);
    UniValue boundsArr(UniValue::VARR);
    boundsArr.push_back("0");
    boundsArr.push_back("0.01");
    feestructArr.push_back(boundsArr);
    UniValue boundsArr1(UniValue::VARR);
    boundsArr1.push_back("10");
    boundsArr1.push_back("0.004");
    feestructArr.push_back(boundsArr1);
    UniValue boundsArr2(UniValue::VARR);
    boundsArr2.push_back("250");
    boundsArr2.push_back("0.002");
    feestructArr.push_back(boundsArr2);
    UniValue boundsArr3(UniValue::VARR);
    boundsArr3.push_back("2500");
    boundsArr3.push_back("0.0007");
    feestructArr.push_back(boundsArr3);
    auxfeesObj.pushKV("fee_struct", feestructArr);
    pubDataObj.pushKV("aux_fees", auxfeesObj);
    CWitnessAddress address;
    const std::string& pubDataStr = pubDataObj.write();
 
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 10*COIN,8, address), 10000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 3000*COIN,8, address), 591000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 2500*COIN,8, address), 556000000);
    BOOST_CHECK_EQUAL(getAuxFee(pubDataStr, 7500*COIN,8, address), 906000000);
}
BOOST_AUTO_TEST_CASE(generate_asset_auxfees)
{
	tfm::format(std::cout,"Running generate_asset_auxfees...\n");
	string newaddress = GetNewFundedAddress("node1");
    string newaddressfee = GetNewFundedAddress("node1");
    string newaddressrecv = GetNewFundedAddress("node1");
    UniValue auxfeesObj(UniValue::VOBJ);
    auxfeesObj.pushKV("address", newaddressfee);
    UniValue feestructArr(UniValue::VARR);
    UniValue boundsArr(UniValue::VARR);
    boundsArr.push_back("0");
    boundsArr.push_back("0.01");
    feestructArr.push_back(boundsArr);
    UniValue boundsArr1(UniValue::VARR);
    boundsArr1.push_back("10");
    boundsArr1.push_back("0.004");
    feestructArr.push_back(boundsArr1);
    UniValue boundsArr2(UniValue::VARR);
    boundsArr2.push_back("250");
    boundsArr2.push_back("0.002");
    feestructArr.push_back(boundsArr2);
    UniValue boundsArr3(UniValue::VARR);
    boundsArr3.push_back("2500");
    boundsArr3.push_back("0.0007");
    feestructArr.push_back(boundsArr3);
    UniValue boundsArr4(UniValue::VARR);
    boundsArr4.push_back("25000");
    boundsArr4.push_back("0.00007");
    feestructArr.push_back(boundsArr4);
    UniValue boundsArr5(UniValue::VARR);
    boundsArr5.push_back("2500000");
    boundsArr5.push_back("0");
    feestructArr.push_back(boundsArr5);
    auxfeesObj.pushKV("fee_struct", feestructArr);
    string auxfees = auxfeesObj.write();
	string guid = AssetNew("node1", newaddress, "AGX silver backed token, licensed and operated by Interfix corporation", "''", "8", "1000", "1000", "31", "''", "AGX", auxfees);
    AssetSend("node1", guid, "[{\"address\":\"" + newaddress + "\",\"amount\":1000}]");
    string txid = AssetAllocationTransfer(false, "node1", guid, newaddress, "[{\"address\":\"" + newaddressrecv + "\",\"amount\":250}]");
	UniValue r;
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddressrecv + "\"" ));
	UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 250 * COIN);
    // ensure auxfee is attached
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getrawtransaction" , "\"" + txid + "\",true"));

	UniValue systxObj = find_value(r.get_obj(), "systx").get_obj();
    UniValue allocationsArr = find_value(systxObj, "allocations").get_array();
    bool foundAuxFee = false;
	for (unsigned int i = 0; i<allocationsArr.size(); i++) {
		string allocationRecvAddress = find_value(allocationsArr[i].get_obj(), "address").get_str();
		CAmount allocationRecvAmount = AmountFromValue(find_value(allocationsArr[i].get_obj(), "amount"));
		if(allocationRecvAddress == newaddressfee && allocationRecvAmount == 106000000){
            foundAuxFee = true;
            break;
        }
	}
    BOOST_CHECK(foundAuxFee);
    // remove auxfee
    AssetUpdate("node1", guid);
    // ensure auxfee is not attached
    txid = AssetAllocationTransfer(false, "node1", guid, newaddress, "[{\"address\":\"" + newaddressrecv + "\",\"amount\":250}]");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddressrecv + "\"" ));
	balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 500 * COIN);

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getrawtransaction" , "\"" + txid + "\",true"));
	systxObj = find_value(r.get_obj(), "systx").get_obj();
    allocationsArr = find_value(systxObj, "allocations").get_array();
    foundAuxFee = false;
	for (unsigned int i = 0; i<allocationsArr.size(); i++) {
		string allocationRecvAddress = find_value(allocationsArr[i].get_obj(), "address").get_str();
		CAmount allocationRecvAmount = AmountFromValue(find_value(allocationsArr[i].get_obj(), "amount"));
		if(allocationRecvAddress == newaddressfee){
            foundAuxFee = true;
            break;
        }
	}
    BOOST_CHECK(!foundAuxFee);
}

BOOST_AUTO_TEST_CASE(generate_asset_audittxroot)
{
    tfm::format(std::cout,"Running generate_asset_audittxroot...\n");
    UniValue r;
    BOOST_CHECK_NO_THROW(CallExtRPC("node1", "syscoinsetethheaders", "[[709780,\"709780\",\"709779\",\"a\",\"a\",0],[707780,\"707780\",\"707779\",\"a\",\"a\",0],[707772,\"707772\",\"707771\",\"a\",\"a\",0],[707776,\"707776\",\"707775\",\"a\",\"a\",0],[707770,\"707770\",\"707769\",\"a\",\"a\",0],[707778,\"707778\",\"707777\",\"a\",\"a\",0],[707774,\"707774\",\"707773\",\"a\",\"a\",0]]"));
    int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "syscoinsetethstatus", "\"synced\",709780"));
    int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tfm::format(std::cout,"syscoinsetethstatus elasped time %lld\n", end-start);
    UniValue blocksArray = find_value(r.get_obj(), "missing_blocks").get_array();
    // the - MAX_ETHEREUM_TX_ROOTS check to ensure you have at least that many roots stored from the tip
    BOOST_CHECK_EQUAL(find_value(blocksArray[0].get_obj(), "from").get_uint() , 659780);
    BOOST_CHECK(find_value(blocksArray[0].get_obj(), "to").get_uint() == 707769);

    BOOST_CHECK(find_value(blocksArray[1].get_obj(), "from").get_uint() == 707771);
    BOOST_CHECK(find_value(blocksArray[1].get_obj(), "to").get_uint() == 707771);
    BOOST_CHECK(find_value(blocksArray[2].get_obj(), "from").get_uint() == 707773);
    BOOST_CHECK(find_value(blocksArray[2].get_obj(), "to").get_uint() == 707773);
    BOOST_CHECK(find_value(blocksArray[3].get_obj(), "from").get_uint() == 707775);
    BOOST_CHECK(find_value(blocksArray[3].get_obj(), "to").get_uint() == 707775);
    BOOST_CHECK(find_value(blocksArray[4].get_obj(), "from").get_uint() == 707777);
    BOOST_CHECK(find_value(blocksArray[4].get_obj(), "to").get_uint() == 707777);
    BOOST_CHECK(find_value(blocksArray[5].get_obj(), "from").get_uint() == 707779);
    BOOST_CHECK(find_value(blocksArray[5].get_obj(), "to").get_uint() == 707779);
    BOOST_CHECK(find_value(blocksArray[6].get_obj(), "from").get_uint() == 707781);
    BOOST_CHECK(find_value(blocksArray[6].get_obj(), "to").get_uint() == 709779);
    BOOST_CHECK_NO_THROW(CallExtRPC("node1", "syscoinsetethheaders", "[[707773,\"707773\",\"707772\",\"a\",\"a\",0],[707775,\"707775\",\"707774\",\"a\",\"a\",0],[707771,\"707771\",\"707770\",\"a\",\"a\",0],[707777,\"707777\",\"707776\",\"a\",\"a\",0],[707779,\"707779\",\"707778\",\"a\",\"a\",0],[707781,\"707781\",\"707780\",\"a\",\"a\",0]]"));
    start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "syscoinsetethstatus", "\"synced\",709780"));
    end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tfm::format(std::cout,"syscoinsetethstatus1 elasped time %lld\n", end-start);
    blocksArray = find_value(r.get_obj(), "missing_blocks").get_array();
    BOOST_CHECK(find_value(blocksArray[0].get_obj(), "from").get_uint() == 659780);
    BOOST_CHECK(find_value(blocksArray[0].get_obj(), "to").get_uint() == 707769);

    BOOST_CHECK(find_value(blocksArray[1].get_obj(), "from").get_uint() == 707782);
    BOOST_CHECK(find_value(blocksArray[1].get_obj(), "to").get_uint() == 709779);

    // now fork and check it revalidates chain
    // 707771 (should be 707772) -> 707773 and 707773 (should be 707774) -> 707775
    BOOST_CHECK_NO_THROW(CallExtRPC("node1", "syscoinsetethheaders", "[[707773,\"707773\",\"707771\",\"a\",\"a\",0],[707775,\"707775\",\"707773\",\"a\",\"a\",0]]"));
    start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "syscoinsetethstatus", "\"synced\",709780"));
    end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tfm::format(std::cout,"syscoinsetethstatus2 elasped time %lld\n", end-start);
    blocksArray = find_value(r.get_obj(), "missing_blocks").get_array();

    BOOST_CHECK_EQUAL(blocksArray.size(), 4);
    // we should still have the missing ranges prior to the forks
    BOOST_CHECK_EQUAL(find_value(blocksArray[0].get_obj(), "from").get_uint(),659780);
    BOOST_CHECK_EQUAL(find_value(blocksArray[0].get_obj(), "to").get_uint() ,707769);
    
    // 707773 is affected, -50 is 707723 and +50 is 707823
    BOOST_CHECK_EQUAL(find_value(blocksArray[1].get_obj(), "from").get_uint() , 707723);
    BOOST_CHECK_EQUAL(find_value(blocksArray[1].get_obj(), "to").get_uint() , 707823);
    
    BOOST_CHECK_EQUAL(find_value(blocksArray[2].get_obj(), "from").get_uint() , 707725);
    BOOST_CHECK_EQUAL(find_value(blocksArray[2].get_obj(), "to").get_uint() , 707825);
    
    // last missing range stays in the missing range list
    BOOST_CHECK_EQUAL(find_value(blocksArray[3].get_obj(), "from").get_uint() , 707782);
    BOOST_CHECK_EQUAL(find_value(blocksArray[3].get_obj(), "to").get_uint() , 709779);
}
BOOST_AUTO_TEST_CASE(generate_asset_audittxroot1)
{
    tfm::format(std::cout,"Running generate_asset_audittxroot1...\n");
    UniValue r;
    std::string roots = "";
    unsigned int nStartHeight = 700000;
    unsigned int nMissingRange1 = 700059;
    unsigned int nMissingRange2 = 800022;
    unsigned int nMissingRange3 = 814011;
    unsigned int nCount = 0;
    for(unsigned int i = nStartHeight;i<(nStartHeight+120000);i++){
        if(i == nMissingRange1 || i == nMissingRange2 || i == nMissingRange3)
            continue;
        if(nCount > 0)
            roots += ",";
        roots += strprintf("[%d,\"%d\",\"%d\",\"a\",\"a\",0]", i, i, i-1);
        if(nCount > 0 && (nCount % 400) == 0){
            BOOST_CHECK_NO_THROW(CallExtRPC("node1", "syscoinsetethheaders", "[" + roots + "]"));
            roots = "";
            nCount = 0;
            continue;
        }
        nCount++;
    }
    if(!roots.empty())
        BOOST_CHECK_NO_THROW(CallExtRPC("node1", "syscoinsetethheaders","[" + roots + "]"));
    int64_t start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "syscoinsetethstatus", "\"synced\",820000"));
    int64_t end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tfm::format(std::cout,"syscoinsetethstatus elasped time %lld\n", end-start);
    UniValue blocksArray = find_value(r.get_obj(), "missing_blocks").get_array();
    BOOST_CHECK(find_value(blocksArray[0].get_obj(), "from").get_uint() == 700059);
    BOOST_CHECK(find_value(blocksArray[0].get_obj(), "to").get_uint() == 700059);
    BOOST_CHECK(find_value(blocksArray[1].get_obj(), "from").get_uint() == 800022);
    BOOST_CHECK(find_value(blocksArray[1].get_obj(), "to").get_uint() == 800022);
    BOOST_CHECK(find_value(blocksArray[2].get_obj(), "from").get_uint() == 814011);
    BOOST_CHECK(find_value(blocksArray[2].get_obj(), "to").get_uint() == 814011);
    BOOST_CHECK_NO_THROW(CallExtRPC("node1", "syscoinsetethheaders", "[[814011,\"814011\",\"814010\",\"a\",\"a\",0],[700059,\"700059\",\"700058\",\"a\",\"a\",0],[800022,\"800022\",\"800021\",\"a\",\"a\",0]]"));
    start = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "syscoinsetethstatus", "\"synced\",820000"));
    end = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    tfm::format(std::cout,"syscoinsetethstatus1 elasped time %lld\n", end-start);
    blocksArray = find_value(r.get_obj(), "missing_blocks").get_array();
    BOOST_CHECK(blocksArray.empty());
}
BOOST_AUTO_TEST_CASE(generate_asset_throughput)
{

    int64_t start = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    UniValue r;
    tfm::format(std::cout,"Running generate_asset_throughput...\n");
    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node3");
    vector<string> vecAssets;
    // setup senders and receiver node addresses
    vector<string> senders;
    vector<string> receivers;
    senders.push_back("node1");
    senders.push_back("node2");
    receivers.push_back("node3");
    BOOST_CHECK(receivers.size() == 1);
    // user modifiable variables

    // for every asset you add numberOfAssetSendsPerBlock tx's effectively
    int numAssets = 10;
    BOOST_CHECK(numAssets >= 1);

    int numberOfAssetSendsPerBlock = 250;
    BOOST_CHECK(numberOfAssetSendsPerBlock >= 1 && numberOfAssetSendsPerBlock <= 250);

      // NOT MEANT TO BE MODIFIED! CALCULATE INSTEAD!
    const int numberOfTransactionToSend = numAssets*numberOfAssetSendsPerBlock;

    // make sure numberOfAssetSendsPerBlock isn't a fraction of numberOfTransactionToSend
    BOOST_CHECK((numberOfTransactionToSend % numberOfAssetSendsPerBlock) == 0);

    vector<string> unfundedAccounts;
    vector<string> rawSignedAssetAllocationSends;
    vector<string> vecFundedAddresses;
    GenerateBlocks((numAssets+1)/ numberOfAssetSendsPerBlock);
    // PHASE 1:  GENERATE UNFUNDED ADDRESSES FOR RECIPIENTS TO ASSETALLOCATIONSEND
    tfm::format(std::cout,"Throughput test: Total transaction count: %d, Receivers Per Asset Allocation Transfer %d, Total Number of Assets needed %d\n\n", numberOfTransactionToSend, numberOfAssetSendsPerBlock, numAssets);
    tfm::format(std::cout,"creating %d unfunded addresses...\n", numberOfAssetSendsPerBlock);
    for(int i =0;i<numberOfAssetSendsPerBlock;i++){
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
        unfundedAccounts.emplace_back(r.get_str());
    }

   // PHASE 2:  GENERATE FUNDED ADDRESSES FOR CREATING AND SENDING ASSETS
    // create address for funding
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
    string fundedAccount = r.get_str();
    tfm::format(std::cout,"creating %d funded accounts for using with assetsend/assetallocationsend in subsequent steps...\n", numAssets*numberOfAssetSendsPerBlock);
    string sendManyString = "";
    for(int i =0;i<numAssets;i++){
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
        string fundedAccount = r.get_str();
        if(sendManyString != "")
            sendManyString += ",";
        sendManyString += "\"" + fundedAccount + "\":1";
        if(((i+1)% numberOfAssetSendsPerBlock)==0){
            tfm::format(std::cout,"Sending funds to batch of %d funded accounts, approx. %d batches remaining\n", numberOfAssetSendsPerBlock, (numAssets-i)/ numberOfAssetSendsPerBlock);
            std::string strSendMany = "sendmany \"\" {" + sendManyString + "}";
            CallExtRPC("node1", "sendmany", "\"\",{" + sendManyString + "}");
            sendManyString = "";

        }
        vecFundedAddresses.push_back(fundedAccount);
    }
    if(!sendManyString.empty()){
        std::string strSendMany = "sendmany \"\" {" + sendManyString + "}";
        CallExtRPC("node1", "sendmany", "\"\",{" + sendManyString + "}");
    }
    GenerateBlocks(5);

    // PHASE 3:  SET tpstestsetenabled ON ALL SENDER/RECEIVER NODES
    for (auto &sender : senders)
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "tpstestsetenabled", "true"));
    for (auto &receiver : receivers)
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "tpstestsetenabled", "true"));

     // PHASE 4:  CREATE ASSETS
    // create assets needed
    tfm::format(std::cout,"creating %d sender assets...\n", numAssets);
    for(int i =0;i<numAssets;i++){
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew" , "\"" +  vecFundedAddresses[i] + "\",\"tps\",\"''\",\"''\",8,250,250,31,{},\"''\""));
        string guid = itostr(find_value(r.get_obj(), "asset_guid").get_uint());
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet", "\"" + find_value(r.get_obj(), "hex").get_str() + "\""));
        string hex_str = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hex_str + "\""));
        vecAssets.push_back(guid);
    }
    GenerateBlocks(5);
    tfm::format(std::cout,"sending assets with assetsend...\n");
    // PHASE 5:  SEND ASSETS TO NEW ALLOCATIONS
    for(int i =0;i<numAssets;i++){
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "listassetindexassets" , "\"" +  vecFundedAddresses[i] + "\""));	
        UniValue indexArray = r.get_array();	
        BOOST_CHECK_EQUAL(indexArray.size(), 1);	
        uint32_t nAsset = find_value(indexArray[0].get_obj(), "asset_guid").get_uint();
        uint32_t nAssetStored;
        ParseUInt32(vecAssets[i], &nAssetStored);
        BOOST_CHECK_EQUAL(nAsset, nAssetStored);
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetsendmany" ,  vecAssets[i] + ",[{\"address\":\"" + vecFundedAddresses[i] + "\",\"amount\":250}],\"''\""));

        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet", "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
        string hex_str = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hex_str + "\""));       
    }

    GenerateBlocks(5);

    // PHASE 6:  SEND ALLOCATIONS TO NEW ALLOCATIONS (UNFUNDED ADDRESSES) USING ZDAG
    tfm::format(std::cout,"Creating assetallocationsend transactions...\n");
    int unfoundedAccountIndex = 0;
    // create vector of signed transactions
    string assetAllocationSendMany = "";
    for(int i =0;i<numAssets;i++){
        // send asset to numberOfAssetSendsPerBlock addresses
        string assetAllocationSendMany = "";
        // +1 to account for change output
        for (int j = 0; j < numberOfAssetSendsPerBlock; j++) {
            if(assetAllocationSendMany != "")
                assetAllocationSendMany += ",";
            assetAllocationSendMany += "{\"address\":\"" + unfundedAccounts[unfoundedAccountIndex++] + "\",\"amount\":1}";
            if(unfoundedAccountIndex >= unfundedAccounts.size())
                unfoundedAccountIndex = 0;
        }

        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , vecAssets[i] + ",\"" + vecFundedAddresses[i] + "\",[" + assetAllocationSendMany + "],\"''\""));
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" + find_value(r.get_obj(), "hex").get_str() + "\""));
        string hex_str = find_value(r.get_obj(), "hex").get_str();
        rawSignedAssetAllocationSends.push_back(hex_str);
    }

    BOOST_CHECK(assetAllocationSendMany.empty());

    // PHASE 7:  DISTRIBUTE LOAD AMONG SENDERS
    // push vector of signed transactions to tpstestadd on every sender node distributed evenly
    int txPerSender = 1;
    tfm::format(std::cout,"Dividing work (%d transactions) between %d senders (%d per sender)...\n", rawSignedAssetAllocationSends.size(), senders.size(), txPerSender);

    unsigned int j=0;
    unsigned int i=0;
    unsigned int senderIndex=0;
    while(j < rawSignedAssetAllocationSends.size()){
        string vecTX = "[";
        unsigned int currentTxIndex = i * txPerSender;
        unsigned int nextTxIndex = (i+1) * txPerSender;
        if((nextTxIndex+txPerSender) > rawSignedAssetAllocationSends.size())
            nextTxIndex += txPerSender;
        for(j=currentTxIndex;j< nextTxIndex;j++){
            if(j >= rawSignedAssetAllocationSends.size())
                break;
            if(vecTX != "[")
                vecTX += ",";
            vecTX += "{\"tx\":\"" + rawSignedAssetAllocationSends[j] + "\"}";
        }
        if(vecTX != "["){
            vecTX += "]";
            BOOST_CHECK_NO_THROW(CallExtRPC(senders[senderIndex++], "tpstestadd", "0," + vecTX));
        }
        if(senderIndex >= senders.size())
            senderIndex = 0;
        i++;
    }

    // PHASE 8:  CALL tpstestadd ON ALL SENDER/RECEIVER NODES WITH A FUTURE TIME
    // set the start time to 1 second from now (this needs to be profiled, if the tpstestadd setting time to every node exceeds say 500ms then this time should be extended to account for the latency).
    // rule of thumb if sender count is high (> 25) then profile how long it takes and multiple by 10 and get ceiling of next second needed to send this rpc to every node to have them sync up

    // this will set a start time to every node which will send the vector of signed txs to the network
    int64_t tpstarttime = GetTimeMicros();
    int microsInSecond =  1000 * 1000;
    tpstarttime = tpstarttime + microsInSecond;
    tfm::format(std::cout,"Adding assetsend transactions to queue on sender nodes...\n");
    // ensure mnsync isn't doing its thing before the test starts
    for (auto &sender : senders){
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "mnsync", "\"next\"", false));
    }
    for (auto &receiver : receivers){
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "mnsync", "\"next\"", false));
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "mnsync", "\"next\"", false));
    }
    for (auto &sender : senders){
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "tpstestadd",  i64tostr(tpstarttime)));
        BOOST_CHECK_NO_THROW(r = CallExtRPC(sender, "tpstestinfo"));
        BOOST_CHECK_EQUAL(find_value(r.get_obj(), "testinitiatetime").get_int64(), tpstarttime);
    }
    for (auto &receiver : receivers){
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "tpstestadd", i64tostr(tpstarttime)));
        BOOST_CHECK_EQUAL(find_value(r.get_obj(), "testinitiatetime").get_int64(), tpstarttime);
    }

    // PHASE 9:  WAIT 10 SECONDS + DELAY SET ABOVE (1 SECOND)
    tfm::format(std::cout,"Waiting 11 seconds as per protocol...\n");
    // start 11 second wait
    UninterruptibleSleep(std::chrono::milliseconds(11000));

    // PHASE 10:  CALL tpstestinfo ON SENDERS AND GET AVERAGE START TIME (TIME SENDERS PUSHED TRANSACTIONS TO THE SOCKETS)
    // get the elapsed time of each node on how long it took to push the vector of signed txs to the network
    int64_t avgteststarttime = 0;
    for (auto &sender : senders) {
        BOOST_CHECK_NO_THROW(r = CallExtRPC(sender, "tpstestinfo"));
        avgteststarttime += find_value(r.get_obj(), "testinitiatetime").get_int64();
    }
    avgteststarttime /= senders.size();

    // PHASE 11:  CALL tpstestinfo ON RECEIVERS AND GET AVERAGE RECEIVE TIME, CALCULATE AVERAGE
    // gather received transfers on the receiver, you can query any receiver node here, in general they all should see the same state after the elapsed time.
    BOOST_CHECK_NO_THROW(r = CallExtRPC(receivers[0], "tpstestinfo"));
    UniValue tpsresponse = r.get_obj();
    UniValue tpsresponsereceivers = find_value(tpsresponse, "receivers").get_array();

    float totalTime = 0;
    for (size_t i = 0; i < tpsresponsereceivers.size(); i++) {
        const UniValue &responseObj = tpsresponsereceivers[i].get_obj();
        totalTime += find_value(responseObj, "time").get_int64() - avgteststarttime;
    }
    // average the start time - received time by the number of responses received (usually number of responses should match number of transactions sent beginning of test)
    totalTime /= tpsresponsereceivers.size();


    // PHASE 12:  DISPLAY RESULTS

    tfm::format(std::cout,"tpstarttime %lld avgteststarttime %lld totaltime %.2f, num responses %zu\n", tpstarttime, avgteststarttime, totalTime, tpsresponsereceivers.size());
    for (auto &sender : senders)
        BOOST_CHECK_NO_THROW(CallExtRPC(sender, "tpstestsetenabled", "false"));
    for (auto &receiver : receivers)
        BOOST_CHECK_NO_THROW(CallExtRPC(receiver, "tpstestsetenabled", "false"));
    int64_t end = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    const int64_t &startblock = GetTimeMicros();
    tfm::format(std::cout,"creating %d blocks\n", (numAssets/(93*4)) + 2);
    GenerateBlocks((numAssets/(93*4)) + 2, receivers[0]);

    const int64_t &endblock = GetTimeMicros();
    tfm::format(std::cout,"elapsed time in block creation: %lld\n", endblock-startblock);
    tfm::format(std::cout,"elapsed time in seconds: %lld\n", end-start);
    tfm::format(std::cout,"checking indexes...\n");	
    unfoundedAccountIndex = 0;	
    for(int i =0;i<numAssets;i++){	
        uint32_t nAssetStored;	
        ParseUInt32(vecAssets[i], &nAssetStored);	
        for (int j = 0; j < numberOfAssetSendsPerBlock; j++) {	
            BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "listassetindexallocations" , "\"" + unfundedAccounts[unfoundedAccountIndex++] + "\""));	
            UniValue indexArray = r.get_array();	
            BOOST_CHECK_EQUAL(indexArray.size(), numAssets);       	
            if(unfoundedAccountIndex >= unfundedAccounts.size())	
                unfoundedAccountIndex = 0;	
        }	

        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "listassetindexassets" , "\"" + vecFundedAddresses[i] + "\""));	
        UniValue indexArray = r.get_array();	
        BOOST_CHECK_EQUAL(indexArray.size(), 1);	
        uint32_t nAsset = find_value(indexArray[0].get_obj(), "asset_guid").get_uint();	
        BOOST_CHECK_EQUAL(nAsset, nAssetStored);	
    }

}
BOOST_AUTO_TEST_CASE(generate_assetallocationmint)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_assetallocationmint...\n");  
    // txid 0xae3df53837f66b9fe06359646d92394fd9f567b9e3749c7febea5971c6e14620 on rinkeby
    // remove a0 RLP encoding header because when geth gets the tx root from relayer and pushes to syscoin, it does not include the RLP header
	std::string spv_tx_root = "f18ad7a19f08efbedbcbf0eefea94bf2d523dbbf1fd3714908db02129d901cd3";
	std::string spv_tx_parent_nodes = "f9025bf851a0766aa7492d2ae288a54aca97778392bf5613d2915d90dde26464b48d5be8da3580808080808080a0093a0a6203d65f7e44887c52ace8f8a2192709a3a6bc0543d8a2b37f5cc646268080808080808080f8b180a065e039f5f3af69df62b366f4eb7a97ff4e1c7d86848ec55771cec5a8699959c2a012abac8fbc8a63519c678d7cc92d5efad164996d11a8a301071ede0bd2ec19d9a0a013abe590ec132a63e580e2ea0a18d1886480a889974798ab33028678c8cf72a0c6dbae6978c357521d177ddd94b3468d33707df73986369a1f4c037bacf9945ca012f8fdd16c30ad5d71514c0ec6e703888211ae468cffc5aabf4339e18a1deb198080808080808080808080f9015220b9014ef9014b8204d5843b9aca008307a12094443d9a14fb6ba2a45465bec3767186f404ccea2580b8e45f959b69000000000000000000000000000000000000000000000000000000004190ab0000000000000000000000000000000000000000000000000000000000752cbd74000000000000000000000000e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8000000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000000000150073c237a0171e48890a824abbda619de705c0c21f00000000000000000000002ba00f68add3f6a7f77623f6da12e8f70c14f71bd3fdcbb1b831b3e076d8d8e24e1da0624dbf368d261761e92980ef2e152edb64b62c859e35b05f95f8a06e9c543d34";
	std::string spv_tx_value = "f9014b8204d5843b9aca008307a12094443d9a14fb6ba2a45465bec3767186f404ccea2580b8e45f959b69000000000000000000000000000000000000000000000000000000004190ab0000000000000000000000000000000000000000000000000000000000752cbd74000000000000000000000000e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8000000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000000000150073c237a0171e48890a824abbda619de705c0c21f00000000000000000000002ba00f68add3f6a7f77623f6da12e8f70c14f71bd3fdcbb1b831b3e076d8d8e24e1da0624dbf368d261761e92980ef2e152edb64b62c859e35b05f95f8a06e9c543d34";
    std::string spv_tx_path = "03";
    
    // remove a0 RLP header from spv_receipt_root as well
    std::string spv_receipt_root = "a6c0bf9f924ef1cfc4d0b010b2312276807ab5bca40d28c6dd98b21d29de51f0";
    std::string spv_receipt_parent_nodes = "f903f0f851a0c6024de1de1393e3ce892ea58a0e013dc6b881c26f22f5c4d78de7ebddf0d54080808080808080a0640ecd1152922fee801514f1d52d67a4c932d4cfce07b5704cf3fe06b04e797f8080808080808080f8b180a090a37ae3593541d04e8f18a913f2a67c08b97d437a807497871c0ab5acc96292a0e4ca2f77e9b824594cf0f104a8289216a1f17756c0e17ae00e8b6f1763ec7d3fa09529ccfee51cba8bdc2a77fd39f5588a15d6c5796bffdbc6149962ce43c99308a01cd036c395d623cc975c1222a51856aaf80ece64d59ef3eacca3b73462cbb2d9a05d0de9048ae4214f2af0ee374e0a0024a64b2bdac08b3bd2f53dfee85e23ed068080808080808080808080f902e720b902e3f902e00183043a62b901000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000021000008000000000000000000000a000000000004000000000000200000000000000000000000000000000000000100000000000000000000000000000010040000000000000000000010000000000000000000000000000000000000001000000000020000000000000000000000000000000000000000008000000000000000000000000002000000000000020000000000000000000000000000000000000000000010000100000000800000000000000000000000000080000000000000000000f901d5f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a0ddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3efa0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a0000000000000000000000000000000000000000000000000000000004190ab00f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a08c5be1e5ebec7d5bd14f71427d1e84f3dd0314c0f7b2291e5b200ac8c7c3b925a0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a00000000000000000000000000000000000000000000000000000000000000000f89994443d9a14fb6ba2a45465bec3767186f404ccea25e1a0aabab1db49e504b5156edf3f99042aeecb9607a08f392589571cd49743aaba8db860000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053000000000000000000000000000000000000000000000000000000004190ab000000000000000000000000000000000000000000000000000000000000000003";
    std::string spv_receipt_value = "f902e00183043a62b901000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000021000008000000000000000000000a000000000004000000000000200000000000000000000000000000000000000100000000000000000000000000000010040000000000000000000010000000000000000000000000000000000000001000000000020000000000000000000000000000000000000000008000000000000000000000000002000000000000020000000000000000000000000000000000000000000010000100000000800000000000000000000000000080000000000000000000f901d5f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a0ddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3efa0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a0000000000000000000000000000000000000000000000000000000004190ab00f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a08c5be1e5ebec7d5bd14f71427d1e84f3dd0314c0f7b2291e5b200ac8c7c3b925a0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a00000000000000000000000000000000000000000000000000000000000000000f89994443d9a14fb6ba2a45465bec3767186f404ccea25e1a0aabab1db49e504b5156edf3f99042aeecb9607a08f392589571cd49743aaba8db860000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053000000000000000000000000000000000000000000000000000000004190ab000000000000000000000000000000000000000000000000000000000000000003";
    int height = 5596513;
    string newaddress = GetNewFundedAddress("node1");
    string amount = "11";
    string assetguid = AssetNew("node1", newaddress, "pubdata", "0xe3d9cCBaEDAbd8fD4401AAb7752F6f224A7EF1C8", "8", "1000", "10000");
    AssetSend("node1", assetguid, "[{\"address\":\"burn\",\"amount\":110}]");
    string txid1 = AssetAllocationMint("node1", assetguid, newaddress, amount, height, spv_tx_value, spv_tx_root, spv_tx_parent_nodes, spv_tx_path, spv_receipt_value, spv_receipt_root, spv_receipt_parent_nodes);
    // try to mint again
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationmint" , assetguid + ",\"" + newaddress + "\"," + amount + "," + itostr(height) + ",\"" + spv_tx_value + "\",\"a0" + spv_tx_root + "\",\"" + spv_tx_parent_nodes + "\",\"" + spv_tx_path + "\",\"" + spv_receipt_value + "\",\"a0" + spv_receipt_root + "\",\"" + spv_receipt_parent_nodes +  "\",\"''\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\""  + find_value(r.get_obj(), "hex").get_str() + "\""));
    string hex_str = find_value(r.get_obj(), "hex").get_str();
    // should fail: already minted with that eth txid
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hex_str + "\""), runtime_error); 

    // increase time by 1 week and assetallocationmint should throw
	SleepFor(604800 * 1000);
	BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationmint" , assetguid + ",\"" + newaddress + "\"," + amount + "," + itostr(height) + ",\"" + spv_tx_value + "\",\"a0" + spv_tx_root + "\",\"" + spv_tx_parent_nodes + "\",\"" + spv_tx_path + "\",\"" + spv_receipt_value + "\",\"a0" + spv_receipt_root + "\",\"" + spv_receipt_parent_nodes +  "\",\"''\""), runtime_error);
}
/*BOOST_AUTO_TEST_CASE(generate_assetallocationmint_linked)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_assetallocationmint_linked...\n");  
    // txid 0xae3df53837f66b9fe06359646d92394fd9f567b9e3749c7febea5971c6e14620 on rinkeby
    // remove a0 RLP encoding header because when geth gets the tx root from relayer and pushes to syscoin, it does not include the RLP header
	std::string spv_tx_root = "f18ad7a19f08efbedbcbf0eefea94bf2d523dbbf1fd3714908db02129d901cd3";
	std::string spv_tx_parent_nodes = "f9025bf851a0766aa7492d2ae288a54aca97778392bf5613d2915d90dde26464b48d5be8da3580808080808080a0093a0a6203d65f7e44887c52ace8f8a2192709a3a6bc0543d8a2b37f5cc646268080808080808080f8b180a065e039f5f3af69df62b366f4eb7a97ff4e1c7d86848ec55771cec5a8699959c2a012abac8fbc8a63519c678d7cc92d5efad164996d11a8a301071ede0bd2ec19d9a0a013abe590ec132a63e580e2ea0a18d1886480a889974798ab33028678c8cf72a0c6dbae6978c357521d177ddd94b3468d33707df73986369a1f4c037bacf9945ca012f8fdd16c30ad5d71514c0ec6e703888211ae468cffc5aabf4339e18a1deb198080808080808080808080f9015220b9014ef9014b8204d5843b9aca008307a12094443d9a14fb6ba2a45465bec3767186f404ccea2580b8e45f959b69000000000000000000000000000000000000000000000000000000004190ab0000000000000000000000000000000000000000000000000000000000752cbd74000000000000000000000000e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8000000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000000000150073c237a0171e48890a824abbda619de705c0c21f00000000000000000000002ba00f68add3f6a7f77623f6da12e8f70c14f71bd3fdcbb1b831b3e076d8d8e24e1da0624dbf368d261761e92980ef2e152edb64b62c859e35b05f95f8a06e9c543d34";
	std::string spv_tx_value = "f9014b8204d5843b9aca008307a12094443d9a14fb6ba2a45465bec3767186f404ccea2580b8e45f959b69000000000000000000000000000000000000000000000000000000004190ab0000000000000000000000000000000000000000000000000000000000752cbd74000000000000000000000000e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8000000000000000000000000000000000000000000000000000000000000000800000000000000000000000000000000000000000000000000000000000000a000000000000000000000000000000000000000000000000000000000000000150073c237a0171e48890a824abbda619de705c0c21f00000000000000000000002ba00f68add3f6a7f77623f6da12e8f70c14f71bd3fdcbb1b831b3e076d8d8e24e1da0624dbf368d261761e92980ef2e152edb64b62c859e35b05f95f8a06e9c543d34";
    std::string spv_tx_path = "03";
    
    // remove a0 RLP header from spv_receipt_root as well
    std::string spv_receipt_root = "a6c0bf9f924ef1cfc4d0b010b2312276807ab5bca40d28c6dd98b21d29de51f0";
    std::string spv_receipt_parent_nodes = "f903f0f851a0c6024de1de1393e3ce892ea58a0e013dc6b881c26f22f5c4d78de7ebddf0d54080808080808080a0640ecd1152922fee801514f1d52d67a4c932d4cfce07b5704cf3fe06b04e797f8080808080808080f8b180a090a37ae3593541d04e8f18a913f2a67c08b97d437a807497871c0ab5acc96292a0e4ca2f77e9b824594cf0f104a8289216a1f17756c0e17ae00e8b6f1763ec7d3fa09529ccfee51cba8bdc2a77fd39f5588a15d6c5796bffdbc6149962ce43c99308a01cd036c395d623cc975c1222a51856aaf80ece64d59ef3eacca3b73462cbb2d9a05d0de9048ae4214f2af0ee374e0a0024a64b2bdac08b3bd2f53dfee85e23ed068080808080808080808080f902e720b902e3f902e00183043a62b901000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000021000008000000000000000000000a000000000004000000000000200000000000000000000000000000000000000100000000000000000000000000000010040000000000000000000010000000000000000000000000000000000000001000000000020000000000000000000000000000000000000000008000000000000000000000000002000000000000020000000000000000000000000000000000000000000010000100000000800000000000000000000000000080000000000000000000f901d5f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a0ddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3efa0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a0000000000000000000000000000000000000000000000000000000004190ab00f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a08c5be1e5ebec7d5bd14f71427d1e84f3dd0314c0f7b2291e5b200ac8c7c3b925a0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a00000000000000000000000000000000000000000000000000000000000000000f89994443d9a14fb6ba2a45465bec3767186f404ccea25e1a0aabab1db49e504b5156edf3f99042aeecb9607a08f392589571cd49743aaba8db860000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053000000000000000000000000000000000000000000000000000000004190ab000000000000000000000000000000000000000000000000000000000000000003";
    std::string spv_receipt_value = "f902e00183043a62b901000002000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000021000008000000000000000000000a000000000004000000000000200000000000000000000000000000000000000100000000000000000000000000000010040000000000000000000010000000000000000000000000000000000000001000000000020000000000000000000000000000000000000000008000000000000000000000000002000000000000020000000000000000000000000000000000000000000010000100000000800000000000000000000000000080000000000000000000f901d5f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a0ddf252ad1be2c89b69c2b068fc378daa952ba7f163c4a11628f55a4df523b3efa0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a0000000000000000000000000000000000000000000000000000000004190ab00f89b94e3d9ccbaedabd8fd4401aab7752f6f224a7ef1c8f863a08c5be1e5ebec7d5bd14f71427d1e84f3dd0314c0f7b2291e5b200ac8c7c3b925a0000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053a0000000000000000000000000443d9a14fb6ba2a45465bec3767186f404ccea25a00000000000000000000000000000000000000000000000000000000000000000f89994443d9a14fb6ba2a45465bec3767186f404ccea25e1a0aabab1db49e504b5156edf3f99042aeecb9607a08f392589571cd49743aaba8db860000000000000000000000000b0ea8c9ee8aa87efd28a12de8c034f947c144053000000000000000000000000000000000000000000000000000000004190ab000000000000000000000000000000000000000000000000000000000000000003";
    int height = 5596513;
    string newaddress = GetNewFundedAddress("node1");
    string amount = "11";
    string assetguid = AssetNew("node1", newaddress, "pubdata", "0xe3d9cCBaEDAbd8fD4401AAb7752F6f224A7EF1C8", "8", "1000", "10000");
    AssetSend("node1", assetguid, "[{\"address\":\"burn\",\"amount\":110}]");
    string txid1 = AssetAllocationMint("node1", assetguid, newaddress, amount, height, spv_tx_value, spv_tx_root, spv_tx_parent_nodes, spv_tx_path, spv_receipt_value, spv_receipt_root, spv_receipt_parent_nodes, "''", false);
    
    string txid3 = BurnAssetAllocation("node1", assetguid, useraddress, amount, false, "''");
}*/
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");
    string useraddress = GetNewFundedAddress("node1");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0x931d387731bbbc988b312206c74f77d004d6b84b");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress + "\",\"amount\":0.5}]");
    // try to burn more than we own
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress + "\",0.6,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet", "\"" + find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
    // this one is ok
    BurnAssetAllocation("node1", assetguid, useraddress, "0.5");
    
    // because allocation is empty it should have been erased
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo", assetguid + ",\"" + useraddress + "\""), runtime_error);
    // make sure you can't move coins from burn recipient
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"burn\",[{\"address\":\"" + useraddress + "\",\"amount\":0.5}],\"''\""), runtime_error);


}
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_multiple)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_multiple...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");
    string useraddress = GetNewFundedAddress("node1");
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress + "\",\"1\"", false);
    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress + "\",\"amount\":1.0}]");

    // 2 options for burns, all good, 1 good 1 bad
    // all good, burn 0.4 + 0.5 + 0.05

    BurnAssetAllocation("node1", assetguid, useraddress, "0.4", false);
    BurnAssetAllocation("node1", assetguid, useraddress, "0.5", false);
    BurnAssetAllocation("node1", assetguid, useraddress, "0.05", false);
    GenerateBlocks(5, "node1");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo", assetguid + ",\"" + useraddress + "\"" ));
    UniValue balance2 = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance2.getValStr(), "0.05000000");


    assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress + "\",\"amount\":1.0}]");
    // 1 bad 1 good, burn 0.6+0.6 only 1 should go through

    BurnAssetAllocation("node1", assetguid, useraddress, "0.6", false);
    // try burn more than we own
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress + "\",0.6,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" + find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    // this is a double spend attempt between the two burns so it will relay it and first one that gets to miner wins
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""));

    // this will stop the chain if both burns were allowed in the chain, the miner must throw away one of the burns to avoid his block from being flagged as invalid
    GenerateBlocks(5, "node1");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress + "\"" ));
    balance2 = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance2.getValStr(), "0.40000000");


}
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag_within_block)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag_within_block...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress2 = GetNewFundedAddress("node2");
    string useraddress1 = GetNewFundedAddress("node1");


    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa", "8", "10", "100");
    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress2 + "\",\"amount\":2.0}]");
    // From W2 send 1 SPT to W1 - Wait for Confirm
    AssetAllocationTransfer(false, "node2", assetguid, useraddress2, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");
    // From W2 send 1 SPT to W1 - DO NOT WAIT FOR CONFIRM
    AssetAllocationTransfer(true, "node2", assetguid, useraddress2, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");
    UninterruptibleSleep(std::chrono::milliseconds(500));
    // From W1 send 1 SPT to W2 within same Block
    AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":1.0}]");
    GenerateBlocks(1);

    // useraddress1 should have 1
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    // useraddress2 should have 2
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    // useraddress1 should have 1
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    // useraddress2 should have 2
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");


    // useraddress1 should have 1
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    // useraddress2 should have 2
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000"); 
}

BOOST_AUTO_TEST_CASE(generate_syscoin_asset_zdag_dependency)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_syscoin_asset_zdag_dependency...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress2 = GetNewFundedAddress("node2");
    string useraddress3 = GetNewFundedAddress("node2");
    string useraddress1 = GetNewFundedAddress("node1");
    CallExtRPC("node1","sendtoaddress" , "\"" + useraddress2 + "\",1", false);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "dumpprivkey", "\"" + useraddress1 + "\""));
	string useraddress1privkey = r.get_str();
	useraddress1privkey.erase(std::remove(useraddress1privkey.begin(), useraddress1privkey.end(), '\n'), useraddress1privkey.end());
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "importprivkey", "\"" + useraddress1privkey + "\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "dumpprivkey", "\"" + useraddress2 + "\""));
	string useraddress2privkey = r.get_str();
	useraddress2privkey.erase(std::remove(useraddress2privkey.begin(), useraddress2privkey.end(), '\n'), useraddress2privkey.end());
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "importprivkey", "\"" + useraddress2privkey + "\""));
    GenerateBlocks(1, "node1");
    GenerateBlocks(1, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa", "8", "10", "100");
    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":2.0}]");
    // Send from address 2 to address 1, send away from address 1 and back to 1, and then send all from address 1 to 3, the idea is.. if its out of chronological order some txs won't get mined
    // but they all should be mined and miner code should preserve order even if priority of transactions change
    string txid1 = AssetAllocationTransfer(false, "node2", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":1.5}]");
    //printf("txid1 %s\n", txid1.c_str());
    string txid2 = AssetAllocationTransfer(true, "node2", assetguid, useraddress2, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.5}]");
    //printf("txid2 %s\n", txid2.c_str());
    string txid3 = AssetAllocationTransfer(true, "node2", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.5}]");
    //printf("txid3 %s\n", txid3.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(100));
    // we hope that this gets pushed ahead to the queue by increasing fees, this will validate the block but txid2/txid3 will be discarded erroneously
    // we want txid2/txid3 to get mined and txid4 to get mined after like choronological order of mempool acceptance
    string txid4 = AssetAllocationTransfer(true, "node1", assetguid, useraddress2, "[{\"address\":\"" + useraddress3 + "\",\"amount\":1.0}]");
    //printf("txid4 %s\n", txid4.c_str());
    GenerateBlocks(1);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.50000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.50000000");
}

BOOST_AUTO_TEST_CASE(generate_syscoin_asset_zdag_dependency1)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_syscoin_asset_zdag_dependency1...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");


    string b11a = GetNewFundedAddress("node1");
    string b11b = GetNewFundedAddress("node1");
    string b21 = GetNewFundedAddress("node2");
    string b41 = GetNewFundedAddress("node2");
    string b10b = GetNewFundedAddress("node2");
    string b10a = GetNewFundedAddress("node2");
    string b224 = GetNewFundedAddress("node2");
    CallExtRPC("node3","sendtoaddress" , "\"" + b11a + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b11b + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b21 + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b41 + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b10b + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b10a + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b224 + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b11a + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b11b + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b21 + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b41 + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b10b + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b10a + "\",1", false);
    CallExtRPC("node3","sendtoaddress" , "\"" + b224 + "\",1", false);
    string creatoraddress = GetNewFundedAddress("node1");

    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa", "8", "10", "100");
    AssetSend("node1", assetguid, "[{\"address\":\"" + b10a + "\",\"amount\":0.5},{\"address\":\"" + b41 + "\",\"amount\":0.5},{\"address\":\"" + b11a + "\",\"amount\":1.0},{\"address\":\"" + b11b + "\",\"amount\":0.6},{\"address\":\"" + b10b + "\",\"amount\":1.1}]");

    string b10a_to_b224 = AssetAllocationTransfer(true, "node2", assetguid, b10a, "[{\"address\":\"" + b224 + "\",\"amount\":0.5}]");
    //printf("b10a_to_b224 %s\n", b10a_to_b224.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(250));
    string b41_to_b10b = AssetAllocationTransfer(true, "node2", assetguid, b41, "[{\"address\":\"" + b10b + "\",\"amount\":0.5}]");
    //printf("b41_to_b10b %s\n", b41_to_b10b.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(250));
    string b11a_to_b10b = AssetAllocationTransfer(true, "node1", assetguid, b11a, "[{\"address\":\"" + b10b + "\",\"amount\":0.5}]");
    //printf("b11a_to_b10b %s\n", b11a_to_b10b.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(250));
    string b11a_to_b11b = AssetAllocationTransfer(true, "node1", assetguid, b11a, "[{\"address\":\"" + b11b + "\",\"amount\":0.5}]");
    //printf("b11a_to_b11b %s\n", b11a_to_b11b.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(250));
    string b10b_to_b224 = AssetAllocationTransfer(true, "node2", assetguid, b10b, "[{\"address\":\"" + b224 + "\",\"amount\":0.5}]");
    //printf("b10b_to_b224 %s\n", b10b_to_b224.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(250));
    string b10b_to_b21 = AssetAllocationTransfer(true, "node2", assetguid, b10b, "[{\"address\":\"" + b21 + "\",\"amount\":0.6}]");
    //printf("b10b_to_b21 %s\n", b10b_to_b21.c_str());
    UninterruptibleSleep(std::chrono::milliseconds(400));
    string b11b_to_b21 = AssetAllocationTransfer(true, "node1", assetguid, b11b, "[{\"address\":\"" + b21 + "\",\"amount\":0.6}]");
    //printf("b11b_to_b21 %s\n", b11b_to_b21.c_str());

    UninterruptibleSleep(std::chrono::milliseconds(250));
   

    GenerateBlocks(1);



    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b224 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b21 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.20000000");

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b10a + "\""), runtime_error);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b10b + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.00000000");

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b41 + "\""), runtime_error);

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b11a + "\""), runtime_error);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + b11b + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.50000000");
   
}
// a = 1, a->b(0.4), a->c(0.2), burn a(0.4) (a=0, b=0.4, c=0.2 and burn=0.4)
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    string useraddress1 = GetNewFundedAddress("node1");
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);


    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");

    AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.4}]");
    AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}]");
    BurnAssetAllocation("node1", assetguid, useraddress1, "0.4", false);

    GenerateBlocks(5, "node1");

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""), runtime_error);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.40000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"burn\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.40000000");
}
// a = 1, burn a(0.8) a->b (0.4), a->c(0.2) (a=0.4, b=0.4, c=0.2 and burn=0)
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag1)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag1...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    string useraddress1 = GetNewFundedAddress("node1");
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);


    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa", "8", "5");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0},{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}]");

    string burntxid = BurnAssetAllocation("node1", assetguid, useraddress1, "0.8", false);
    // try xfer more than we own, prev output link in mempool ensures we classify this as sender conflict and potential dbl spend
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.4}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "decoderawtransaction" , "\"" +  hexStr + "\""));
    string txid1 = find_value(r.get_obj(), "txid").get_str();
    // this will with CPFP try to build ontop of the above transaction which was relayed but is part of the conflict structure setToRemoveFromMempool
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    hexStr = find_value(r.get_obj(), "hex").get_str();
    // shouldn't allow to build ontop of an invalid sys tx
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

    UninterruptibleSleep(std::chrono::milliseconds(500));

    GenerateBlocks(2, "node1");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""), runtime_error);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"burn\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.80000000");


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag" , "\"" + burntxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    // txid1 should still be in mempool but will be conflicting
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag" , "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
}
// a = 1, b = 0.2, c = 0.1, a->b (0.2), b->a(0.2),  a->c(0.2), c->a(0.2), burn a(0.5), burn a(0.5), b->c(0.2), burn c(0.3) (a=0, b=0, c=0 and burn=1.3)
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag2)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag2...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");
    string useraddress1 = GetNewFundedAddress("node1");
    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress2 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress3 + "\",\"1\"", false);
    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa", "8", "3");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.5},{\"address\":\"" + useraddress2 + "\",\"amount\":0.4},{\"address\":\"" + useraddress3 + "\",\"amount\":0.5}]");
   
    AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.2}]");


    AssetAllocationTransfer(true, "node1", assetguid, useraddress2, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.2}]");
  
    AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}]");

    AssetAllocationTransfer(true, "node1", assetguid, useraddress3, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.2}]");
   
    BurnAssetAllocation("node1", assetguid, useraddress1, "0.5", false);
    BurnAssetAllocation("node1", assetguid, useraddress1, "0.5", false);

    // try to burn more than you own
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress1 + "\",0.2,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    // this is a double spend attempt between the two burns so it will relay it and first one that gets to miner wins
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""));
    
    
    AssetAllocationTransfer(true, "node1", assetguid, useraddress2, "[{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}]");
    BurnAssetAllocation("node1", assetguid, useraddress3, "0.3", false);

    GenerateBlocks(5, "node1");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.50000000");
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.50000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.40000000");
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.40000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo", assetguid + ",\"burn\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.30000000");
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "1.30000000");
}
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag3)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag3...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress1 = GetNewFundedAddress("node1");
    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + creatoraddress + "\",\"1\"", false);
    GenerateBlocks(5, "node1");
       
    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");
    
    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");
    AssetAllocationTransfer(false, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    
    // try to burn more than you own
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress2 + "\",0.4,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
    string txid1 = AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    string txid2 = AssetAllocationTransfer(true, "node1", assetguid, useraddress2, "[{\"address\":\"" + useraddress3 + "\",\"amount\":0.1}]");
    GenerateBlocks(5, "node1");
    
    // no zdag tx found after block
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.80000000"); 

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.10000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.10000000");
    
    AssetAllocationTransfer(true, "node1", assetguid, useraddress2, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.1}]");
    
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"burn\""), runtime_error);
   

    // now do more zdag and check status are ok this time
    txid1 = AssetAllocationTransfer(true, "node1", assetguid, useraddress3, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.1}]");
    txid2 = AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.05}]");

    // sender useraddress2 was involved in the double-spend so these senders should be OK
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
 
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.75000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.00000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.00000000");
    
    GenerateBlocks(5, "node1");
    
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.95000000"); 

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.05000000");

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""), runtime_error);
}
// try a dbl spend with zdag
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag4)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag4...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress1 = GetNewFundedAddress("node1");
    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + creatoraddress + "\",\"1\"", false);
    GenerateBlocks(5, "node1");
       
    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");
    
    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");
    AssetAllocationTransfer(false, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    // burn and transfer at same time for dbl spend attempt
    // burn
 
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress1 + "\",0.8,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string burnHex = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "decoderawtransaction" , "\"" + burnHex + "\""));
    string burntxid = find_value(r.get_obj(), "txid").get_str();
    // asset xfer
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.8}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string assetHex = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "decoderawtransaction" , "\"" + assetHex + "\""));
    string assettxid = find_value(r.get_obj(), "txid").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2","sendrawtransaction" , "\"" + burnHex + "\""));
    // ensure duplicate input is detected
    UninterruptibleSleep(std::chrono::milliseconds(500));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "sendrawtransaction" , "\"" +  assetHex + "\""));
    UninterruptibleSleep(std::chrono::milliseconds(500));
    string txid1 = AssetAllocationTransfer(true, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    string txid2 = AssetAllocationTransfer(true, "node1", assetguid, useraddress2, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.05}]");
    UninterruptibleSleep(std::chrono::milliseconds(500));
    // txid1 depends on double spend so it is conflicted
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    // because we only can send pow balance, any other sender is not affected by major conflict
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
    
   
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
    // the burn txid is not zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + burntxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    // likewise with the asset txid
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + assettxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + burntxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    // likewise with the asset txid
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + assettxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + burntxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    // likewise with the asset txid
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + assettxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    GenerateBlocks(5);
    // asset xfer on node3 ensure it doesn't link against previous tx
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.01}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    assetHex = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "decoderawtransaction" , "\"" + assetHex + "\""));
    string assettxid2 = find_value(r.get_obj(), "txid").get_str();
    // on node2 it would have had a conflicting output for this sender since it double spent so we can't send
    BOOST_CHECK_THROW(r = CallExtRPC("node2", "sendrawtransaction" , "\"" +  assetHex + "\""), runtime_error);
    UninterruptibleSleep(std::chrono::milliseconds(100));
    // can't find because it wasn't sent
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + assettxid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + assettxid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + assettxid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
  
    tfm::format(std::cout,"Setting time ahead 300 seconds...\n");
    SleepFor(300 * 1000, 0);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.05000000");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""));
    balance = find_value(r.get_obj(), "balance");
    // depending on which one arrived first, the above 0.05 balance is good because that means that at least 1 zdag transaction went through
    BOOST_CHECK(balance.getValStr() == "0.95000000" || balance.getValStr() == "0.15000000");
    // if the send went through then burn should be invalid
    if(balance.getValStr() == "0.95000000"){
        BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"burn\""), runtime_error);
    }
    // if the send didn't go through the burn should be valid
    else if (balance.getValStr() == "0.15000000") {
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"burn\""));
        balance = find_value(r.get_obj(), "balance");
        BOOST_CHECK_EQUAL(balance.getValStr(), "0.80000000");
    }
    // no zdag tx found after block
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    string creatoraddress2 = GetNewFundedAddress("node2");
    // give time for mempool to get tx and then clear conflict via expire
    AssetNew("node2", creatoraddress2, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");
    // asset xfer on node3 ensure it doesn't link against previous tx but this time should be ok
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.01}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    assetHex = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "decoderawtransaction" , "\"" + assetHex + "\""));
    string assettxid3 = find_value(r.get_obj(), "txid").get_str();
    // send on node2 where it would have had a conflicting output for this sender since it double spent
    // but now should be cleared due to the 300 second expiry and assetnew previously would have triggered expiry code
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "sendrawtransaction" , "\"" +  assetHex + "\""));
    UninterruptibleSleep(std::chrono::milliseconds(500));
    // status should be ok since timeout should clear bad status
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + assettxid3 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
    
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + assettxid3 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + assettxid3 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);
}
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag_dbl_spend_same_input)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag_dbl_spend_same_input...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress1 = GetNewFundedAddress("node1");
    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + creatoraddress + "\",\"1\"", false);
    GenerateBlocks(5, "node1");
       
    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");
    
    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");
    AssetAllocationTransfer(false, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    // burn and transfer at same time for dbl spend attempt
    // burn
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress1 + "\",0.8,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string burnHex = find_value(r.get_obj(), "hex").get_str();

    // asset xfer
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.8}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string assetHex = find_value(r.get_obj(), "hex").get_str();

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "sendrawtransaction" , "\"" + assetHex + "\""));
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "decoderawtransaction" , "\"" + assetHex + "\"" + ",true"));
	string txid1 = find_value(r.get_obj(), "txid").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2","sendrawtransaction" , "\"" + burnHex + "\""));

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "decoderawtransaction" , "\"" + burnHex + "\"" + ",true"));
	string txid2 = find_value(r.get_obj(), "txid").get_str();
    
    UninterruptibleSleep(std::chrono::milliseconds(500));
    // check just sender, burn marks as major issue on zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    
    // not zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    // check just sender, burn marks as major issue on zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    GenerateBlocks(5);
} 
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag_dbl_spend_same_input_no_overflow)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag_dbl_spend_same_input_no_overflow...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress1 = GetNewFundedAddress("node1");
    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node1", "sendtoaddress" , "\"" + creatoraddress + "\",\"1\"", false);
    GenerateBlocks(5, "node1");
       
    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");
    
    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0}]");
    AssetAllocationTransfer(false, "node1", assetguid, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    // burn and transfer at same time for use same input but don't overflow balance, it should flag the sender in conflict even though balance doesn't overflow, 
    // there is no guarantee the tx will get mined in order so raise it up as issue
    // burn only 0.4
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress1 + "\",0.4,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string burnHex = find_value(r.get_obj(), "hex").get_str();

    // asset xfer only 0.4 (together its 0.8 (< 0.9 available) but still using same input it should flag it as a conflict)
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.4}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string assetHex = find_value(r.get_obj(), "hex").get_str();

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "sendrawtransaction" , "\"" + assetHex + "\""));
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "decoderawtransaction" , "\"" + assetHex + "\"" + ",true"));
	string txid1 = find_value(r.get_obj(), "txid").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2","sendrawtransaction" , "\"" + burnHex + "\""));

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "decoderawtransaction" , "\"" + burnHex + "\"" + ",true"));
	string txid2 = find_value(r.get_obj(), "txid").get_str();
    
    UninterruptibleSleep(std::chrono::milliseconds(500));
    // check just sender, burn marks as major issue on zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    
    // not zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + txid2 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    // check just sender, burn marks as major issue on zdag
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    GenerateBlocks(5);
} 
// same as last but try to build a sys tx ontop of the bad sys tx
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag_dbl_spend_same_input_no_overflow1)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag_dbl_spend_same_input_no_overflow1...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    string useraddress1 = GetNewFundedAddress("node1");
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + useraddress1 + "\",\"1\"", false);


    GenerateBlocks(5, "node1");
    GenerateBlocks(5, "node2");

    string assetguid = AssetNew("node1", creatoraddress, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa", "8", "5");

    AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":1.0},{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}]");


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationburn" , assetguid + ",\"" + useraddress1 + "\",0.6,\"0x931d387731bbbc988b312206c74f77d004d6b84b\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string burnHex = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "decoderawtransaction" , "\"" + burnHex + "\""));
    string burntxid = find_value(r.get_obj(), "txid").get_str();
    // send with dup input and no balance overflow
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":0.2}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string assetHex = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "decoderawtransaction" , "\"" + assetHex + "\""));
    string txid1 = find_value(r.get_obj(), "txid").get_str();
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2","sendrawtransaction" , "\"" + burnHex + "\""));
    UninterruptibleSleep(std::chrono::milliseconds(500));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "sendrawtransaction" , "\"" +  assetHex + "\""));
    UninterruptibleSleep(std::chrono::milliseconds(500));

    // now we will with try to build ontop of the burn transaction which was relayed but is part of the conflict structure setToRemoveFromMempool
    
    // should not allow to build on top on node3 because node2 had assetHex first and burnHex second, thus it will complain because of setToRemoveFromMempool
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    // shouldn't allow to build ontop of an invalid sys tx
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

    
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , assetguid + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress3 + "\",\"amount\":0.2}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    hexStr = find_value(r.get_obj(), "hex").get_str();
    // should allow to build ontop of burn tx because node1 has assetHex as a conflict, and burn is OK
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""));


    GenerateBlocks(2, "node1");
    // even though there is no balance overflow, it should deduct the balances for transactions above
    // even though only one will get mined
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress1 + "\""));
    UniValue balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.20000000");

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress2 + "\""), runtime_error);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"" + useraddress3 + "\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.40000000");

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  assetguid + ",\"burn\""));
    balance = find_value(r.get_obj(), "balance_zdag");
    BOOST_CHECK_EQUAL(balance.getValStr(), "0.60000000");


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag" , "\"" + burntxid + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    // txid1 should still be in mempool but will be conflicting
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag" , "\"" + txid1 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
}
BOOST_AUTO_TEST_CASE(generate_burn_syscoin_asset_zdag_dbl_spend_long_chain)
{
    UniValue r;
    tfm::format(std::cout,"Running generate_burn_syscoin_asset_zdag_dbl_spend_long_chain...\n");
    GenerateBlocks(5);
    GenerateBlocks(5, "node2");
    GenerateBlocks(5, "node3");

    string creatoraddress = GetNewFundedAddress("node1");

    string useraddress1 = GetNewFundedAddress("node1");
    string useraddress2 = GetNewFundedAddress("node1");
    string useraddress3 = GetNewFundedAddress("node1");
    GenerateBlocks(5, "node1");
    string assetguid = AssetNew("node1", useraddress3, "pubdata", "0x931d387731bbbc988b312206c74f77d004d6b84b");

    SyscoinBurn("node1", useraddress1, strSYSXAsset, "1");
    SyscoinBurn("node1", useraddress2, strSYSXAsset, "1");
    string tx1 = AssetAllocationTransfer(true, "node1", strSYSXAsset, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]");
    string tx2 = AssetAllocationTransfer(true, "node1", strSYSXAsset, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.01}]");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx1, tx2));
    string tx3 = AssetAllocationTransfer(true, "node1", strSYSXAsset, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.001}]");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx2, tx3));
    string tx4 = AssetAllocationTransfer(true, "node1", strSYSXAsset, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.0001}]");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx3, tx4));
    string tx5 = AssetAllocationTransfer(true, "node1", strSYSXAsset, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.00001}]");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx4, tx5));
    string tx6 = AssetAllocationTransfer(true, "node1", strSYSXAsset, useraddress1, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.000001}]");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx5, tx6));
    string tx7 = BurnAssetAllocation("node1", strSYSXAsset, useraddress1, "0.01", false, "''");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx6, tx7));
    string tx8 = BurnAssetAllocation("node1", strSYSXAsset, useraddress2, "0.01", false, "''");
    BOOST_CHECK(!AreTwoTransactionsLinked("node1", tx6, tx8));
    string tx9 = BurnAssetAllocation("node1", strSYSXAsset, useraddress1, "0.001", false, "''");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx7, tx9));
    string tx10 = BurnAssetAllocation("node1", strSYSXAsset, useraddress2, "0.001", false, "''");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx8, tx10));
    string tx11 = BurnAssetAllocation("node1", strSYSXAsset, useraddress2, "0.0001", false, "''");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx10, tx11));
    string tx12 = BurnAssetAllocation("node1", strSYSXAsset, useraddress1, "0.0001", false, "''");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx9, tx12));
    string tx13 = SyscoinBurn("node1", useraddress1, strSYSXAsset, "1", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx12, tx13));
    string tx14 = AssetUpdate("node1", strSYSXAsset , "''", "''", "''", "''", "''", false);
    string tx15 = SyscoinBurn("node1", useraddress2, strSYSXAsset, "1", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx11, tx15)); // last one in sequence for useraddress2
    string tx16 = AssetUpdate("node1", strSYSXAsset, "''", "''", "''", "''", "''", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx14, tx16));
    string tx17 = SyscoinBurn("node1", useraddress1, strSYSXAsset, "1", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx13, tx17)); // last one in sequence for useraddress1
    string tx18 = AssetUpdate("node1", strSYSXAsset, "''", "''", "''", "''", "''", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx16, tx18));
    string tx19 = AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]", "''", true, true, false);
    string tx20 = AssetUpdate("node1", strSYSXAsset, "''", "''", "''", "''", "''", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx18, tx20));
    string tx21 = AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress1 + "\",\"amount\":0.1}]", "''", true, true, false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx19, tx21));
    string tx22 = AssetUpdate("node1", strSYSXAsset, "''", "''", "''", "''", "''", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx20, tx22));
    string tx23 = AssetSend("node1", assetguid, "[{\"address\":\"" + useraddress2 + "\",\"amount\":0.1}]", "''", true, true, false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx21, tx23));
    string tx24 = AssetUpdate("node1", strSYSXAsset, "''", "''", "''", "''", "''", false);
    BOOST_CHECK(AreTwoTransactionsLinked("node1", tx22, tx24));
    UninterruptibleSleep(std::chrono::milliseconds(500));
    
    // not zdag transactions
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + tx24 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
  
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + tx17 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + tx17 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    // dbl spend attempt
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationsendmany" , strSYSXAsset + ",\"" + useraddress1 + "\",[{\"address\":\"" + useraddress2 + "\",\"amount\":50.8}],\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string assetHex = find_value(r.get_obj(), "hex").get_str();

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" +  assetHex + "\""));
    
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "decoderawtransaction" , "\"" + assetHex + "\",true"));
    string dblspendtx = find_value(r.get_obj(), "txid").get_str();
    UninterruptibleSleep(std::chrono::milliseconds(500));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + tx17 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);


    // original transactions are now marked as conflicted because the actor's are flagged
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + dblspendtx + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + dblspendtx + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);  
  
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + dblspendtx + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + tx5 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);  


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + dblspendtx + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + tx6 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_MAJOR_CONFLICT);
    tfm::format(std::cout, "Setting time ahead 300 seconds...\n");
    SleepFor(300 * 1000, 0);
    UninterruptibleSleep(std::chrono::milliseconds(250));
    // allow mempool expiry to work
    AssetUpdate("node1", strSYSXAsset, "''", "''", "''", "''", "''", false);
    UninterruptibleSleep(std::chrono::milliseconds(250));
    // after confirm we shouldn't have any txs to check
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + dblspendtx + "\""));
    // could be not found or status ok , status ok if node1 wallet accepting to mempool again because it detected unconfirmed transactions
    BOOST_CHECK(find_value(r.get_obj(), "status").get_int() == ZDAG_NOT_FOUND || find_value(r.get_obj(), "status").get_int() == ZDAG_STATUS_OK);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + dblspendtx + "\""));
    BOOST_CHECK(find_value(r.get_obj(), "status").get_int() == ZDAG_NOT_FOUND || find_value(r.get_obj(), "status").get_int() == ZDAG_STATUS_OK);
    

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + tx17 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + tx17 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);


    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + tx17 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
  
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "assetallocationverifyzdag", "\"" + tx15 + "\""));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
}
BOOST_AUTO_TEST_CASE(generate_bad_assetmaxsupply_address)
{
    UniValue r;
	GenerateBlocks(5);
	tfm::format(std::cout,"Running generate_bad_assetmaxsupply_address...\n");
	GenerateBlocks(5);
	string newaddress = GetNewFundedAddress("node1");
	// 256 bytes long
	string gooddata = "SfsddfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsfDsdsdsdsfsfsdsfsdsfdsfsdsfdsfsdsfsdSfsdfdfsdsfSfsdfdfsdsDfdfdd";
	// 0 max supply good
	BOOST_CHECK_NO_THROW(CallExtRPC("node1", "assetnew" , "\"" + newaddress + "\",\"tmp\",\"" + gooddata + "\",\"''\",8,1,0,31,{},\"''\""));
	// 1 max supply good
	BOOST_CHECK_NO_THROW(CallExtRPC("node1", "assetnew" , "\"" + newaddress + "\",\"tmp\",\"" + gooddata + "\",\"''\",8,1,1,31,{},\"''\""));
	// balance > max supply
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew" , "\"" + newaddress + "\",\"tmp\",\"" + gooddata + "\",\"''\",3,2000,1000,31,{},\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
    
}

BOOST_AUTO_TEST_CASE(generate_assetupdate_address)
{
	tfm::format(std::cout,"Running generate_assetupdate_address...\n");
	string newaddress = GetNewFundedAddress("node1");
	string guid = AssetNew("node1", newaddress, "data");
	// update an asset that isn't yours
	UniValue r;
	// "assetupdate [asset] [description] [contract] [supply] [update_flags] [aux_fees] [witness]\n"
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetupdate" , guid + ",\"pub\",\"''\",1,31,{},\"''\""));

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
    
	AssetUpdate("node1", guid, "pub1");
	// shouldn't update data, just uses prev data because it hasn't changed
	AssetUpdate("node1", guid);
	// update supply, ensure balance gets updated properly, 5+1, 1 comes from the initial assetnew, 1 above doesn't actually get set because asset wasn't yours so total should be 6
	AssetUpdate("node1", guid, "pub12", "5");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid + "\""));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 6 * COIN);
	// update supply
    int updateflags = ASSET_UPDATE_ALL & ~ASSET_UPDATE_SUPPLY;
	string guid1 = AssetNew("node1", newaddress, "data", "''", "8", "1", "10", "31");
    // can't change supply > max supply (current balance already 6, max is 10)
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetupdate" , guid + ",\"pub\",\"''\",5," + itostr(updateflags) + ",{},\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
        

	AssetUpdate("node1", guid1, "pub12", "1", itostr(updateflags));
	// ensure can't update supply (update flags is set to not allowsupply update)
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetupdate" , guid1 + ",\"pub\",\"''\",1," + itostr(updateflags) + ",{},\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

}
BOOST_AUTO_TEST_CASE(generate_assetupdate_precision_address)
{
	tfm::format(std::cout,"Running generate_assetupdate_precision_address...\n");
	UniValue r;
    string hexStr;
	for (int i = 0; i <= 8; i++) {
		string istr = itostr(i);
		string addressName = GetNewFundedAddress("node1");
        string addressName1 = GetNewFundedAddress("node1");
        string addressName2 = GetNewFundedAddress("node1");
        string addressName3 = GetNewFundedAddress("node1");
		// test max supply for every possible precision
		
		UniValue negonevalue(UniValue::VSTR);
		negonevalue.setStr("-1");
		CAmount precisionCoin = pow(10, i);
		// get max value - 1 (1 is already the supply, and this value is cumulative)
		CAmount negonesupply = AssetAmountFromValue(negonevalue, i) - precisionCoin;
		string maxstrupdate = ValueFromAssetAmount(negonesupply, i).write();
		// can't create asset with more than max+1 balance or max+1 supply
		string maxstrplusone = ValueFromAssetAmount(negonesupply + (precisionCoin *2), i).write();
		string maxstrnew = ValueFromAssetAmount(negonesupply + precisionCoin, i).write();

		string guid = AssetNew("node1", addressName, "data", "''", istr, "1", maxstrnew);
		AssetUpdate("node1", guid, "pub12", maxstrupdate);
		// "assetupdate [asset] [description] [contract] [supply] [update_flags] [aux_fees] [witness]\n"
		// can't go above max balance (10^18) / (10^i) for i decimal places
		BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetupdate" , guid + ",\"pub\",\"''\",1,31,{},\"''\""));
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
        hexStr = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);


		// "assetnew [address] [symbol] [description] [contract] [precision=8] [supply] [max_supply] [update_flags] [aux_fees] [witness]\n"
		BOOST_CHECK_NO_THROW(r= CallExtRPC("node1", "assetnew" , "\"" + addressName + "\",\"cat\",\"pub\",\"''\"," + istr + "," + maxstrnew + "," + maxstrnew + ",31,{},\"''\""));
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
        hexStr = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""));

        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew","\"" + addressName1 + "\",\"sysx\",\"pub\",\"''\"," + istr + ",1," + maxstrnew + ",31,{},\"''\""));
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
        hexStr = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""));

        // will try to use 0 for total supply + balance
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew","\"" + addressName2 + "\",\"syse\",\"pub\",\"''\"," + istr + "," + maxstrplusone + "," + maxstrnew + ",31,{},\"''\""));
        string guidZero = itostr(find_value(r.get_obj(), "asset_guid").get_uint());
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
        hexStr = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

        GenerateBlocks(1);


		BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew" , "\"" + addressName3 + "\",\"syst\",\"pub\",\"''\"," + istr + ",1," + maxstrplusone + ",31,{},\"''\""));
        BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
        hexStr = find_value(r.get_obj(), "hex").get_str();
        BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

	}
    string newaddress = GetNewFundedAddress("node1");
	// invalid precisions
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetnew" , "\"" + newaddress + "\",\"dow\",\"pub\",\"''\",9,1,2,31,{},\"''\""));
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

	BOOST_CHECK_THROW(r = CallExtRPC("node1", "assetnew" , "\"" + newaddress + "\",\"ep500\",\"pub\",\"''\",-1,1,2,31,{},\"''\""), runtime_error);

}
BOOST_AUTO_TEST_CASE(generate_assetsend_address)
{
	UniValue r;
	tfm::format(std::cout,"Running generate_assetsend_address...\n");
	string newaddress = GetNewFundedAddress("node1");
	string newaddress1 = GetNewFundedAddress("node1");
	string guid = AssetNew("node1", newaddress, "data", "''", "8", "10", "20");
	// [{\"address\":\"address\",\"amount\":amount},...]
	AssetSend("node1", guid, "[{\"address\":\"" + newaddress1 + "\",\"amount\":7}]");
	// ensure amounts are correct
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid + "\""));
	UniValue balance = find_value(r.get_obj(), "balance");
	UniValue totalsupply = find_value(r.get_obj(), "total_supply");
	UniValue maxsupply = find_value(r.get_obj(), "max_supply");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 3 * COIN);
	BOOST_CHECK_EQUAL(AssetAmountFromValue(totalsupply, 8), 10 * COIN);
	BOOST_CHECK_EQUAL(AssetAmountFromValue(maxsupply, 8), 20 * COIN);

	// ensure receiver gets it
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress1 + "\"" ));

	balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 7 * COIN);

	// add balances
	AssetUpdate("node1", guid, "pub12", "1");
	// check balance is added to end
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid + "\""));
	balance = find_value(r.get_obj(), "balance");
	totalsupply = find_value(r.get_obj(), "total_supply");
	maxsupply = find_value(r.get_obj(), "max_supply");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 4 * COIN);
	BOOST_CHECK_EQUAL(AssetAmountFromValue(totalsupply, 8), 11 * COIN);
	BOOST_CHECK_EQUAL(AssetAmountFromValue(maxsupply, 8), 20 * COIN);

	AssetUpdate("node1", guid, "pub12", "9");
	// check balance is added to end
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid + "\""));
	balance = find_value(r.get_obj(), "balance");
	totalsupply = find_value(r.get_obj(), "total_supply");
	maxsupply = find_value(r.get_obj(), "max_supply");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 13 * COIN);
	BOOST_CHECK_EQUAL(AssetAmountFromValue(totalsupply, 8), 20 * COIN);
	BOOST_CHECK_EQUAL(AssetAmountFromValue(maxsupply, 8), 20 * COIN);

	// can't go over 20 supply
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetupdate" , guid + ",\"pub\",\"''\",1,31,{},\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);

}
BOOST_AUTO_TEST_CASE(generate_assettransfer_address)
{
    UniValue r;
	tfm::format(std::cout,"Running generate_assettransfer_address...\n");
	GenerateBlocks(15, "node1");
	GenerateBlocks(15, "node2");
	GenerateBlocks(15, "node3");
	string newaddres1 = GetNewFundedAddress("node1");
    string newaddres2 = GetNewFundedAddress("node2");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node3", "getnewaddress"));
    string newaddres3 = r.get_str();
	string guid1 = AssetNew("node1", newaddres1, "pubdata");
	string guid2 = AssetNew("node2", newaddres2, "pubdata");
    	
	AssetUpdate("node1", guid1, "pub3");
	AssetTransfer("node1", "node2", guid1, newaddres2);

	AssetTransfer("node2", "node3", guid2, newaddres3);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid1 + "\""));
    BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == newaddres2);

	// xfer an asset that isn't yours
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assettransfer" , guid1 + ",\"" + newaddres3 + "\",\"''\""));

    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" +  find_value(r.get_obj(), "hex").get_str() + "\""));
    string hexStr = find_value(r.get_obj(), "hex").get_str();
    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hexStr + "\""), runtime_error);
    GenerateBlocks(5, "node1");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetinfo", "\"" + guid1 + "\""));
    BOOST_CHECK(find_value(r.get_obj(), "address").get_str() == newaddres2);

	// update xferred asset
	AssetUpdate("node2", guid1, "public");

	// retransfer asset
	AssetTransfer("node2", "node3", guid1, newaddres3);
}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_address_sync)
{
	UniValue r;
	tfm::format(std::cout,"Running generate_asset_allocation_address_sync...\n");
	GenerateBlocks(5);
	string newaddress = GetNewFundedAddress("node1");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
    string newaddressreceiver = r.get_str();
	string guid = AssetNew("node1", newaddress, "data", "''", "8", "10000", "1000000");

	AssetSend("node1", guid, "[{\"address\":\"" + newaddressreceiver + "\",\"amount\":5000}]");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddressreceiver + "\"" ));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationinfo",  guid + ",\"" + newaddressreceiver + "\""  ));
	balance = find_value(r.get_obj(), "balance");
	StopNode("node2");
	StartNode("node2");
	GenerateBlocks(5, "node2");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddressreceiver + "\""  ));
	balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node2", "assetallocationinfo",  guid + ",\"" + newaddressreceiver + "\""  ));
	balance = find_value(r.get_obj(), "balance");

}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_lock)
{
	UniValue r;
	tfm::format(std::cout,"Running generate_asset_allocation_lock...\n");
	GenerateBlocks(5);
	string txid;
	string newaddress1 = GetNewFundedAddress("node2");
	string newaddress = GetNewFundedAddress("node1", txid);

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getrawtransaction" , "\"" + txid + "\",true"));
	UniValue voutArray = find_value(r.get_obj(), "vout");
	int vout = 0;
	CAmount nAmount = 0;
	for (unsigned int i = 0; i<voutArray.size(); i++) {
		UniValue scriptObj = find_value(voutArray[i].get_obj(), "scriptPubKey").get_obj();
		UniValue addressesArray = find_value(scriptObj, "addresses");
		for (unsigned int j = 0; j<addressesArray.size(); j++) {
			if (addressesArray[j].get_str() == newaddress) {
				vout = i;
				nAmount = AmountFromValue(find_value(voutArray[i].get_obj(), "value"));
			}
		}
	}
	// for fees;
	nAmount -= 2000000;
	BOOST_CHECK(nAmount > 0);
	string strAmount = ValueFromAmount(nAmount).write();
	string voutstr = itostr(vout);
	// lock outpoint so other txs can't spend through wallet auto-selection 
	BOOST_CHECK_NO_THROW(CallExtRPC("node1", "lockunspent", "false,[{\"txid\":\"" + txid + "\",\"vout\":" + voutstr + "}]"));


	string guid = AssetNew("node1", newaddress, "data", "''", "8", "1", "100000");

	AssetSend("node1", guid, "[{\"address\":\"" + newaddress + "\",\"amount\":1}]");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress + "\""  ));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 1 * COIN);

	LockAssetAllocation("node1", guid, newaddress, txid, voutstr);

    // unlock now to test spending
    BOOST_CHECK_NO_THROW(CallExtRPC("node1", "lockunspent","true,[{\"txid\":\"" + txid + "\",\"vout\":" + voutstr + "}]"));
    // cannot spend as normal through sendrawtransaction
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "createrawtransaction", "[{\"txid\":\"" + txid + "\",\"vout\":" + voutstr + "}],[{\"" + newaddress1 + "\":" + strAmount + "}]"));

    string rawTx = r.get_str();
    rawTx.erase(std::remove(rawTx.begin(), rawTx.end(), '\n'), rawTx.end());
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "signrawtransactionwithwallet" , "\"" + rawTx + "\""));

    string hex_str = find_value(r.get_obj(), "hex").get_str();

    BOOST_CHECK_THROW(r = CallExtRPC("node1", "sendrawtransaction" , "\"" + hex_str + "\""), runtime_error);
    string txid0 = AssetAllocationTransfer(false, "node1", guid, newaddress, "[{\"address\":\"" + newaddress1 + "\",\"amount\":0.11}]");
    BOOST_CHECK(AreTwoTransactionsLinked("node1", txid, txid0));
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress1 + "\"" ));
	balance = find_value(r.get_obj(), "balance");
    BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.11 * COIN);
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getrawtransaction" , "\"" + txid0 + "\",true"));
	UniValue vinArray = find_value(r.get_obj(), "vin");
	bool found = false;
	for (unsigned int i = 0; i<vinArray.size(); i++) {
		if (find_value(vinArray[i].get_obj(), "txid").get_str() == txid && find_value(vinArray[i].get_obj(), "vout").get_uint() == vout) {
			found = true;
			break;
		}
	}
	BOOST_CHECK(found);

}
BOOST_AUTO_TEST_CASE(generate_asset_allocation_send_address)
{
	UniValue r;
	tfm::format(std::cout,"Running generate_asset_allocation_send_address...\n");
	GenerateBlocks(5);
    GenerateBlocks(5, "node2");
	string newaddress1 = GetNewFundedAddress("node1");
    CallExtRPC("node2", "sendtoaddress" , "\"" + newaddress1 + "\",1", false);
    CallExtRPC("node2", "sendtoaddress" , "\"" + newaddress1 + "\",1", false);
	GenerateBlocks(5);
	GenerateBlocks(5, "node2");
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getnewaddress"));
    string newaddress2 = r.get_str();
        
	string guid = AssetNew("node1", newaddress1, "data","''", "8", "1", "100000");

	AssetSend("node1", guid, "[{\"address\":\"" + newaddress1 + "\",\"amount\":1}]");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress1 + "\""));
	UniValue balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 1 * COIN);

	string txid0 = AssetAllocationTransfer(false, "node1", guid, newaddress1, "[{\"address\":\"" + newaddress2 + "\",\"amount\":0.11}]");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress2 + "\""));
	balance = find_value(r.get_obj(), "balance");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.11 * COIN);

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid0 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	// send using zdag, balance won't go up only down within block
	string txid1 = AssetAllocationTransfer(true, "node1", guid, newaddress1, "[{\"address\":\"" + newaddress2 + "\",\"amount\":0.12}]");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress2 + "\""));
	balance = find_value(r.get_obj(), "balance_zdag");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.11 * COIN);
	UninterruptibleSleep(std::chrono::milliseconds(500));
	// second send
	string txid2 = AssetAllocationTransfer(true, "node1", guid, newaddress1, "[{\"address\":\"" + newaddress2 + "\",\"amount\":0.13}]");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress2 + "\""));
	balance = find_value(r.get_obj(), "balance_zdag");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.11 * COIN);

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid0 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	// first ones now OK because it was found explicitly
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);


	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid2 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_STATUS_OK);

	// after pow, should get not found on all of them
	GenerateBlocks(1);

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid0 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);

	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid1 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);


	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationverifyzdag", "\"" + txid2 + "\""));
	BOOST_CHECK_EQUAL(find_value(r.get_obj(), "status").get_int(), ZDAG_NOT_FOUND);
    // now balance adjusts after block
    BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "assetallocationinfo",  guid + ",\"" + newaddress2 + "\""));
	balance = find_value(r.get_obj(), "balance_zdag");
	BOOST_CHECK_EQUAL(AssetAmountFromValue(balance, 8), 0.36 * COIN);
}
BOOST_AUTO_TEST_CASE(generate_asset_consistency_check)
{
	UniValue assetInvalidatedResults, assetNowResults, assetValidatedResults;
	UniValue assetAllocationsInvalidatedResults, assetAllocationsNowResults, assetAllocationsValidatedResults;
	UniValue r;
	tfm::format(std::cout,"Running generate_asset_consistency_check...\n");
	GenerateBlocks(5);
    tfm::format(std::cout,"Setting time ahead 300 seconds...\n");
    // trigger expiry of txs -- zdag balances should all match (no more zdag txs, fall back to db balance so string checks after reindex pass)
    SleepFor(300 * 1000, 0);
    string creatoraddress2 = GetNewFundedAddress("node2");
    // give time for mempool to get tx and then clear conflict via expire
    AssetNew("node2", creatoraddress2, "pubdata", "0xc47bD54a3Df2273426829a7928C3526BF8F7Acaa");
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getblockcount"));
	string strBeforeBlockCount = itostr(r.get_int());
	// first check around disconnect/connect by invalidating and revalidating an early block
	BOOST_CHECK_NO_THROW(assetNowResults = CallExtRPC("node1", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK_NO_THROW(assetAllocationsNowResults = CallExtRPC("node1", "listassetallocations" , itostr(INT_MAX) + ",0"));

	// disconnect back to block 10 where no assets would have existed
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getblockhash", "10"));
	string blockHashInvalidated = r.get_str();
	BOOST_CHECK_NO_THROW(CallExtRPC("node1", "invalidateblock" , "\"" + blockHashInvalidated + "\""));
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getblockcount"));
    string strAfterBlockCount = itostr(r.get_int());
	BOOST_CHECK_EQUAL(strAfterBlockCount, "9");
    // sysx asset shouldn't exist
    BOOST_CHECK_THROW(CallExtRPC("node1", "assetinfo" , strSYSXAsset), runtime_error);
	UniValue emptyResult(UniValue::VARR);
	BOOST_CHECK_NO_THROW(assetInvalidatedResults = CallExtRPC("node1", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK_EQUAL(assetInvalidatedResults.write(), emptyResult.write());
	BOOST_CHECK_NO_THROW(assetAllocationsInvalidatedResults = CallExtRPC("node1", "listassetallocations" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK_EQUAL(assetAllocationsInvalidatedResults.write(), emptyResult.write());

	// reconnect to tip and ensure block count matches
	BOOST_CHECK_NO_THROW(CallExtRPC("node1", "reconsiderblock" , "\"" + blockHashInvalidated + "\""));
	BOOST_CHECK_NO_THROW(r = CallExtRPC("node1", "getblockcount"));
    string strRevalidatedBlockCount = itostr(r.get_int());
	BOOST_CHECK_EQUAL(strRevalidatedBlockCount, strBeforeBlockCount);

	BOOST_CHECK_NO_THROW(assetValidatedResults = CallExtRPC("node1", "listassets" , itostr(INT_MAX) + ",0"));
	string validatedResStr = assetValidatedResults.write();
	string nowResStr = assetNowResults.write();
	BOOST_CHECK(validatedResStr == nowResStr);
	BOOST_CHECK_NO_THROW(assetAllocationsValidatedResults = CallExtRPC("node1", "listassetallocations" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK(assetAllocationsValidatedResults.write() == assetAllocationsNowResults.write());
	// try to check after reindex
	StopNode("node1");
	StartNode("node1", true, "", true);

	BOOST_CHECK_NO_THROW(assetValidatedResults = CallExtRPC("node1", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK(assetValidatedResults.write() == assetNowResults.write());
	BOOST_CHECK_NO_THROW(assetValidatedResults = CallExtRPC("node2", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK(assetValidatedResults.write() == assetNowResults.write());
	BOOST_CHECK_NO_THROW(assetValidatedResults = CallExtRPC("node3", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK(assetValidatedResults.write() == assetNowResults.write());

	StopNode("node2");
	StartNode("node2", true, "", true);
	BOOST_CHECK_NO_THROW(assetValidatedResults = CallExtRPC("node2", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK(assetValidatedResults.write() == assetNowResults.write());
	
	StopNode("node3");
	StartNode("node3", true, "", true);
	BOOST_CHECK_NO_THROW(assetValidatedResults = CallExtRPC("node3", "listassets" , itostr(INT_MAX) + ",0"));
	BOOST_CHECK(assetValidatedResults.write() == assetNowResults.write());

	BOOST_CHECK_NO_THROW(assetAllocationsValidatedResults = CallExtRPC("node1", "listassetallocations" , itostr(INT_MAX) + ",0"));
    std::string first = assetAllocationsValidatedResults.write();
    const std::string &second = assetAllocationsNowResults.write();
	BOOST_CHECK(first == second);
	BOOST_CHECK_NO_THROW(assetAllocationsValidatedResults = CallExtRPC("node2", "listassetallocations" , itostr(INT_MAX) + ",0"));
    first = assetAllocationsValidatedResults.write();
	BOOST_CHECK(first == second);
	BOOST_CHECK_NO_THROW(assetAllocationsValidatedResults = CallExtRPC("node3", "listassetallocations" , itostr(INT_MAX) + ",0"));
    first = assetAllocationsValidatedResults.write();
	BOOST_CHECK(first == second);
}

BOOST_AUTO_TEST_SUITE_END ()
