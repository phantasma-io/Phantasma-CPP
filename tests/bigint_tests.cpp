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

	PHANTASMA_VECTOR<UInt32> buffer;
	buffer.push_back(0x55667788);
	buffer.push_back(0x11223344);
	const BigInteger fromBuffer(buffer, 1);
	Report(ctx, fromBuffer.ToString() == String(PHANTASMA_LITERAL("1234605616436508552")), "BigInt ctor buffer +");

	const BigInteger fromBufferNeg(buffer, -1);
	Report(ctx, fromBufferNeg.ToString() == String(PHANTASMA_LITERAL("-1234605616436508552")), "BigInt ctor buffer -");

	BigInteger fromMove(std::move(buffer), 1);
	Report(ctx, fromMove.ToString() == String(PHANTASMA_LITERAL("1234605616436508552")), "BigInt ctor buffer move");
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

} // namespace testcases
