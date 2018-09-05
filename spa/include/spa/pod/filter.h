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

#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <spa/param/props.h>
#include <spa/pod/iter.h>
#include <spa/pod/builder.h>
#include <spa/pod/compare.h>

static inline int spa_pod_choice_fix_default(struct spa_pod_choice *choice)
{
	void *val, *alt;
	int i, nvals;
	uint32_t type, size;

	nvals = SPA_POD_CHOICE_N_VALUES(choice);
	type = SPA_POD_CHOICE_VALUE_TYPE(choice);
	size = SPA_POD_CHOICE_VALUE_SIZE(choice);
	alt = val = SPA_POD_CHOICE_VALUES(choice);

	switch (choice->body.type) {
	case SPA_CHOICE_None:
		break;
	case SPA_CHOICE_Range:
	case SPA_CHOICE_Step:
		if (nvals > 1) {
			alt = SPA_MEMBER(alt, size, void);
			if (spa_pod_compare_value(type, val, alt) < 0)
				memcpy(val, alt, size);
		}
		if (nvals > 2) {
			alt = SPA_MEMBER(alt, size, void);
			if (spa_pod_compare_value(type, val, alt) > 0)
				memcpy(val, alt, size);
		}
		break;
	case SPA_CHOICE_Enum:
	{
		void *best = NULL;

		for (i = 1; i < nvals; i++) {
			alt = SPA_MEMBER(alt, size, void);
			if (spa_pod_compare_value(type, val, alt) == 0) {
				best = alt;
				break;
			}
			if (best == NULL)
				best = alt;
		}
		if (best)
			memcpy(val, best, size);

		if (nvals <= 1)
			choice->body.type = SPA_CHOICE_None;
		break;
	}
	case SPA_CHOICE_Flags:
		break;
	}
	return 0;
}

static inline int
spa_pod_filter_prop(struct spa_pod_builder *b,
	    const struct spa_pod_prop *p1,
	    const struct spa_pod_prop *p2)
{
	const struct spa_pod *v1, *v2;
	struct spa_pod_choice *nc;
	uint32_t nalt1, nalt2;
	void *alt1, *alt2, *a1, *a2;
	uint32_t type, size, p1c, p2c;
	int j, k;

	v1 = spa_pod_get_values(&p1->value, &nalt1, &p1c);
	alt1 = SPA_POD_BODY(v1);
	v2 = spa_pod_get_values(&p2->value, &nalt2, &p2c);
	alt2 = SPA_POD_BODY(v2);

	type = v1->type;
	size = v1->size;

	/* incompatible property types */
	if (type != v2->type || size != v2->size || p1->key != p2->key)
		return -EINVAL;

	if (p1c == SPA_CHOICE_None) {
		nalt1 = 1;
	} else {
		alt1 = SPA_MEMBER(alt1, size, void);
		nalt1--;
	}

	if (p2c == SPA_CHOICE_None) {
		nalt2 = 1;
	} else {
		alt2 = SPA_MEMBER(alt2, size, void);
		nalt2--;
	}

	/* start with copying the property */
	spa_pod_builder_prop(b, p1->key, 0);
	nc = spa_pod_builder_deref(b, spa_pod_builder_push_choice(b, 0, 0));

	/* default value */
	spa_pod_builder_primitive(b, v1);

	if ((p1c == SPA_CHOICE_None && p2c == SPA_CHOICE_None) ||
	    (p1c == SPA_CHOICE_None && p2c == SPA_CHOICE_Enum) ||
	    (p1c == SPA_CHOICE_Enum && p2c == SPA_CHOICE_None) ||
	    (p1c == SPA_CHOICE_Enum && p2c == SPA_CHOICE_Enum)) {
		int n_copied = 0;
		/* copy all equal values but don't copy the default value again */
		for (j = 0, a1 = alt1; j < nalt1; j++, a1 += size) {
			for (k = 0, a2 = alt2; k < nalt2; k++, a2 += size) {
				if (spa_pod_compare_value(type, a1, a2) == 0) {
					if (p1c == SPA_CHOICE_Enum || j > 0)
						spa_pod_builder_raw(b, a1, size);
					n_copied++;
				}
			}
		}
		if (n_copied == 0)
			return -EINVAL;
		nc->body.type = SPA_CHOICE_Enum;
	}

	if ((p1c == SPA_CHOICE_None && p2c == SPA_CHOICE_Range) ||
	    (p1c == SPA_CHOICE_Enum && p2c == SPA_CHOICE_Range)) {
		int n_copied = 0;
		/* copy all values inside the range */
		for (j = 0, a1 = alt1, a2 = alt2; j < nalt1; j++, a1 += size) {
			if (spa_pod_compare_value(type, a1, a2) < 0)
				continue;
			if (spa_pod_compare_value(type, a1, a2 + size) > 0)
				continue;
			spa_pod_builder_raw(b, a1, size);
			n_copied++;
		}
		if (n_copied == 0)
			return -EINVAL;
		nc->body.type = SPA_CHOICE_Enum;
	}

	if ((p1c == SPA_CHOICE_None && p2c == SPA_CHOICE_Step) ||
	    (p1c == SPA_CHOICE_Enum && p2c == SPA_CHOICE_Step)) {
		return -ENOTSUP;
	}

	if ((p1c == SPA_CHOICE_Range && p2c == SPA_CHOICE_None) ||
	    (p1c == SPA_CHOICE_Range && p2c == SPA_CHOICE_Enum)) {
		int n_copied = 0;
		/* copy all values inside the range */
		for (k = 0, a1 = alt1, a2 = alt2; k < nalt2; k++, a2 += size) {
			if (spa_pod_compare_value(type, a2, a1) < 0)
				continue;
			if (spa_pod_compare_value(type, a2, a1 + size) > 0)
				continue;
			spa_pod_builder_raw(b, a2, size);
			n_copied++;
		}
		if (n_copied == 0)
			return -EINVAL;
		nc->body.type = SPA_CHOICE_Enum;
	}

	if (p1c == SPA_CHOICE_Range && p2c == SPA_CHOICE_Range) {
		if (spa_pod_compare_value(type, alt1, alt2) < 0)
			spa_pod_builder_raw(b, alt2, size);
		else
			spa_pod_builder_raw(b, alt1, size);

		alt1 += size;
		alt2 += size;

		if (spa_pod_compare_value(type, alt1, alt2) < 0)
			spa_pod_builder_raw(b, alt1, size);
		else
			spa_pod_builder_raw(b, alt2, size);

		nc->body.type = SPA_CHOICE_Range;
	}

	if (p1c == SPA_CHOICE_None && p2c == SPA_CHOICE_Flags)
		return -ENOTSUP;

	if (p1c == SPA_CHOICE_Range && p2c == SPA_CHOICE_Step)
		return -ENOTSUP;

	if (p1c == SPA_CHOICE_Range && p2c == SPA_CHOICE_Flags)
		return -ENOTSUP;

	if (p1c == SPA_CHOICE_Enum && p2c == SPA_CHOICE_Flags)
		return -ENOTSUP;

	if (p1c == SPA_CHOICE_Step && p2c == SPA_CHOICE_None)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Step && p2c == SPA_CHOICE_Range)
		return -ENOTSUP;

	if (p1c == SPA_CHOICE_Step && p2c == SPA_CHOICE_Step)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Step && p2c == SPA_CHOICE_Enum)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Step && p2c == SPA_CHOICE_Flags)
		return -ENOTSUP;

	if (p1c == SPA_CHOICE_Flags && p2c == SPA_CHOICE_None)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Flags && p2c == SPA_CHOICE_Range)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Flags && p2c == SPA_CHOICE_Step)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Flags && p2c == SPA_CHOICE_Enum)
		return -ENOTSUP;
	if (p1c == SPA_CHOICE_Flags && p2c == SPA_CHOICE_Flags)
		return -ENOTSUP;

	spa_pod_builder_pop(b);
	spa_pod_choice_fix_default(nc);

	return 0;
}

static inline int spa_pod_filter_part(struct spa_pod_builder *b,
	       const struct spa_pod *pod, uint32_t pod_size,
	       const struct spa_pod *filter, uint32_t filter_size)
{
	const struct spa_pod *pp, *pf;
	int res = 0;

	pf = filter;

	SPA_POD_FOREACH(pod, pod_size, pp) {
		bool do_copy = false, do_advance = false;
		uint32_t filter_offset = 0;

		switch (SPA_POD_TYPE(pp)) {
		case SPA_TYPE_Object:
			if (pf != NULL) {
				struct spa_pod_object *obj = (struct spa_pod_object *) pp;
				struct spa_pod_prop *p1, *p2;

				if (SPA_POD_TYPE(pf) != SPA_POD_TYPE(pp))
					return -EINVAL;

				spa_pod_builder_push_object(b, obj->body.type, obj->body.id);
				SPA_POD_OBJECT_FOREACH(obj, p1) {
					p2 = spa_pod_find_prop(pf, p1->key);
					if (p2 != NULL)
						res = spa_pod_filter_prop(b, p1, p2);
					else
						spa_pod_builder_raw_padded(b, p1, SPA_POD_PROP_SIZE(p1));
					if (res < 0)
						break;
				}
				spa_pod_builder_pop(b);
				do_advance = true;
			}
			else
				do_copy = true;
			break;

		case SPA_TYPE_Struct:
			if (pf != NULL) {
				if (SPA_POD_TYPE(pf) != SPA_POD_TYPE(pp))
					return -EINVAL;

				filter_offset = sizeof(struct spa_pod_struct);
				spa_pod_builder_push_struct(b);
				res = spa_pod_filter_part(b,
					SPA_MEMBER(pp,filter_offset,void),
					SPA_POD_SIZE(pp) - filter_offset,
					SPA_MEMBER(pf,filter_offset,void),
					SPA_POD_SIZE(pf) - filter_offset);
			        spa_pod_builder_pop(b);
				do_advance = true;
			}
			else
				do_copy = true;
			break;

		default:
			if (pf != NULL) {
				if (SPA_POD_SIZE(pp) != SPA_POD_SIZE(pf))
					return -EINVAL;
				if (memcmp(pp, pf, SPA_POD_SIZE(pp)) != 0)
					return -EINVAL;
				do_advance = true;
			}
			do_copy = true;
			break;
		}
		if (do_copy)
			spa_pod_builder_raw_padded(b, pp, SPA_POD_SIZE(pp));
		if (do_advance) {
			pf = spa_pod_next(pf);
			if (!spa_pod_is_inside(filter, filter_size, pf))
				pf = NULL;
		}
		if (res < 0)
			break;
	}
	return res;
}

static inline int
spa_pod_filter(struct spa_pod_builder *b,
	       struct spa_pod **result,
	       const struct spa_pod *pod,
	       const struct spa_pod *filter)
{
	int res;
	struct spa_pod_builder_state state;

        spa_return_val_if_fail(pod != NULL, -EINVAL);
        spa_return_val_if_fail(b != NULL, -EINVAL);

	if (filter == NULL) {
		*result = spa_pod_builder_deref(b,
			spa_pod_builder_raw_padded(b, pod, SPA_POD_SIZE(pod)));
		return 0;
	}

	spa_pod_builder_get_state(b, &state);
	if ((res = spa_pod_filter_part(b, pod, SPA_POD_SIZE(pod), filter, SPA_POD_SIZE(filter))) < 0)
		spa_pod_builder_reset(b, &state);
	else
		*result = spa_pod_builder_deref(b, state.offset);

	return res;
}
