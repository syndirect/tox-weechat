/*
 * Copyright (c) 2014 Håvard Pettersson <haavard.pettersson@gmail.com>
 *
 * This file is part of Tox-WeeChat.
 *
 * Tox-WeeChat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Tox-WeeChat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Tox-WeeChat.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include <weechat/weechat-plugin.h>

#include "twc-profile.h"
#include "twc-commands.h"
#include "twc-gui.h"
#include "twc-friend-request.h"
#include "twc-config.h"
#include "twc-data.h"
#include "twc-completion.h"

#include "twc.h"

WEECHAT_PLUGIN_NAME("tox");
WEECHAT_PLUGIN_DESCRIPTION("Tox protocol");
WEECHAT_PLUGIN_AUTHOR("Håvard Pettersson <haavard.pettersson@gmail.com>");
WEECHAT_PLUGIN_VERSION("0.1");
WEECHAT_PLUGIN_LICENSE("GPL3");

struct t_weechat_plugin *weechat_plugin = NULL;

int
weechat_plugin_init(struct t_weechat_plugin *plugin, int argc, char *argv[])
{
    weechat_plugin = plugin;

    twc_profile_init();
    twc_commands_init();
    twc_gui_init();
    twc_completion_init();

    twc_config_init();
    twc_config_read();

    // TODO: respect weechat flag for no autoconnect
    twc_profile_autoload();

    return WEECHAT_RC_OK;
}

int
weechat_plugin_end(struct t_weechat_plugin *plugin)
{
    twc_config_write();

    twc_profile_free_all();

    return WEECHAT_RC_OK;
}
