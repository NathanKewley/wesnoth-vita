/* Vita gio-lite: GIO cut down to GListModel, the one part of it Pango uses.
 *
 * Full GIO needs sockets, a resolver, file monitoring and D-Bus, none of which the Vita has, and
 * none of which a game's text rendering needs.
 */
#ifndef __GIO_TYPES_H__
#define __GIO_TYPES_H__

#include <glib-object.h>

#define GIO_AVAILABLE_IN_ALL _GLIB_EXTERN
#define GIO_AVAILABLE_IN_2_44 _GLIB_EXTERN
#define GIO_VAR _GLIB_EXTERN

#endif /* __GIO_TYPES_H__ */
