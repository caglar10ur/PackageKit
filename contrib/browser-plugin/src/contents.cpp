/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is packagekit-plugin code.
 *
 * The Initial Developer of the Original Code is
 * Red Hat, Inc.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#define MOZ_X11

#include <config.h>

#include <string.h>

#include <glib/gi18n-lib.h>

#include <cairo-xlib.h>
#include <dlfcn.h>
#include <pango/pangocairo.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gio/gdesktopappinfo.h>
#include <packagekit-glib/packagekit.h>

#include "plugin.h"

#define MARGIN 5

#if !GTK_CHECK_VERSION(2,14,0)
#define GTK_ICON_LOOKUP_FORCE_SIZE (GtkIconLookupFlags) 0
#endif

////////////////////////////////////////
//
// PkpContents class implementation
//

static std::vector<std::string>
splitString(const gchar *str)
{
	std::vector<std::string> v;

	if (str) {
		char **split = g_strsplit(str, " ", -1);
		for (char **s = split; *s; s++) {
			char *stripped = strdup(*s);
			g_strstrip(stripped);
			v.push_back(stripped);
			g_free(stripped);
		}

		g_strfreev(split);
	}

	return v;
}

PkpContents::PkpContents(const gchar *displayName, const gchar *packageNames) :
	mPlugin(0),
	mStatus(IN_PROGRESS),
	mAppInfo(0),
	mDisplayName(displayName),
	mPackageNames(splitString(packageNames)),
	mLayout(0),
	mInstallPackageProxy(0),
	mInstallPackageCall(0)
{
	recheck();
}

PkpContents::~PkpContents()
{
	clearLayout();

	if (mAppInfo != 0) {
		g_object_unref(mAppInfo);
		mAppInfo = 0;
	}

	if (mInstallPackageCall != 0) {
		dbus_g_proxy_cancel_call(mInstallPackageProxy, mInstallPackageCall);
		g_object_unref(mInstallPackageProxy);
		mInstallPackageProxy = 0;
		mInstallPackageCall = 0;
	}

	while (!mClients.empty())
		removeClient(mClients.front());
}

void PkpContents::recheck()
{
	mStatus = IN_PROGRESS;
	mAvailableVersion = "";
	mAvailablePackageName = "";
	mInstalledPackageName = "";

	for (std::vector<std::string>::iterator i = mPackageNames.begin(); i != mPackageNames.end(); i++) {
		GError *error = NULL;
		PkClient *client = pk_client_new();
		gchar **package_ids;
		package_ids = pk_package_ids_from_id (i->c_str());
		if (!pk_client_resolve(client, PK_FILTER_ENUM_NONE, package_ids, &error)) {
			g_warning("%s", error->message);
			g_clear_error(&error);
			g_object_unref(client);
		} else {
			g_signal_connect(client, "package", G_CALLBACK(onClientPackage), this);
			g_signal_connect(client, "error-code", G_CALLBACK(onClientErrorCode), this);
			g_signal_connect(client, "finished", G_CALLBACK(onClientFinished), this);
			mClients.push_back(client);
		}
		g_strfreev (package_ids);
	}

	if (mClients.empty() && getStatus() == IN_PROGRESS)
		setStatus(UNAVAILABLE);
}

void PkpContents::removeClient(PkClient *client)
{
	for (std::vector<PkClient *>::iterator i = mClients.begin(); i != mClients.end(); i++) {
		if (*i == client) {
			mClients.erase(i);
			g_signal_handlers_disconnect_by_func(client, (void *)onClientPackage, this);
			g_signal_handlers_disconnect_by_func(client, (void *)onClientErrorCode, this);
			g_signal_handlers_disconnect_by_func(client, (void *)onClientFinished, this);
			g_object_unref(client);
			break;
		}
	}

	if (mClients.empty()) {
		if (getStatus() == IN_PROGRESS)
			setStatus(UNAVAILABLE);
	}
}

void
PkpContents::setStatus(PackageStatus status)
{
	if (mStatus != status) {
		mStatus = status;
		clearLayout();
		refresh();
	}
}

void
PkpContents::setAvailableVersion(const gchar *version)
{
	mAvailableVersion = version;
	clearLayout();
	refresh();
}

void
PkpContents::setAvailablePackageName(const gchar *name)
{
	mAvailablePackageName = name;
}

void
PkpContents::setInstalledPackageName(const gchar *name)
{
	mInstalledPackageName = name;
}

void
PkpContents::setInstalledVersion(const gchar *version)
{
	mInstalledVersion = version;
	clearLayout();
	refresh();
}

void
PkpContents::clearLayout()
{
	if (mLayout) {
		g_object_unref(mLayout);
		mLayout = 0;
	}
}

static void
append_markup(GString *str, const gchar *format, ...)
{
	va_list vap;

	va_start(vap, format);
	char *tmp = g_markup_vprintf_escaped(format, vap);
	va_end(vap);

	g_string_append(str, tmp);
	g_free(tmp);
}

static guint32
rgba_from_gdk_color(GdkColor *color)
{
	return (((color->red >> 8) << 24) |
		((color->green >> 8) << 16) |
		((color->blue >> 8) << 8) |
		0xff);
}

static void
set_source_from_rgba(cairo_t *cr, guint32 rgba)
{
	cairo_set_source_rgba(cr,
			 ((rgba & 0xff000000) >> 24) / 255.,
			 ((rgba & 0x00ff0000) >> 16) / 255.,
			 ((rgba & 0x0000ff00) >> 8) / 255.,
			  (rgba & 0x000000ff) / 255.);

}

/* Retrieve the system colors and fonts.
 * This looks incredibly expensive .... to create a GtkWindow for
 * every expose ... but actually it's only moderately expensive;
 * Creating a GtkWindow is just normal GObject creation overhead --
 * the extra expense beyond that will come when we actually create
 * the window.
 */
static void
get_style(PangoFontDescription **font_desc, guint32 *foreground, guint32 *background, guint32 *link)
{
	GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	gtk_widget_ensure_style(window);

	*foreground = rgba_from_gdk_color(&window->style->text[GTK_STATE_NORMAL]);
	*background = rgba_from_gdk_color(&window->style->base[GTK_STATE_NORMAL]);

	GdkColor link_color = { 0, 0, 0, 0xeeee };
	GdkColor *tmp = NULL;

	gtk_widget_style_get (GTK_WIDGET (window), "link-color", &tmp, NULL);
	if (tmp != NULL) {
		link_color = *tmp;
		gdk_color_free(tmp);
	}

	*link = rgba_from_gdk_color(&link_color);

	*font_desc = pango_font_description_copy(window->style->font_desc);

	gtk_widget_destroy(window);
}

void
PkpContents::ensureLayout(cairo_t *cr, PangoFontDescription *font_desc, guint32 link_color)
{
	GString *markup = g_string_new(NULL);

	if (mLayout)
		return;

	mLayout = pango_cairo_create_layout(cr);
	pango_layout_set_font_description(mLayout, font_desc);

	/* WARNING: Any changes to what links are created here will require corresponding
	 * changes to the buttonRelease() method
	 */
	switch (mStatus) {
	case IN_PROGRESS:
		/* TRANSLATORS: when we are getting data from the daemon */
		append_markup(markup, _("Getting package information..."));
		break;
	case INSTALLED:
		if (mAppInfo != 0) {
			append_markup(markup, "<span color='#%06x' underline='single'>", link_color >> 8);
			/* TRANSLATORS: run an applicaiton */
			append_markup(markup, _("Run %s"), mDisplayName.c_str());
			append_markup(markup, "</span>");
		} else
			append_markup(markup, "<big>%s</big>", mDisplayName.c_str());
		if (!mInstalledVersion.empty())
			/* TRANSLATORS: show the installed version of a package */
			append_markup(markup, "\n<small>%s: %s</small>", _("Installed version"), mInstalledVersion.c_str());
		break;
	case UPGRADABLE:
		append_markup(markup, "<big>%s</big>", mDisplayName.c_str());
		if (mAppInfo != 0) {
			if (!mInstalledVersion.empty()) {
				append_markup(markup, "\n<span color='#%06x' underline='single'>", link_color >> 8);
				/* TRANSLATORS: run the application now */
				append_markup(markup, _("Run version %s now"), mInstalledVersion.c_str());
				append_markup(markup, "</span>");
			} else {
				append_markup(markup,
				              "\n<span color='#%06x' underline='single'>%s</span>",
					      /* TRANSLATORS: run the application now */
					      _("Run now"), link_color >> 8);
		        }
		}

		append_markup(markup, "\n<span color='#%06x' underline='single'>", link_color >> 8);
		/* TRANSLATORS: update to a new version of the package */
		append_markup(markup, _("Update to version %s"), mAvailableVersion.c_str());
		append_markup(markup, "</span>");
		break;
	case AVAILABLE:
		append_markup(markup, "<span color='#%06x' underline='single'>", link_color >> 8);
		/* TRANSLATORS: To install a package */
		append_markup(markup, _("Install %s now"), mDisplayName.c_str());
		append_markup(markup, "</span>");
		/* TRANSLATORS: the version of the package */
		append_markup(markup, "\n<small>%s: %s</small>", _("Version"), mAvailableVersion.c_str());
		break;
	case UNAVAILABLE:
		append_markup(markup, "<big>%s</big>", mDisplayName.c_str());
		/* TRANSLATORS: noting found, so can't install */
		append_markup(markup, "\n<small>%s</small>", _("No packages found for your system"));
		break;
	case INSTALLING:
		append_markup(markup, "<big>%s</big>", mDisplayName.c_str());
		/* TRANSLATORS: package is being installed */
		append_markup(markup, "\n<small>%s</small>", _("Installing..."));
		break;
	}

	pango_layout_set_markup(mLayout, markup->str, -1);
	g_string_free(markup, TRUE);
}

void
PkpContents::refresh()
{
	if (mPlugin != 0)
		mPlugin->refresh();
}

void
PkpContents::setPlugin(PkpPluginInstance *plugin)
{
	mPlugin = plugin;
}

gchar *
PkpContents::getBestDesktopFile()
{
	GPtrArray *array = NULL;
	PkDesktop *desktop;
	gboolean ret;
	gchar *data = NULL;
	const gchar *package;

	/* open desktop database */
	desktop = pk_desktop_new();
	ret = pk_desktop_open_database(desktop, NULL);
	if (!ret)
		goto out;

	/* get files */
	package = mInstalledPackageName.c_str();
	array = pk_desktop_get_shown_for_package(desktop, package, NULL);
	if (array == NULL)
		goto out;
	if (array->len == 0)
		goto out;

	/* just use the first entry */
	data = g_strdup((const gchar*) g_ptr_array_index(array, 0));

out:
	if (array != NULL) {
		g_ptr_array_foreach(array, (GFunc) g_free, NULL);
		g_ptr_array_free (array, TRUE);
	}
	g_object_unref(desktop);
	return data;
}

gchar *
PkpContents::getPackageIcon()
{
	gboolean ret;
	GKeyFile *file;
	gchar *data = NULL;
	const gchar *filename;

	/* get data from the best file */
	file = g_key_file_new();
	filename = getBestDesktopFile();
	if (filename == NULL)
		goto out;

	ret = g_key_file_load_from_file(file, filename, G_KEY_FILE_NONE, NULL);
	if (!ret) {
		g_warning("failed to open %s", filename);
		goto out;
	}
	data = g_key_file_get_string(file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
	g_key_file_free(file);
out:
	return data;
}

void
PkpContents::draw(cairo_t *cr)
{
	guint32 foreground, background, link;
	PangoFontDescription *font_desc;
	guint x = mPlugin->getX();
	guint y = mPlugin->getY();
	cairo_surface_t *surface = NULL;
	const gchar *filename;
	GtkIconTheme *theme;
	GdkPixbuf *pixbuf;

	/* get properties */
	get_style(&font_desc, &foreground, &background, &link);

        /* fill background */
	set_source_from_rgba(cr, background);
	cairo_rectangle(cr, x, y, mPlugin->getWidth(), mPlugin->getHeight());
	cairo_fill(cr);

        /* grey outline */
	cairo_set_source_rgb(cr, 0.5, 0.5, 0.5);
	cairo_rectangle(cr, x + 0.5, y + 0.5, mPlugin->getWidth() - 1, mPlugin->getHeight() - 1);
	cairo_set_line_width(cr, 1);
	cairo_stroke(cr);


	/* get themed icon */
	filename = getPackageIcon();
	if (filename == NULL)
		filename = "package-x-generic";
	theme = gtk_icon_theme_get_default();
	pixbuf = gtk_icon_theme_load_icon(theme, filename, 48, GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
	if (pixbuf == NULL)
		goto skip;
	gdk_cairo_set_source_pixbuf(cr, pixbuf, x + MARGIN, y + MARGIN);
	cairo_rectangle(cr, x + MARGIN, y + MARGIN, 48, 48);
	cairo_fill(cr);
	cairo_surface_destroy(surface);
	g_object_unref(pixbuf);

skip:
	/* write text */
	ensureLayout(cr, font_desc, link);
	cairo_move_to(cr,(x + MARGIN*2) + 48, y + MARGIN + MARGIN);
	set_source_from_rgba(cr, foreground);
	pango_cairo_show_layout(cr, mLayout);
}

/* Cut and paste from pango-layout.c; determines if a layout iter is on
 * a line terminated by a real line break (rather than a line break from
 * wrapping). We use this to determine whether the empty run at the end
 * of a display line should be counted as a break between links or not.
 *
 * (Code in pango-layout.c is by me, Copyright Red Hat, and hereby relicensed
 * to the license of this file)
 */
static gboolean
line_is_terminated (PangoLayoutIter *iter)
{
	/* There is a real terminator at the end of each paragraph other
	 * than the last.
	 */
	PangoLayoutLine *line = pango_layout_iter_get_line(iter);
	GSList *lines = pango_layout_get_lines(pango_layout_iter_get_layout(iter));
	GSList *link = g_slist_find(lines, line);
	if (!link) {
		g_warning("Can't find line in layout line list\n");
		return FALSE;
	}

	if (link->next) {
		PangoLayoutLine *next_line = (PangoLayoutLine *)link->next->data;
		if (next_line->is_paragraph_start)
			return TRUE;
	}

	return FALSE;
}

/* This function takes an X,Y position and determines whether it is over one
 * of the underlined portions of the layout (a link). It works by iterating
 * through the runs of the layout (a run is a segment with a consistent
 * font and display attributes, more or less), and counting the underlined
 * segments that we see. A segment that is underlined could be broken up
 * into multiple runs if it is drawn with multiple fonts due to fonts
 * substitution, so we actually count non-underlined => underlined
 * transitions.
 */
int
PkpContents::getLinkIndex(int x, int y)
{
	/* Coordinates are relative to origin of plugin (different from drawing) */

	if (!mLayout)
		return -1;

	x -= (MARGIN * 2) + 48;
	y -= (MARGIN * 2);

	int index;
	int trailing;
	if (!pango_layout_xy_to_index(mLayout, x * PANGO_SCALE, y * PANGO_SCALE, &index, &trailing))
		return - 1;

	PangoLayoutIter *iter = pango_layout_get_iter(mLayout);
	int seen_links = 0;
	bool in_link = false;
	int result = -1;

	while (TRUE) {
		PangoLayoutRun *run = pango_layout_iter_get_run(iter);
		if (run) {
			PangoItem *item = run->item;
			PangoUnderline uline = PANGO_UNDERLINE_NONE;

			for (GSList *l = item->analysis.extra_attrs; l; l = l->next) {
				PangoAttribute *attr = (PangoAttribute *)l->data;
				if (attr->klass->type == PANGO_ATTR_UNDERLINE) {
					uline = (PangoUnderline)((PangoAttrInt *)attr)->value;
				}
			}

			if (uline == PANGO_UNDERLINE_NONE)
				in_link = FALSE;
			else if (!in_link) {
				in_link = TRUE;
				seen_links++;
			}

			if (item->offset <= index && index < item->offset + item->length) {
				if (in_link)
					result = seen_links - 1;

				break;
			}
		} else {
			/* We have an empty run at the end of each line. A line break doesn't
			 * terminate the link, but a real newline does.
			 */
			if (line_is_terminated(iter))
				in_link = FALSE;
		}

		if (!pango_layout_iter_next_run (iter))
			break;
	}

	pango_layout_iter_free(iter);

	return result;
}

void
PkpContents::buttonPress(int x, int y, Time time)
{
}

void
PkpContents::buttonRelease(int x, int y, Time time)
{
	int index = getLinkIndex(x, y);
	if (index < 0)
		return;

	switch (mStatus) {
	case IN_PROGRESS:
	case INSTALLING:
	case UNAVAILABLE:
		break;
	case INSTALLED:
		if (mAppInfo != 0)
			runApplication(time);
		break;
	case UPGRADABLE:
		if (mAppInfo != 0 && index == 0)
			runApplication(time);
		else
			installPackage(time);
		break;
	case AVAILABLE:
		if (!mAvailablePackageName.empty())
			installPackage(time);
		break;
	}
}

void
PkpContents::motion(int x, int y)
{
}

void
PkpContents::enter(int x, int y)
{
}

void
PkpContents::leave(int x, int y)
{
}

static guint32
get_server_timestamp()
{
	GtkWidget *invisible = gtk_invisible_new();
	gtk_widget_realize(invisible);
	return gdk_x11_get_server_time(invisible->window);
	gtk_widget_destroy(invisible);
}

void
PkpContents::runApplication (Time time)
{
	GError *error = NULL;
#ifdef HAVE_GDK_APP_LAUNCH_CONTEXT_NEW
	GdkAppLaunchContext *context;
#endif

	if (mAppInfo == 0) {
		g_warning("Didn't find application to launch");
		return;
	}

	if (time == 0)
		time = get_server_timestamp();

#ifdef HAVE_GDK_APP_LAUNCH_CONTEXT_NEW
	context = gdk_app_launch_context_new();
	gdk_app_launch_context_set_timestamp(context, time);
	if (!g_app_info_launch(mAppInfo, NULL, G_APP_LAUNCH_CONTEXT (context), &error)) {
#else
	if (!g_app_info_launch(mAppInfo, NULL, NULL, &error)) {
#endif
		g_warning("%s\n", error->message);
		g_clear_error(&error);
		return;
	}

#ifdef HAVE_GDK_APP_LAUNCH_CONTEXT_NEW
	if (context != NULL)
		g_object_unref(context);
#endif
}

void
PkpContents::installPackage (Time time)
{
	GdkEvent *event;
	GdkWindow *window;
	guint xid = 0;

	if (mAvailablePackageName.empty()) {
		g_warning("No available package to install");
		return;
	}

	if (mInstallPackageCall != 0) {
		g_warning("Already installing package");
		return;
	}

	/* Get a proxy to the *session* PackageKit service */
	DBusGConnection *connection = dbus_g_bus_get (DBUS_BUS_SESSION, NULL);
	mInstallPackageProxy = dbus_g_proxy_new_for_name(connection,
							 "org.freedesktop.PackageKit",
							 "/org/freedesktop/PackageKit",
							 "org.freedesktop.PackageKit");

	/* will be NULL when activated not using a keyboard or a mouse */
	event = gtk_get_current_event();
	if (event != NULL && event->any.window != NULL) {
		window = gdk_window_get_toplevel(event->any.window);
		xid = GDK_DRAWABLE_XID(window);
	}

	mInstallPackageCall = dbus_g_proxy_begin_call_with_timeout(mInstallPackageProxy,
								   "InstallPackageName",
								   onInstallPackageFinished,
								   this,
								   (GDestroyNotify)0,
								   24 * 60 * 1000 * 1000, /* one day */
								   G_TYPE_UINT, xid, /* xid */
								   G_TYPE_UINT, 0, /* timespec */
								   G_TYPE_STRING, mAvailablePackageName.c_str(),
								   G_TYPE_INVALID,
								   G_TYPE_INVALID);

	 setStatus(INSTALLING);
}

void
PkpContents::onClientPackage(PkClient *client, const PkPackageObj *obj, PkpContents *contents)
{
	gchar *filename;

	/* if we didn't use displayname, use the summary */
	if (contents->mDisplayName.size() == 0)
		contents->mDisplayName = obj->summary;

	/* parse the data */
	if (obj->info == PK_INFO_ENUM_AVAILABLE) {
		if (contents->getStatus() == IN_PROGRESS)
			contents->setStatus(AVAILABLE);
		else if (contents->getStatus() == INSTALLED)
			contents->setStatus(UPGRADABLE);
		contents->setAvailableVersion(obj->id->version);
		contents->setAvailablePackageName(obj->id->name);

#if 0
		/* if we have data from the repo, override the user:
		 *  * we don't want the remote site pretending to install another package
		 *  * it might be localised if the backend supports it */
		if (obj->summary != NULL && obj->summary[0] != '\0')
			contents->mDisplayName = obj->summary;
#endif

	} else if (obj->info == PK_INFO_ENUM_INSTALLED) {
		if (contents->getStatus() == IN_PROGRESS)
			contents->setStatus(INSTALLED);
		else if (contents->getStatus() == AVAILABLE)
			contents->setStatus(UPGRADABLE);
		contents->setInstalledVersion(obj->id->version);
		contents->setInstalledPackageName(obj->id->name);

		/* get desktop file information */
		filename = contents->getBestDesktopFile();
		if (filename != NULL) {
			contents->mAppInfo = G_APP_INFO(g_desktop_app_info_new_from_filename(filename));
#if 0
			/* override, as this will have translation */
			contents->mDisplayName = g_app_info_get_name(contents->mAppInfo);
#endif
		}
		g_free(filename);

		if (contents->mAppInfo != 0)
			contents->setStatus(INSTALLED);
	}
}

void
PkpContents::onClientErrorCode(PkClient *client, PkErrorCodeEnum code, const gchar *details, PkpContents *contents)
{
	g_warning("Error getting data from PackageKit: %s\n", details);
	contents->removeClient(client);
}

void
PkpContents::onClientFinished(PkClient *client, PkExitEnum exit, guint runtime, PkpContents *contents)
{
	contents->removeClient(client);
}

void
PkpContents::onInstallPackageFinished (DBusGProxy *proxy, DBusGProxyCall *call, void *user_data)
{
	PkpContents *contents = (PkpContents *)user_data;

	GError *error = NULL;
	if (!dbus_g_proxy_end_call(proxy, call, &error, G_TYPE_INVALID)) {
		g_warning("Error occurred during install: %s", error->message);
		g_clear_error(&error);
	}

	g_object_unref(contents->mInstallPackageProxy);
	contents->mInstallPackageProxy = 0;
	contents->mInstallPackageCall = 0;

	contents->recheck();
}