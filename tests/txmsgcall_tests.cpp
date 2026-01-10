#include "test_cases.h"

namespace testcases {
using namespace testutil;

void RunCallSectionsTests(TestContext& ctx)
{
	Allocator alloc;
	Blockchain::MsgCallArgs sections[2];
	sections[0].registerOffset = -1;
	sections[0].args = {};
	const ByteArray argsData{ (Byte)0x0A, (Byte)0x0B };
	sections[1].registerOffset = 0;
	sections[1].args = ByteView{ argsData.data(), argsData.size() };

	Blockchain::TxMsgCall call{};
	call.moduleId = 1;
	call.methodId = 2;
	call.sections.numArgSections_negative = -2;
	call.sections.argSections = sections;

	ByteArray buf;
	WriteView w(buf);
	Write(call, w);
	const std::string expected = "0100000002000000FEFFFFFFFFFFFFFF020000000A0B";
	const std::string got = ToUpper(BytesToHex(buf));
	Report(ctx, got == expected, "CALL-ARG-SECTIONS-ENCODE", got + " vs " + expected);

	ReadView r(buf.empty() ? nullptr : (void*)buf.data(), buf.size());
	Blockchain::TxMsgCall decoded{};
	const bool decodedOk =
		Read(decoded, r, alloc) &&
		decoded.sections.numArgSections_negative == -2 &&
		decoded.args.length == 0 &&
		decoded.moduleId == call.moduleId &&
		decoded.methodId == call.methodId &&
		decoded.sections.argSections &&
		decoded.sections.argSections[0].registerOffset == -1 &&
		decoded.sections.argSections[0].args.length == 0 &&
		decoded.sections.argSections[1].registerOffset == 0 &&
		ByteArray(decoded.sections.argSections[1].args.bytes, decoded.sections.argSections[1].args.bytes + decoded.sections.argSections[1].args.length) == ByteArray({ (Byte)0x0A, (Byte)0x0B });
	Report(ctx, decodedOk, "CALL-ARG-SECTIONS-DECODE");
}

} // namespace testcases
