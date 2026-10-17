// GCC 13+ <iostream> references std::ios_base_library_init() (GLIBCXX_3.4.32),
// which older libstdc++ (Steam Runtime) lacks. Provide a hidden local version
// that does what older headers did: keep a static ios_base::Init alive.
// Do not include <iostream> here.
#if defined(__linux__)
#include <ios>
extern "C" __attribute__((visibility("hidden"), used)) void hns_ios_base_library_init() __asm__("_ZSt21ios_base_library_initv");
extern "C" void hns_ios_base_library_init()
{
	static std::ios_base::Init init;
}
#endif
