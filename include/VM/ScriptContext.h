#pragma once
#include "VMObject.h"
#include "VirtualMachine.h"
#include "Opcodes.h"
#include "../Utils/BinaryReader.h"

#if defined PHANTASMA_EXCEPTION_ENABLE // script VM requires exceptions
namespace phantasma {

inline void Expect(bool condition, const char* msg)
{
	if(!condition)
		PHANTASMA_EXCEPTION_MESSAGE("expectation", msg);
}

class ExecutionFrame
{
public:
	ExecutionFrame(ExecutionContext& c, UInt32 offset)
		: m_context(c)
		, m_offset(offset)
	{}

	VMObject&         Register(int i) { Expect(i >= 0 && i < VirtualMachine::MaxRegisterCount, "invalid register"); return m_regs[i]; }
	ExecutionContext& Context() const { return m_context; }
	UInt32            Offset() const { return m_offset; }
private:
	ExecutionContext& m_context;
	ExecutionState m_state = ExecutionState::Running;
	const UInt32   m_offset; // Current instruction pointer **before** the frame was entered.
	VMObject       m_regs[VirtualMachine::MaxRegisterCount];
};

class ExecutionContext
{
public:
	ExecutionContext(const String& name, const ByteArray& script, UInt32 offset, VirtualMachine::FnInterop builtIn={}, int instructionLimit=0)
		: m_name(name)
		, m_script(script)
		, m_reader(m_script, offset)
		, m_builtIn(builtIn)
		, m_instructionLimit(instructionLimit)
	{}

	ExecutionContext(const ExecutionContext& o)
		: m_name(o.m_name)
		, m_script(o.m_script)
		, m_reader(m_script, o.m_reader.Position())
		, m_builtIn(o.m_builtIn)
		, m_instructionLimit(o.m_instructionLimit)
	{
	}
	ExecutionContext& operator=(const ExecutionContext& o)=delete;

	const String& Name() const { return m_name; }
	Address GetAddress() const
	{
		return Address::FromHash(m_name);
	}
	ExecutionState Execute(VirtualMachine* vm, ExecutionFrame* frame, VMStack& stack);
private:
	void StepOpcode(VirtualMachine* vm, ExecutionFrame& frame, VMStack& stack);

	ExecutionState            m_state = ExecutionState::Running;
	String                    m_name;
	//Address                   m_address;
	//const ByteArray&          m_script;//todo use byte view
	ByteArray                 m_script;
	BinaryReader              m_reader;
	VirtualMachine::FnInterop m_builtIn;

	const int m_instructionLimit = 0;
	int m_instructionCount = 0;
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

inline Address PopAddress(VMStack& stack)
{
	PHANTASMA_PROFILE(PopAddress);
	Expect(!stack.empty(), "empty stack");
	Address v;
	if( stack.back().Type() == VMType::String )
	{
		auto text = stack.back().AsString();
		stack.pop_back();
		bool error = false;
		v = Address::FromText(text.c_str(), &error);
		if( error )
		{
			String exmsg = "unsupported address: " + text;
			//todo - look up name!
			PHANTASMA_EXCEPTION_MESSAGE("unsupported address", exmsg);
		}
	}
	else
	{
		auto bytes = stack.back().AsByteArray();
		stack.pop_back();
		if(bytes.empty())
			v = Address();
		else if(bytes.size() == Address::LengthInBytes + 1)
		{
			BinaryReader r(bytes);
			v.UnserializeData(r);
		}
		else
			v = Address(bytes);
	}
	return v;
}
inline String PopString(VMStack& stack)
{
	PHANTASMA_PROFILE(PopString);
	Expect(!stack.empty(), "empty stack");
	auto v = stack.back().AsString();
	stack.pop_back();
	return v;
}
inline BigInteger PopNumber(VMStack& stack)
{
	PHANTASMA_PROFILE(PopNumber);
	Expect(!stack.empty(), "empty stack");
	auto v = stack.back().AsNumber();
	stack.pop_back();
	return v;
}
inline ByteArray PopBytes(VMStack& stack)
{
	PHANTASMA_PROFILE(PopBytes);
	Expect(!stack.empty(), "empty stack");
	auto v = stack.back().AsByteArray();
	stack.pop_back();
	return v;
}
inline Hash PopHash(VMStack& stack)
{
	PHANTASMA_PROFILE(PopHash);
	Expect(!stack.empty(), "empty stack");
	auto v = stack.back().AsHash();
	stack.pop_back();
	return v;
}
inline Timestamp PopTimestamp(VMStack& stack)
{
	PHANTASMA_PROFILE(PopTimestamp);
	Expect(!stack.empty(), "empty stack");
	auto v = stack.back().AsTimestamp();
	stack.pop_back();
	return v;
}

//------------------------------------------------------------------------------
inline VirtualMachine::VirtualMachine(const ByteArray& script, UInt32 offset, const String& contextName, const FnOpcode& onOpcode, const FnInterop& onInterop, const FnLoadContext& loadContext, const FnLog& log, BehaviorVersion v, int instructionLimit)
	: m_onOpcode(onOpcode)
	, m_onInterop(onInterop)
	, m_loadContext(loadContext)
	, m_log(log)
	, m_version(v)
{
	const static String defaultName = "entry";
	const String& name = contextName.empty() ? defaultName : contextName;

	auto it = m_contexts.insert(std::make_pair( name, ExecutionContext(name, script, offset, {}, instructionLimit) ));
	m_entryContext = &it.first->second;
}
inline VirtualMachine::VirtualMachine(const ExecutionContext& ctx, const FnOpcode& onOpcode, const FnInterop& onInterop, const FnLoadContext& loadContext, const FnLog& log, BehaviorVersion v)
	: m_onOpcode(onOpcode)
	, m_onInterop(onInterop)
	, m_loadContext(loadContext)
	, m_log(log)
	, m_version(v)
{
	auto it = m_contexts.insert(std::make_pair( ctx.Name(), ctx ));
	m_entryContext = &it.first->second;
}

inline ExecutionState VirtualMachine::Execute()
{
	return SwitchContext(*m_entryContext, 0);
}

inline void            VirtualMachine::PushFrame(ExecutionContext& ctx, UInt32 instructionPointer, UInt32 registers)
                                                      { m_frames.push_back(ExecutionFrame(ctx, instructionPointer)); }
inline UInt32          VirtualMachine::PopFrame()     { UInt32 pc = m_frames.back().Offset(); m_frames.pop_back(); return pc; }
inline int             VirtualMachine::NumFrames()    { return (int)m_frames.size(); }
inline ExecutionFrame& VirtualMachine::CurrentFrame() { return m_frames.back(); }
inline ExecutionFrame& VirtualMachine::NextFrame()    { Expect(m_frames.size() >= 2, "Not enough frames available"); return m_frames[m_frames.size()-2]; }

inline ExecutionState VirtualMachine::OnOpcode(Opcode o)
{
	return m_onOpcode ? m_onOpcode(o) : ExecutionState::Running;
}
inline ExecutionState VirtualMachine::ExecuteInterop(const String& method)
{
	if( m_log ) m_log(method.c_str());
	return m_onInterop ? m_onInterop(method, m_stack) : ExecutionState::Running;
}

inline ExecutionContext& VirtualMachine::FindContext(const String& name)
{
	auto f = m_contexts.find(name);
	if( f != m_contexts.end() )
		return f->second;
	return LoadContext(name);
}

inline ExecutionContext& VirtualMachine::LoadContext(const String& name)
{
	return m_contexts.insert({name, m_loadContext(name)}).first->second;
}

inline ExecutionState VirtualMachine::SwitchContext(ExecutionContext& context, UInt32 instructionPointer)
{
	PushFrame(context, instructionPointer, DefaultRegisterCount);
	return context.Execute(this, &CurrentFrame(), m_stack);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------

inline ExecutionState ExecutionContext::Execute(VirtualMachine* vm, ExecutionFrame* frame, VMStack& stack)
{
	if( vm && vm->m_log ) { vm->m_log("ExecutionContext"); vm->m_log(Name().c_str()); }
	if( !vm || vm->Version() > VirtualMachine::BehaviorVersion::v0 )
	{
		if( m_builtIn )
		{
			m_state = ExecutionState::Running;
			Expect( !stack.empty(), "no method");
			String methodName = PopString(stack);
			ExecutionState state = m_builtIn(methodName, stack); // either returns an ExecutionState, or a negative integer
			if( state >= (ExecutionState)0 ) // if it returned ExecutionState, return the result now.
				return m_state = state;
			// else, it returned an offset into the program, but negative, and starting from -1 as the first offset value...
			int offset = (-(int)state)-1;
			Expect(offset >= 0, "jump offset can't be negative value");
			Expect(offset < m_script.size(), "trying to jump outside of script bounds");
			m_reader.Seek(offset);
			//Drop out of the if block and keep running the code at this offset:
		}

		while( m_state == ExecutionState::Running )
		{
			if( !frame )
				break;
			StepOpcode(vm, *frame, stack);
			if( vm )
				frame = &vm->CurrentFrame();
		}
	}
	else
	{
		if( m_builtIn )
		{
			m_state = ExecutionState::Running;
			Expect( !stack.empty(), "no method");
			String methodName = PopString(stack);
			m_state = m_builtIn(methodName, stack);
			Expect( (int)m_state >= 0, "unknown call result");
		}
		else
		{
			while( m_state == ExecutionState::Running )
			{
				if( !frame )
					break;
				StepOpcode(vm, *frame, stack);
				if( vm )
					frame = &vm->CurrentFrame();
			}
		}
	}
	if( vm && vm->m_log )
	{
		switch( m_state )
		{
		case ExecutionState::Running: vm->m_log("state:Running"); break;
		case ExecutionState::Break:   vm->m_log("state:Break"); break;
		case ExecutionState::Fault:   vm->m_log("state:Fault"); break;
		case ExecutionState::Halt:    vm->m_log("state:Halt"); break;
		default:                      vm->m_log("state:???"); break;
		}
		
	}
	return m_state;
}

inline void ExecutionContext::StepOpcode(VirtualMachine* vm, ExecutionFrame& frame, VMStack& stack)
{
	const int scriptLength = (int)m_script.size();
	auto reg = [&](uint8_t i)->VMObject& { return frame.Register(i); };

	if( Version(vm) == VirtualMachine::BehaviorVersion::v0 )
	{
		if( scriptLength == 0 )
		{
			m_state = vm->OnOpcode((Opcode)0xFF);
			PHANTASMA_EXCEPTION("uhhh");
		}
	}

	if( m_reader.Finished() )
	{
		m_state = ExecutionState::Halt;
		return;
	}

	Opcode opcode = (Opcode)m_reader.ReadByte();
	if( vm )
	{
		if( vm->m_log )
			vm->m_log(ToString(opcode));
		m_state = vm->OnOpcode(opcode);
		if( m_state != ExecutionState::Running )
			return;
	}
	int count;
	uint8_t  src, sr2, dst, type;
	int64_t  length, index;
	uint16_t offset;
	uint8_t  data[0xFFFF];
	switch(opcode)
	{
	default:
		PHANTASMA_EXCEPTION("Unknown VM opcode");
		m_state = ExecutionState::Break;
		return;
	case Opcode::NOP:
		if( vm && vm->m_log ) vm->m_log("NOP");
		break;
	case Opcode::MOVE:
		if( vm && vm->m_log ) vm->m_log("MOVE");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = std::move( reg(src) );
		break;
	case Opcode::COPY:
		if( vm && vm->m_log ) vm->m_log("COPY");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = reg(src);
		break;
	case Opcode::PUSH:
		if( vm && vm->m_log ) vm->m_log("PUSH");
		src = m_reader.ReadByte();

		stack.push_back(reg(src));
		//todo - max stack size? throw overflow exception
		break;
	case Opcode::POP:
		if( vm && vm->m_log ) vm->m_log("POP");
		dst = m_reader.ReadByte();
		
		if(stack.empty())
		{
			//PHANTASMA_EXCEPTION("popped empty stack");
			m_state = ExecutionState::Fault;
			return;
		}
		reg(dst) = stack.back();
		stack.pop_back();
		break;
	case Opcode::SWAP:
		if( vm && vm->m_log ) vm->m_log("SWAP");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		std::swap(reg(src), reg(dst));
		break;
	case Opcode::CALL:
		if( vm && vm->m_log ) vm->m_log("CALL");
		count = m_reader.ReadByte();
		m_reader.Read(offset);

		Expect(vm, "VM required");
		Expect(offset < scriptLength, "invalid jump offset");
		Expect(count >= 1, "at least 1 register required");
		Expect(count <= VirtualMachine::MaxRegisterCount, "invalid register allocs");

		vm->PushFrame(*this, m_reader.Position(), count);
		m_reader.Seek(offset);

		break;
	case Opcode::EXTCALL:
	{
		if( vm && vm->m_log ) vm->m_log("EXTCALL");
		src = m_reader.ReadByte();

		Expect(vm, "VM required");
		const String& method = reg(src).AsString();
		auto state = vm->ExecuteInterop(method);
		if (state != ExecutionState::Running)
		{
			//PHANTASMA_EXCEPTION("VM extcall failed");
			m_state = ExecutionState::Fault;
			return;
		}
		break;
	}
	case Opcode::JMPIF:
	case Opcode::JMPNOT:
		src = m_reader.ReadByte();
	case Opcode::JMP:
		m_reader.Read(offset);

		if( vm && vm->m_log ) vm->m_log("JMP (todo or simmilar)");

		Expect(offset >= 0, "jump offset can't be negative value");
		Expect(offset < scriptLength, "trying to jump outside of script bounds");

		if( (opcode == Opcode::JMP) || 
			(opcode == Opcode::JMPIF && reg(src).AsBool()) ||
			(opcode == Opcode::JMPNOT && !reg(src).AsBool()))
			m_reader.Seek(offset);
		break;
	case Opcode::RET:
		if( vm && vm->m_log ) vm->m_log("RET");
		if (!vm || vm->NumFrames() <= 1)
			m_state = ExecutionState::Halt;
		else
		{
			ExecutionFrame& temp = vm->NextFrame();
			if (temp.Context().m_name == m_name)
				m_reader.Seek(vm->PopFrame());
			else
				m_state = ExecutionState::Halt;
		}
		break;
	case Opcode::THROW:
		if( vm && vm->m_log ) vm->m_log("THROW");
		src = m_reader.ReadByte();
		PHANTASMA_EXCEPTION_MESSAGE("THROW", reg(src).ToString());
		m_state = ExecutionState::Fault;
		return;
	case Opcode::LOAD:
		if( vm && vm->m_log ) vm->m_log("LOAD");
		dst = m_reader.ReadByte();
		type = m_reader.ReadByte();
		count = m_reader.ReadByteArray(data, PHANTASMA_ARRAY_SIZE(data));
		// Shipped this bug in pheonix 2025, preserved for old result reproduction
		if( Version(vm) == VirtualMachine::BehaviorVersion::v0 )
		{
			count &= 0xFF;
		}

		reg(dst).SetValue(data, count, (VMType)type);
		break;
	case Opcode::CAST:
		if( vm && vm->m_log ) vm->m_log("CAST");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();
		type = m_reader.ReadByte();

		reg(dst) = std::move(reg(src).CastTo((VMType)type));
		break;
	case Opcode::CAT:
	{
		if( vm && vm->m_log ) vm->m_log("CAT");
		src = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		const VMObject& a = reg(src);
		const VMObject& b = reg(sr2);
		if (!a.IsEmpty())
		{
			if (b.IsEmpty())
				reg(dst) = a;
			else
			{
				Expect(a.Type() == b.Type(), "Invalid cast during concat opcode");
				ByteArray bytesA = a.AsByteArray();
				ByteArray bytesB = b.AsByteArray();
				ByteArray result(bytesA.size() + bytesB.size());
				std::copy(bytesA.begin(), bytesA.end(), result.begin());
				std::copy(bytesB.begin(), bytesB.end(), result.begin() + bytesA.size());
				reg(dst).SetValue(result, a.Type());
			}
		}
		else if (b.IsEmpty())
			reg(dst) = VMObject();
		else
			reg(dst) = b;
		break;
	}
	case Opcode::RANGE:
	{
		if( vm && vm->m_log ) vm->m_log("RANGE");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();
		m_reader.ReadVarInt(index);
		m_reader.ReadVarInt(length);

		Expect(index <= 0xFFFF, "Input exceed max");
		Expect(length <= 0xFFFF, "Input exceed max");
		ByteArray bytes = reg(src).AsByteArray();
		Expect((size_t)length <= bytes.size(), "invalid length");
		Expect(index >= 0, "invalid negative index");
		size_t end = index + length;
		if (end > bytes.size())
		{
			length = bytes.size() - index;
			Expect(length > 0, "empty range");
		}
		if( length > 0 )
			reg(dst).SetValue(&bytes.front(), (int)length, reg(src).Type());
		else
			reg(dst).SetValue(nullptr, 0, reg(src).Type());
		break;
	}
	case Opcode::LEFT:
	{
		if( vm && vm->m_log ) vm->m_log("LEFT");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();
		m_reader.ReadVarInt(length);

		Expect(length <= 0xFFFF, "Input exceed max");
		ByteArray src_array = reg(src).AsByteArray();
		Expect((size_t)length <= src_array.size(), "invalid length");
		if( !src_array.empty() )
			reg(dst).SetValue(&src_array.front(), (int)length, VMType::Bytes);
		else
			reg(dst).SetValue(0, 0, VMType::Bytes);
		break;
	}
	case Opcode::RIGHT:
	{
		if( vm && vm->m_log ) vm->m_log("RIGHT");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();
		m_reader.ReadVarInt(length);

		Expect(length <= 0xFFFF, "Input exceed max");
		ByteArray src_array = reg(src).AsByteArray();
		Expect((size_t)length <= src_array.size(), "invalid length");
		offset = (uint16_t)(src_array.size() - length);
		if( !src_array.empty() )
			reg(dst).SetValue(&src_array.front() + offset, (int)length, VMType::Bytes);
		else
			reg(dst).SetValue(0, 0, VMType::Bytes);
		break;
	}
	case Opcode::SIZE:
	{
		if( vm && vm->m_log ) vm->m_log("SIZE");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		const VMObject& src_val = reg(src);
		switch (src_val.Type())
		{
		case VMType::String:
			length = src_val.Data<String>().size();
			break;

		case VMType::Timestamp:
		case VMType::Number:
		case VMType::Enum:
		case VMType::Bool:
			length = 1;
			break;

		case VMType::None:
			length = 0;
			break;

		default:
		{
			ByteArray src_array= src_val.AsByteArray();
			length = src_array.size();
			break;
		}
		}
		reg(dst) = BigInteger(length);
		break;
	}
	case Opcode::COUNT:
	{
		if( vm && vm->m_log ) vm->m_log("COUNT");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		const VMObject& src_val = reg(src);
		switch (src_val.Type())
		{
		case VMType::Struct:
			length = src_val.Data<VMStructure>().size();
			break;
		case VMType::None:
			length = 0;
			break;
		default:
			length = 1;
			break;
		}
		reg(dst) = BigInteger(length);
		break;
	}
	case Opcode::NOT:
		if( vm && vm->m_log ) vm->m_log("NOT");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = !reg(src).AsBool();
		break;
	case Opcode::AND:
	case Opcode::OR:
	case Opcode::XOR:
	{
		if( vm && vm->m_log ) vm->m_log("AND/OR/ (todo)");
		src = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		const VMObject& valA = reg(src);
		const VMObject& valB = reg(sr2);

		switch (valA.Type())
		{
		case VMType::Bool:
		{
			bool a = valA.AsBool();
			bool b = valB.AsBool();
			bool result;
			switch (opcode)
			{
			case Opcode::AND: result = (a && b); break;
			case Opcode::OR:  result = (a || b); break;
			case Opcode::XOR: result = (a ^ b); break;
			default: PHANTASMA_ASSERT(false);
			}
			reg(dst) = result;
			break;
		}
		case VMType::Enum:
		{
			Expect(opcode == Opcode::AND, "expected AND for flag op");
			Expect(valB.Type() == VMType::Enum, "expected VMType::Enum for flag op");
			BigInteger numA = valA.AsNumber();
			BigInteger numB = valB.AsNumber();
			Expect(numA.GetBitLength() <= 32, "too many bits");
			Expect(numB.GetBitLength() <= 32, "too many bits");
			Int64 a = (Int64)numA;
			Int64 b = (Int64)numB;
			bool result = (a & b) != 0;
			reg(dst) = result;
			break;
		}
		case VMType::Number:
		{
			Expect(valB.Type() == VMType::Number, "expected Number for logical op");
			BigInteger numA = valA.AsNumber();
			BigInteger numB = valB.AsNumber();
			Expect(numA.GetBitLength() <= 64, "too many bits");
			Expect(numB.GetBitLength() <= 64, "too many bits");
			Int64 a = (Int64)numA;
			Int64 b = (Int64)numB;
			Int64 result;
			switch (opcode)
			{
			case Opcode::AND: result = (a & b); break;
			case Opcode::OR:  result = (a | b); break;
			case Opcode::XOR: result = (a ^ b); break;
			default: PHANTASMA_ASSERT(false);
			}
			reg(dst) = BigInteger(result);
			break;

		}
		default:
			Expect(false, "logical op unsupported for type ");
		}
		break;
	}
	case Opcode::EQUAL:
		if( vm && vm->m_log ) vm->m_log("EQUAL");
		src = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = reg(src) == reg(sr2);
		break;
	case Opcode::LT:
	case Opcode::GT:
	case Opcode::LTE:
	case Opcode::GTE:
	{
		if( vm && vm->m_log ) vm->m_log("LT (todo - or simmilar)");
		src = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		BigInteger a = reg(src).AsNumber();
		BigInteger b = reg(sr2).AsNumber();
		bool result;
		switch (opcode)
		{
		case Opcode::LT:  result = (a <  b); break;
		case Opcode::GT:  result = (a >  b); break;
		case Opcode::LTE: result = (a <= b); break;
		case Opcode::GTE: result = (a >= b); break;
		default: PHANTASMA_ASSERT(false);
		}
		reg(dst) = result;
		break;
	}
	case Opcode::INC:
		if( vm && vm->m_log ) vm->m_log("INC");
		src = m_reader.ReadByte();

		reg(src) = reg(src).AsNumber() + BigInteger::One();
		break;
	case Opcode::DEC:
		if( vm && vm->m_log ) vm->m_log("DEC");
		src = m_reader.ReadByte();

		reg(src) = reg(src).AsNumber() - BigInteger::One();
		break;
	case Opcode::SIGN:
	{
		if( vm && vm->m_log ) vm->m_log("SIGN");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		BigInteger a = reg(src).AsNumber();
		reg(dst) = BigInteger(a.IsZero() ? 0 : (a.IsNegative() ? -1 : 1));
		break;
	}
	case Opcode::NEGATE:
		if( vm && vm->m_log ) vm->m_log("NEGATE");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = -reg(src).AsNumber();
		break;
	case Opcode::ABS:
		if( vm && vm->m_log ) vm->m_log("ABS");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = reg(src).AsNumber().Abs();
		break;
	case Opcode::ADD:
	case Opcode::SUB:
	case Opcode::MUL:
	case Opcode::DIV:
	case Opcode::MOD:
	case Opcode::SHL:
	case Opcode::SHR:
	case Opcode::MIN:
	case Opcode::MAX:
	case Opcode::POW:
	{
		if( vm && vm->m_log ) vm->m_log("ADD (todo - or other)");
		src = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		const VMObject& valA = reg(src);
		const VMObject& valB = reg(sr2);
		if (opcode == Opcode::ADD && valA.Type() == VMType::String)
		{
			Expect(valB.Type() == VMType::String, "invalid string as right operand");
			reg(dst) = valA.AsString() + valB.AsString();
		}
		else
		{
			BigInteger a = valA.AsNumber();
			BigInteger b = valB.AsNumber();
			BigInteger result;
			switch (opcode)
			{
			case Opcode::ADD: result = a + b; break;
			case Opcode::SUB: result = a - b; break;
			case Opcode::MUL: result = a * b; break;
			case Opcode::DIV: result = a / b; break;
			case Opcode::MOD: result = a % b; break;
			case Opcode::SHR: result = a >> (int)b; break;
			case Opcode::SHL: result = a << (int)b; break;
			case Opcode::MIN: result = a < b ? a : b; break;
			case Opcode::MAX: result = a > b ? a : b; break;
			case Opcode::POW: result = BigInteger::Pow(a, (int)b); break;
			default: PHANTASMA_ASSERT(false);
			}
			reg(dst) = result;
		}
		break;
	}
	case Opcode::CTX:
		if( vm && vm->m_log ) vm->m_log("CTX");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		Expect(vm, "VM required");
		reg(dst) = vm->FindContext(reg(src).AsString());
		break;
	case Opcode::SWITCH:
	{
		if( vm && vm->m_log ) vm->m_log("SWITCH");
		src = m_reader.ReadByte();

		Expect(vm, "VM required");
		ExecutionContext* ctx = reg(src).AsExecutionContext();
		Expect(ctx, "SwitchContext failed, context can't be null");
		m_state = vm->SwitchContext(*ctx, m_reader.Position());
		if (m_state != ExecutionState::Halt)
		{
		//	PHANTASMA_EXCEPTION("VM switch instruction failed: execution state did not halt");
			m_state = ExecutionState::Fault;
			return;
		}
		m_state = ExecutionState::Running;
		vm->PopFrame();
		break;
	}
	case Opcode::PUT:
		if( vm && vm->m_log ) vm->m_log("PUT");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();

		reg(dst).SetKey(reg(sr2), reg(src));
		break;
	case Opcode::GET:
	{
		if( vm && vm->m_log ) vm->m_log("GET");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();
		sr2 = m_reader.ReadByte();

		reg(dst) = reg(src).GetKey(reg(sr2));
		break;
	}
	case Opcode::CLEAR:
		if( vm && vm->m_log ) vm->m_log("CLEAR");
		dst = m_reader.ReadByte();

		reg(dst) = VMObject();
		break;
	case Opcode::UNPACK:
		if( vm && vm->m_log ) vm->m_log("UNPACK");
		src = m_reader.ReadByte();
		dst = m_reader.ReadByte();

		reg(dst) = VMObject::FromBytes( reg(src).AsByteArray() );
		break;
	case Opcode::PACK:
		if( vm && vm->m_log ) vm->m_log("PACK");
		PHANTASMA_ASSERT(false, "todo");
		m_state = ExecutionState::Fault;
		return;
	case Opcode::DEBUG_:
		if( vm && vm->m_log ) vm->m_log("DEBUG_");
		PHANTASMA_BREAKPOINT;
		break;
	}

	if( m_reader.Error() )
		m_state = ExecutionState::Fault;
	else if( m_reader.Position() == (uint32_t)scriptLength )
		m_state = ExecutionState::Halt;

	++m_instructionCount;
	if( m_instructionLimit && m_instructionCount > m_instructionLimit )
		m_state = ExecutionState::Fault;
}

}
#endif
