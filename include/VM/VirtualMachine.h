#pragma once

#include "Opcodes.h"
#include <functional>

namespace phantasma {

class ExecutionFrame;
class ExecutionContext;
class VMObject;

enum class ExecutionState
{
	Running = 0,
	Break,
	Fault,
	Halt
};

typedef PHANTASMA_VECTOR<VMObject> VMStack;


class VirtualMachine
{
public:
	static constexpr int DefaultRegisterCount = 32; // TODO temp hack, this should be 4
	static constexpr int MaxRegisterCount = 32;

	enum class BehaviorVersion
	{
		v0,
		v1,
		latest = v1
	};

	typedef std::function<void(const char*)> FnLog;
	typedef std::function<ExecutionState(Opcode)> FnOpcode;
	typedef std::function<ExecutionState(const String&, VMStack&)> FnInterop;
	typedef std::function<ExecutionContext(const String&)> FnLoadContext;

	VirtualMachine(const ByteArray& script, UInt32 offset, const String& contextName, const FnOpcode&, const FnInterop&, const FnLoadContext&, const FnLog&, BehaviorVersion v = BehaviorVersion::latest, int instructionLimit=0);
	VirtualMachine(const ExecutionContext& ctx, const FnOpcode& onOpcode, const FnInterop& onInterop, const FnLoadContext& loadContext, const FnLog& log, BehaviorVersion v = BehaviorVersion::latest);

	void PushFrame(ExecutionContext&, UInt32 instructionPointer, UInt32 registers);
	ExecutionFrame& CurrentFrame();
	ExecutionFrame& NextFrame();
	UInt32 PopFrame();
	int NumFrames();

	const VMStack& Stack() const { return m_stack; }
	void StackPush(const VMObject& o) { m_stack.push_back(o); }

	ExecutionState Execute();

	BehaviorVersion Version() const { return m_version; }

	ExecutionState OnOpcode(Opcode);
	ExecutionState ExecuteInterop(const String& method);

	ExecutionContext& FindContext(const String& name);
	ExecutionState SwitchContext(ExecutionContext&, UInt32 instructionPointer); //pushes a frame
private:
	ExecutionContext& LoadContext(const String& name);
	FnOpcode m_onOpcode;
	FnInterop m_onInterop;
	FnLoadContext m_loadContext;
	VMStack m_stack;
	PHANTASMA_VECTOR<ExecutionFrame> m_frames;
	PHANTASMA_MAP<String, ExecutionContext> m_contexts;

	ExecutionContext* m_entryContext = 0;
	BehaviorVersion m_version = BehaviorVersion::v0;
public:
	FnLog m_log;
};

inline VirtualMachine::BehaviorVersion Version(const VirtualMachine& v) { return v.Version(); }
inline VirtualMachine::BehaviorVersion Version(const VirtualMachine* v) { return v ? v->Version() : VirtualMachine::BehaviorVersion::latest; }

}
