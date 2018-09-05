/* Simple Plugin API
 * Copyright (C) 2018 Wim Taymans <wim.taymans@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __SPA_DEBUG_POD_H__
#define __SPA_DEBUG_POD_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <spa/debug/mem.h>
#include <spa/debug/types.h>
#include <spa/pod/pod.h>
#include <spa/pod/iter.h>

#ifndef spa_debug
#define spa_debug(...)	({ fprintf(stderr, __VA_ARGS__);fputc('\n', stderr); })
#endif

#if 0
static inline int
spa_debug_pod_prop(int indent, const struct spa_type_info *info, struct spa_pod_prop *prop)
{
	void *alt;
	int i;
	const struct spa_type_info *ti = spa_debug_type_find(info, prop->key);

	spa_debug("%*s" "Prop: key %s, flags %d", indent, "",
			ti ? ti->name : "unknown", prop->flags);

	info = ti ? ti->values : info;

	if (prop->flags & SPA_POD_PROP_FLAG_UNSET)
		spa_debug("%*s" "Unset (Default):", indent + 2, "");
	else
		spa_debug("%*s" "Value: size %u", indent + 2, "", prop->value.size);

	spa_debug_pod_value(indent + 4, info, prop->value.type, SPA_POD_BODY(&prop->value),
			prop->value.size);

	i = 0;
	switch (prop->flags & SPA_POD_PROP_RANGE_MASK) {
	case SPA_POD_PROP_RANGE_NONE:
		break;
	case SPA_POD_PROP_RANGE_MIN_MAX:
		SPA_POD_PROP_ALTERNATIVE_FOREACH(b, size, alt) {
			if (i == 0)
				spa_debug("%*s" "Min: ", indent + 2, "");
			else if (i == 1)
				spa_debug("%*s" "Max: ", indent + 2, "");
			else
				break;
			spa_debug_pod_value(indent + 4, info, prop->value.type, alt, prop->value.size);
			i++;
		}
		break;
	case SPA_POD_PROP_RANGE_STEP:
		SPA_POD_PROP_ALTERNATIVE_FOREACH(b, size, alt) {
			if (i == 0)
				spa_debug("%*s" "Min:  ", indent + 2, "");
			else if (i == 1)
				spa_debug("%*s" "Max:  ", indent + 2, "");
			else if (i == 2)
				spa_debug("%*s" "Step: ", indent + 2, "");
			else
				break;
			spa_debug_pod_value(indent + 4, info, prop->value.type, alt, prop->value.size);
			i++;
		}
		break;
	case SPA_POD_PROP_RANGE_ENUM:
		SPA_POD_PROP_ALTERNATIVE_FOREACH(b, size, alt) {
			if (i == 0) {
				spa_debug("%*s" "Enum:", indent + 2, "");
			}
			spa_debug_pod_value(indent + 4, info, prop->value.type, alt, prop->value.size);
			i++;
		}
		break;
	case SPA_POD_PROP_RANGE_FLAGS:
		break;
	}
	break;
}
#endif

static inline int
spa_debug_pod_value(int indent, const struct spa_type_info *info,
		uint32_t type, void *body, uint32_t size)
{
	switch (type) {
	case SPA_TYPE_Bool:
		spa_debug("%*s" "Bool %d", indent, "", *(int32_t *) body);
		break;
	case SPA_TYPE_Id:
		spa_debug("%*s" "Id %d (%s)", indent, "", *(int32_t *) body,
		       spa_debug_type_find_name(info, *(int32_t *) body));
		break;
	case SPA_TYPE_Int:
		spa_debug("%*s" "Int %d", indent, "", *(int32_t *) body);
		break;
	case SPA_TYPE_Long:
		spa_debug("%*s" "Long %" PRIi64 "", indent, "", *(int64_t *) body);
		break;
	case SPA_TYPE_Float:
		spa_debug("%*s" "Float %f", indent, "", *(float *) body);
		break;
	case SPA_TYPE_Double:
		spa_debug("%*s" "Double %f", indent, "", *(double *) body);
		break;
	case SPA_TYPE_String:
		spa_debug("%*s" "String \"%s\"", indent, "", (char *) body);
		break;
	case SPA_TYPE_Fd:
		spa_debug("%*s" "Fd %d", indent, "", *(int *) body);
		break;
	case SPA_TYPE_Pointer:
	{
		struct spa_pod_pointer_body *b = body;
		spa_debug("%*s" "Pointer %s %p", indent, "",
		       spa_debug_type_find_name(SPA_TYPE_ROOT, b->type), b->value);
		break;
	}
	case SPA_TYPE_Rectangle:
	{
		struct spa_rectangle *r = body;
		spa_debug("%*s" "Rectangle %dx%d", indent, "", r->width, r->height);
		break;
	}
	case SPA_TYPE_Fraction:
	{
		struct spa_fraction *f = body;
		spa_debug("%*s" "Fraction %d/%d", indent, "", f->num, f->denom);
		break;
	}
	case SPA_TYPE_Bitmap:
		spa_debug("%*s" "Bitmap", indent, "");
		break;
	case SPA_TYPE_Array:
	{
		struct spa_pod_array_body *b = body;
		void *p;
		const struct spa_type_info *ti = spa_debug_type_find(SPA_TYPE_ROOT, b->child.type);

		spa_debug("%*s" "Array: child.size %d, child.type %s", indent, "",
		       b->child.size, ti ? ti->name : "unknown");

		SPA_POD_ARRAY_BODY_FOREACH(b, size, p)
			spa_debug_pod_value(indent + 2, info, b->child.type, p, b->child.size);
		break;
	}
	case SPA_TYPE_Choice:
	{
		struct spa_pod_choice_body *b = body;
		void *p;
		const struct spa_type_info *ti = spa_debug_type_find(spa_type_choice, b->type);

		spa_debug("%*s" "Choice: type %s, flags %08x %d %d", indent, "",
		       ti ? ti->name : "unknown", b->flags, size, b->child.size);

		SPA_POD_CHOICE_BODY_FOREACH(b, size, p)
			spa_debug_pod_value(indent + 2, info, b->child.type, p, b->child.size);
		break;
	}
	case SPA_TYPE_Struct:
	{
		struct spa_pod *b = body, *p;
		spa_debug("%*s" "Struct: size %d", indent, "", size);
		SPA_POD_FOREACH(b, size, p)
			spa_debug_pod_value(indent + 2, info, p->type, SPA_POD_BODY(p), p->size);
		break;
	}
	case SPA_TYPE_Object:
	{
		struct spa_pod_object_body *b = body;
		struct spa_pod_prop *p;
		const struct spa_type_info *ti, *ii;

		ti = spa_debug_type_find(info, b->type);
		ii = ti ? spa_debug_type_find(ti->values, 0) : NULL;
		ii = ii ? spa_debug_type_find(ii->values, b->id) : NULL;

		spa_debug("%*s" "Object: size %d, type %s, id %s", indent, "", size,
		       ti ? ti->name : "unknown", ii ? ii->name : "unknown");

		info = ti ? ti->values : info;

		SPA_POD_OBJECT_BODY_FOREACH(b, size, p) {
			ii = spa_debug_type_find(info, p->key);

			spa_debug("%*s" "Prop: key %s, flags %08x", indent+2, "",
					ii ? ii->name : "unknown", p->flags);

			spa_debug_pod_value(indent + 4, ii->values,
					p->value.type,
					SPA_POD_CONTENTS(struct spa_pod_prop, p),
					p->value.size);
		}
		break;
	}
	case SPA_TYPE_Sequence:
	{
		struct spa_pod_sequence_body *b = body;
		const struct spa_type_info *ti, *ii;
		struct spa_pod_control *c;

		ti = spa_debug_type_find(info, b->unit);

		spa_debug("%*s" "Sequence: size %d, unit %s", indent, "", size,
		       ti ? ti->name : "unknown");

		SPA_POD_SEQUENCE_BODY_FOREACH(b, size, c) {
			ii = spa_debug_type_find(spa_type_control, c->type);

			spa_debug("%*s" "Control: offset %d, type %s", indent+2, "",
					c->offset, ii ? ii->name : "unknown");

			spa_debug_pod_value(indent + 4, ii->values,
					c->value.type,
					SPA_POD_CONTENTS(struct spa_pod_control, c),
					c->value.size);
		}
		break;
	}
	case SPA_TYPE_Bytes:
		spa_debug("%*s" "Bytes", indent, "");
		spa_debug_mem(indent + 2, body, size);
		break;
	case SPA_TYPE_None:
		spa_debug("%*s" "None", indent, "");
		spa_debug_mem(indent + 2, body, size);
		break;
	default:
		spa_debug("%*s" "unhandled POD type %d", indent, "", type);
		break;
	}
	return 0;
}

static inline int spa_debug_pod(int indent,
		const struct spa_type_info *info, const struct spa_pod *pod)
{
	return spa_debug_pod_value(indent, info ? info : SPA_TYPE_ROOT,
			SPA_POD_TYPE(pod),
			SPA_POD_BODY(pod),
			SPA_POD_BODY_SIZE(pod));
}

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* __SPA_DEBUG_POD_H__ */
