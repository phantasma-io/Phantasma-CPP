#pragma once

namespace phantasma {

enum class Opcode
{
	NOP,

	// register
	MOVE,    // copy reference
	COPY,   // copy by value
	PUSH,
	POP,
	SWAP,

	// flow
	CALL,
	EXTCALL,
	JMP,
	JMPIF,
	JMPNOT,
	RET,
	THROW,

	// data
	LOAD,
	CAST,
	CAT,
	RANGE,
	LEFT,
	RIGHT,
	SIZE,
	COUNT,

	// logical
	NOT,
	AND,
	OR,
	XOR,
	EQUAL,
	LT,
	GT,
	LTE,
	GTE,

	// numeric
	INC,
	DEC,
	SIGN,
	NEGATE,
	ABS,
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	SHL,
	SHR,
	MIN,
	MAX,
	POW,

	// context
	CTX,
	SWITCH,

	// array
	PUT,
	GET, // lookups a key and copies a reference into register
	CLEAR, // clears a register
	UNPACK, // unpacks serialized struct based on ref struct
	PACK, // unused for now

	//  debugger
	DEBUG_ // underscore because projects often have a #define DEBUG
};

inline UInt64 OpcodeGasCost(Opcode opcode)
{
	switch (opcode)
	{
	case Opcode::GET:
	case Opcode::PUT:
	case Opcode::CALL:
	case Opcode::LOAD:
		return 5;

	case Opcode::EXTCALL:
	case Opcode::CTX:
		return 10;

	case Opcode::SWITCH:
		return 100;

	case Opcode::NOP:
	case Opcode::RET:
		return 0;

	default: return 1;
	}
}

inline const Char* ToString(Opcode opcode)
{
	switch(opcode)
	{
	case Opcode::NOP:	  return PHANTASMA_LITERAL("NOP");
	case Opcode::MOVE:	  return PHANTASMA_LITERAL("MOVE");
	case Opcode::COPY:	  return PHANTASMA_LITERAL("COPY");
	case Opcode::PUSH:	  return PHANTASMA_LITERAL("PUSH");
	case Opcode::POP:	  return PHANTASMA_LITERAL("POP");
	case Opcode::SWAP:	  return PHANTASMA_LITERAL("SWAP");
	case Opcode::CALL:	  return PHANTASMA_LITERAL("CALL");
	case Opcode::EXTCALL: return PHANTASMA_LITERAL("EXTCALL");
	case Opcode::JMPIF:	  return PHANTASMA_LITERAL("JMPIF");
	case Opcode::JMPNOT:  return PHANTASMA_LITERAL("JMPNOT");
	case Opcode::JMP:	  return PHANTASMA_LITERAL("JMP");
	case Opcode::RET:	  return PHANTASMA_LITERAL("RET");
	case Opcode::THROW:	  return PHANTASMA_LITERAL("THROW");
	case Opcode::LOAD:	  return PHANTASMA_LITERAL("LOAD");
	case Opcode::CAST:	  return PHANTASMA_LITERAL("CAST");
	case Opcode::CAT:	  return PHANTASMA_LITERAL("CAT");
	case Opcode::RANGE:	  return PHANTASMA_LITERAL("RANGE");
	case Opcode::LEFT:	  return PHANTASMA_LITERAL("LEFT");
	case Opcode::RIGHT:	  return PHANTASMA_LITERAL("RIGHT");
	case Opcode::SIZE:	  return PHANTASMA_LITERAL("SIZE");
	case Opcode::COUNT:	  return PHANTASMA_LITERAL("COUNT");
	case Opcode::NOT:	  return PHANTASMA_LITERAL("NOT");
	case Opcode::AND:	  return PHANTASMA_LITERAL("AND");
	case Opcode::OR:	  return PHANTASMA_LITERAL("OR");
	case Opcode::XOR:	  return PHANTASMA_LITERAL("XOR");
	case Opcode::EQUAL:	  return PHANTASMA_LITERAL("EQUAL");
	case Opcode::LT:	  return PHANTASMA_LITERAL("LT");
	case Opcode::GT:	  return PHANTASMA_LITERAL("GT");
	case Opcode::LTE:	  return PHANTASMA_LITERAL("LTE");
	case Opcode::GTE:	  return PHANTASMA_LITERAL("GTE");
	case Opcode::INC:	  return PHANTASMA_LITERAL("INC");
	case Opcode::DEC:	  return PHANTASMA_LITERAL("DEC");
	case Opcode::SIGN:	  return PHANTASMA_LITERAL("SIGN");
	case Opcode::NEGATE:  return PHANTASMA_LITERAL("NEGATE");
	case Opcode::ABS:	  return PHANTASMA_LITERAL("ABS");
	case Opcode::ADD:	  return PHANTASMA_LITERAL("ADD");
	case Opcode::SUB:	  return PHANTASMA_LITERAL("SUB");
	case Opcode::MUL:	  return PHANTASMA_LITERAL("MUL");
	case Opcode::DIV:	  return PHANTASMA_LITERAL("DIV");
	case Opcode::MOD:	  return PHANTASMA_LITERAL("MOD");
	case Opcode::SHL:	  return PHANTASMA_LITERAL("SHL");
	case Opcode::SHR:	  return PHANTASMA_LITERAL("SHR");
	case Opcode::MIN:	  return PHANTASMA_LITERAL("MIN");
	case Opcode::MAX:	  return PHANTASMA_LITERAL("MAX");
	case Opcode::POW:	  return PHANTASMA_LITERAL("POW");
	case Opcode::CTX:	  return PHANTASMA_LITERAL("CTX");
	case Opcode::SWITCH:  return PHANTASMA_LITERAL("SWITCH");
	case Opcode::PUT:	  return PHANTASMA_LITERAL("PUT");
	case Opcode::GET:	  return PHANTASMA_LITERAL("GET");
	case Opcode::CLEAR:	  return PHANTASMA_LITERAL("CLEAR");
	case Opcode::UNPACK:  return PHANTASMA_LITERAL("UNPACK");
	case Opcode::PACK:	  return PHANTASMA_LITERAL("PACK");
	case Opcode::DEBUG_:  return PHANTASMA_LITERAL("DEBUG");
	}
	return 0;
}

}
