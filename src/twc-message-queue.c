/*
 * Copyright (c) 2018 Håvard Pettersson <mail@haavard.me>
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

#include <string.h>

#include <tox/tox.h>
#include <weechat/weechat-plugin.h>

#include "twc-list.h"
#include "twc-profile.h"
#include "twc-utils.h"
#include "twc.h"

#include "twc-message-queue.h"

/**
 * Get a message queue for a friend, or create one if it does not exist.
 */
struct t_twc_list *
twc_message_queue_get_or_create(struct t_twc_profile *profile,
                                int32_t friend_number)
{
    struct t_twc_list *message_queue =
        weechat_hashtable_get(profile->message_queues, &friend_number);
    if (!message_queue)
    {
        message_queue = twc_list_new();
        weechat_hashtable_set(profile->message_queues, &friend_number,
                              message_queue);
    }

    return message_queue;
}

/**
 * Add a friend message to the message queue and tries to send it if the
 * friend is online. Handles splitting of messages. (TODO: actually split
 * messages)
 */
void
twc_message_queue_add_friend_message(struct t_twc_profile *profile,
                                     int32_t friend_number, const char *message,
                                     TOX_MESSAGE_TYPE message_type)
{
    int len = strlen(message);
    while (len > 0)
    {
        int fit_len = twc_fit_utf8(message, TWC_MAX_FRIEND_MESSAGE_LENGTH);

        struct t_twc_queued_message *queued_message =
            malloc(sizeof(struct t_twc_queued_message));

        time_t rawtime = time(NULL);
        queued_message->time = malloc(sizeof(struct tm));
        memcpy(queued_message->time, gmtime(&rawtime), sizeof(struct tm));

        queued_message->message = strndup(message, fit_len);
        queued_message->message_type = message_type;

        message += fit_len;
        len -= fit_len;

        /* create a queue if needed and add message */
        struct t_twc_list *message_queue =
            twc_message_queue_get_or_create(profile, friend_number);
        twc_list_item_new_data_add(message_queue, queued_message);
    }

    /* flush if friend is online */
    if (profile->tox &&
        (tox_friend_get_connection_status(profile->tox, friend_number, NULL) !=
         TOX_CONNECTION_NONE))
        twc_message_queue_flush_friend(profile, friend_number);
}

/**
 * Try sending queued messages for a friend.
 */
void
twc_message_queue_flush_friend(struct t_twc_profile *profile,
                               int32_t friend_number)
{
    struct t_twc_list *message_queue =
        twc_message_queue_get_or_create(profile, friend_number);
    size_t index;
    struct t_twc_list_item *item;
    twc_list_foreach (message_queue, index, item)
    {
        struct t_twc_queued_message *queued_message = item->queued_message;

        /* TODO: store and deal with message IDs */
        TOX_ERR_FRIEND_SEND_MESSAGE err;
        (void)tox_friend_send_message(profile->tox, friend_number,
                                      queued_message->message_type,
                                      (uint8_t *)queued_message->message,
                                      strlen(queued_message->message), &err);

        if (err == TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_CONNECTED)
        {
            /* break if message send failed */
            break;
        }
        else
        {
            char *err_str;
            /* check if error occured */
            switch (err)
            {
                case TOX_ERR_FRIEND_SEND_MESSAGE_TOO_LONG:
                    err_str = "message too long";
                    break;
                case TOX_ERR_FRIEND_SEND_MESSAGE_NULL:
                    err_str = "NULL fields for tox_friend_send_message";
                    break;
                case TOX_ERR_FRIEND_SEND_MESSAGE_FRIEND_NOT_FOUND:
                    err_str = "friend not found";
                    break;
                case TOX_ERR_FRIEND_SEND_MESSAGE_SENDQ:
                    err_str = "queue allocation error";
                    break;
                case TOX_ERR_FRIEND_SEND_MESSAGE_EMPTY:
                    err_str = "tried to send empty message";
                    break;
                case TOX_ERR_FRIEND_SEND_MESSAGE_OK:
                    err_str = "no error";
                    break;
                default:
                    err_str = "unknown error";
            }
            if (err != TOX_ERR_FRIEND_SEND_MESSAGE_OK)
            {
                struct t_twc_chat *friend_chat =
                    twc_chat_search_friend(profile, friend_number, true);

                weechat_printf(
                    friend_chat->buffer, "%s%sFailed to send message: %s%s",
                    weechat_prefix("error"), weechat_color("chat_highlight"),
                    err_str, weechat_color("reset"));
            }
            twc_message_queue_free_message(queued_message);
            item->queued_message = NULL;
        }
    }

    /* remove any now-empty items */
    while (message_queue->head && !(message_queue->head->queued_message))
        twc_list_remove(message_queue->head);
}

/**
 * Free a queued message.
 */
void
twc_message_queue_free_message(struct t_twc_queued_message *message)
{
    free(message->time);
    free(message->message);
    free(message);
}

void
twc_message_queue_free_map_callback(void *data, struct t_hashtable *hashtable,
                                    const void *key, const void *value)
{
    struct t_twc_list *message_queue = ((struct t_twc_list *)value);

    struct t_twc_queued_message *message;
    while ((message = twc_list_pop(message_queue)))
        twc_message_queue_free_message(message);

    free(message_queue);
}

/**
 * Free the entire message queue for a profile.
 */
void
twc_message_queue_free_profile(struct t_twc_profile *profile)
{
    weechat_hashtable_map(profile->message_queues,
                          twc_message_queue_free_map_callback, NULL);
    weechat_hashtable_free(profile->message_queues);
}
