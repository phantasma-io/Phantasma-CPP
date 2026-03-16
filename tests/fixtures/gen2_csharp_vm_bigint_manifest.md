# Gen2 C# VM BigInteger Fixtures

These fixtures are generated from the authoritative legacy Gen2 C# VM implementation.

- Source repo: `Phantasma_Gen2`
- Source commit: `30f1afdc26d917d7dc8e82e2e0a9956d5b2e0b37`
- Generated UTC: `2026-03-16T21:26:25.1433671+00:00`

## Authoritative files

- `Phantasma.Core/src/Utils/IOUtils.cs`
- `Phantasma.Core/src/Numerics/BigIntegerExtension.cs`
- `Phantasma.Core/src/Domain/VM/VMObject.cs`
- `Phantasma.Business/src/VM/Utils/ScriptBuilder.cs`
- `Phantasma.Business/src/VM/ScriptContext.cs`
- `Phantasma.Business/src/VM/VirtualMachine.cs`
- `Phantasma.Business/src/Blockchain/VM/GasMachine.cs`

## Fixture families

- `gen2_csharp_vm_bigint_binary.tsv`: `BigInteger` byte encodings used by `IOUtils.WriteBigInteger`, `BigIntegerExtension`, and `ScriptBuilder.EmitLoad(BigInteger)`.
- `gen2_csharp_vm_bigint_decimal.tsv`: authoritative decimal parse/normalize/ToString fixtures for wide values and legacy Phantasma ids.
- `gen2_csharp_vm_bigint_narrow_int.tsv`: authoritative `(int)BigInteger` narrowing results or overflow exceptions.
- `gen2_csharp_vm_bigint_ops.tsv`: authoritative direct VM numeric semantics for `ADD/SUB/MUL/DIV/MOD/SHL/SHR/MIN/MAX/POW` using the same C# expressions as `ScriptContext`.
- `gen2_csharp_vm_bigint_unary_ops.tsv`: authoritative unary VM numeric semantics for `SIGN/NEGATE/ABS` using the same Gen2 C# expressions as `ScriptContext`.
- `gen2_csharp_vmobject_asnumber.tsv`: authoritative `VMObject.AsNumber()` conversion cases, including exception scenarios.
- `gen2_csharp_vmobject_asbool.tsv`: authoritative `VMObject.AsBool()` conversion cases, including the exact Gen2 rejection of legacy truthy types.
- `gen2_csharp_vmobject_asbytes.tsv`: authoritative `VMObject.AsByteArray()` conversion cases, including object and struct behavior.
- `gen2_csharp_vmobject_serde.tsv`: authoritative `VMObject.SerializeData/UnserializeData` round-trips, including object reclassification behavior.
- `gen2_csharp_vmobject_arraytype.tsv`: authoritative `VMObject.GetArrayType()` results for sequential arrays, gaps, mixed structs, and empty structs.
- `gen2_csharp_vmobject_asstring.tsv`: authoritative `VMObject.AsString()` behavior for struct arrays (Unicode-number arrays and Base64 fallback structs).
- `gen2_csharp_vmobject_cast_struct.tsv`: authoritative `VMObject.CastTo(..., Struct)` behavior for strings, bytes, numbers, and runtime object payloads.
- `gen2_csharp_vm_scriptcontext_ops.tsv`: authoritative end-to-end VM execution cases for binary numeric and logic opcodes (`ADD/SUB/MUL/DIV/MOD/SHL/SHR/MIN/MAX/POW/AND/OR/XOR`).
- `gen2_csharp_vm_scriptcontext_unary.tsv`: authoritative end-to-end VM execution cases for unary opcodes (`NOT/SIGN/NEGATE/ABS`).
