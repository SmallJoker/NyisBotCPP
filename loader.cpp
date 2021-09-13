// Unix only
#include <dlfcn.h>
#include <iostream>

int main()
{
	/*
		Load the entire bot which is a shared library
		This step is necessary to get API calls from
		Bot <--> Module working in both directions
	*/

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
