
#include "channel.h"
#include "chatcommand.h"
#include "client.h"
#include "logger.h"
#include "module.h"
#include "settings.h"
#include "utils.h"

#include <filesystem>
// Unix only
#include <dlfcn.h>

static std::string NAME_PREFIX("libnbm_");


struct ModuleInternal {
	ModuleInternal(cstr_t &name_, cstr_t &path_) :
		name(name_), path(path_) {}
	~ModuleInternal()
	{
		if (module) {
			ERROR("Module not properly deleted");
		}
	}

	bool load(IClient *cli);
	void unload(Network *net);

	void *dll_handle = nullptr;
	IModule *module = nullptr;
	Settings *settings = nullptr;
	std::string name;
	std::string path;
};


// ================= IModule =================

ModuleMgr *IModule::getModuleMgr() const
{
	return m_client ?  m_client->getModuleMgr() : nullptr;
}

Network *IModule::getNetwork() const
{
	return m_client ? m_client->getNetwork() : nullptr;
}

void IModule::sendRaw(cstr_t &what) const
{
	m_client->sendRaw(what);
}

void IModule::addClientRequest(ClientRequest && cr)
{
	m_client->addRequest(std::move(cr));
}

bool IModule::checkBotAdmin(Channel *c, UserInstance *ui, bool alert) const
{
	cstr_t &admin = getModuleMgr()->getGlobalSettings()->get("client.admin");
	if (ui->nickname == admin && ui->account == UserInstance::UAS_LOGGED_IN)
		return true;

	if (alert)
		c->reply(ui, "Insufficient privileges.");
	return false;
}


// ================= ModuleMgr =================

ModuleMgr::ModuleMgr(IClient *cli)
{
	m_client = cli;
	m_commands = new ChatCommand(nullptr);

	if (m_client && m_client->getSettings()) {
		cstr_t &type = m_client->getSettings()->get("_internal.type");
		m_settings = new Settings("config/module." + type + ".conf");
		m_settings->syncFileContents();
	} else {
		// Unittest
		m_settings = new Settings("config/tmp.conf");
	}

	m_last_step = std::chrono::high_resolution_clock::now();
}

ModuleMgr::~ModuleMgr()
{
	unloadModules();
	delete m_commands;

	m_settings->syncFileContents(SR_WRITE);
	delete m_settings;
}

bool ModuleMgr::loadModules()
{
	MutexLock _(m_lock);
	if (!m_modules.empty())
		return false;

	bool good = true;
	// Find modules in path, named "libnbm_?????.so"
	for (const auto &entry : std::filesystem::directory_iterator("modules")) {
		const std::string &filename = entry.path().filename();
		size_t cut_a = filename.rfind(NAME_PREFIX);
		size_t cut_b = filename.find('.');
		if (cut_a == std::string::npos || cut_b == std::string::npos)
			continue;
		if (entry.path().extension() != ".so")
			continue;

		cut_a += NAME_PREFIX.size();
		std::string module_name(filename.substr(cut_a, cut_b - cut_a));

		auto mi = new ModuleInternal(module_name, entry.path().string());
		if (!loadSingleModule(mi)) {
			delete mi;
			good = false;
			break;
		}
	}

	if (!good)
		unloadModules();
	return good;
}

bool ModuleMgr::reloadModule(std::string name, bool keep_data)
{
	bool can_lock = m_lock.try_lock();
	if (!can_lock) {
		LOG("Delaying module reload");
		m_client->addRequest(ClientRequest {
			.type = ClientRequest::RT_RELOAD_MODULE,
			.reload_module = {
				.path = new std::string(name),
				.keep_data = keep_data
			}
		});
		return true;
	}

	name = NAME_PREFIX + name;

	ModuleInternal *mi = nullptr;
	for (ModuleInternal *it : m_modules) {
		if (it->path.find(name) == std::string::npos)
			continue;

		mi = it;
		break;
	}

	if (!mi) {
		WARN("No such module: " << name);
		m_lock.unlock();
		return false;
	}

	Network *net = m_client ? m_client->getNetwork() : nullptr;
	IModule *expired_ptr = mi->module;

	unloadSingleModule(mi, keep_data);
	bool ok = loadSingleModule(mi);

	if (!ok) {
		// Well shit. Now it's too late to free the containers
		keep_data = false;
		m_modules.erase(mi);
	}

	if (net && keep_data) {
		// Update and re-assign old references
		// let's hope the two classes were not modified between the module loads

		auto &users = net->getAllUsers();
		for (auto ui : users)
			ui->move(expired_ptr, mi->module);

		for (Channel *c : net->getAllChannels())
			c->getContainers()->move(expired_ptr, mi->module);

		m_commands->move(expired_ptr, mi->module);
	}

	m_lock.unlock();
	return ok;
}

void ModuleMgr::unloadModules()
{
	MutexLock _(m_lock);
	if (m_modules.empty())
		return;

	LOG("Unloading modules...");

	while (!m_modules.empty()) {
		ModuleInternal *mi = *m_modules.begin();
		unloadSingleModule(mi);
		m_modules.erase(mi);
		delete mi;
	}

	*m_commands = ChatCommand(nullptr); // reset
}

std::vector<std::string> ModuleMgr::getModuleList() const
{
	std::vector<std::string> list;
	for (ModuleInternal *it : m_modules)
		list.emplace_back(it->name);

	return list;
}

bool ModuleMgr::loadSingleModule(ModuleInternal *mi)
{
	bool ok = mi->load(m_client);

	if (ok) {
		// std::set has unique keys
		m_modules.insert(mi);

		if (m_client)
			mi->module->onClientReady();
	}

	return ok;
}

void ModuleMgr::unloadSingleModule(ModuleInternal *mi, bool keep_data)
{
	// Remove all invalid module-registered commands
	Network *net = m_client ? m_client->getNetwork() : nullptr;
	if (net) {
		// Remove soon invalid command scopes
		for (Channel *c : net->getAllChannels())
			m_commands->resetScope(c, nullptr);
	}

	mi->unload(keep_data ? nullptr : net);

	if (!keep_data) {
		m_commands->remove(mi->module);
	}
}

Settings *ModuleMgr::getSettings(IModule *module) const
{
	ModuleInternal *mi = nullptr;
	for (ModuleInternal *it : m_modules) {
		if (it->module == module) {
			mi = it;
			break;
		}
	}
	if (!mi)
		return nullptr;

	if (!mi->settings) {
		mi->settings = m_settings->fork(mi->name);
		// No need to sync: forks are up-to-date. Let's do it anyway.
		mi->settings->syncFileContents(SR_READ);
	}

	return mi->settings;
}

Settings *ModuleMgr::getGlobalSettings() const
{
	return m_client->getSettings();
}

void ModuleMgr::onStep(float time)
{
	MutexLock _(m_lock);

	auto time_now = std::chrono::high_resolution_clock::now();
	// Allow custom intervals
	if (time <= 0.0f)
		time = std::chrono::duration<float>(time_now - m_last_step).count();

	if (time < 0.2f)
		return;

	m_last_step = time_now;

	for (ModuleInternal *mi : m_modules)
		mi->module->onStep(time);

	std::vector<UserInstance *> to_execute;
	for (auto &[ui, countdown] : m_status_update_timeout) {
		countdown -= time;
		if (countdown <= 0.0f)
			to_execute.emplace_back(ui);
	}
	for (UserInstance *ui : to_execute) {
		if (!((IUserOwner *)m_client->getNetwork())->contains(ui)) {
			m_status_update_timeout.erase(ui);
			continue;
		}

		m_lock.unlock();
		// Trigger timeout callback
		onUserStatusUpdate(ui, true);
		m_lock.lock();
	}
}

void ModuleMgr::onChannelJoin(Channel *c)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onChannelJoin(c);
}

void ModuleMgr::onChannelLeave(Channel *c)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onChannelLeave(c);
}

void ModuleMgr::onUserJoin(Channel *c, UserInstance *ui)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserJoin(c, ui);
}

void ModuleMgr::onUserLeave(Channel *c, UserInstance *ui)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserLeave(c, ui);
}

void ModuleMgr::onUserRename(UserInstance *ui, cstr_t &old_name)
{
	MutexLock _(m_lock);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserRename(ui, old_name);
}

bool ModuleMgr::onUserSay(Channel *c, UserInstance *ui, std::string &msg)
{
	MutexLock _(m_lock);

	std::string backup(msg);
	if (m_commands->run(c, ui, msg))
		return true;

	for (ModuleInternal *mi : m_modules) {
		if (msg.size() != backup.size())
			msg.assign(backup);

		if (mi->module->onUserSay(c, ui, msg))
			return true;
	}
	return false;
}

void ModuleMgr::onUserStatusUpdate(UserInstance *ui, bool is_timeout)
{
	MutexLock _(m_lock);
	m_status_update_timeout.erase(ui);
	for (ModuleInternal *mi : m_modules)
		mi->module->onUserStatusUpdate(ui, is_timeout);
}

void ModuleMgr::client_privatefunc_1(UserInstance *ui)
{
	m_status_update_timeout[ui] = 3;
}

// ================= ModuleInternal =================

bool ModuleInternal::load(IClient *cli)
{
	ASSERT(!module, "Not allowing to overwrite module");

	LOG("Loading module " << path);
	void *handle = dlopen(path.c_str(), RTLD_NOW);
	if (!handle) {
		ERROR("Failed to load module: " << dlerror());
		return false;
	}

	using InitType = IModule*(*)();
	// "_Z8nbm_initv" for C++, or "nbm_init" for extern "C"
	auto func = reinterpret_cast<InitType>(dlsym(handle, "nbm_init"));
	if (!func) {
		ERROR("Missing init function");
		dlclose(handle);
		return false;
	}

	dll_handle = handle;
	module = func();
	if (!module) {
		unload(nullptr);
		return false;
	}
	module->m_path = &path;
	module->m_client = cli;
	return true;
}

void ModuleInternal::unload(Network *net)
{
	if (!module)
		return;

	if (net) {
		// Remove this module data from all locations before the destructor turns invalid
		for (Channel *c : net->getAllChannels()) {
			module->onChannelLeave(c);
			c->getContainers()->remove(module);
		}

		auto &users = net->getAllUsers();
		for (auto ui : users) {
			ui->remove(module);
		}
	}

	delete module;
	dlclose(dll_handle);

	module = nullptr;
	dll_handle = nullptr;

	delete settings;
	settings = nullptr;
}
