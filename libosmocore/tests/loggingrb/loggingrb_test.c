/* simple test for the debug interface */
/*
 * (C) 2008, 2009 by Holger Hans Peter Freyther <zecke@selfish.org>
 * (C) 2012-2013 by Katerina Barone-Adesi <kat.obsc@gmail.com>
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <osmocom/core/logging.h>
#include <osmocom/core/utils.h>
#include <osmocom/core/ringb.h>
#include <osmocom/vty/logging_rbvty.h>

enum {
	DRLL,
	DCC,
	DMM,
};

static const struct log_info_cat default_categories[] = {
	[DRLL] = {
		  .name = "DRLL",
		  .description = "A-bis Radio Link Layer (RLL)",
		  .color = "\033[1;31m",
		  .enabled = 1, .loglevel = LOGL_NOTICE,
		  },
	[DCC] = {
		 .name = "DCC",
		 .description = "Layer3 Call Control (CC)",
		 .color = "\033[1;32m",
		 .enabled = 1, .loglevel = LOGL_NOTICE,
		 },
	[DMM] = {
		 .name = NULL,
		 .description = "Layer3 Mobility Management (MM)",
		 .color = "\033[1;33m",
		 .enabled = 1, .loglevel = LOGL_NOTICE,
		 },
};

const struct log_info log_info = {
	.cat = default_categories,
	.num_cat = ARRAY_SIZE(default_categories),
};

int main(int argc, char **argv)
{
	struct log_target *ringbuf_target;

	log_init(&log_info, NULL);
	ringbuf_target = log_target_create_rbvty(NULL, 0x1000);
	log_add_target(ringbuf_target);
	log_set_all_filter(ringbuf_target, 1);
	log_set_print_filename(ringbuf_target, 0);

	log_parse_category_mask(ringbuf_target, "DRLL:DCC");
	log_parse_category_mask(ringbuf_target, "DRLL");
	DEBUGP(DCC, "You should not see this\n");

	log_parse_category_mask(ringbuf_target, "DRLL:DCC");
	DEBUGP(DRLL, "You should see this\n");
	DEBUGP(DCC, "You should see this\n");
	DEBUGP(DMM, "You should not see this\n");
	fprintf(stderr, ringbuffer_get_nth(ringbuf_target->tgt_rbvty.rb, 0));
	fprintf(stderr, ringbuffer_get_nth(ringbuf_target->tgt_rbvty.rb, 1));
	OSMO_ASSERT(!ringbuffer_get_nth(ringbuf_target->tgt_rbvty.rb, 2));

	return 0;
}
