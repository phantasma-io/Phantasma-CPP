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
	Report(ctx, !tryParseInvalid, "BigInt TryParse invalid");

	const BigInteger hexValue = BigInteger::FromHex(String(PHANTASMA_LITERAL("0001")));
	Report(ctx, hexValue.ToString() == String(PHANTASMA_LITERAL("1")), "BigInt FromHex 0001");

	const BigInteger hexValueUpper = BigInteger::FromHex(String(PHANTASMA_LITERAL("FF")));
	Report(ctx, hexValueUpper.ToString() == String(PHANTASMA_LITERAL("255")), "BigInt FromHex FF");

	const BigInteger hexValueLower = BigInteger::FromHex(String(PHANTASMA_LITERAL("ff")));
	Report(ctx, hexValueLower.ToString() == String(PHANTASMA_LITERAL("255")), "BigInt FromHex ff");

	Report(ctx, BigInteger(0).ToHex() == String(PHANTASMA_LITERAL("00000000")), "BigInt ToHex 0");
	Report(ctx, BigInteger(1).ToHex() == String(PHANTASMA_LITERAL("00000001")), "BigInt ToHex 1");
	Report(ctx, BigInteger(0x1234ABCD).ToHex() == String(PHANTASMA_LITERAL("1234abcd")), "BigInt ToHex 32-bit");
	Report(ctx, BigInteger((Int64)0x1122334455667788LL).ToHex() == String(PHANTASMA_LITERAL("1122334455667788")), "BigInt ToHex 64-bit");
}

void RunBigIntByteArrayTests(TestContext& ctx)
{
	const BigInteger n(0x12345678);
	const ByteArray expectedUnsigned = { (Byte)0x78, (Byte)0x56, (Byte)0x34, (Byte)0x12 };
	const ByteArray expectedSigned = { (Byte)0x78, (Byte)0x56, (Byte)0x34, (Byte)0x12, (Byte)0x00 };
	Report(ctx, n.ToUnsignedByteArray() == expectedUnsigned, "BigInt ToUnsignedByteArray 0x12345678");
	Report(ctx, n.ToSignedByteArray() == expectedSigned, "BigInt ToSignedByteArray 0x12345678");

	const BigInteger one(1);
	Report(ctx, one.ToSignedByteArray() == ByteArray{ (Byte)0x01, (Byte)0x00 }, "BigInt ToSignedByteArray 1");
	Report(ctx, BigInteger(0).ToSignedByteArray() == ByteArray{ (Byte)0x00 }, "BigInt ToSignedByteArray 0");

	const BigInteger negOne(-1);
	Report(ctx, negOne.ToSignedByteArray() == ByteArray{ (Byte)0xFF, (Byte)0xFF, (Byte)0xFF }, "BigInt ToSignedByteArray -1");

	const BigInteger negTwo(-2);
	Report(ctx, negTwo.ToSignedByteArray() == ByteArray{ (Byte)0xFE, (Byte)0xFF, (Byte)0xFF }, "BigInt ToSignedByteArray -2");

	const BigInteger val128(128);
	Report(ctx, val128.ToSignedByteArray() == ByteArray{ (Byte)0x80, (Byte)0x00 }, "BigInt ToSignedByteArray 128");

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
	Report(ctx, byteCount == 4, "BigInt ToUnsignedByteArray size");
	const int written = n.ToUnsignedByteArray(buffer, (int)sizeof(buffer));
	Report(ctx, written == 4, "BigInt ToUnsignedByteArray write size");
	Report(ctx, memcmp(buffer, expectedUnsigned.data(), expectedUnsigned.size()) == 0, "BigInt ToUnsignedByteArray write bytes");

	const int zeroSize = BigInteger(0).ToUnsignedByteArray(nullptr, 0);
	Report(ctx, zeroSize == 0, "BigInt ToUnsignedByteArray size zero");
}

void RunBigIntBitHelperTests(TestContext& ctx)
{
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
}

} // namespace testcases
