/*
 * Copyright (C) 2000-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2018 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <stdint.h>

#include <sys/types.h>
#include <cstdio>
#include <cstdlib>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#ifdef HAVE_LRDF
#include <lrdf.h>
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/vst_info_file.h"
#include "fst.h"
#include "pbd/basename.h"
#include <cstring>

// dll-info
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
#include "ardour/vst_info_file.h"
#include "ardour/linux_vst_support.h"
#include "pbd/basename.h"
#include <cstring>
#endif //LXVST_SUPPORT

#ifdef MACVST_SUPPORT
#include "ardour/vst_info_file.h"
#include "ardour/mac_vst_support.h"
#include "ardour/mac_vst_plugin.h"
#include "pbd/basename.h"
#include "pbd/pathexpand.h"
#include <cstring>
#endif //MACVST_SUPPORT

#include <glibmm/miscutils.h>
#include <glibmm/pattern.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/convert.h"
#include "pbd/file_utils.h"
#include "pbd/tokenizer.h"
#include "pbd/whitespace.h"

#include "ardour/directory_names.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/ladspa.h"
#include "ardour/ladspa_plugin.h"
#include "ardour/luascripting.h"
#include "ardour/luaproc.h"
#include "ardour/plugin.h"
#include "ardour/plugin_manager.h"
#include "ardour/rc_configuration.h"

#include "ardour/search_paths.h"

#ifdef LV2_SUPPORT
#include "ardour/lv2_plugin.h"
#endif

#ifdef WINDOWS_VST_SUPPORT
#include "ardour/windows_vst_plugin.h"
#endif

#ifdef LXVST_SUPPORT
#include "ardour/lxvst_plugin.h"
#endif

#ifdef AUDIOUNIT_SUPPORT
#include "ardour/audio_unit.h"
#include <Carbon/Carbon.h>
#endif

#include "pbd/error.h"
#include "pbd/stl_delete.h"

#include "pbd/i18n.h"

#include "ardour/debug.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

PluginManager* PluginManager::_instance = 0;
std::string PluginManager::scanner_bin_path = "";

PluginManager&
PluginManager::instance()
{
	if (!_instance) {
		_instance = new PluginManager;
	}
	return *_instance;
}

PluginManager::PluginManager ()
	: _windows_vst_plugin_info(0)
	, _lxvst_plugin_info(0)
	, _mac_vst_plugin_info(0)
	, _ladspa_plugin_info(0)
	, _lv2_plugin_info(0)
	, _au_plugin_info(0)
	, _lua_plugin_info(0)
	, _cancel_scan(false)
	, _cancel_timeout(false)
{
	char* s;
	string lrdf_path;

#if defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT
	// source-tree (ardev, etc)
	PBD::Searchpath vstsp(Glib::build_filename(ARDOUR::ardour_dll_directory(), "fst"));

#ifdef PLATFORM_WINDOWS
	// on windows the .exe needs to be in the same folder with libardour.dll
	vstsp += Glib::build_filename(windows_package_directory_path(), "bin");
#else
	// on Unices additional internal-use binaries are deployed to $libdir
	vstsp += ARDOUR::ardour_dll_directory();
#endif

	if (!PBD::find_file (vstsp,
#ifdef PLATFORM_WINDOWS
    #ifdef DEBUGGABLE_SCANNER_APP
        #if defined(DEBUG) || defined(_DEBUG)
				"ardour-vst-scannerD.exe"
        #else
				"ardour-vst-scannerRDC.exe"
        #endif
    #else
				"ardour-vst-scanner.exe"
    #endif
#else
				"ardour-vst-scanner"
#endif
				, scanner_bin_path)) {
		PBD::warning << "VST scanner app (ardour-vst-scanner) not found in path " << vstsp.to_string() << endmsg;
	}
#endif

	load_statuses ();

	load_tags ();

	if ((s = getenv ("LADSPA_RDF_PATH"))){
		lrdf_path = s;
	}

	if (lrdf_path.length() == 0) {
		lrdf_path = "/usr/local/share/ladspa/rdf:/usr/share/ladspa/rdf";
	}

	add_lrdf_data(lrdf_path);
	add_ladspa_presets();
#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst ()) {
		add_windows_vst_presets ();
	}
#endif /* WINDOWS_VST_SUPPORT */

#ifdef LXVST_SUPPORT
	if (Config->get_use_lxvst()) {
		add_lxvst_presets();
	}
#endif /* Native LinuxVST support*/

#ifdef MACVST_SUPPORT
	if (Config->get_use_macvst ()) {
		add_mac_vst_presets ();
	}
#endif

	if ((s = getenv ("VST_PATH"))) {
		windows_vst_path = s;
	} else if ((s = getenv ("VST_PLUGINS"))) {
		windows_vst_path = s;
	}

	if (windows_vst_path.length() == 0) {
		windows_vst_path = vst_search_path ();
	}

	if ((s = getenv ("LXVST_PATH"))) {
		lxvst_path = s;
	} else if ((s = getenv ("LXVST_PLUGINS"))) {
		lxvst_path = s;
	}

	if (lxvst_path.length() == 0) {
		lxvst_path = "/usr/local/lib64/lxvst:/usr/local/lib/lxvst:/usr/lib64/lxvst:/usr/lib/lxvst:"
			"/usr/local/lib64/linux_vst:/usr/local/lib/linux_vst:/usr/lib64/linux_vst:/usr/lib/linux_vst:"
			"/usr/lib/vst:/usr/local/lib/vst";
	}

	/* first time setup, use 'default' path */
	if (Config->get_plugin_path_lxvst() == X_("@default@")) {
		Config->set_plugin_path_lxvst(get_default_lxvst_path());
	}
	if (Config->get_plugin_path_vst() == X_("@default@")) {
		Config->set_plugin_path_vst(get_default_windows_vst_path());
	}

	if (_instance == 0) {
		_instance = this;
	}

	BootMessage (_("Discovering Plugins"));

	LuaScripting::instance().scripts_changed.connect_same_thread (lua_refresh_connection, boost::bind (&PluginManager::lua_refresh_cb, this));
}


PluginManager::~PluginManager()
{
	if (getenv ("ARDOUR_RUNNING_UNDER_VALGRIND")) {
		// don't bother, just exit quickly.
		delete _windows_vst_plugin_info;
		delete _lxvst_plugin_info;
		delete _mac_vst_plugin_info;
		delete _ladspa_plugin_info;
		delete _lv2_plugin_info;
		delete _au_plugin_info;
		delete _lua_plugin_info;
	}
}

void
PluginManager::refresh (bool cache_only)
{
	Glib::Threads::Mutex::Lock lm (_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return;
	}

	DEBUG_TRACE (DEBUG::PluginManager, "PluginManager::refresh\n");
	_cancel_scan = false;

	BootMessage (_("Scanning LADSPA Plugins"));
	ladspa_refresh ();
	BootMessage (_("Scanning Lua DSP Processors"));
	lua_refresh ();
#ifdef LV2_SUPPORT
	BootMessage (_("Scanning LV2 Plugins"));
	lv2_refresh ();

	if (Config->get_conceal_lv1_if_lv2_exists()) {
		for (PluginInfoList::const_iterator i = _ladspa_plugin_info->begin(); i != _ladspa_plugin_info->end(); ++i) {
			for (PluginInfoList::const_iterator j = _lv2_plugin_info->begin(); j != _lv2_plugin_info->end(); ++j) {
				if ((*i)->creator == (*j)->creator && (*i)->name == (*j)->name) {
					PluginStatus ps (LADSPA, (*i)->unique_id, Concealed);
					if (find (statuses.begin(), statuses.end(), ps) == statuses.end()) {
						statuses.erase (ps);
						statuses.insert (ps);
					}
				}
			}
		}
	} else {
		for (PluginStatusList::iterator i = statuses.begin(); i != statuses.end();) {
			PluginStatusList::iterator j = i++;
			if ((*j).status == Concealed) {
				statuses.erase (j);
			}
		}
	}
#endif
#ifdef WINDOWS_VST_SUPPORT
	if (Config->get_use_windows_vst()) {
		if (cache_only) {
			BootMessage (_("Scanning Windows VST Plugins"));
		} else {
			BootMessage (_("Discovering Windows VST Plugins"));
		}
		windows_vst_refresh (cache_only);
	}
#endif // WINDOWS_VST_SUPPORT

#ifdef LXVST_SUPPORT
	if(Config->get_use_lxvst()) {
		if (cache_only) {
			BootMessage (_("Scanning Linux VST Plugins"));
		} else {
			BootMessage (_("Discovering Linux VST Plugins"));
		}
		lxvst_refresh(cache_only);
	}
#endif //Native linuxVST SUPPORT

#ifdef MACVST_SUPPORT
	if(Config->get_use_macvst ()) {
		if (cache_only) {
			BootMessage (_("Scanning Mac VST Plugins"));
		} else {
			BootMessage (_("Discovering Mac VST Plugins"));
		}
		mac_vst_refresh (cache_only);
	} else if (_mac_vst_plugin_info) {
		_mac_vst_plugin_info->clear ();
	} else {
		_mac_vst_plugin_info = new ARDOUR::PluginInfoList();
	}
#endif //Native Mac VST SUPPORT

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
		if (!cache_only) {
			string fn = Glib::build_filename (ARDOUR::user_cache_directory(), VST_BLACKLIST);
			if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
				gchar *bl = NULL;
				if (g_file_get_contents(fn.c_str (), &bl, NULL, NULL)) {
					if (Config->get_verbose_plugin_scan()) {
						PBD::info << _("VST Blacklist: ") << fn << "\n" << bl << "-----" << endmsg;
					} else {
						PBD::info << _("VST Blacklist:") << "\n" << bl << "-----" << endmsg;
					}
					g_free (bl);
				}
			}
		}
#endif

#ifdef AUDIOUNIT_SUPPORT
	if (cache_only) {
		BootMessage (_("Scanning AU Plugins"));
	} else {
		BootMessage (_("Discovering AU Plugins"));
	}
	au_refresh (cache_only);
#endif

	BootMessage (_("Plugin Scan Complete..."));
	PluginListChanged (); /* EMIT SIGNAL */
	PluginScanMessage(X_("closeme"), "", false);
	_cancel_scan = false;
}

void
PluginManager::cancel_plugin_scan ()
{
	_cancel_scan = true;
}

void
PluginManager::cancel_plugin_timeout ()
{
	_cancel_timeout = true;
}

void
PluginManager::clear_vst_cache ()
{
#if 1 // clean old cache and error files. (remove this code after 4.3 or 5.0)
#ifdef WINDOWS_VST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_vst(), "\\" VST_EXT_INFOFILE "$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_vst(), "\\.fsi$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_vst(), "\\.err$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif

#ifdef LXVST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_lxvst(), "\\" VST_EXT_INFOFILE "$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_lxvst(), "\\.fsi$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_lxvst(), "\\.err$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif
#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT)
	{
		string dir = Glib::build_filename (ARDOUR::user_cache_directory(), "fst_info");
		if (Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
			PBD::remove_directory (dir);
		}
	}
#endif
#endif // old cache cleanup

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	{
		string dn = Glib::build_filename (ARDOUR::user_cache_directory(), "vst");
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, dn, "\\" VST_EXT_INFOFILE "$", /* user cache is flat, no recursion */ false);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif
}

void
PluginManager::clear_vst_blacklist ()
{
#if 1 // remove old blacklist files. (remove this code after 4.3 or 5.0)

#ifdef WINDOWS_VST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_vst(), "\\" VST_EXT_BLACKLIST "$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif

#ifdef LXVST_SUPPORT
	{
		vector<string> fsi_files;
		find_files_matching_regex (fsi_files, Config->get_plugin_path_lxvst(), "\\" VST_EXT_BLACKLIST "$", true);
		for (vector<string>::iterator i = fsi_files.begin(); i != fsi_files.end (); ++i) {
			::g_unlink(i->c_str());
		}
	}
#endif
#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT)
	{
		string dir = Glib::build_filename (ARDOUR::user_cache_directory(), "fst_blacklist");
		if (Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
			PBD::remove_directory (dir);
		}
	}
#endif

#endif // old blacklist cleanup

#if (defined WINDOWS_VST_SUPPORT || defined LXVST_SUPPORT || defined MACVST_SUPPORT)
	{
		string fn = Glib::build_filename (ARDOUR::user_cache_directory(), VST_BLACKLIST);
		if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
			::g_unlink (fn.c_str());
		}
	}
#endif

}

void
PluginManager::clear_au_cache ()
{
#ifdef AUDIOUNIT_SUPPORT
	AUPluginInfo::clear_cache ();
#endif
}

void
PluginManager::clear_au_blacklist ()
{
#ifdef AUDIOUNIT_SUPPORT
	string fn = Glib::build_filename (ARDOUR::user_cache_directory(), "au_blacklist.txt");
	if (Glib::file_test (fn, Glib::FILE_TEST_EXISTS)) {
		::g_unlink(fn.c_str());
	}
#endif
}

void
PluginManager::lua_refresh ()
{
	if (_lua_plugin_info) {
		_lua_plugin_info->clear ();
	} else {
		_lua_plugin_info = new ARDOUR::PluginInfoList ();
	}
	ARDOUR::LuaScriptList & _scripts (LuaScripting::instance ().scripts (LuaScriptInfo::DSP));
	for (LuaScriptList::const_iterator s = _scripts.begin(); s != _scripts.end(); ++s) {
		LuaPluginInfoPtr lpi (new LuaPluginInfo(*s));
		_lua_plugin_info->push_back (lpi);
		set_tags (lpi->type, lpi->unique_id, lpi->category, lpi->name, FromPlug);
	}
}

void
PluginManager::lua_refresh_cb ()
{
	Glib::Threads::Mutex::Lock lm (_lock, Glib::Threads::TRY_LOCK);
	if (!lm.locked()) {
		return;
	}
	lua_refresh ();
	PluginListChanged (); /* EMIT SIGNAL */
}

void
PluginManager::ladspa_refresh ()
{
	if (_ladspa_plugin_info) {
		_ladspa_plugin_info->clear ();
	} else {
		_ladspa_plugin_info = new ARDOUR::PluginInfoList ();
	}

	/* allow LADSPA_PATH to augment, not override standard locations */

	/* Only add standard locations to ladspa_path if it doesn't
	 * already contain them. Check for trailing G_DIR_SEPARATOR too.
	 */

	vector<string> ladspa_modules;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("LADSPA: search along: [%1]\n", ladspa_search_path().to_string()));

	find_files_matching_pattern (ladspa_modules, ladspa_search_path (), "*.so");
	find_files_matching_pattern (ladspa_modules, ladspa_search_path (), "*.dylib");
	find_files_matching_pattern (ladspa_modules, ladspa_search_path (), "*.dll");

	for (vector<std::string>::iterator i = ladspa_modules.begin(); i != ladspa_modules.end(); ++i) {
		ARDOUR::PluginScanMessage(_("LADSPA"), *i, false);
		ladspa_discover (*i);
	}
}

#ifdef HAVE_LRDF
static bool rdf_filter (const string &str, void* /*arg*/)
{
	return str[0] != '.' &&
		   ((str.find(".rdf")  == (str.length() - 4)) ||
            (str.find(".rdfs") == (str.length() - 5)) ||
		    (str.find(".n3")   == (str.length() - 3)) ||
		    (str.find(".ttl")  == (str.length() - 4)));
}
#endif

void
PluginManager::add_ladspa_presets()
{
	add_presets ("ladspa");
}

void
PluginManager::add_windows_vst_presets()
{
	add_presets ("windows-vst");
}

void
PluginManager::add_mac_vst_presets()
{
	add_presets ("mac-vst");
}

void
PluginManager::add_lxvst_presets()
{
	add_presets ("lxvst");
}

void
PluginManager::add_presets(string domain)
{
#ifdef HAVE_LRDF
	vector<string> presets;
	vector<string>::iterator x;

	char* envvar;
	if ((envvar = getenv ("HOME")) == 0) {
		return;
	}

	string path = string_compose("%1/.%2/rdf", envvar, domain);
	find_files_matching_filter (presets, path, rdf_filter, 0, false, true);

	for (x = presets.begin(); x != presets.end (); ++x) {
		string file = "file:" + *x;
		if (lrdf_read_file(file.c_str())) {
			warning << string_compose(_("Could not parse rdf file: %1"), *x) << endmsg;
		}
	}

#endif
}

void
PluginManager::add_lrdf_data (const string &path)
{
#ifdef HAVE_LRDF
	vector<string> rdf_files;
	vector<string>::iterator x;

	find_files_matching_filter (rdf_files, path, rdf_filter, 0, false, true);

	for (x = rdf_files.begin(); x != rdf_files.end (); ++x) {
		const string uri(string("file://") + *x);

		if (lrdf_read_file(uri.c_str())) {
			warning << "Could not parse rdf file: " << uri << endmsg;
		}
	}
#endif
}

int
PluginManager::ladspa_discover (string path)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Checking for LADSPA plugin at %1\n", path));

	Glib::Module module (path);
	const LADSPA_Descriptor *descriptor;
	LADSPA_Descriptor_Function dfunc;
	void* func = 0;

	if (!module) {
		error << string_compose(_("LADSPA: cannot load module \"%1\" (%2)"),
			path, Glib::Module::get_last_error()) << endmsg;
		return -1;
	}


	if (!module.get_symbol("ladspa_descriptor", func)) {
		error << string_compose(_("LADSPA: module \"%1\" has no descriptor function."), path) << endmsg;
		error << Glib::Module::get_last_error() << endmsg;
		return -1;
	}

	dfunc = (LADSPA_Descriptor_Function)func;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("LADSPA plugin found at %1\n", path));

	for (uint32_t i = 0; ; ++i) {
		/* if a ladspa plugin allocates memory here
		 * it is never free()ed (or plugin-dependent only when unloading).
		 * For some plugins memory allocated is incremental, we should
		 * avoid re-scanning plugins and file bug reports.
		 */
		if ((descriptor = dfunc (i)) == 0) {
			break;
		}

		if (!ladspa_plugin_whitelist.empty()) {
			if (find (ladspa_plugin_whitelist.begin(), ladspa_plugin_whitelist.end(), descriptor->UniqueID) == ladspa_plugin_whitelist.end()) {
				continue;
			}
		}

		PluginInfoPtr info(new LadspaPluginInfo);
		info->name = descriptor->Name;
		info->category = get_ladspa_category(descriptor->UniqueID);
		info->path = path;
		info->index = i;
		info->n_inputs = ChanCount();
		info->n_outputs = ChanCount();
		info->type = ARDOUR::LADSPA;

		string::size_type pos = 0;
		string creator = descriptor->Maker;
		/* stupid LADSPA creator strings */
#ifdef PLATFORM_WINDOWS
		while (pos < creator.length() && creator[pos] > -2 && creator[pos] < 256 && (isalnum (creator[pos]) || isspace (creator[pos]) || creator[pos] == '.')) ++pos;
#else
		while (pos < creator.length() && (isalnum (creator[pos]) || isspace (creator[pos]) || creator[pos] == '.')) ++pos;
#endif

		/* If there were too few characters to create a
		 * meaningful name, mark this creator as 'Unknown'
		 */
		if (creator.length() < 2 || pos < 3) {
			info->creator = "Unknown";
		} else{
			info->creator = creator.substr (0, pos);
			strip_whitespace_edges (info->creator);
		}

		char buf[32];
		snprintf (buf, sizeof (buf), "%lu", descriptor->UniqueID);
		info->unique_id = buf;

		for (uint32_t n=0; n < descriptor->PortCount; ++n) {
			if (LADSPA_IS_PORT_AUDIO (descriptor->PortDescriptors[n])) {
				if (LADSPA_IS_PORT_INPUT (descriptor->PortDescriptors[n])) {
					info->n_inputs.set_audio(info->n_inputs.n_audio() + 1);
				}
				else if (LADSPA_IS_PORT_OUTPUT (descriptor->PortDescriptors[n])) {
					info->n_outputs.set_audio(info->n_outputs.n_audio() + 1);
				}
			}
		}

		if(_ladspa_plugin_info->empty()){
			_ladspa_plugin_info->push_back (info);
		}

		//Ensure that the plugin is not already in the plugin list.

		bool found = false;

		for (PluginInfoList::const_iterator i = _ladspa_plugin_info->begin(); i != _ladspa_plugin_info->end(); ++i) {
			if(0 == info->unique_id.compare((*i)->unique_id)){
				found = true;
			}
		}

		if(!found){
			_ladspa_plugin_info->push_back (info);
			set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);
		}

		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Found LADSPA plugin, name: %1, Inputs: %2, Outputs: %3\n", info->name, info->n_inputs, info->n_outputs));
	}

	return 0;
}

string
PluginManager::get_ladspa_category (uint32_t plugin_id)
{
#ifdef HAVE_LRDF
	char buf[256];
	lrdf_statement pattern;

	snprintf(buf, sizeof(buf), "%s%" PRIu32, LADSPA_BASE, plugin_id);
	pattern.subject = buf;
	pattern.predicate = const_cast<char*>(RDF_TYPE);
	pattern.object = 0;
	pattern.object_type = lrdf_uri;

	lrdf_statement* matches1 = lrdf_matches (&pattern);

	if (!matches1) {
		return "Unknown";
	}

	pattern.subject = matches1->object;
	pattern.predicate = const_cast<char*>(LADSPA_BASE "hasLabel");
	pattern.object = 0;
	pattern.object_type = lrdf_literal;

	lrdf_statement* matches2 = lrdf_matches (&pattern);
	lrdf_free_statements(matches1);

	if (!matches2) {
		return ("Unknown");
	}

	string label = matches2->object;
	lrdf_free_statements(matches2);

	/* Kludge LADSPA class names to be singular and match LV2 class names.
	   This avoids duplicate plugin menus for every class, which is necessary
	   to make the plugin category menu at all usable, but is obviously a
	   filthy kludge.

	   In the short term, lrdf could be updated so the labels match and a new
	   release made. To support both specs, we should probably be mapping the
	   URIs to the same category in code and perhaps tweaking that hierarchy
	   dynamically to suit the user. Personally, I (drobilla) think that time
	   is better spent replacing the little-used LRDF.

	   In the longer term, we will abandon LRDF entirely in favour of LV2 and
	   use that class hierarchy. Aside from fixing this problem properly, that
	   will also allow for translated labels. SWH plugins have been LV2 for
	   ages; TAP needs porting. I don't know of anything else with LRDF data.
	*/
	if (label == "Utilities") {
		return "Utility";
	} else if (label == "Pitch shifters") {
		return "Pitch Shifter";
	} else if (label != "Dynamics" && label != "Chorus"
	           &&label[label.length() - 1] == 's'
	           && label[label.length() - 2] != 's') {
		return label.substr(0, label.length() - 1);
	} else {
		return label;
	}
#else
		return ("Unknown");
#endif
}

#ifdef LV2_SUPPORT
void
PluginManager::lv2_refresh ()
{
	DEBUG_TRACE (DEBUG::PluginManager, "LV2: refresh\n");
	delete _lv2_plugin_info;
	_lv2_plugin_info = LV2PluginInfo::discover();

	for (PluginInfoList::iterator i = _lv2_plugin_info->begin(); i != _lv2_plugin_info->end(); ++i) {
		set_tags ((*i)->type, (*i)->unique_id, (*i)->category, (*i)->name, FromPlug);
	}
}
#endif

#ifdef AUDIOUNIT_SUPPORT
void
PluginManager::au_refresh (bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, "AU: refresh\n");

	// disable automatic discovery in case we crash
	bool discover_at_start = Config->get_discover_audio_units ();
	Config->set_discover_audio_units (false);
	Config->save_state();

	delete _au_plugin_info;
	_au_plugin_info = AUPluginInfo::discover(cache_only && !discover_at_start);

	// successful scan re-enabled automatic discovery if it was set
	Config->set_discover_audio_units (discover_at_start);
	Config->save_state();

	for (PluginInfoList::iterator i = _au_plugin_info->begin(); i != _au_plugin_info->end(); ++i) {
		set_tags ((*i)->type, (*i)->unique_id, (*i)->category, (*i)->name, FromPlug);
	}
}

#endif

#ifdef WINDOWS_VST_SUPPORT

void
PluginManager::windows_vst_refresh (bool cache_only)
{
	if (_windows_vst_plugin_info) {
		_windows_vst_plugin_info->clear ();
	} else {
		_windows_vst_plugin_info = new ARDOUR::PluginInfoList();
	}

	windows_vst_discover_from_path (Config->get_plugin_path_vst(), cache_only);
}

static bool windows_vst_filter (const string& str, void * /*arg*/)
{
	/* Not a dotfile, has a prefix before a period, suffix is "dll" */
	return str[0] != '.' && str.length() > 4 && strings_equal_ignore_case (".dll", str.substr(str.length() - 4));
}

int
PluginManager::windows_vst_discover_from_path (string path, bool cache_only)
{
	vector<string> plugin_objects;
	vector<string>::iterator x;
	int ret = 0;

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Discovering Windows VST plugins along %1\n", path));

	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled WindowsVST scan (safe mode)") << endmsg;
		return -1;
	}

	if (Config->get_verbose_plugin_scan()) {
		info << string_compose (_("--- Windows VST plugins Scan: %1"), path) << endmsg;
	}

	find_files_matching_filter (plugin_objects, path, windows_vst_filter, 0, false, true, true);

	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x) {
		ARDOUR::PluginScanMessage(_("VST"), *x, !cache_only && !cancelled());
		windows_vst_discover (*x, cache_only || cancelled());
	}

	if (Config->get_verbose_plugin_scan()) {
		info << _("--- Windows VST plugins Scan Done") << endmsg;
	}

	return ret;
}

static std::string dll_info (std::string path) {
	std::string rv;
	uint8_t buf[68];
	uint16_t type = 0;
	off_t pe_hdr_off = 0;

	int fd = g_open(path.c_str(), O_RDONLY, 0444);

	if (fd < 0) {
		return _("cannot open dll"); // TODO strerror()
	}

	if (68 != read (fd, buf, 68)) {
		rv = _("invalid dll, file too small");
		goto errorout;
	}
	if (buf[0] != 'M' && buf[1] != 'Z') {
		rv = _("not a dll");
		goto errorout;
	}

	pe_hdr_off = *((int32_t*) &buf[60]);
	if (pe_hdr_off !=lseek (fd, pe_hdr_off, SEEK_SET)) {
		rv = _("cannot determine dll type");
		goto errorout;
	}
	if (6 != read (fd, buf, 6)) {
		rv = _("cannot read dll PE header");
		goto errorout;
	}

	if (buf[0] != 'P' && buf[1] != 'E') {
		rv = _("invalid dll PE header");
		goto errorout;
	}

	type = *((uint16_t*) &buf[4]);
	switch (type) {
		case 0x014c:
			rv = _("i386 (32-bit)");
			break;
		case  0x0200:
			rv = _("Itanium");
			break;
		case 0x8664:
			rv = _("x64 (64-bit)");
			break;
		case 0:
			rv = _("Native Architecture");
			break;
		default:
			rv = _("Unknown Architecture");
			break;
	}
errorout:
	assert (rv.length() > 0);
	close (fd);
	return rv;
}

int
PluginManager::windows_vst_discover (string path, bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("windows_vst_discover '%1'\n", path));

	if (Config->get_verbose_plugin_scan()) {
		if (cache_only) {
			info << string_compose (_(" *  %1 (cache only)"), path) << endmsg;
		} else {
			info << string_compose (_(" *  %1 - %2"), path, dll_info (path)) << endmsg;
		}
	}

	_cancel_timeout = false;
	vector<VSTInfo*> * finfos = vstfx_get_info_fst (const_cast<char *> (path.c_str()),
			cache_only ? VST_SCAN_CACHE_ONLY : VST_SCAN_USE_APP);

	// TODO get extended error messae from vstfx_get_info_fst() e.g blacklisted, 32/64bit compat,
	// .err file scanner output etc.

	if (finfos->empty()) {
		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot get Windows VST information from '%1'\n", path));
		if (Config->get_verbose_plugin_scan()) {
			info << _(" -> Cannot get Windows VST information, plugin ignored.") << endmsg;
		}
		return -1;
	}

	uint32_t discovered = 0;
	for (vector<VSTInfo *>::iterator x = finfos->begin(); x != finfos->end(); ++x) {
		VSTInfo* finfo = *x;

		if (!finfo->canProcessReplacing) {
			warning << string_compose (_("VST plugin %1 does not support processReplacing, and cannot be used in %2 at this time"),
							 finfo->name, PROGRAM_NAME)
				<< endl;
			continue;
		}

		PluginInfoPtr info (new WindowsVSTPluginInfo (finfo));
		info->path = path;

		/* what a joke freeware VST is */
		if (!strcasecmp ("The Unnamed plugin", finfo->name)) {
			info->name = PBD::basename_nosuffix (path);
		}

		/* if we don't have any tags for this plugin, make some up. */
		set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);

		// TODO: check dup-IDs (lxvst AND windows vst)
		bool duplicate = false;

		if (!_windows_vst_plugin_info->empty()) {
			for (PluginInfoList::iterator i =_windows_vst_plugin_info->begin(); i != _windows_vst_plugin_info->end(); ++i) {
				if ((info->type == (*i)->type) && (info->unique_id == (*i)->unique_id)) {
					warning << string_compose (_("Ignoring duplicate Windows VST plugin \"%1\""), info->name) << endmsg;
					duplicate = true;
					break;
				}
			}
		}

		if (!duplicate) {
			DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Windows VST plugin ID '%1'\n", info->unique_id));
			_windows_vst_plugin_info->push_back (info);
			discovered++;
			if (Config->get_verbose_plugin_scan()) {
				PBD::info << string_compose (_(" -> OK (VST Plugin \"%1\" was added)."), info->name) << endmsg;
			}
		}
	}

	vstfx_free_info_list (finfos);
	return discovered > 0 ? 0 : -1;
}

#endif // WINDOWS_VST_SUPPORT

#ifdef MACVST_SUPPORT
void
PluginManager::mac_vst_refresh (bool cache_only)
{
	if (_mac_vst_plugin_info) {
		_mac_vst_plugin_info->clear ();
	} else {
		_mac_vst_plugin_info = new ARDOUR::PluginInfoList();
	}

	mac_vst_discover_from_path ("~/Library/Audio/Plug-Ins/VST:/Library/Audio/Plug-Ins/VST", cache_only);
}

static bool mac_vst_filter (const string& str)
{
	string plist = Glib::build_filename (str, "Contents", "Info.plist");
	if (!Glib::file_test (plist, Glib::FILE_TEST_IS_REGULAR)) {
		return false;
	}
	return str[0] != '.' && str.length() > 4 && strings_equal_ignore_case (".vst", str.substr(str.length() - 4));
}

int
PluginManager::mac_vst_discover_from_path (string path, bool cache_only)
{
	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled MacVST scan (safe mode)") << endmsg;
		return -1;
	}

	Searchpath paths (path);
	/* customized version of run_functor_for_paths() */
	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		string expanded_path = path_expand (*i);
		if (!Glib::file_test (expanded_path, Glib::FILE_TEST_IS_DIR)) continue;
		try {
			Glib::Dir dir(expanded_path);
			for (Glib::DirIterator di = dir.begin(); di != dir.end(); di++) {
				string fullpath = Glib::build_filename (expanded_path, *di);

				/* we're only interested in bundles */
				if (!Glib::file_test (fullpath, Glib::FILE_TEST_IS_DIR)) {
					continue;
				}

				if (mac_vst_filter (fullpath)) {
					ARDOUR::PluginScanMessage(_("MacVST"), fullpath, !cache_only && !cancelled());
					mac_vst_discover (fullpath, cache_only || cancelled());
					continue;
				}

				/* don't descend into AU bundles in the VST dir */
				if (fullpath[0] == '.' || (fullpath.length() > 10 && strings_equal_ignore_case (".component", fullpath.substr(fullpath.length() - 10)))) {
					continue;
				}

				/* recurse */
				mac_vst_discover_from_path (fullpath, cache_only);
			}
		} catch (Glib::FileError& err) { }
	}

	return 0;
}

int
PluginManager::mac_vst_discover (string path, bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("checking apparent MacVST plugin at %1\n", path));

	_cancel_timeout = false;

	vector<VSTInfo*>* finfos = vstfx_get_info_mac (const_cast<char *> (path.c_str()),
			cache_only ? VST_SCAN_CACHE_ONLY : VST_SCAN_USE_APP);

	if (finfos->empty()) {
		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot get Mac VST information from '%1'\n", path));
		return -1;
	}

	uint32_t discovered = 0;
	for (vector<VSTInfo *>::iterator x = finfos->begin(); x != finfos->end(); ++x) {
		VSTInfo* finfo = *x;

		if (!finfo->canProcessReplacing) {
			warning << string_compose (_("Mac VST plugin %1 does not support processReplacing, and so cannot be used in %2 at this time"),
							 finfo->name, PROGRAM_NAME)
				<< endl;
			continue;
		}

		PluginInfoPtr info (new MacVSTPluginInfo (finfo));
		info->path = path;

		/* if we don't have any tags for this plugin, make some up. */
		set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);

		bool duplicate = false;
		if (!_mac_vst_plugin_info->empty()) {
			for (PluginInfoList::iterator i =_mac_vst_plugin_info->begin(); i != _mac_vst_plugin_info->end(); ++i) {
				if ((info->type == (*i)->type)&&(info->unique_id == (*i)->unique_id)) {
					warning << "Ignoring duplicate Mac VST plugin " << info->name << "\n";
					duplicate = true;
					break;
				}
			}
		}

		if (!duplicate) {
			_mac_vst_plugin_info->push_back (info);
			discovered++;
		}
	}

	vstfx_free_info_list (finfos);
	return discovered > 0 ? 0 : -1;
}

#endif // MAC_VST_SUPPORT

#ifdef LXVST_SUPPORT

void
PluginManager::lxvst_refresh (bool cache_only)
{
	if (_lxvst_plugin_info) {
		_lxvst_plugin_info->clear ();
	} else {
		_lxvst_plugin_info = new ARDOUR::PluginInfoList();
	}

	lxvst_discover_from_path (Config->get_plugin_path_lxvst(), cache_only);
}

static bool lxvst_filter (const string& str, void *)
{
	/* Not a dotfile, has a prefix before a period, suffix is "so" */

	return str[0] != '.' && (str.length() > 3 && str.find (".so") == (str.length() - 3));
}

int
PluginManager::lxvst_discover_from_path (string path, bool cache_only)
{
	vector<string> plugin_objects;
	vector<string>::iterator x;
	int ret = 0;

	if (Session::get_disable_all_loaded_plugins ()) {
		info << _("Disabled LinuxVST scan (safe mode)") << endmsg;
		return -1;
	}

#ifndef NDEBUG
	(void) path;
#endif

	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Discovering linuxVST plugins along %1\n", path));

	find_files_matching_filter (plugin_objects, Config->get_plugin_path_lxvst(), lxvst_filter, 0, false, true, true);

	for (x = plugin_objects.begin(); x != plugin_objects.end (); ++x) {
		ARDOUR::PluginScanMessage(_("LXVST"), *x, !cache_only && !cancelled());
		lxvst_discover (*x, cache_only || cancelled());
	}

	return ret;
}

int
PluginManager::lxvst_discover (string path, bool cache_only)
{
	DEBUG_TRACE (DEBUG::PluginManager, string_compose ("checking apparent LXVST plugin at %1\n", path));

	_cancel_timeout = false;
	vector<VSTInfo*> * finfos = vstfx_get_info_lx (const_cast<char *> (path.c_str()),
			cache_only ? VST_SCAN_CACHE_ONLY : VST_SCAN_USE_APP);

	if (finfos->empty()) {
		DEBUG_TRACE (DEBUG::PluginManager, string_compose ("Cannot get Linux VST information from '%1'\n", path));
		return -1;
	}

	uint32_t discovered = 0;
	for (vector<VSTInfo *>::iterator x = finfos->begin(); x != finfos->end(); ++x) {
		VSTInfo* finfo = *x;

		if (!finfo->canProcessReplacing) {
			warning << string_compose (_("linuxVST plugin %1 does not support processReplacing, and so cannot be used in %2 at this time"),
							 finfo->name, PROGRAM_NAME)
				<< endl;
			continue;
		}

		PluginInfoPtr info(new LXVSTPluginInfo (finfo));
		info->path = path;

		if (!strcasecmp ("The Unnamed plugin", finfo->name)) {
			info->name = PBD::basename_nosuffix (path);
		}

		set_tags (info->type, info->unique_id, info->category, info->name, FromPlug);

		/* Make sure we don't find the same plugin in more than one place along
		 * the LXVST_PATH We can't use a simple 'find' because the path is included
		 * in the PluginInfo, and that is the one thing we can be sure MUST be
		 * different if a duplicate instance is found. So we just compare the type
		 * and unique ID (which for some VSTs isn't actually unique...)
		 */

		// TODO: check dup-IDs with windowsVST, too
		bool duplicate = false;
		if (!_lxvst_plugin_info->empty()) {
			for (PluginInfoList::iterator i =_lxvst_plugin_info->begin(); i != _lxvst_plugin_info->end(); ++i) {
				if ((info->type == (*i)->type)&&(info->unique_id == (*i)->unique_id)) {
					warning << "Ignoring duplicate Linux VST plugin " << info->name << "\n";
					duplicate = true;
					break;
				}
			}
		}

		if (!duplicate) {
			_lxvst_plugin_info->push_back (info);
			discovered++;
		}
	}

	vstfx_free_info_list (finfos);
	return discovered > 0 ? 0 : -1;
}

#endif // LXVST_SUPPORT


PluginManager::PluginStatusType
PluginManager::get_status (const PluginInfoPtr& pi) const
{
	PluginStatus ps (pi->type, pi->unique_id);
	PluginStatusList::const_iterator i = find (statuses.begin(), statuses.end(), ps);
	if (i == statuses.end()) {
		return Normal;
	} else {
		return i->status;
	}
}

void
PluginManager::save_statuses ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_statuses");
	stringstream ofs;

	for (PluginStatusList::iterator i = statuses.begin(); i != statuses.end(); ++i) {
		if ((*i).status == Concealed) {
			continue;
		}
		switch ((*i).type) {
		case LADSPA:
			ofs << "LADSPA";
			break;
		case AudioUnit:
			ofs << "AudioUnit";
			break;
		case LV2:
			ofs << "LV2";
			break;
		case Windows_VST:
			ofs << "Windows-VST";
			break;
		case LXVST:
			ofs << "LXVST";
			break;
		case MacVST:
			ofs << "MacVST";
			break;
		case Lua:
			ofs << "Lua";
			break;
		}

		ofs << ' ';

		switch ((*i).status) {
		case Normal:
			ofs << "Normal";
			break;
		case Favorite:
			ofs << "Favorite";
			break;
		case Hidden:
			ofs << "Hidden";
			break;
		case Concealed:
			ofs << "Hidden";
			assert (0);
			break;
		}

		ofs << ' ';

		ofs << (*i).unique_id;;
		ofs << endl;
	}
	g_file_set_contents (path.c_str(), ofs.str().c_str(), -1, NULL);
}

void
PluginManager::load_statuses ()
{
	std::string path;
	find_file (plugin_metadata_search_path(), "plugin_statuses", path); // note: if no user folder is found, this will find the resources path
	gchar *fbuf = NULL;
	if (!g_file_get_contents (path.c_str(), &fbuf, NULL, NULL)) {
		return;
	}
	stringstream ifs (fbuf);
	g_free (fbuf);

	std::string stype;
	std::string sstatus;
	std::string id;
	PluginType type;
	PluginStatusType status;
	char buf[1024];

	while (ifs) {

		ifs >> stype;
		if (!ifs) {
			break;

		}

		ifs >> sstatus;
		if (!ifs) {
			break;
		}

		/* rest of the line is the plugin ID */

		ifs.getline (buf, sizeof (buf), '\n');
		if (!ifs) {
			break;
		}

		if (sstatus == "Normal") {
			status = Normal;
		} else if (sstatus == "Favorite") {
			status = Favorite;
		} else if (sstatus == "Hidden") {
			status = Hidden;
		} else {
			error << string_compose (_("unknown plugin status type \"%1\" - all entries ignored"), sstatus) << endmsg;
			statuses.clear ();
			break;
		}

		if (stype == "LADSPA") {
			type = LADSPA;
		} else if (stype == "AudioUnit") {
			type = AudioUnit;
		} else if (stype == "LV2") {
			type = LV2;
		} else if (stype == "Windows-VST") {
			type = Windows_VST;
		} else if (stype == "LXVST") {
			type = LXVST;
		} else if (stype == "MacVST") {
			type = MacVST;
		} else if (stype == "Lua") {
			type = Lua;
		} else {
			error << string_compose (_("unknown plugin type \"%1\" - ignored"), stype)
			      << endmsg;
			continue;
		}

		id = buf;
		strip_whitespace_edges (id);
		set_status (type, id, status);
	}
}

void
PluginManager::set_status (PluginType t, string id, PluginStatusType status)
{
	PluginStatus ps (t, id, status);
	statuses.erase (ps);

	if (status != Normal && status != Concealed) {
		statuses.insert (ps);
	}

	PluginStatusChanged (t, id, status); /* EMIT SIGNAL */
}

PluginType
PluginManager::to_generic_vst (const PluginType t)
{
	switch (t) {
		case Windows_VST:
		case LXVST:
		case MacVST:
			return LXVST;
		default:
			break;
	}
	return t;
}

struct SortByTag {
	bool operator() (std::string a, std::string b) {
		return a.compare (b) < 0;
	}
};

vector<std::string>
PluginManager::get_tags (const PluginInfoPtr& pi) const
{
	vector<std::string> tags;

	PluginTag ps (to_generic_vst(pi->type), pi->unique_id, "", "", FromPlug);
	PluginTagList::const_iterator i = find (ptags.begin(), ptags.end(), ps);
	if (i != ptags.end ()) {
		PBD::tokenize (i->tags, string(" "), std::back_inserter (tags), true);
		SortByTag sorter;
		sort (tags.begin(), tags.end(), sorter);
	}
	return tags;
}

std::string
PluginManager::get_tags_as_string (PluginInfoPtr const& pi) const
{
	std::string ret;

	vector<std::string> tags = get_tags(pi);
	for (vector<string>::iterator t = tags.begin(); t != tags.end(); ++t) {
		if (t != tags.begin ()) {
			ret.append(" ");
		}
		ret.append(*t);
	}

	return ret;
}

std::string
PluginManager::user_plugin_metadata_dir () const
{
	std::string dir = Glib::build_filename (user_config_directory(), plugin_metadata_dir_name);
	g_mkdir_with_parents (dir.c_str(), 0744);
	return dir;
}

bool
PluginManager::load_plugin_order_file (XMLNode &n) const
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_order");

	info << string_compose (_("Loading plugin order file %1"), path) << endmsg;
	if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		return false;
	}

	XMLTree tree;
	if (tree.read (path)) {
		n = *(tree.root());
		return true;
	} else {
		error << string_compose (_("Cannot parse Plugin Order info from %1"), path) << endmsg;
		return false;
	}
}


void
PluginManager::save_plugin_order_file (XMLNode &elem) const
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_order");

	info << string_compose (_("Saving plugin order file %1"), path) << endmsg;

	XMLTree tree;
	tree.set_root (&elem);
	if (!tree.write (path)) {
		error << string_compose (_("Could not save Plugin Order info to %1"), path) << endmsg;
	}
	tree.set_root (0); // note: must disconnect the elem from XMLTree, or it will try to delete memory it didn't allocate
}


void
PluginManager::save_tags ()
{
	std::string path = Glib::build_filename (user_plugin_metadata_dir(), "plugin_tags");
	XMLNode* root = new XMLNode (X_("PluginTags"));

	for (PluginTagList::iterator i = ptags.begin(); i != ptags.end(); ++i) {
#ifdef MIXBUS
		if ((*i).type == LADSPA) {
			uint32_t id = atoi ((*i).unique_id);
			if (id >= 9300 && id <= 9399) {
				continue; /* do not write mixbus channelstrip ladspa's in the tagfile */
			}
		}
#endif
		if ((*i).tagtype <= FromFactoryFile) {
			/* user file should contain only plugins that are user-tagged */
			continue;
		}
		XMLNode* node = new XMLNode (X_("Plugin"));
		node->set_property (X_("type"), to_generic_vst ((*i).type));
		node->set_property (X_("id"), (*i).unique_id);
		node->set_property (X_("tags"), (*i).tags);
		node->set_property (X_("name"), (*i).name);
		node->set_property (X_("user-set"), "1");
		root->add_child_nocopy (*node);
	}

	XMLTree tree;
	tree.set_root (root);
	if (!tree.write (path)) {
		error << string_compose (_("Could not save Plugin Tags info to %1"), path) << endmsg;
	}
}

void
PluginManager::load_tags ()
{
	vector<std::string> tmp;
	find_files_matching_pattern (tmp, plugin_metadata_search_path (), "plugin_tags");

	for (vector<std::string>::const_reverse_iterator p = tmp.rbegin ();
			p != (vector<std::string>::const_reverse_iterator)tmp.rend(); ++p) {
		std::string path = *p;
		info << string_compose (_("Loading plugin meta data file %1"), path) << endmsg;
		if (!Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
			return;
		}

		XMLTree tree;
		if (!tree.read (path)) {
			error << string_compose (_("Cannot parse plugin tag info from %1"), path) << endmsg;
			return;
		}

		for (XMLNodeConstIterator i = tree.root()->children().begin(); i != tree.root()->children().end(); ++i) {
			PluginType type;
			string id;
			string tags;
			string name;
			bool user_set;
			if (!(*i)->get_property (X_("type"), type) ||
			    !(*i)->get_property (X_("id"), id) ||
			    !(*i)->get_property (X_("tags"), tags) ||
			    !(*i)->get_property (X_("name"), name)) {
				continue;
			}
			if (!(*i)->get_property (X_("user-set"), user_set)) {
				user_set = false;
			}
			strip_whitespace_edges (tags);
			set_tags (type, id, tags, name, user_set ? FromUserFile : FromFactoryFile);
		}
	}
}

void
PluginManager::set_tags (PluginType t, string id, string tag, std::string name, TagType ttype)
{
	string sanitized = sanitize_tag (tag);

	PluginTag ps (to_generic_vst (t), id, sanitized, name, ttype);
	PluginTagList::const_iterator i = find (ptags.begin(), ptags.end(), ps);
	if (i == ptags.end()) {
		ptags.insert (ps);
	} else if ((uint32_t) ttype >= (uint32_t) (*i).tagtype) { // only overwrite if we are more important than the existing. Gui > UserFile > FactoryFile > Plugin
		ptags.erase (ps);
		ptags.insert (ps);
	}
	if (ttype == FromFactoryFile) {
		if (find (ftags.begin(), ftags.end(), ps) != ftags.end()) {
			ftags.erase (ps);
		}
		ftags.insert (ps);
	}
	if (ttype == FromGui) {
		PluginTagChanged (t, id, sanitized); /* EMIT SIGNAL */
	}
}

void
PluginManager::reset_tags (PluginInfoPtr const& pi)
{
	PluginTag ps (pi->type, pi->unique_id, pi->category, pi->name, FromPlug);

	PluginTagList::const_iterator j = find (ftags.begin(), ftags.end(), ps);
	if (j != ftags.end()) {
		ps = *j;
	}

	PluginTagList::const_iterator i = find (ptags.begin(), ptags.end(), ps);
	if (i != ptags.end()) {
		ptags.erase (ps);
		ptags.insert (ps);
		PluginTagChanged (ps.type, ps.unique_id, ps.tags); /* EMIT SIGNAL */
	}
}

std::string
PluginManager::sanitize_tag (const std::string to_sanitize) const
{
	if (to_sanitize.empty ()) {
		return "";
	}
	string sanitized = to_sanitize;
	vector<string> tags;
	if (!PBD::tokenize (sanitized, string(" ,\n"), std::back_inserter (tags), true)) {
#ifndef NDEBUG
		cerr << _("PluginManager::sanitize_tag could not tokenize string: ") << sanitized << endmsg;
#endif
		return "";
	}

	/* convert tokens to lower-case, space-separated list */
	sanitized = "";
	for (vector<string>::iterator t = tags.begin(); t != tags.end(); ++t) {
		if (t != tags.begin ()) {
			sanitized.append(" ");
		}
		sanitized.append (downcase (*t));
	}

	return sanitized;
}

std::vector<std::string>
PluginManager::get_all_tags (TagFilter tag_filter) const
{
	std::vector<std::string> ret;

	PluginTagList::const_iterator pt;
	for (pt = ptags.begin(); pt != ptags.end(); ++pt) {
		if ((*pt).tags.empty ()) {
			continue;
		}

		/* if favorites_only then we need to check the info ptr and maybe skip */
		if (tag_filter == OnlyFavorites) {
			PluginStatus stat ((*pt).type, (*pt).unique_id);
			PluginStatusList::const_iterator i = find (statuses.begin(), statuses.end(), stat);
			if ((i != statuses.end()) && (i->status == Favorite)) {
				/* it's a favorite! */
			} else {
				continue;
			}
		}
		if (tag_filter == NoHidden) {
			PluginStatus stat ((*pt).type, (*pt).unique_id);
			PluginStatusList::const_iterator i = find (statuses.begin(), statuses.end(), stat);
			if ((i != statuses.end()) && ((i->status == Hidden) || (i->status == Concealed))) {
				continue;
			}
		}

		/* parse each plugin's tag string into separate tags */
		vector<string> tags;
		if (!PBD::tokenize ((*pt).tags, string(" "), std::back_inserter (tags), true)) {
#ifndef NDEBUG
			cerr << _("PluginManager: Could not tokenize string: ") << (*pt).tags << endmsg;
#endif
			continue;
		}

		/* maybe add the tags we've found */
		for (vector<string>::iterator t = tags.begin(); t != tags.end(); ++t) {
			/* if this tag isn't already in the list, add it */
			vector<string>::iterator i = find (ret.begin(), ret.end(), *t);
			if (i == ret.end()) {
				ret.push_back (*t);
			}
		}
	}

	/* sort in alphabetical order */
	SortByTag sorter;
	sort (ret.begin(), ret.end(), sorter);

	return ret;
}


const ARDOUR::PluginInfoList&
PluginManager::windows_vst_plugin_info ()
{
#ifdef WINDOWS_VST_SUPPORT
	if (!_windows_vst_plugin_info) {
		windows_vst_refresh ();
	}
	return *_windows_vst_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

const ARDOUR::PluginInfoList&
PluginManager::mac_vst_plugin_info ()
{
#ifdef MACVST_SUPPORT
	assert(_mac_vst_plugin_info);
	return *_mac_vst_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

const ARDOUR::PluginInfoList&
PluginManager::lxvst_plugin_info ()
{
#ifdef LXVST_SUPPORT
	assert(_lxvst_plugin_info);
	return *_lxvst_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

const ARDOUR::PluginInfoList&
PluginManager::ladspa_plugin_info ()
{
	assert(_ladspa_plugin_info);
	return *_ladspa_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::lv2_plugin_info ()
{
#ifdef LV2_SUPPORT
	assert(_lv2_plugin_info);
	return *_lv2_plugin_info;
#else
	return _empty_plugin_info;
#endif
}

const ARDOUR::PluginInfoList&
PluginManager::au_plugin_info ()
{
#ifdef AUDIOUNIT_SUPPORT
	if (_au_plugin_info) {
		return *_au_plugin_info;
	}
#endif
	return _empty_plugin_info;
}

const ARDOUR::PluginInfoList&
PluginManager::lua_plugin_info ()
{
	assert(_lua_plugin_info);
	return *_lua_plugin_info;
}
