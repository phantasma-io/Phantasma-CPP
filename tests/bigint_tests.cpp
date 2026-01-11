#include "test_cases.h"

namespace testcases {
using namespace testutil;

static bool TryOpenFixture(std::ifstream& file, const char* name)
{
	file.open(std::string("tests/fixtures/") + name);
	if (!file.is_open())
	{
		file.open(std::string("fixtures/") + name);
	}
	return file.is_open();
}

static bool SplitColumns(const std::string& line, std::vector<std::string>& cols)
{
	// TSV helper for fixture rows; columns are consumed by per-fixture parsers.
	cols.clear();
	std::stringstream ss(line);
	std::string col;
	while (std::getline(ss, col, '\t'))
	{
		cols.push_back(col);
	}
	return !cols.empty();
}

void RunBigIntSerializationTests(TestContext& ctx)
{
	std::ifstream file;
	if (!TryOpenFixture(file, "phantasma_bigint_vectors.tsv"))
	{
		Report(ctx, false, "BigInt fixture", "missing phantasma_bigint_vectors.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}
		std::vector<std::string> cols;
		if (!SplitColumns(line, cols))
		{
			continue;
		}
		if (cols.size() < 3)
		{
			continue;
		}

		const std::string& number = cols[0];
		const ByteArray expectedPha = ParseDecBytes(cols[1]);
		const ByteArray expectedCsharp = ParseDecBytes(cols[2]);
		const BigInteger n = BigInteger::Parse(String(number.c_str()));
		const bool isNegative = !number.empty() && number[0] == '-';
		const bool isZero = number == "0";
		const std::string absText = isNegative ? number.substr(1) : number;

		const ByteArray phaBytes = n.ToSignedByteArray();
		Report(ctx, phaBytes == expectedPha, "BigInt PHA " + number);

		const BigInteger fromSigned = BigInteger::FromSignedArray(expectedPha);
		Report(ctx, fromSigned.ToString() == String(number.c_str()), "BigInt FromSignedArray " + number);

		const BigInteger fromSignedPtr = BigInteger::FromSignedArray(
			expectedPha.empty() ? nullptr : expectedPha.data(),
			(int)expectedPha.size());
		Report(ctx, fromSignedPtr.ToString() == String(number.c_str()), "BigInt FromSignedArray ptr " + number);

		Report(ctx, n.IsZero() == isZero, "BigInt IsZero " + number);
		Report(ctx, n.IsNegative() == isNegative, "BigInt IsNegative " + number);

		const BigInteger absVal = BigInteger::Abs(n);
		Report(ctx, absVal.ToString() == String(absText.c_str()), "BigInt Abs " + number);

		const BigInteger neg = -n;
		if (isZero)
		{
			Report(ctx, neg == n, "BigInt Negate zero " + number);
		}
		else
		{
			Report(ctx, neg != n, "BigInt Negate sign " + number);
			Report(ctx, (-neg).ToString() == String(number.c_str()), "BigInt Double negate " + number);
		}

		BinaryWriter writer;
		writer.WriteBigInteger(n);
		const ByteArray serialized = writer.ToArray();
		const bool lengthOk = serialized.size() == expectedPha.size() + 1 &&
			!serialized.empty() &&
			serialized[0] == (Byte)expectedPha.size();
		Report(ctx, lengthOk, "BigInt PHA length " + number);
		if (lengthOk)
		{
			ByteArray serializedBytes;
			serializedBytes.assign(serialized.begin() + 1, serialized.end());
			Report(ctx, serializedBytes == expectedPha, "BigInt PHA bytes " + number);
		}
		BinaryReader reader(serialized);
		BigInteger roundtripPha;
		reader.ReadBigInteger(roundtripPha);
		Report(ctx, roundtripPha.ToString() == String(number.c_str()), "BigInt PHA roundtrip " + number);

		ScriptBuilder sb;
		sb.EmitLoad(0, n);
		const ByteArray script = sb.ToScript();
		BinaryReader scriptReader(script);
		Byte opcode = 0;
		Byte reg = 0;
		Byte type = 0;
		scriptReader.Read(opcode);
		scriptReader.Read(reg);
		scriptReader.Read(type);
		(void)reg;
		Int64 len = 0;
		scriptReader.ReadVarInt(len);
		ByteArray csharpBytes;
		scriptReader.Read(csharpBytes, (int)len);
		Report(ctx, opcode == (Byte)Opcode::LOAD, "BigInt C# opcode " + number);
		Report(ctx, type == (Byte)VMType::Number, "BigInt C# type " + number);
		Report(ctx, csharpBytes == expectedCsharp, "BigInt C# " + number);
	}
}

void RunBigIntOperationFixtureTests(TestContext& ctx)
{
	// Fixture: phantasma_bigint_ops.tsv
	// Columns: a, b, shift, cmp, add, sub, mul, div, mod, shl, shr (C# BigInteger semantics).
	std::ifstream file;
	if (!TryOpenFixture(file, "phantasma_bigint_ops.tsv"))
	{
		Report(ctx, false, "BigInt ops fixture", "missing phantasma_bigint_ops.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}

		std::vector<std::string> cols;
		if (!SplitColumns(line, cols))
		{
			continue;
		}

		if (cols.size() < 11)
		{
			continue;
		}

		const std::string& aText = cols[0];
		const std::string& bText = cols[1];
		const int shift = std::stoi(cols[2]);
		const int expectedCmp = std::stoi(cols[3]);

		const BigInteger a = BigInteger::Parse(String(aText.c_str()));
		const BigInteger b = BigInteger::Parse(String(bText.c_str()));

		const BigInteger addExpected = BigInteger::Parse(String(cols[4].c_str()));
		const BigInteger subExpected = BigInteger::Parse(String(cols[5].c_str()));
		const BigInteger mulExpected = BigInteger::Parse(String(cols[6].c_str()));
		const BigInteger divExpected = BigInteger::Parse(String(cols[7].c_str()));
		const BigInteger modExpected = BigInteger::Parse(String(cols[8].c_str()));
		const BigInteger shlExpected = BigInteger::Parse(String(cols[9].c_str()));
		const BigInteger shrExpected = BigInteger::Parse(String(cols[10].c_str()));

		const std::string key = aText + " | " + bText;
		Report(ctx, a.CompareTo(b) == expectedCmp, "BigInt Ops compare " + key);
		Report(ctx, (a + b).ToString() == addExpected.ToString(), "BigInt Ops add " + key);
		Report(ctx, (a - b).ToString() == subExpected.ToString(), "BigInt Ops sub " + key);
		Report(ctx, (a * b).ToString() == mulExpected.ToString(), "BigInt Ops mul " + key);
		Report(ctx, (a / b).ToString() == divExpected.ToString(), "BigInt Ops div " + key);
		Report(ctx, (a % b).ToString() == modExpected.ToString(), "BigInt Ops mod " + key);
		Report(ctx, (a << shift).ToString() == shlExpected.ToString(), "BigInt Ops shl " + key);
		Report(ctx, (a >> shift).ToString() == shrExpected.ToString(), "BigInt Ops shr " + key);
	}
}

void RunBigIntBitwiseFixtureTests(TestContext& ctx)
{
	// Fixture: phantasma_bigint_bitwise.tsv
	// Columns: a, b, and, or, xor, notA, notB (two's complement semantics).
	std::ifstream file;
	if (!TryOpenFixture(file, "phantasma_bigint_bitwise.tsv"))
	{
		Report(ctx, false, "BigInt bitwise fixture", "missing phantasma_bigint_bitwise.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}

		std::vector<std::string> cols;
		if (!SplitColumns(line, cols))
		{
			continue;
		}
		if (cols.size() < 7)
		{
			continue;
		}

		const std::string& aText = cols[0];
		const std::string& bText = cols[1];

		const BigInteger a = BigInteger::Parse(String(aText.c_str()));
		const BigInteger b = BigInteger::Parse(String(bText.c_str()));

		const BigInteger andExpected = BigInteger::Parse(String(cols[2].c_str()));
		const BigInteger orExpected = BigInteger::Parse(String(cols[3].c_str()));
		const BigInteger xorExpected = BigInteger::Parse(String(cols[4].c_str()));
		const BigInteger notAExpected = BigInteger::Parse(String(cols[5].c_str()));
		const BigInteger notBExpected = BigInteger::Parse(String(cols[6].c_str()));

		const std::string key = aText + " | " + bText;
		Report(ctx, (a & b).ToString() == andExpected.ToString(), "BigInt Bitwise and " + key);
		Report(ctx, (a | b).ToString() == orExpected.ToString(), "BigInt Bitwise or " + key);
		Report(ctx, (a ^ b).ToString() == xorExpected.ToString(), "BigInt Bitwise xor " + key);
		Report(ctx, (~a).ToString() == notAExpected.ToString(), "BigInt Bitwise notA " + key);
		Report(ctx, (~b).ToString() == notBExpected.ToString(), "BigInt Bitwise notB " + key);
	}
}

void RunBigIntPowFixtureTests(TestContext& ctx)
{
	// Fixture: phantasma_bigint_pow.tsv
	// Columns: a, exp, pow (BigInteger.Pow with non-negative exponent).
	std::ifstream file;
	if (!TryOpenFixture(file, "phantasma_bigint_pow.tsv"))
	{
		Report(ctx, false, "BigInt pow fixture", "missing phantasma_bigint_pow.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}

		std::vector<std::string> cols;
		if (!SplitColumns(line, cols))
		{
			continue;
		}
		if (cols.size() < 3)
		{
			continue;
		}

		const std::string& aText = cols[0];
		const int exp = std::stoi(cols[1]);

		const BigInteger a = BigInteger::Parse(String(aText.c_str()));
		const BigInteger powExpected = BigInteger::Parse(String(cols[2].c_str()));

		const BigInteger expValue = BigInteger(exp);
		const BigInteger powActual = BigInteger::Pow(a, expValue);
		const std::string key = aText + " ^ " + std::to_string(exp);
		Report(ctx, powActual.ToString() == powExpected.ToString(), "BigInt Pow " + key);
	}
}

void RunBigIntModPowFixtureTests(TestContext& ctx)
{
	// Fixture: phantasma_bigint_modpow.tsv
	// Columns: a, exp, mod, modpow (supports negative exp via ModInverse path).
	std::ifstream file;
	if (!TryOpenFixture(file, "phantasma_bigint_modpow.tsv"))
	{
		Report(ctx, false, "BigInt modpow fixture", "missing phantasma_bigint_modpow.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}

		std::vector<std::string> cols;
		if (!SplitColumns(line, cols))
		{
			continue;
		}
		if (cols.size() < 4)
		{
			continue;
		}

		const std::string& aText = cols[0];
		const std::string& expText = cols[1];
		const std::string& modText = cols[2];

		const BigInteger a = BigInteger::Parse(String(aText.c_str()));
		const BigInteger exp = BigInteger::Parse(String(expText.c_str()));
		const BigInteger mod = BigInteger::Parse(String(modText.c_str()));
		const BigInteger modPowExpected = BigInteger::Parse(String(cols[3].c_str()));

		const std::string key = aText + " ^ " + expText + " % " + modText;
		const BigInteger modPowActual = a.ModPow(exp, mod);
		Report(ctx, modPowActual.ToString() == modPowExpected.ToString(), "BigInt ModPow " + key);
	}
}

void RunBigIntModInverseFixtureTests(TestContext& ctx)
{
	// Fixture: phantasma_bigint_modinv.tsv
	// Columns: a, mod, inv (inverse defined for coprime values).
	std::ifstream file;
	if (!TryOpenFixture(file, "phantasma_bigint_modinv.tsv"))
	{
		Report(ctx, false, "BigInt modinv fixture", "missing phantasma_bigint_modinv.tsv");
		return;
	}

	std::string line;
	bool header = true;
	while (std::getline(file, line))
	{
		if (line.empty())
		{
			continue;
		}
		if (header)
		{
			header = false;
			continue;
		}

		std::vector<std::string> cols;
		if (!SplitColumns(line, cols))
		{
			continue;
		}
		if (cols.size() < 3)
		{
			continue;
		}

		const std::string& aText = cols[0];
		const std::string& modText = cols[1];

		const BigInteger a = BigInteger::Parse(String(aText.c_str()));
		const BigInteger mod = BigInteger::Parse(String(modText.c_str()));
		const BigInteger invExpected = BigInteger::Parse(String(cols[2].c_str()));

		const BigInteger invActual = a.ModInverse(mod);
		const std::string key = aText + " mod " + modText;
		Report(ctx, invActual.ToString() == invExpected.ToString(), "BigInt ModInverse " + key);
	}
}

void RunBigIntParseFormatTests(TestContext& ctx)
{
	// Use explicit expected strings so parse/format behavior is validated without round-trip bias.
	struct ToStringCase
	{
		Int64 value;
		const Char* expected;
	};
	const ToStringCase toStringCases[] = {
		{ 0, PHANTASMA_LITERAL("0") },
		{ 1, PHANTASMA_LITERAL("1") },
		{ -1, PHANTASMA_LITERAL("-1") },
		{ 42, PHANTASMA_LITERAL("42") },
		{ -42, PHANTASMA_LITERAL("-42") },
		{ 9223372036854775807LL, PHANTASMA_LITERAL("9223372036854775807") },
	};

	for (const auto& testCase : toStringCases)
	{
		const BigInteger n(testCase.value);
		const std::string label = "BigInt ToString " + std::to_string(testCase.value);
		Report(ctx, n.ToString() == String(testCase.expected), label);
	}

	struct ParseCase
	{
		const Char* text;
		int radix;
		const Char* expected;
		bool checkInt64;
		Int64 expectedInt64;
	};
	const ParseCase parseCases[] = {
		{ PHANTASMA_LITERAL("0"), 10, PHANTASMA_LITERAL("0"), true, 0 },
		{ PHANTASMA_LITERAL("00042"), 10, PHANTASMA_LITERAL("42"), true, 42 },
		{ PHANTASMA_LITERAL("-0017"), 10, PHANTASMA_LITERAL("-17"), true, -17 },
		{ PHANTASMA_LITERAL("FF"), 16, PHANTASMA_LITERAL("255"), true, 255 },
		{ PHANTASMA_LITERAL("7f"), 16, PHANTASMA_LITERAL("127"), true, 127 },
		{ PHANTASMA_LITERAL("101010"), 2, PHANTASMA_LITERAL("42"), true, 42 },
		{ PHANTASMA_LITERAL("Z"), 36, PHANTASMA_LITERAL("35"), true, 35 },
		{ PHANTASMA_LITERAL("-80000000"), 16, PHANTASMA_LITERAL("-2147483648"), true, -2147483648LL },
		{ PHANTASMA_LITERAL("123456789012345678901234567890"), 10, PHANTASMA_LITERAL("123456789012345678901234567890"), false, 0 },
		{ PHANTASMA_LITERAL("123\n"), 10, PHANTASMA_LITERAL("123"), true, 123 },
		// Trim leading/trailing line breaks.
		{ PHANTASMA_LITERAL("\n\r-42\r\n"), 10, PHANTASMA_LITERAL("-42"), true, -42 },
	};

	for (size_t i = 0; i < sizeof(parseCases) / sizeof(parseCases[0]); ++i)
	{
		// Mix radix and formatting variations (leading zeros, sign, lowercase hex, trailing newline).
		const ParseCase& testCase = parseCases[i];
		const BigInteger n = BigInteger::Parse(String(testCase.text), testCase.radix);
		Report(ctx, n.ToString() == String(testCase.expected), "BigInt Parse case " + std::to_string(i + 1));
		if (testCase.checkInt64)
		{
			Report(ctx, (Int64)n == testCase.expectedInt64, "BigInt Parse int64 case " + std::to_string(i + 1));
		}
	}

	BigInteger tryParseValue;
	const bool tryParseOk = BigInteger::_TryParse(String(PHANTASMA_LITERAL("12345")), tryParseValue);
	Report(ctx, tryParseOk, "BigInt TryParse ok");
	Report(ctx, tryParseValue.ToString() == String(PHANTASMA_LITERAL("12345")), "BigInt TryParse value");

	BigInteger tryParseInvalidValue;
	const bool tryParseInvalid = BigInteger::_TryParse(String(PHANTASMA_LITERAL("12X")), tryParseInvalidValue);
	// Invalid input should fail without throwing and without altering control flow.
	Report(ctx, !tryParseInvalid, "BigInt TryParse invalid");

	// Explicit constructor error path with auto-length and invalid characters.
	bool parseError = false;
	const BigInteger lenAuto(PHANTASMA_LITERAL("321"), 0, 10, &parseError);
	Report(ctx, !parseError, "BigInt ctor len auto error");
	Report(ctx, lenAuto.ToString() == String(PHANTASMA_LITERAL("321")), "BigInt ctor len auto value");

	parseError = false;
	const BigInteger emptyInput(PHANTASMA_LITERAL(""), 0, 10, &parseError);
	Report(ctx, !parseError, "BigInt ctor empty error");
	Report(ctx, emptyInput.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt ctor empty value");

	parseError = false;
	(void)BigInteger(PHANTASMA_LITERAL("12X"), 3, 10, &parseError);
	Report(ctx, parseError, "BigInt ctor invalid error");

	const BigInteger hexValue = BigInteger::FromHex(String(PHANTASMA_LITERAL("0001")));
	Report(ctx, hexValue.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt FromHex 0001");

	const BigInteger hexValueUpper = BigInteger::FromHex(String(PHANTASMA_LITERAL("FF")));
	Report(ctx, hexValueUpper.ToString() == String(PHANTASMA_LITERAL("255")), "BigInt FromHex FF");

	const BigInteger hexValueLower = BigInteger::FromHex(String(PHANTASMA_LITERAL("ff")));
	Report(ctx, hexValueLower.ToString() == String(PHANTASMA_LITERAL("255")), "BigInt FromHex ff");

	// ToHex uses 32-bit word formatting; expected strings are fixed-width per word.
	Report(ctx, BigInteger(0).ToHex() == String(PHANTASMA_LITERAL("00000000")), "BigInt ToHex 0");
	Report(ctx, BigInteger(1).ToHex() == String(PHANTASMA_LITERAL("00000001")), "BigInt ToHex 1");
	Report(ctx, BigInteger(0x1234ABCD).ToHex() == String(PHANTASMA_LITERAL("1234abcd")), "BigInt ToHex 32-bit");
	Report(ctx, BigInteger((Int64)0x1122334455667788LL).ToHex() == String(PHANTASMA_LITERAL("1122334455667788")), "BigInt ToHex 64-bit");
}

void RunBigIntByteArrayTests(TestContext& ctx)
{
	// Byte arrays are little-endian; signed arrays include sign guards.
	const BigInteger n(0x12345678);
	const ByteArray expectedUnsigned = { (Byte)0x78, (Byte)0x56, (Byte)0x34, (Byte)0x12 };
	const ByteArray expectedSigned = { (Byte)0x78, (Byte)0x56, (Byte)0x34, (Byte)0x12, (Byte)0x00 };
	Report(ctx, n.ToUnsignedByteArray() == expectedUnsigned, "BigInt ToUnsignedByteArray 0x12345678");
	Report(ctx, n.ToSignedByteArray() == expectedSigned, "BigInt ToSignedByteArray 0x12345678");

	const BigInteger one(1);
	Report(ctx, one.ToSignedByteArray() == ByteArray{ (Byte)0x01, (Byte)0x00 }, "BigInt ToSignedByteArray 1");
	Report(ctx, BigInteger(0).ToSignedByteArray() == ByteArray{ (Byte)0x00 }, "BigInt ToSignedByteArray 0");

	const BigInteger negOne(-1);
	// Negative values are two's complement with trailing guard bytes (Phantasma/C# rule).
	Report(ctx, negOne.ToSignedByteArray() == ByteArray{ (Byte)0xFF, (Byte)0xFF, (Byte)0xFF }, "BigInt ToSignedByteArray -1");

	const BigInteger negTwo(-2);
	Report(ctx, negTwo.ToSignedByteArray() == ByteArray{ (Byte)0xFE, (Byte)0xFF, (Byte)0xFF }, "BigInt ToSignedByteArray -2");

	// Values that flip the sign-bit guard logic (sign bit not set after two's complement).
	const BigInteger neg255(-255);
	Report(ctx, neg255.ToSignedByteArray() == ByteArray{ (Byte)0x01, (Byte)0xFF, (Byte)0xFF }, "BigInt ToSignedByteArray -255");

	const BigInteger val128(128);
	Report(ctx, val128.ToSignedByteArray() == ByteArray{ (Byte)0x80, (Byte)0x00 }, "BigInt ToSignedByteArray 128");

	const BigInteger valSparse(0x01000000);
	const ByteArray expectedSparse = { (Byte)0x00, (Byte)0x00, (Byte)0x00, (Byte)0x01 };
	Report(ctx, valSparse.ToUnsignedByteArray() == expectedSparse, "BigInt ToUnsignedByteArray sparse");

	const ByteArray unsignedBytes = { (Byte)0x78, (Byte)0x56, (Byte)0x34, (Byte)0x12 };
	const BigInteger unsignedPos = BigInteger::FromUnsignedArray(unsignedBytes, true);
	Report(ctx, unsignedPos.ToString() == String(PHANTASMA_LITERAL("305419896")), "BigInt FromUnsignedArray +");

	const BigInteger unsignedNeg = BigInteger::FromUnsignedArray(unsignedBytes, false);
	Report(ctx, unsignedNeg.ToString() == String(PHANTASMA_LITERAL("-305419896")), "BigInt FromUnsignedArray -");

	const BigInteger unsignedPtr = BigInteger::FromUnsignedArray(unsignedBytes.data(), (int)unsignedBytes.size(), true);
	Report(ctx, unsignedPtr.ToString() == String(PHANTASMA_LITERAL("305419896")), "BigInt FromUnsignedArray ptr");

	const ByteArray signedOne = { (Byte)0x01, (Byte)0x00 };
	const ByteArray signedNegTwo = { (Byte)0xFE, (Byte)0xFF, (Byte)0xFF };
	const ByteArray signed128 = { (Byte)0x80, (Byte)0x00 };
	Report(ctx, BigInteger::FromSignedArray(signedOne).ToString() == String(PHANTASMA_LITERAL("1")), "BigInt FromSignedArray 1");
	Report(ctx, BigInteger::FromSignedArray(signedNegTwo).ToString() == String(PHANTASMA_LITERAL("-2")), "BigInt FromSignedArray -2");
	Report(ctx, BigInteger::FromSignedArray(signed128).ToString() == String(PHANTASMA_LITERAL("128")), "BigInt FromSignedArray 128");

	const BigInteger signedPtr = BigInteger::FromSignedArray(signedNegTwo.data(), (int)signedNegTwo.size());
	Report(ctx, signedPtr.ToString() == String(PHANTASMA_LITERAL("-2")), "BigInt FromSignedArray ptr");

	// Empty signed arrays should normalize to zero.
	const Byte* nullSigned = nullptr;
	const BigInteger signedEmpty = BigInteger::FromSignedArray(nullSigned, 0);
	Report(ctx, signedEmpty.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt FromSignedArray empty");

	Byte buffer[8] = {};
	const int byteCount = n.ToUnsignedByteArray(nullptr, 0);
	// Size-only call should report required length without writing.
	Report(ctx, byteCount == 4, "BigInt ToUnsignedByteArray size");
	const int written = n.ToUnsignedByteArray(buffer, (int)sizeof(buffer));
	Report(ctx, written == 4, "BigInt ToUnsignedByteArray write size");
	Report(ctx, memcmp(buffer, expectedUnsigned.data(), expectedUnsigned.size()) == 0, "BigInt ToUnsignedByteArray write bytes");

	ExpectThrowContains(ctx, "BigInt ToUnsignedByteArray short buffer", "invalid argument", [&]() {
		(void)n.ToUnsignedByteArray(buffer, 1);
	});

	const int zeroSize = BigInteger(0).ToUnsignedByteArray(nullptr, 0);
	Report(ctx, zeroSize == 0, "BigInt ToUnsignedByteArray size zero");
}

void RunBigIntBitHelperTests(TestContext& ctx)
{
	// GetBitLength is defined on magnitude; check a few boundary values.
	Report(ctx, BigInteger(0).GetBitLength() == 0, "BigInt GetBitLength 0");
	Report(ctx, BigInteger(1).GetBitLength() == 1, "BigInt GetBitLength 1");
	Report(ctx, BigInteger(2).GetBitLength() == 2, "BigInt GetBitLength 2");
	Report(ctx, BigInteger(3).GetBitLength() == 2, "BigInt GetBitLength 3");
	Report(ctx, BigInteger(4).GetBitLength() == 3, "BigInt GetBitLength 4");
	Report(ctx, BigInteger(255).GetBitLength() == 8, "BigInt GetBitLength 255");
	Report(ctx, BigInteger(256).GetBitLength() == 9, "BigInt GetBitLength 256");

	const BigInteger pattern(42);
	Report(ctx, !pattern.TestBit(0), "BigInt TestBit 42 bit0");
	Report(ctx, pattern.TestBit(1), "BigInt TestBit 42 bit1");
	Report(ctx, !pattern.TestBit(2), "BigInt TestBit 42 bit2");
	Report(ctx, pattern.TestBit(3), "BigInt TestBit 42 bit3");
	Report(ctx, !pattern.TestBit(4), "BigInt TestBit 42 bit4");
	Report(ctx, pattern.TestBit(5), "BigInt TestBit 42 bit5");

	Report(ctx, BigInteger(0).GetLowestSetBit() == -1, "BigInt GetLowestSetBit 0");
	Report(ctx, BigInteger(1).GetLowestSetBit() == 0, "BigInt GetLowestSetBit 1");
	Report(ctx, BigInteger(2).GetLowestSetBit() == 1, "BigInt GetLowestSetBit 2");
	Report(ctx, BigInteger(40).GetLowestSetBit() == 3, "BigInt GetLowestSetBit 40");
	Report(ctx, BigInteger(256).GetLowestSetBit() == 8, "BigInt GetLowestSetBit 256");
	Report(ctx, BigInteger(-1).GetLowestSetBit() == 0, "BigInt GetLowestSetBit -1");
	Report(ctx, BigInteger(-2).GetLowestSetBit() == 1, "BigInt GetLowestSetBit -2");

	Report(ctx, BigInteger(0).CalcIsEven(), "BigInt CalcIsEven 0");
	Report(ctx, !BigInteger(1).CalcIsEven(), "BigInt CalcIsEven 1");
	Report(ctx, BigInteger(2).CalcIsEven(), "BigInt CalcIsEven 2");
	Report(ctx, BigInteger(-2).CalcIsEven(), "BigInt CalcIsEven -2");

	Report(ctx, BigInteger(0).Sqrt().ToString() == String(PHANTASMA_LITERAL("0")), "BigInt Sqrt 0");
	Report(ctx, BigInteger(1).Sqrt().ToString() == String(PHANTASMA_LITERAL("1")), "BigInt Sqrt 1");
	Report(ctx, BigInteger(4).Sqrt().ToString() == String(PHANTASMA_LITERAL("2")), "BigInt Sqrt 4");
	Report(ctx, BigInteger(9).Sqrt().ToString() == String(PHANTASMA_LITERAL("3")), "BigInt Sqrt 9");
	Report(ctx, BigInteger(15).Sqrt().ToString() == String(PHANTASMA_LITERAL("3")), "BigInt Sqrt 15");

	const BigInteger wordsValue((Int64)0x1122334455667788LL);
	const auto words = wordsValue.ToUintArray();
	const bool wordsMatch = words.size() == 2 && words[0] == 0x55667788 && words[1] == 0x11223344;
	Report(ctx, wordsMatch, "BigInt ToUintArray 64-bit");

	const BigInteger bigShift = BigInteger(1) << 100;
	Report(ctx, bigShift.GetBitLength() == 101, "BigInt GetBitLength large");
	Report(ctx, bigShift.TestBit(100), "BigInt TestBit large on");
	Report(ctx, !bigShift.TestBit(99), "BigInt TestBit large off");

	ExpectThrowContains(ctx, "BigInt Sqrt negative", "cannot be negative", []() {
		(void)BigInteger(-1).Sqrt();
	});
}

void RunBigIntConstructorTests(TestContext& ctx)
{
	// Word/byte constructors are little-endian; verify sign flag handling.
	UInt32 words[2] = { 0x55667788, 0x11223344 };
	const BigInteger fromWords(words, 2, 1);
	Report(ctx, fromWords.ToString() == String(PHANTASMA_LITERAL("1234605616436508552")), "BigInt ctor words +");

	const BigInteger fromWordsNeg(words, 2, -1);
	Report(ctx, fromWordsNeg.ToString() == String(PHANTASMA_LITERAL("-1234605616436508552")), "BigInt ctor words -");

	const Byte rawBytes[4] = { 0x78, 0x56, 0x34, 0x12 };
	const BigInteger fromRaw(rawBytes, 4, 1);
	Report(ctx, fromRaw.ToString() == String(PHANTASMA_LITERAL("305419896")), "BigInt ctor bytes +");

	const BigInteger fromRawNeg(rawBytes, 4, -1);
	Report(ctx, fromRawNeg.ToString() == String(PHANTASMA_LITERAL("-305419896")), "BigInt ctor bytes -");

	// Non-4-byte input should pack bytes manually.
	const Byte rawBytes3[3] = { 0xAA, 0xBB, 0xCC };
	const BigInteger fromRaw3(rawBytes3, 3, 1);
	Report(ctx, fromRaw3.ToString() == String(PHANTASMA_LITERAL("13417386")), "BigInt ctor bytes 3");

	PHANTASMA_VECTOR<UInt32> buffer;
	buffer.push_back(0x55667788);
	buffer.push_back(0x11223344);
	const BigInteger fromBuffer(buffer, 1);
	Report(ctx, fromBuffer.ToString() == String(PHANTASMA_LITERAL("1234605616436508552")), "BigInt ctor buffer +");

	const BigInteger fromBufferNeg(buffer, -1);
	Report(ctx, fromBufferNeg.ToString() == String(PHANTASMA_LITERAL("-1234605616436508552")), "BigInt ctor buffer -");

	BigInteger fromMove(std::move(buffer), 1);
	Report(ctx, fromMove.ToString() == String(PHANTASMA_LITERAL("1234605616436508552")), "BigInt ctor buffer move");

	// All-zero input should normalize to zero.
	const UInt32 zeroWords[2] = { 0, 0 };
	const BigInteger zeroFromWords(zeroWords, 2, 1);
	Report(ctx, zeroFromWords.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt ctor words zero");

	// Trailing zero words should be trimmed during initialization.
	const UInt32 trimmedWords[3] = { 0xABCD1234, 0, 0 };
	const BigInteger trimmedFromWords(trimmedWords, 3, 1);
	Report(ctx, trimmedFromWords.ToString() == String(PHANTASMA_LITERAL("2882343476")), "BigInt ctor words trim");

	// Null word pointer with length 0 should normalize to zero.
	const UInt32* emptyWords = nullptr;
	const BigInteger emptyFromWords(emptyWords, 0, 1);
	Report(ctx, emptyFromWords.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt ctor words empty");

	// Empty byte vectors should normalize to zero.
	PHANTASMA_VECTOR<Byte> emptyBytes;
	const BigInteger emptyVec(emptyBytes);
	Report(ctx, emptyVec.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt ctor vector empty");

	// Signed byte vectors use two's complement (FF FE == -257).
	PHANTASMA_VECTOR<Byte> signedBytes;
	signedBytes.push_back((Byte)0xFF);
	signedBytes.push_back((Byte)0xFE);
	const BigInteger signedVec(signedBytes);
	Report(ctx, signedVec.ToString() == String(PHANTASMA_LITERAL("-257")), "BigInt ctor vector signed");

	// Vector inputs with 4-byte length should use the fast InitFromArray path.
	PHANTASMA_VECTOR<Byte> alignedBytes;
	alignedBytes.push_back((Byte)0x78);
	alignedBytes.push_back((Byte)0x56);
	alignedBytes.push_back((Byte)0x34);
	alignedBytes.push_back((Byte)0x12);
	const BigInteger alignedVec(alignedBytes);
	Report(ctx, alignedVec.ToString() == String(PHANTASMA_LITERAL("305419896")), "BigInt ctor vector aligned");

	// Move and assignment should preserve value.
	BigInteger moveSource(777);
	BigInteger moveTarget(std::move(moveSource));
	Report(ctx, moveTarget.ToString() == String(PHANTASMA_LITERAL("777")), "BigInt ctor move");

	BigInteger assignTarget;
	assignTarget = BigInteger(888);
	Report(ctx, assignTarget.ToString() == String(PHANTASMA_LITERAL("888")), "BigInt operator=");
}

void RunBigIntOperatorTests(TestContext& ctx)
{
	// Exercise compound operators and helpers with small deterministic values.
	BigInteger add(5);
	add += BigInteger(3);
	Report(ctx, add.ToString() == String(PHANTASMA_LITERAL("8")), "BigInt operator +=");

	BigInteger sub(5);
	sub -= BigInteger(8);
	Report(ctx, sub.ToString() == String(PHANTASMA_LITERAL("-3")), "BigInt operator -=");

	BigInteger mul(-2);
	mul *= BigInteger(4);
	Report(ctx, mul.ToString() == String(PHANTASMA_LITERAL("-8")), "BigInt operator *=");

	BigInteger div(-9);
	div /= BigInteger(2);
	Report(ctx, div.ToString() == String(PHANTASMA_LITERAL("-4")), "BigInt operator /=");

	BigInteger mod(-9);
	mod %= BigInteger(4);
	Report(ctx, mod.ToString() == String(PHANTASMA_LITERAL("-1")), "BigInt operator %=");

	BigInteger shl(1);
	shl <<= 5;
	Report(ctx, shl.ToString() == String(PHANTASMA_LITERAL("32")), "BigInt operator <<=");

	BigInteger shr(32);
	shr >>= 3;
	Report(ctx, shr.ToString() == String(PHANTASMA_LITERAL("4")), "BigInt operator >>=");

	const BigInteger shlWide = BigInteger(0x80000000) << 1;
	Report(ctx, shlWide.ToString() == String(PHANTASMA_LITERAL("4294967296")), "BigInt operator << wide");

	const BigInteger shlWord = BigInteger(1) << 32;
	Report(ctx, shlWord.ToString() == String(PHANTASMA_LITERAL("4294967296")), "BigInt operator << 32");

	const BigInteger shrLarge = BigInteger(1) >> 100;
	Report(ctx, shrLarge.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt operator >> large");

	// Arithmetic shift for negative values.
	const BigInteger shrNegative = BigInteger(-7) >> 1;
	Report(ctx, shrNegative.ToString() == String(PHANTASMA_LITERAL("-4")), "BigInt operator >> negative");

	// ShiftRight extra shrinkage when the MSD becomes zero after shifting.
	const BigInteger shrExtra = shlWord >> 1;
	Report(ctx, shrExtra.ToString() == String(PHANTASMA_LITERAL("2147483648")), "BigInt operator >> extra shrinkage");

	// Multiply with a zero low word to exercise the skip branch in Multiply.
	const BigInteger mulZeroWord = shlWord * BigInteger(123456);
	Report(ctx, mulZeroWord.ToString() == String(PHANTASMA_LITERAL("530239482494976")), "BigInt operator * zero word");

	// Shift-assign for negatives should round toward -inf (arithmetic shift).
	BigInteger shrAssignNeg(-7);
	shrAssignNeg >>= 1;
	Report(ctx, shrAssignNeg.ToString() == String(PHANTASMA_LITERAL("-4")), "BigInt operator >>= negative");

	// Shift-assign beyond bit length should normalize to zero.
	BigInteger shrAssignZero(1);
	shrAssignZero >>= 100;
	Report(ctx, shrAssignZero.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt operator >>= zero");

	// Zero/One factories should return normalized values.
	Report(ctx, BigInteger::Zero().ToString() == String(PHANTASMA_LITERAL("0")), "BigInt Zero()");
	Report(ctx, BigInteger::One().ToString() == String(PHANTASMA_LITERAL("1")), "BigInt One()");

	BigInteger inc(1);
	BigInteger postInc = inc++;
	Report(ctx, postInc.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt operator post++");
	Report(ctx, inc.ToString() == String(PHANTASMA_LITERAL("2")), "BigInt operator post++ value");

	BigInteger preInc = ++inc;
	Report(ctx, preInc.ToString() == String(PHANTASMA_LITERAL("3")), "BigInt operator pre++");
	Report(ctx, inc.ToString() == String(PHANTASMA_LITERAL("3")), "BigInt operator pre++ value");

	BigInteger dec(3);
	BigInteger postDec = dec--;
	Report(ctx, postDec.ToString() == String(PHANTASMA_LITERAL("3")), "BigInt operator post--");
	Report(ctx, dec.ToString() == String(PHANTASMA_LITERAL("2")), "BigInt operator post-- value");

	BigInteger preDec = --dec;
	Report(ctx, preDec.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt operator pre--");
	Report(ctx, dec.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt operator pre-- value");

	const BigInteger eqA(42);
	const BigInteger eqB(42);
	const BigInteger eqC(-42);
	Report(ctx, eqA.Equals(eqB), "BigInt Equals true");
	Report(ctx, !eqA.Equals(eqC), "BigInt Equals false");

	Report(ctx, BigInteger(5) < BigInteger(6), "BigInt operator <");
	Report(ctx, BigInteger(-2) < BigInteger(1), "BigInt operator < sign");
	Report(ctx, BigInteger(6) > BigInteger(5), "BigInt operator >");
	Report(ctx, BigInteger(5) <= BigInteger(5), "BigInt operator <=");
	Report(ctx, BigInteger(6) >= BigInteger(6), "BigInt operator >=");

	// CompareTo should return 0 for equal values.
	Report(ctx, BigInteger(7).CompareTo(BigInteger(7)) == 0, "BigInt CompareTo equal");

	// Subtraction should handle both-negative operands with |a| < |b|.
	Report(ctx, (BigInteger(-5) - BigInteger(-9)).ToString() == String(PHANTASMA_LITERAL("4")), "BigInt operator - both negative");

	const BigInteger modMethod = BigInteger(-7).Mod(BigInteger(5));
	Report(ctx, modMethod.ToString() == String(PHANTASMA_LITERAL("-2")), "BigInt Mod method");

	Report(ctx, BigInteger(0).FlipBit(0).ToString() == String(PHANTASMA_LITERAL("1")), "BigInt FlipBit 0->1");
	Report(ctx, BigInteger(1).FlipBit(0).ToString() == String(PHANTASMA_LITERAL("0")), "BigInt FlipBit 1->0");
	Report(ctx, BigInteger(2).FlipBit(1).ToString() == String(PHANTASMA_LITERAL("0")), "BigInt FlipBit 2 bit1");

	BigInteger quot;
	BigInteger rem;
	BigInteger::DivideAndModulus(BigInteger(20), BigInteger(6), quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("3")), "BigInt DivideAndModulus quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("2")), "BigInt DivideAndModulus rem");

	BigInteger::DivideAndModulus(BigInteger(3), BigInteger(10), quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt DivideAndModulus small quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("3")), "BigInt DivideAndModulus small rem");

	const BigInteger denomWide = BigInteger::Parse(String(PHANTASMA_LITERAL("18446744073709551616")));
	const BigInteger numerWide = BigInteger::Parse(String(PHANTASMA_LITERAL("4294967296")));
	BigInteger::DivideAndModulus(numerWide, denomWide, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt DivideAndModulus wide quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("4294967296")), "BigInt DivideAndModulus wide rem");

	const BigInteger numerBig = BigInteger::Parse(String(PHANTASMA_LITERAL("18446744073709551621")));
	const BigInteger denomBig = BigInteger::Parse(String(PHANTASMA_LITERAL("4294967296")));
	BigInteger::DivideAndModulus(numerBig, denomBig, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("4294967296")), "BigInt DivideAndModulus multi-digit quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("5")), "BigInt DivideAndModulus multi-digit rem");

	quot = BigInteger(99);
	rem = BigInteger(88);
	BigInteger::DivideAndModulus(BigInteger(5), BigInteger(0), quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt DivideAndModulus div0 quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt DivideAndModulus div0 rem");

	const BigInteger round1 = BigInteger::DivideAndRoundToClosest(BigInteger(5), BigInteger(2));
	const BigInteger round2 = BigInteger::DivideAndRoundToClosest(BigInteger(7), BigInteger(2));
	const BigInteger round3 = BigInteger::DivideAndRoundToClosest(BigInteger(-5), BigInteger(2));
	Report(ctx, round1.ToString() == String(PHANTASMA_LITERAL("3")), "BigInt DivideAndRoundToClosest 5/2");
	Report(ctx, round2.ToString() == String(PHANTASMA_LITERAL("4")), "BigInt DivideAndRoundToClosest 7/2");
	Report(ctx, round3.ToString() == String(PHANTASMA_LITERAL("-2")), "BigInt DivideAndRoundToClosest -5/2");

	const BigInteger modPowStatic = BigInteger::ModPow(BigInteger(2), BigInteger(5), BigInteger(13));
	Report(ctx, modPowStatic.ToString() == String(PHANTASMA_LITERAL("6")), "BigInt ModPow static");

	const BigInteger modPowZero = BigInteger::ModPow(BigInteger(5), BigInteger(0), BigInteger(13));
	Report(ctx, modPowZero.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt ModPow static exp0");

	// Pow uses a simple loop; non-positive exponent yields 1 in this implementation.
	const BigInteger powZero = BigInteger::Pow(BigInteger(123), BigInteger(0));
	Report(ctx, powZero.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt Pow exp0");

	const BigInteger powNegative = BigInteger::Pow(BigInteger(123), BigInteger(-1));
	Report(ctx, powNegative.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt Pow exp negative");

	const int intValue = (int)BigInteger(12345);
	Report(ctx, intValue == 12345, "BigInt operator int +");

	const int intValueNeg = (int)BigInteger(-12345);
	Report(ctx, intValueNeg == -12345, "BigInt operator int -");

	const Int64 longValue = (Int64)BigInteger((Int64)-9876543210LL);
	Report(ctx, longValue == -9876543210LL, "BigInt operator Int64");

	const BigInteger hashRef(123456);
	const int hashA = hashRef.GetHashCode();
	const int hashB = BigInteger(123456).GetHashCode();
	Report(ctx, hashA == hashB, "BigInt GetHashCode stable");
}

void RunSecureBigIntTests(TestContext& ctx)
{
	// Exercise SecureBigInteger to cover UseSecureMemory code paths in the template.
	SecureBigInteger a(12345);
	SecureBigInteger b(-6789);
	Report(ctx, a.ToString() == String(PHANTASMA_LITERAL("12345")), "SecureBigInt ToString");

	const SecureBigInteger sum = a + b;
	Report(ctx, sum.ToString() == String(PHANTASMA_LITERAL("5556")), "SecureBigInt add");

	// Secure addition/subtraction should cover negative operand branches.
	const SecureBigInteger negA(-50);
	const SecureBigInteger negB(-20);
	Report(ctx, (negA + negB).ToString() == String(PHANTASMA_LITERAL("-70")), "SecureBigInt add negative");
	Report(ctx, (negA - negB).ToString() == String(PHANTASMA_LITERAL("-30")), "SecureBigInt sub negative");

	const SecureBigInteger negSmall(-5);
	const SecureBigInteger posBig(20);
	Report(ctx, (negSmall + posBig).ToString() == String(PHANTASMA_LITERAL("15")), "SecureBigInt add mixed");
	Report(ctx, (posBig - negSmall).ToString() == String(PHANTASMA_LITERAL("25")), "SecureBigInt sub mixed");

	// Exercise remaining sign/magnitude branches in + and - (neg dominates or pos<abs(neg)).
	Report(ctx, (SecureBigInteger(-30) + SecureBigInteger(5)).ToString() == String(PHANTASMA_LITERAL("-25")), "SecureBigInt add neg dominates");
	Report(ctx, (SecureBigInteger(5) + SecureBigInteger(-30)).ToString() == String(PHANTASMA_LITERAL("-25")), "SecureBigInt add pos<abs(neg)");
	Report(ctx, (SecureBigInteger(-5) - SecureBigInteger(-20)).ToString() == String(PHANTASMA_LITERAL("15")), "SecureBigInt sub neg abs");
	Report(ctx, (SecureBigInteger(-10) - SecureBigInteger(3)).ToString() == String(PHANTASMA_LITERAL("-13")), "SecureBigInt sub neg-pos");
	Report(ctx, (SecureBigInteger(3) - SecureBigInteger(10)).ToString() == String(PHANTASMA_LITERAL("-7")), "SecureBigInt sub pos-pos");

	const SecureBigInteger prod = a * SecureBigInteger(2);
	Report(ctx, prod.ToString() == String(PHANTASMA_LITERAL("24690")), "SecureBigInt mul");

	const SecureBigInteger div = a / SecureBigInteger(10);
	Report(ctx, div.ToString() == String(PHANTASMA_LITERAL("1234")), "SecureBigInt div");

	const SecureBigInteger mod = a % SecureBigInteger(10);
	Report(ctx, mod.ToString() == String(PHANTASMA_LITERAL("5")), "SecureBigInt mod");

	const SecureBigInteger shifted = (a << 40) >> 40;
	Report(ctx, shifted.ToString() == String(PHANTASMA_LITERAL("12345")), "SecureBigInt shift roundtrip");

	// ShiftRight with extra shrinkage and loop work for secure storage.
	const SecureBigInteger secShiftBase = (SecureBigInteger(1) << 64) + (SecureBigInteger(1) << 32);
	const SecureBigInteger secShift = secShiftBase >> 1;
	Report(ctx, secShift.ToString() == String(PHANTASMA_LITERAL("9223372039002259456")), "SecureBigInt shift extra shrinkage");

	// Secure shift-assign on negative values should round toward -inf.
	SecureBigInteger secShiftNeg(-7);
	secShiftNeg >>= 1;
	Report(ctx, secShiftNeg.ToString() == String(PHANTASMA_LITERAL("-4")), "SecureBigInt >>= negative");

	// Secure shift-assign beyond bit length should normalize to zero.
	SecureBigInteger secShiftZero(1);
	secShiftZero >>= 100;
	Report(ctx, secShiftZero.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt >>= zero");

	// Secure Zero/One factories should return normalized values.
	Report(ctx, SecureBigInteger::Zero().ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt Zero()");
	Report(ctx, SecureBigInteger::One().ToString() == String(PHANTASMA_LITERAL("1")), "SecureBigInt One()");

	const ByteArray signedBytes = a.ToSignedByteArray();
	Report(ctx, !signedBytes.empty(), "SecureBigInt ToSignedByteArray");

	Byte buffer[16] = {};
	const int written = a.ToUnsignedByteArray(buffer, (int)sizeof(buffer));
	Report(ctx, written > 0, "SecureBigInt ToUnsignedByteArray");

	PHANTASMA_VECTOR<Byte> bytes = { (Byte)0x01, (Byte)0x00 };
	const SecureBigInteger fromBytes(bytes);
	Report(ctx, fromBytes.ToString() == String(PHANTASMA_LITERAL("1")), "SecureBigInt ctor bytes");

	// Empty vector inputs should normalize to zero.
	PHANTASMA_VECTOR<Byte> emptyVec;
	const SecureBigInteger emptyVector(emptyVec);
	Report(ctx, emptyVector.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt ctor vector empty");

	// SecureVector constructor path (two's complement for negative values).
	SecureVector<Byte> secureBytes;
	secureBytes.resize(3);
	secureBytes[0] = (Byte)0xFF;
	secureBytes[1] = (Byte)0xFF;
	secureBytes[2] = (Byte)0xFF;
	const SecureBigInteger fromSecureBytes(secureBytes);
	Report(ctx, fromSecureBytes.ToString() == String(PHANTASMA_LITERAL("-1")), "SecureBigInt ctor secure bytes");

	// Empty secure byte vectors should normalize to zero.
	SecureVector<Byte> emptySecure;
	const SecureBigInteger emptySecureBytes(emptySecure);
	Report(ctx, emptySecureBytes.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt ctor secure empty");

	// Signed byte vectors use two's complement (FF FE == -257).
	PHANTASMA_VECTOR<Byte> signedVec;
	signedVec.push_back((Byte)0xFF);
	signedVec.push_back((Byte)0xFE);
	const SecureBigInteger fromSignedVec(signedVec);
	Report(ctx, fromSignedVec.ToString() == String(PHANTASMA_LITERAL("-257")), "SecureBigInt ctor vector signed");

	// 4-byte vector input should take the aligned InitFromArray path.
	PHANTASMA_VECTOR<Byte> alignedVec;
	alignedVec.push_back((Byte)0x78);
	alignedVec.push_back((Byte)0x56);
	alignedVec.push_back((Byte)0x34);
	alignedVec.push_back((Byte)0x12);
	const SecureBigInteger alignedVector(alignedVec);
	Report(ctx, alignedVector.ToString() == String(PHANTASMA_LITERAL("305419896")), "SecureBigInt ctor vector aligned");

	// 4-byte vector input should take the aligned InitFromArray path.
	SecureVector<Byte> alignedSecure;
	alignedSecure.resize(4);
	alignedSecure[0] = (Byte)0x78;
	alignedSecure[1] = (Byte)0x56;
	alignedSecure[2] = (Byte)0x34;
	alignedSecure[3] = (Byte)0x12;
	const SecureBigInteger alignedSecureBytes(alignedSecure);
	Report(ctx, alignedSecureBytes.ToString() == String(PHANTASMA_LITERAL("305419896")), "SecureBigInt ctor secure aligned");

	SecureBigInteger assigned;
	assigned = a;
	Report(ctx, assigned.ToString() == String(PHANTASMA_LITERAL("12345")), "SecureBigInt operator=");

	SecureBigInteger moved(std::move(assigned));
	Report(ctx, moved.ToString() == String(PHANTASMA_LITERAL("12345")), "SecureBigInt ctor move");

	const SecureBigInteger zero = a - a;
	Report(ctx, zero.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt zero");

	const SecureBigInteger modPow = SecureBigInteger::ModPow(SecureBigInteger(2), SecureBigInteger(5), SecureBigInteger(13));
	Report(ctx, modPow.ToString() == String(PHANTASMA_LITERAL("6")), "SecureBigInt ModPow");

	// DivideAndModulus should handle div-by-zero and a<b cases.
	SecureBigInteger divQuot;
	SecureBigInteger divRem;
	SecureBigInteger::DivideAndModulus(SecureBigInteger(5), SecureBigInteger(0), divQuot, divRem);
	Report(ctx, divQuot.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt div0 quot");
	Report(ctx, divRem.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt div0 rem");

	SecureBigInteger::DivideAndModulus(SecureBigInteger(3), SecureBigInteger(10), divQuot, divRem);
	Report(ctx, divQuot.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt div small quot");
	Report(ctx, divRem.ToString() == String(PHANTASMA_LITERAL("3")), "SecureBigInt div small rem");

	// Secure string constructor should auto-size input and accept negatives.
	bool secError = false;
	const SecureBigInteger secFromChars(PHANTASMA_LITERAL("123"), 0, 10, &secError);
	Report(ctx, secFromChars.ToString() == String(PHANTASMA_LITERAL("123")), "SecureBigInt ctor chars");
	Report(ctx, !secError, "SecureBigInt ctor chars error");

	secError = false;
	const SecureBigInteger secTrimmed(PHANTASMA_LITERAL("\n-77\r"), 0, 10, &secError);
	Report(ctx, secTrimmed.ToString() == String(PHANTASMA_LITERAL("-77")), "SecureBigInt ctor chars trim");
	Report(ctx, !secError, "SecureBigInt ctor chars trim error");

	bool secEmptyError = false;
	const SecureBigInteger secEmpty(PHANTASMA_LITERAL(""), 0, 10, &secEmptyError);
	Report(ctx, secEmpty.ToString() == String(PHANTASMA_LITERAL("0")), "SecureBigInt ctor chars empty");
	Report(ctx, !secEmptyError, "SecureBigInt ctor chars empty error");

	// Invalid chars should set out_error; exceptions may be disabled in this build.
	bool secInvalidError = false;
#ifdef PHANTASMA_EXCEPTION_ENABLE
	ExpectThrowContains(ctx, "SecureBigInt ctor chars invalid", "Invalid string", [&]() {
		SecureBigInteger invalid(PHANTASMA_LITERAL("12Z"), 0, 10, &secInvalidError);
	});
	Report(ctx, secInvalidError, "SecureBigInt ctor chars invalid error");
#else
	SecureBigInteger invalid(PHANTASMA_LITERAL("12Z"), 0, 10, &secInvalidError);
	Report(ctx, secInvalidError, "SecureBigInt ctor chars invalid error (no exceptions)");
#endif

	// Multi-word division to exercise MultiDigitDivMod on the secure path.
	const SecureBigInteger secNumer = SecureBigInteger::Parse(String(PHANTASMA_LITERAL("1118186285554272804292416")));
	const SecureBigInteger secDen = SecureBigInteger::Parse(String(PHANTASMA_LITERAL("260348032599872")));
	SecureBigInteger secQuot;
	SecureBigInteger secRem;
	SecureBigInteger::DivideAndModulus(secNumer, secDen, secQuot, secRem);
	Report(ctx, secQuot.ToString() == String(PHANTASMA_LITERAL("4294967295")), "SecureBigInt multi div quot");
	Report(ctx, secRem.ToString() == String(PHANTASMA_LITERAL("220228743106176")), "SecureBigInt multi div rem");
}

void RunBigIntMultiWordTests(TestContext& ctx)
{
	// Multi-word fixtures computed via Python big ints to hit deeper arithmetic branches.
	const BigInteger A = BigInteger::Parse(String(PHANTASMA_LITERAL("340282366920938463481821351505477763073")));
	const BigInteger B = BigInteger::Parse(String(PHANTASMA_LITERAL("79228162514264337597838917633")));
	Report(ctx, (A + B).ToString() == String(PHANTASMA_LITERAL("340282367000166625996085689103316680706")), "BigInt multi A+B");
	Report(ctx, (A - B).ToString() == String(PHANTASMA_LITERAL("340282366841710300967557013907638845440")), "BigInt multi A-B");
	Report(ctx, (A * B).ToString() == String(PHANTASMA_LITERAL("26959946667150639797590018362021718877123908876483488547112335966209")), "BigInt multi A*B");

	BigInteger quot;
	BigInteger rem;
	BigInteger::DivideAndModulus(A, B, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("4294967295")), "BigInt multi A/B quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("79228162514264337593543950338")), "BigInt multi A/B rem");

	const BigInteger C = BigInteger::Parse(String(PHANTASMA_LITERAL("1461501637330902918203686041642102634285107261497")));
	const BigInteger D = BigInteger::Parse(String(PHANTASMA_LITERAL("1329227995784915872903808159792026673")));
	Report(ctx, (C + D).ToString() == String(PHANTASMA_LITERAL("1461501637332232146199470957515006442444899288170")), "BigInt multi C+D");
	Report(ctx, (C - D).ToString() == String(PHANTASMA_LITERAL("1461501637329573690207901125769198826125315234824")), "BigInt multi C-D");
	Report(ctx, (C * D).ToString() == String(PHANTASMA_LITERAL("1942668892225729070919465120699686814853399391364392781967056861672504391794609909481")), "BigInt multi C*D");

	BigInteger::DivideAndModulus(C, D, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("1099511627775")), "BigInt multi C/D quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("1329227995784915872844081588659618922")), "BigInt multi C/D rem");

	const BigInteger E = BigInteger::Parse(String(PHANTASMA_LITERAL("79228162514264337593543950459")));
	const BigInteger F = BigInteger::Parse(String(PHANTASMA_LITERAL("340282366920938463463374607431768211912")));
	BigInteger::DivideAndModulus(E, F, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("0")), "BigInt multi E/F quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("79228162514264337593543950459")), "BigInt multi E/F rem");

	const BigInteger G = BigInteger::Parse(String(PHANTASMA_LITERAL("18446744073709551615")));
	const BigInteger H = BigInteger::Parse(String(PHANTASMA_LITERAL("4294967311")));
	BigInteger::DivideAndModulus(G, H, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("4294967281")), "BigInt multi G/H quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("224")), "BigInt multi G/H rem");

	// Crafted to trigger qHat correction and estimation loop in MultiDigitDivMod.
	const BigInteger I = BigInteger::Parse(String(PHANTASMA_LITERAL("1118186285554272804292416")));
	const BigInteger J = BigInteger::Parse(String(PHANTASMA_LITERAL("260348032599872")));
	BigInteger::DivideAndModulus(I, J, quot, rem);
	Report(ctx, quot.ToString() == String(PHANTASMA_LITERAL("4294967295")), "BigInt multi I/J quot");
	Report(ctx, rem.ToString() == String(PHANTASMA_LITERAL("220228743106176")), "BigInt multi I/J rem");

	Report(ctx, (A >> 73).ToString() == String(PHANTASMA_LITERAL("36028797018963968")), "BigInt multi A>>73");
	Report(ctx, (A << 19).ToString() == String(PHANTASMA_LITERAL("178405961588244985141957152738103925446017024")), "BigInt multi A<<19");
}

} // namespace testcases
