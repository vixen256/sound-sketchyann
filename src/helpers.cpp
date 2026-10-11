void *
sig_scan (const char *signature, std::initializer_list<u64> hints) {
	MODULEINFO info;
	GetModuleInformation (GetCurrentProcess (), GetModuleHandle (nullptr), &info, sizeof (MODULEINFO));

	const qis::signature search (signature);
	for (auto hint : hints) {
		auto pos = qis::scan ((void *)hint, search.size (), search);
		if (pos != qis::npos) return (void *)(hint + pos);
	}
	auto pos = qis::scan (info.lpBaseOfDll, info.SizeOfImage, search);
	if (pos != qis::npos) return (void *)((u64)info.lpBaseOfDll + pos);

	char buf[128];
	if (hints.size () > 0) sprintf (buf, "Could not find pattern %s\nExpected at %llx", signature, *hints.begin ());
	else sprintf (buf, "Could not find pattern %s", signature);
	MessageBoxA (nullptr, buf, "sig_scan", MB_OK);
	return nullptr;
}

void *
sig_scan_xref (const char *signature, std::initializer_list<u64> hints) {
	auto ptr = sig_scan (signature, hints);
	if (ptr == nullptr) return nullptr;

	ZydisDecoder decoder;
	ZydisDecoderInit (&decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64);

	ZydisDecodedInstruction instruction;
	ZydisDecodedOperand operands[ZYDIS_MAX_OPERAND_COUNT];
	if (ZYAN_FAILED (ZydisDecoderDecodeFull (&decoder, ptr, 64, &instruction, operands))) return nullptr;
	for (int i = 0; i < instruction.operand_count; i++)
		if (operands[i].type == ZYDIS_OPERAND_TYPE_IMMEDIATE && operands[i].imm.is_relative) return (void *)((i64)ptr + instruction.length + operands[i].imm.value.s);
		else if (operands[i].type == ZYDIS_OPERAND_TYPE_MEMORY && operands[i].mem.base == ZYDIS_REGISTER_RIP) return (void *)((i64)ptr + instruction.length + operands[i].mem.disp.value);

	char buf[128];
	if (hints.size () > 0) sprintf (buf, "Could not resolve pattern %s XREF\nExpected at %llx", signature, *hints.begin ());
	else sprintf (buf, "Could not find resolve pattern %s XREF", signature);
	MessageBoxA (nullptr, buf, "sig_scan", MB_OK);
	return nullptr;
}
