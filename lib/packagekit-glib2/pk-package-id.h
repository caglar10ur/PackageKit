/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007-2008 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#if !defined (__PACKAGEKIT_H_INSIDE__) && !defined (PK_COMPILATION)
#error "Only <packagekit.h> can be included directly."
#endif

#ifndef __PK_PACKAGE_ID_H
#define __PK_PACKAGE_ID_H

#include <glib.h>

G_BEGIN_DECLS

void		 pk_package_id_test			(gpointer		 user_data);
gchar		*pk_package_id_build			(const gchar		*name,
							 const gchar		*version,
							 const gchar		*arch,
							 const gchar		*data);
gboolean	 pk_package_id_check			(const gchar		*package_id);
gchar		**pk_package_id_split			(const gchar		*package_id);
gchar		*pk_package_id_to_printable		(const gchar		*package_id);
gboolean	 pk_package_id_equal_fuzzy_arch		(const gchar		*package_id1,
							 const gchar		*package_id2);
G_END_DECLS

#endif /* __PK_PACKAGE_ID_H */
