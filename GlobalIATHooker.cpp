#include <cstdio>
#include <Windows.h>
#include <iostream>

#define IATHOOK_LIBRARY

#include "GlobalIATHooker.h"
#include "ASMUtils.h"
#include "IATHook.h"

PVOID lIBHOOKlastOriginalFunc;
std::unordered_map<PVOID, HookCallback*> IATHooker::hookCallbacks;
std::unordered_map<PVOID, std::shared_ptr<IATHooker>> IATHooker::hookers;

/* Defined in the ASM linked file */
extern "C" PVOID GetRSPx64(void);
extern "C" PVOID GetRCXx64(void);
extern "C" PVOID GetRDXx64(void);
extern "C" PVOID GetR8x64(void);
extern "C" PVOID GetR9x64(void);
extern "C" PVOID LIBHOOKDetourFunctionx64(void);
/* end define */

/*void TryDisplayAsString(PVOID p, const char* type) {
	
	std::cout << type << " : ";
	printf("0x%p\t", p);
	__try
	{
		printf("Native : %20llu\t", *((DWORDPTR*)p));
		wprintf(L"Unicode : %s\t", (WCHAR*)p);
		printf("ANSI : %s\n", (TCHAR*)p);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)			//Captures Access Violation
	{
		std::cout << std::endl;
	}
	
}


void HookFunctionBefore(PVOID originalFunc) {
	IATHook* currentHook = IATHook::getHookFromAddress(originalFunc);
	if (currentHook != NULL) {
		std::cout << currentHook->getFunctionName().c_str() << std::endl;

		DWORDPTR rsp = (DWORDPTR)GetRSPx64();
		PVOID rcx = GetRCXx64();
		PVOID rdx = GetRDXx64();
		PVOID r8 = GetR8x64();
		PVOID r9 = GetR9x64();

		//TryDisplayAsString(rcx, "RCX");
		//TryDisplayAsString(rdx, "RDX");
		//TryDisplayAsString(r8, "R8 ");
		//TryDisplayAsString(r9, "R9 ");

		//std::cout << "Stack pointer : " << (PVOID)rsp;
		std::cout << std::endl << std::endl;

	}
	else {
		std::cout << lIBHOOKlastOriginalFunc << std::endl;
	}
}*/


/// <summary>
/// Hooks the original function
/// </summary>
/// <param name="originalFunc">The original (unhooked) function.</param>
extern "C" void LIBHOOKGeneralHookFunc(PVOID originalFunc) {
	//HookFunctionBefore(originalFunc);

	HookCallback * h = IATHooker::getCallback(originalFunc); 
	if (h != NULL) {
#ifdef _WIN64
		/* x86-64 MS calling convention */
		PVOID rcx = GetRCXx64();
		PVOID rdx = GetRDXx64();
		PVOID r8 = GetR8x64();
		PVOID r9 = GetR9x64();
		std::vector<PVOID> registerArgs;
		registerArgs.push_back(rcx);
		registerArgs.push_back(rdx);
		registerArgs.push_back(r8);
		registerArgs.push_back(r9);
#else
#endif
		h->callback(originalFunc, registerArgs, GetRSPx64());
	}
}




/// <summary>
/// Sets the function to hook with the associated callback.
/// </summary>
/// <param name="function">The function.</param>
/// <param name="callback">The callback.</param>
void IATHooker::setHookFunction(PVOID function, HookCallback* callback) {
	hookCallback = callback;
	hookCallbacks[function] = callback;
}


/// <summary>
/// Gets the callback of a hooked function.
/// </summary>
/// <param name="originalFunc">The function.</param>
/// <returns></returns>
HookCallback* IATHooker::getCallback(PVOID originalFunc) {
	if (hookCallbacks.find(originalFunc) != hookCallbacks.end()) {
		return hookCallbacks[originalFunc];
	}
	return NULL;
}

/// <summary>
/// Factory method that creates a hooker instance that hooks a function with a callback.
/// </summary>
/// <param name="function">The function.</param>
/// <param name="callback">The callback.</param>
/// <returns>The hooker instance</returns>
IATHooker* IATHooker::createHooker(PVOID function, HookCallback* callback) {
	hookers[function] = std::shared_ptr<IATHooker>(new IATHooker(function, callback));
	return hookers[function].get();
}

/// <summary>
/// Gets the hooker stored in the internal map of hookers from the hooked function address.
/// </summary>
/// <param name="func">The original hooked function.</param>
/// <returns>The hooker instance or a null pointer if there is no hooker for this function</returns>
std::weak_ptr<IATHooker> IATHooker::getHooker(PVOID func) {
	if (hookers.find(func) != hookers.end()) {
		return hookers[func];
	}
	return std::shared_ptr<IATHooker>(NULL);
}


IATHooker::IATHooker(PVOID function, HookCallback* callback) {
	trampoline = generateTrampolineDetourFunction(function);
	setHookFunction(function, callback);
}

/// <summary>
/// Gets the trampoline function.
/// </summary>
/// <returns>The trampoline function</returns>
PVOID IATHooker::getTrampoline() {
	return trampoline;
}

/// <summary>
/// In assembly language, generates the trampoline detour function that will lead to a proc in an asm project-linked file.
/// This function allows us to first push the address of the original (unhooked) function in the stack and then the asm file
/// will call LIBHOOKGeneralHookFunc with this function as a parameter to retrieve the original function.
/// </summary>
/// <param name="originalFunc">The original function.</param>
/// <returns>A dynamically allocated pointer that leads to the generated ASM code of the trampoline function</returns>
PVOID IATHooker::generateTrampolineDetourFunction(PVOID originalFunc) {
	
	//TODO x86

	BYTE code[] = {
		0x50,													//PUSH RAX
		0x48, 0xB8,												//MOV RAX,
		0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE,			//originalFunc
		0x50,													//PUSH RAX

		0x48, 0xB8,												//MOV RAX,
		0xEF, 0xBE, 0xAD, 0xDE, 0xEF, 0xBE, 0xAD, 0xDE,			//LIBHOOKDetourFunctionx64
		0xFF, 0xE0												//JMP RAX
	};

#ifdef _WIN64
	ASMUtils::reverseAddressx64((DWORD64)originalFunc, code + 3);
	ASMUtils::reverseAddressx64((DWORD64)LIBHOOKDetourFunctionx64, code + 14);
#else
	ASMUtils::reverseAddressx86((DWORD)originalFunc, code + 3);
	//TODO x86 version of LIBHOOKDetourFunction
	ASMUtils::reverseAddressx86((DWORD)LIBHOOKDetourFunctionx64, code + 14);
#endif
	
	return ASMUtils::writeAssembly(code, sizeof(code));
}

/// <summary>
/// Frees the trampoline function.
/// </summary>
void IATHooker::freeTrampoline() {
	/* First, we have to free the dynamically allocated trampoline function */
	VirtualFree(trampoline, NULL, MEM_RELEASE);
	trampoline = NULL;
}

/// <summary>
/// Finalizes an instance of the <see cref="IATHooker"/> class.
/// </summary>
IATHooker::~IATHooker() {
	/* Do not free trampoline here : the code still has to be reachable even after the instance of IATHooker is dead. 
	   To free a trampoline, we have to explicitly call "freeTrampoline" by using the IATHooker map. */
}