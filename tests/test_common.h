#pragma once

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "../include/PhantasmaAPI.h"
#include "../include/Adapters/PhantasmaAPI_openssl.h"
#include "../include/Numerics/Base16.h"
#include "../include/Numerics/BigInteger.h"
#include "../include/Cryptography/KeyPair.h"
#include "../include/Carbon/Carbon.h"
#include "../include/Carbon/DataBlockchain.h"
#include "../include/Carbon/Tx.h"
#include "../include/Carbon/Contracts/Token.h"
#include "../include/Carbon/Contracts/TokenSchemas.h"
#include "../include/Carbon/DataVm.h"
#include "../include/Utils/Timestamp.h"
#include "../include/VM/ScriptBuilder.h"
#include "../include/Blockchain/Transaction.h"
#include "../include/VM/VMObject.h"
#include "../include/Utils/BinaryWriter.h"
#include "../include/Utils/BinaryReader.h"

namespace testutil {
using namespace phantasma;
using namespace phantasma::carbon;

struct TestContext
{
	int total = 0;
	int failed = 0;
};

void Report(TestContext& ctx, bool ok, const std::string& name, const std::string& details = {});

template <typename Fn>
inline void ExpectNoThrow(TestContext& ctx, const std::string& name, Fn&& fn)
{
	try
	{
		fn();
		Report(ctx, true, name);
	}
	catch (const std::exception& ex)
	{
		Report(ctx, false, name, ex.what());
	}
	catch (...)
	{
		Report(ctx, false, name, "unexpected exception");
	}
}

template <typename Fn>
inline void ExpectThrowContains(TestContext& ctx, const std::string& name, const std::string& needle, Fn&& fn)
{
#ifdef PHANTASMA_EXCEPTION_ENABLE
	try
	{
		fn();
		Report(ctx, false, name, "no exception");
	}
	catch (const std::exception& ex)
	{
		const std::string msg = ex.what();
		Report(ctx, msg.find(needle) != std::string::npos, name, msg);
	}
	catch (...)
	{
		Report(ctx, false, name, "unexpected exception");
	}
#else
	(void)fn; // no-op to silence unused warning when exceptions are disabled.
	(void)needle;
	Report(ctx, true, name + " (exceptions disabled)");
#endif
}

struct Row
{
	std::string kind;
	std::string value;
	std::string hex;
	std::string decOrig;
	std::string decBack;
};

std::string ToUpper(const std::string& s);
ByteArray HexToBytes(const std::string& hex);
std::string BytesToHex(const ByteArray& bytes);
ByteArray BytesFromView(const ByteView& view);
ByteArray ParseDecBytes(const std::string& dec);
int64_t ParseNum(const std::string& s);
uint64_t ParseUNum(const std::string& s);
intx ParseIntx(const std::string& s);
int256 ToInt256(const BigInteger& bi);
std::vector<std::string> SplitCsv(const std::string& s);
std::vector<ByteArray> ParseArrBytes2D(const std::string& s);
std::vector<Row> LoadRows(const std::string& path);
const std::string& SampleIcon();
Bytes32 ToBytes32(const ByteArray& bytes);
VmNamedVariableSchema MakeSchema(const char* name, VmType type, const VmStructSchema* structSchema = nullptr);
VmStructSchema MakeStructSchema(VmNamedVariableSchema* fields, uint32_t count, bool allowDynamicExtras = false);
ByteArray BuildConsensusSingleVoteScript();

} // namespace testutil
