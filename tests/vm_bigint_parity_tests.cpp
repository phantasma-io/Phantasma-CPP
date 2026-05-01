#include "test_cases.h"
#include "VM/ScriptBuilder.h"

namespace testcases {
using namespace testutil;

namespace {

const char* VmTypeName(VMType type)
{
	switch( type )
	{
	case VMType::None:
		return "None";
	case VMType::Struct:
		return "Struct";
	case VMType::Bytes:
		return "Bytes";
	case VMType::Number:
		return "Number";
	case VMType::String:
		return "String";
	case VMType::Timestamp:
		return "Timestamp";
	case VMType::Bool:
		return "Bool";
	case VMType::Enum:
		return "Enum";
	case VMType::Object:
		return "Object";
	default:
		return "Unknown";
	}
}

bool TryOpenFixture(std::ifstream& file, const char* name)
{
	file.open(std::string("tests/fixtures/") + name);
	if( !file.is_open() )
	{
		file.open(std::string("fixtures/") + name);
	}
	return file.is_open();
}

bool ReadFixtureLine(std::ifstream& file, std::string& line)
{
	while( std::getline(file, line) )
	{
		if( line.empty() || line[0] == '#' )
		{
			continue;
		}
		return true;
	}
	return false;
}

bool SplitColumns(const std::string& line, std::vector<std::string>& cols)
{
	cols.clear();
	std::stringstream ss(line);
	std::string col;
	while( std::getline(ss, col, '\t') )
	{
		cols.push_back(col);
	}
	return !cols.empty();
}

bool ExceptionMatchesCategory(const std::exception& ex, const std::string& category)
{
	const std::string msg = ex.what();
	if( category == "divide_by_zero" )
	{
		return msg.find("divide by zero") != std::string::npos;
	}
	if( category == "int32_overflow" )
	{
		return msg.find("overflow") != std::string::npos || msg.find("Int32") != std::string::npos;
	}
	if( category == "negative_exponent" )
	{
		return msg.find("exponent") != std::string::npos || msg.find("greater than or equal to zero") != std::string::npos;
	}
	if( category == "string_to_bigint" )
	{
		return msg.find("Cannot convert String") != std::string::npos;
	}
	if( category == "too_many_bits" )
	{
		return msg.find("too many bits") != std::string::npos;
	}
	if( category == "logical_op_type" )
	{
		return msg.find("expected ") != std::string::npos;
	}
	if( category == "invalid_vm_register_type" )
	{
		return msg.find("Invalid VM Register type") != std::string::npos;
	}
	if( category == "invalid_vm_bool_cast" )
	{
		return msg.find("Invalid cast") != std::string::npos && msg.find("bool") != std::string::npos;
	}
	if( category == "invalid_vm_number_cast" )
	{
		return msg.find("Invalid cast") != std::string::npos && msg.find("number") != std::string::npos;
	}
	if( category == "invalid_vm_bytes_cast" )
	{
		return msg.find("Invalid cast") != std::string::npos && msg.find("bytes") != std::string::npos;
	}
	if( category == "invalid_cast" )
	{
		return msg.find("invalid cast") != std::string::npos || msg.find("Invalid cast") != std::string::npos;
	}
	if( category == "v_m_exception" )
	{
		return msg.find("Invalid cast") != std::string::npos ||
		       msg.find("Invalid VM Register type") != std::string::npos ||
		       msg.find("expected ") != std::string::npos ||
		       msg.find("Script execution failed") != std::string::npos;
	}
	return msg.find(category) != std::string::npos;
}

template<typename Fn>
void ReportExpectedException(TestContext& ctx, const std::string& name, const std::string& category, Fn&& fn)
{
	try
	{
		fn();
		Report(ctx, false, name, "no exception");
	}
	catch( const std::exception& ex )
	{
		Report(ctx, ExceptionMatchesCategory(ex, category), name, ex.what());
	}
	catch( ... )
	{
		Report(ctx, false, name, "unexpected exception");
	}
}

BigInteger ParseBigInt(const std::string& text)
{
	return BigInteger::Parse(String(text.c_str()));
}

ByteArray ParseHexOrEmpty(const std::string& hex)
{
	return hex.empty() ? ByteArray() : HexToBytes(hex);
}

std::string LowerHex(const ByteArray& bytes)
{
	std::string hex = BytesToHex(bytes);
	std::transform(hex.begin(), hex.end(), hex.begin(), [](unsigned char c)
	    { return (char)std::tolower(c); });
	return hex;
}

VMObject BuildFixtureVmObject(const std::string& caseId, const std::string& sourceKind, const std::string& payload)
{
	VMObject obj;
	if( sourceKind == "empty" )
	{
		return obj;
	}
	if( sourceKind == "string" )
	{
		obj = VMObject(String(payload.c_str()));
		return obj;
	}
	if( sourceKind == "bytes" )
	{
		const ByteArray bytes = ParseHexOrEmpty(payload);
		obj.SetValue(bytes.empty() ? nullptr : bytes.data(), (int)bytes.size(), VMType::Bytes);
		return obj;
	}
	if( sourceKind == "serialized_vmobject" )
	{
		return VMObject::FromBytes(ParseHexOrEmpty(payload));
	}
	if( sourceKind == "bool" )
	{
		obj = VMObject(payload == "true");
		return obj;
	}
	if( sourceKind == "enum" )
	{
		const uint32_t value = (uint32_t)std::stoul(payload, nullptr, 10);
		Byte raw[4];
		raw[0] = (Byte)(value & 0xFF);
		raw[1] = (Byte)((value >> 8) & 0xFF);
		raw[2] = (Byte)((value >> 16) & 0xFF);
		raw[3] = (Byte)((value >> 24) & 0xFF);
		obj.SetValue(raw, 4, VMType::Enum);
		return obj;
	}
	if( sourceKind == "timestamp" )
	{
		const uint32_t value = (uint32_t)std::stoul(payload, nullptr, 10);
		obj = VMObject(Timestamp(value));
		return obj;
	}
	if( sourceKind == "number" )
	{
		obj = VMObject(ParseBigInt(payload));
		return obj;
	}
	if( sourceKind == "object" )
	{
		const ByteArray bytes = ParseHexOrEmpty(payload);
		// The authoritative Gen2 fixtures use a generic "object" source kind for both
		// Address and Hash payloads. Infer the concrete runtime shape from the payload size
		// so the C++ fixture object matches the exact C# object instance that generated it.
		if( bytes.size() == Address::LengthInBytes )
		{
			obj = VMObject(Address(bytes));
		}
		else if( bytes.size() == Hash::Length )
		{
			obj = VMObject(Hash(bytes));
		}
		else
		{
			PHANTASMA_EXCEPTION("unsupported VMObject object fixture payload");
		}
		return obj;
	}
	if( sourceKind == "struct" )
	{
		VMObject result;
		result.SetKey(VMObject(String("name")), VMObject(String("neo")));
		result.SetKey(VMObject(String("count")), VMObject(ParseBigInt("7")));
		return result;
	}

	PHANTASMA_EXCEPTION("unsupported VMObject fixture");
	return {};
}

std::string DescribeVmObject(const VMObject& obj)
{
	switch( obj.Type() )
	{
	case VMType::None:
		return "None";
	case VMType::Number:
		return "Number:" + std::string(obj.AsNumber().ToString().c_str());
	case VMType::String:
		return "String:" + std::string(obj.AsString().c_str());
	case VMType::Bool:
		return std::string("Bool:") + (obj.AsBool() ? "true" : "false");
	case VMType::Timestamp:
		return "Timestamp:" + std::to_string(obj.AsTimestamp().Value);
	case VMType::Bytes:
		return "Bytes:" + LowerHex(obj.AsByteArray());
	case VMType::Struct:
		return "Struct:" + LowerHex(obj.AsByteArray());
	case VMType::Object:
		if( obj.AsExecutionContext() )
		{
			return "Object:ExecutionContext";
		}
		try
		{
			const Address address = obj.AsAddress();
			return "Object.Address:" + LowerHex(ByteArray(address.ToByteArray(), address.ToByteArray() + Address::LengthInBytes));
		}
		catch( ... )
		{
		}
		try
		{
			const Hash hash = obj.AsHash();
			return "Object.Hash:" + LowerHex(ByteArray(hash.ToByteArray(), hash.ToByteArray() + Hash::Length));
		}
		catch( ... )
		{
			return "Object";
		}
	case VMType::Enum:
		return "Enum:" + std::to_string(obj.AsEnum());
	default:
		return "Unknown";
	}
}

void RunBinaryFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vm_bigint_binary.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM binary fixture", "missing gen2_csharp_vm_bigint_binary.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 6 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const BigInteger n = ParseBigInt(cols[1]);
		const ByteArray expectedSigned = ParseHexOrEmpty(cols[2]);
		const ByteArray expectedUnsigned = ParseHexOrEmpty(cols[3]);
		const ByteArray expectedIo = ParseHexOrEmpty(cols[4]);
		const ByteArray expectedScript = ParseHexOrEmpty(cols[5]);

		Report(ctx, n.ToSignedByteArray() == expectedSigned, "Gen2 C# VM BigInt signed " + caseId);
		Report(ctx, n.ToUnsignedByteArray() == expectedUnsigned, "Gen2 C# VM BigInt unsigned " + caseId);

		BinaryWriter writer;
		writer.WriteBigInteger(n);
		Report(ctx, writer.ToArray() == expectedIo, "Gen2 C# VM BigInt io " + caseId);

		ScriptBuilder sb;
		sb.EmitLoad(0, n);
		Report(ctx, sb.ToScript() == expectedScript, "Gen2 C# VM BigInt scriptbuilder " + caseId);
	}
}

void RunDecimalFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vm_bigint_decimal.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM decimal fixture", "missing gen2_csharp_vm_bigint_decimal.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 7 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const std::string inputText = cols[1];
		const std::string outcome = cols[2];
		const std::string expectedDecimal = cols[3];
		const std::string expectedCategory = cols[5];
		const std::string label = "Gen2 C# VM BigInt decimal " + caseId;

		if( outcome == "ok" )
		{
			const BigInteger value = ParseBigInt(inputText);
			Report(ctx, value.ToString() == String(expectedDecimal.c_str()), label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    {
				    const BigInteger value = ParseBigInt(inputText);
				    (void)value.ToString(); });
		}
	}
}

void RunNarrowIntFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vm_bigint_narrow_int.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM narrow-int fixture", "missing gen2_csharp_vm_bigint_narrow_int.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 7 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const BigInteger n = ParseBigInt(cols[1]);
		const std::string outcome = cols[2];
		const std::string expectedValue = cols[3];
		const std::string expectedCategory = cols[5];
		const std::string label = "Gen2 C# VM BigInt narrow-int " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, std::to_string((int)n) == expectedValue, label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)(int)n; });
		}
	}
}

BigInteger ExecuteFixtureOp(const std::string& op, const BigInteger& a, const BigInteger& b)
{
	if( op == "ADD" )
		return a + b;
	if( op == "SUB" )
		return a - b;
	if( op == "MUL" )
		return a * b;
	if( op == "DIV" )
		return a / b;
	if( op == "MOD" )
		return a % b;
	if( op == "SHL" )
		return a << (int)b;
	if( op == "SHR" )
		return a >> (int)b;
	if( op == "MIN" )
		return a < b ? a : b;
	if( op == "MAX" )
		return a > b ? a : b;
	if( op == "POW" )
		return BigInteger::Pow(a, b);
	PHANTASMA_EXCEPTION("unsupported op");
	return {};
}

BigInteger ExecuteFixtureUnaryOp(const std::string& op, const BigInteger& value)
{
	if( op == "SIGN" )
		return BigInteger(value.IsZero() ? 0 : (value.IsNegative() ? -1 : 1));
	if( op == "NEGATE" )
		return -value;
	if( op == "ABS" )
		return value.Abs();
	PHANTASMA_EXCEPTION("unsupported unary op");
	return {};
}

void RunOpFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vm_bigint_ops.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM op fixture", "missing gen2_csharp_vm_bigint_ops.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 9 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const std::string op = cols[1];
		const BigInteger a = ParseBigInt(cols[2]);
		const BigInteger b = ParseBigInt(cols[3]);
		const std::string outcome = cols[4];
		const std::string expectedValue = cols[5];
		const std::string expectedCategory = cols[7];
		const std::string label = "Gen2 C# VM BigInt op " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, ExecuteFixtureOp(op, a, b).ToString() == String(expectedValue.c_str()), label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)ExecuteFixtureOp(op, a, b); });
		}
	}
}

void RunUnaryOpFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vm_bigint_unary_ops.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM unary-op fixture", "missing gen2_csharp_vm_bigint_unary_ops.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 8 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const std::string op = cols[1];
		const BigInteger value = ParseBigInt(cols[2]);
		const std::string outcome = cols[3];
		const std::string expectedValue = cols[4];
		const std::string expectedCategory = cols[6];
		const std::string label = "Gen2 C# VM BigInt unary-op " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, ExecuteFixtureUnaryOp(op, value).ToString() == String(expectedValue.c_str()), label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)ExecuteFixtureUnaryOp(op, value); });
		}
	}
}

void RunVmObjectAsNumberFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_asnumber.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject.AsNumber fixture", "missing gen2_csharp_vmobject_asnumber.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 9 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		VMObject obj = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const std::string outcome = cols[4];
		const std::string expectedValue = cols[5];
		const std::string expectedCategory = cols[7];
		const std::string label = "Gen2 C# VM VMObject.AsNumber " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, obj.AsNumber().ToString() == String(expectedValue.c_str()), label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)obj.AsNumber(); });
		}
	}
}

void RunVmObjectAsBoolFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_asbool.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject.AsBool fixture", "missing gen2_csharp_vmobject_asbool.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 9 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		VMObject obj = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const std::string outcome = cols[4];
		const std::string expectedValue = cols[5];
		const std::string expectedCategory = cols[7];
		const std::string label = "Gen2 C# VM VMObject.AsBool " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, std::string(obj.AsBool() ? "true" : "false") == expectedValue, label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)obj.AsBool(); });
		}
	}
}

void RunVmObjectAsBytesFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_asbytes.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject.AsByteArray fixture", "missing gen2_csharp_vmobject_asbytes.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 9 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		VMObject obj = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const std::string outcome = cols[4];
		const std::string expectedHex = cols[5];
		const std::string expectedCategory = cols[7];
		const std::string label = "Gen2 C# VM VMObject.AsByteArray " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, LowerHex(obj.AsByteArray()) == expectedHex, label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)obj.AsByteArray(); });
		}
	}
}

void RunVmObjectSerdeFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_serde.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject serde fixture", "missing gen2_csharp_vmobject_serde.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 7 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const VMObject original = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const ByteArray expectedSerialized = ParseHexOrEmpty(cols[4]);
		const std::string expectedRoundtripType = cols[5];
		const std::string expectedDescriptor = cols[6];
		const std::string labelBase = "Gen2 C# VM VMObject serde " + caseId;

		BinaryWriter writer;
		const bool serializeOk = original.SerializeData(writer);
		Report(ctx, serializeOk, labelBase + " serialize ok");
		if( !serializeOk )
		{
			continue;
		}
		Report(ctx, writer.ToArray() == expectedSerialized, labelBase + " bytes");

		BinaryReader reader(expectedSerialized);
		VMObject roundtrip;
		const bool deserializeOk = roundtrip.DeserializeData(reader);
		Report(ctx, deserializeOk, labelBase + " deserialize ok");
		if( !deserializeOk )
		{
			continue;
		}

		const std::string actualType = VmTypeName(roundtrip.Type());
		Report(ctx, actualType == expectedRoundtripType, labelBase + " type");
		Report(ctx, DescribeVmObject(roundtrip) == expectedDescriptor, labelBase + " descriptor");
	}
}

void RunVmObjectArrayTypeFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_arraytype.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject.GetArrayType fixture", "missing gen2_csharp_vmobject_arraytype.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 5 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		const VMObject obj = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const std::string expectedType = cols[4];
		const std::string label = "Gen2 C# VM VMObject.GetArrayType " + caseId;

		Report(ctx, std::string(VmTypeName(obj.GetArrayType())) == expectedType, label);
	}
}

void RunVmObjectAsStringFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_asstring.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject.AsString fixture", "missing gen2_csharp_vmobject_asstring.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 9 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		VMObject obj = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const std::string outcome = cols[4];
		const std::string expectedValue = cols[5];
		const std::string expectedCategory = cols[7];
		const std::string label = "Gen2 C# VM VMObject.AsString " + caseId;

		if( outcome == "ok" )
		{
			Report(ctx, std::string(obj.AsString().c_str()) == expectedValue, label);
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)obj.AsString(); });
		}
	}
}

void RunVmObjectCastStructFixtures(TestContext& ctx)
{
	std::ifstream file;
	if( !TryOpenFixture(file, "gen2_csharp_vmobject_cast_struct.tsv") )
	{
		Report(ctx, false, "Gen2 C# VM VMObject.CastTo Struct fixture", "missing gen2_csharp_vmobject_cast_struct.tsv");
		return;
	}

	std::string line;
	ReadFixtureLine(file, line); // header
	while( ReadFixtureLine(file, line) )
	{
		std::vector<std::string> cols;
		if( !SplitColumns(line, cols) || cols.size() < 10 )
		{
			continue;
		}

		const std::string caseId = cols[0];
		VMObject obj = BuildFixtureVmObject(caseId, cols[1], cols[3]);
		const std::string outcome = cols[4];
		const std::string expectedType = cols[5];
		const std::string expectedDescriptor = cols[6];
		const std::string expectedCategory = cols[8];
		const std::string label = "Gen2 C# VM VMObject.CastTo Struct " + caseId;

		if( outcome == "ok" )
		{
			const VMObject cast = obj.CastTo(VMType::Struct);
			Report(ctx, std::string(VmTypeName(cast.Type())) == expectedType, label + " type");
			Report(ctx, DescribeVmObject(cast) == expectedDescriptor, label + " descriptor");
		}
		else
		{
			ReportExpectedException(ctx, label, expectedCategory, [&]()
			    { (void)obj.CastTo(VMType::Struct); });
		}
	}
}

} // namespace

void RunGen2CSharpVmBigIntFixtureTests(TestContext& ctx)
{
	// These fixtures are generated from authoritative Gen2 C# VM code.
	// The manifest in tests/fixtures/gen2_csharp_vm_bigint_manifest.md records
	// the exact source repo, commit, and source files used for generation.
	RunBinaryFixtures(ctx);
	RunDecimalFixtures(ctx);
	RunNarrowIntFixtures(ctx);
	RunOpFixtures(ctx);
	RunUnaryOpFixtures(ctx);
	RunVmObjectAsNumberFixtures(ctx);
	RunVmObjectAsBoolFixtures(ctx);
	RunVmObjectAsBytesFixtures(ctx);
	RunVmObjectSerdeFixtures(ctx);
	RunVmObjectArrayTypeFixtures(ctx);
	RunVmObjectAsStringFixtures(ctx);
	RunVmObjectCastStructFixtures(ctx);
}

} // namespace testcases
