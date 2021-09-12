// Unix only
#include <dlfcn.h>
#include <iostream>

int main()
{
	void *handle = dlopen("./libNyisBotCPP.so", RTLD_NOW);
	if (!handle) {
		std::cerr << "Failed to load library: " << dlerror() << std::endl;
		return 1;
	}

	using MainType = int(*)();
	auto func = reinterpret_cast<MainType>(dlsym(handle, "main"));
	if (!func) {
		std::cerr << "Entry point not found" << std::endl;
		return 1;
	}
	return func();
}
