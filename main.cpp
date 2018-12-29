#include <angelscript.h>
#include <scriptbuilder/scriptbuilder.h>
#include <string>
#include <iostream>
#include <functional>
#include <type_traits>
#include <utility>

void MessageCallback(const asSMessageInfo *msg, void *param)
{
	const char *type = "ERR ";
	if (msg->type == asMSGTYPE_WARNING)
		type = "WARN";
	else if (msg->type == asMSGTYPE_INFORMATION) 
		type = "INFO";
	printf("%s (%d, %d) : %s : %s\n", msg->section, msg->row, msg->col, type, msg->message);
}

struct S
{
	S() = default;
	S(const S&) = delete;
	S& operator=(const S&) = delete;

	void f(const int& test)
	{
	}
};

// Declare f1
__attribute__((noinline))
void f1(int i, S& s)
{
}

void f1_generic(asIScriptGeneric* gen)
{
	auto p1 = *(int*)gen->GetAddressOfArg(0);
	auto& p2 = **(S**)gen->GetArgAddress(1);
	f1(p1, p2);
}

// Declare f2
__attribute__((noinline))
void f2() {}

void f2_generic(asIScriptGeneric* gen)
{
	f2();
}

// Declare f3
__attribute__((noinline))
int f3()
{
	return 1234;
}

void f3_generic(asIScriptGeneric* gen)
{
	gen->SetReturnDWord(f3());
}

// Declare f4
__attribute__((noinline))
float f4(int a, float b, double c, unsigned long long d)
{
	return 3.141f;
}

void f4_generic(asIScriptGeneric* gen)
{
	auto a = *(int*)gen->GetAddressOfArg(0);
	auto b = *(float*)gen->GetAddressOfArg(1);
	auto c = *(double*)gen->GetAddressOfArg(2);
	auto d = *(unsigned long long*)gen->GetAddressOfArg(3);
	gen->SetReturnFloat(f4(a, b, c, d));
}

// Declare f5
__attribute__((noinline))
void f5(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) {}

// Autowrap stuff

template<class T>
struct func_info;

template<class Ret, class... Args>
struct func_info<Ret(Args...)>
{
	using args_type = std::tuple<Args...>;
	using return_type = Ret;
};

template<class T>
struct member_func_info;
 
template<class T, class U>
struct member_func_info<T U::*>
{
	using function_type = T;
	using class_type = U;
};

template<class T>
T get_parameter(asIScriptGeneric* generic, unsigned n)
{
	if constexpr (std::is_reference_v<T> || std::is_pointer_v<T>)
	{
		return *static_cast<std::remove_reference_t<T>*>(generic->GetArgAddress(n));
	}
	else
	{
		return *static_cast<T*>(generic->GetAddressOfArg(n));
	}
}

template<class TupleType, unsigned N = 0>
auto get_parameter_tuple(asIScriptGeneric* generic)
{
	if constexpr (N < std::tuple_size_v<TupleType>)
	{
		using T = std::tuple_element_t<N, TupleType>;

		return std::tuple_cat(
			std::tuple<T>{get_parameter<T>(generic, N)},
			get_parameter_tuple<TupleType, N+1>(generic));
	}
	else
	{
		return std::tuple {};
	}
}

template<auto Func>
void asfunc(asIScriptGeneric* generic)
{
	auto call = [&] {
		if constexpr (std::is_member_function_pointer_v<decltype(Func)>)
		{
			using MbrFnInfo = member_func_info<decltype(Func)>;
			using FnSignature = std::remove_pointer_t<typename MbrFnInfo::function_type>;
			using FnInfo = func_info<FnSignature>;

			auto* obj = static_cast<typename MbrFnInfo::class_type*>(generic->GetObject());

			return std::apply([&](auto&&... args) {
				return std::invoke(Func, obj, std::forward<decltype(args)>(args)...);
			}, ::get_parameter_tuple<typename FnInfo::args_type>(generic));
		}
		else
		{
			using FnSignature = std::remove_pointer_t<decltype(Func)>;
			using FnInfo = func_info<FnSignature>;

			return std::apply(Func, ::get_parameter_tuple<typename FnInfo::args_type>(generic));
		}
	};

	using ReturnType = std::invoke_result_t<decltype(call)>;

	if constexpr (!std::is_void_v<ReturnType>)
	{
		auto* ret = new (generic->GetAddressOfReturnLocation()) ReturnType(call());
	}
	else
	{
		call();
	}
}

int main()
{
	auto *engine = asCreateScriptEngine();
	engine->SetMessageCallback(asFUNCTION(MessageCallback), 0, asCALL_CDECL);

	engine->RegisterObjectType("S", sizeof(S), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<S>());

	engine->RegisterObjectMethod("S", "void f(const int &in)", asFUNCTION(asfunc<&S::f>), asCALL_GENERIC);
	//engine->RegisterObjectMethod("S", "void f(const int &in)", asMETHOD(S, f), asCALL_THISCALL);

	// Register f1
	//engine->RegisterGlobalFunction("void f1(int, S &in)", asFUNCTION(f1), asCALL_CDECL);
	//engine->RegisterGlobalFunction("void f1(int, S &in)", asFUNCTION(f1_generic), asCALL_GENERIC);
	//engine->RegisterGlobalFunction("void f1(int, S &in)", asFUNCTION(asfunc<f1>), asCALL_GENERIC);

	// Register f2
	//engine->RegisterGlobalFunction("void f2()", asFUNCTION(f2), asCALL_CDECL);
	//engine->RegisterGlobalFunction("void f2()", asFUNCTION(f2_generic), asCALL_GENERIC);
	//engine->RegisterGlobalFunction("void f2()", asFUNCTION(asfunc<f2>), asCALL_GENERIC);

	// Register f3
	//engine->RegisterGlobalFunction("int f3()", asFUNCTION(f3), asCALL_CDECL);
	//engine->RegisterGlobalFunction("int f3()", asFUNCTION(f3_generic), asCALL_GENERIC);
	//engine->RegisterGlobalFunction("int f3()", asFUNCTION(asfunc<f3>), asCALL_GENERIC);

	// Register f4
	//engine->RegisterGlobalFunction("float f4(int, float, double, uint64)", asFUNCTION(f4), asCALL_CDECL);
	//engine->RegisterGlobalFunction("float f4(int, float, double, uint64)", asFUNCTION(f4_generic), asCALL_GENERIC);
	//engine->RegisterGlobalFunction("float f4(int, float, double, uint64)", asFUNCTION(asfunc<f4>), asCALL_GENERIC);

	// Register f5
	//engine->RegisterGlobalFunction("void f5(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int)", asFUNCTION(f5), asCALL_CDECL);
	//engine->RegisterGlobalFunction("void f5(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int)", asFUNCTION(asfunc<f5>), asCALL_GENERIC);

	auto* mod = engine->GetModule("module", asGM_ALWAYS_CREATE);

	const char* script = R"(
void main()
{
	S s;

	/*int a = 1;
	float b = 2.0f;
	double c = 3.0;
	uint64 d = 4;*/

	for (int i = 0; i < 30000000; ++i)
	{
		// f1(1, s);
		// f2();
		// f3();
		// f4(a, b, c, d);
		// f5(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15);
		s.f(3);
	}
}
	)";

	mod->AddScriptSection("script.as", script);
	mod->Build();

	auto* ctx = engine->CreateContext();
	auto* func = engine->GetModule("module")->GetFunctionByName("main");

	ctx->Prepare(func);
	ctx->Execute();
	ctx->Release();
}
