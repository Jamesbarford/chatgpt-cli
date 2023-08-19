/* Copyright (C) 2023 James W M Barford-Evans
 * <jamesbarfordevans at gmail dot com>
 * All Rights Reserved
 *
 * This code is released under the BSD 2 clause license.
 * See the COPYING file for more information. */
#ifndef PANIC_H
#define PANIC_H

/***
 *
 * - Reset: `\033[0m`
 * - Bold: `\033[1m`
 * - Underline: `\033[4m`
 * 
 * Foreground colors:
 * - Black: `\033[30m`
 * - Red: `\033[31m`
 * - Green: `\033[32m`
 * - Yellow: `\033[33m`
 * - Blue: `\033[34m`
 * - Purple: `\033[35m`
 * - Cyan: `\033[36m`
 * - White: `\033[37m`
 * 
 * Background colors:
 * - Black: `\033[40m`
 * - Red: `\033[41m`
 * - Green: `\033[42m`
 * - Yellow: `\033[43m`
 * - Blue: `\033[44m`
 * - Purple: `\033[45m`
 * - Cyan: `\033[46m`
 * - White: `\033[47m`
 */

#define panic(...)                                                         \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
        exit(EXIT_FAILURE);                                                \
    } while (0)

#define warning(...)                                                       \
    do {                                                                   \
        fprintf(stderr, "\033[0;35m%s:%d:%s\t\033[0m", __FILE__, __LINE__, \
                __func__);                                                 \
        fprintf(stderr, __VA_ARGS__);                                      \
    } while (0)

#define prompt_warning(...)                                   \
    do {                                                      \
        fprintf(stderr, "\033[0;35m[PROMPT ERROR]: \033[0m"); \
        fprintf(stderr, __VA_ARGS__);                         \
    } while (0)

#endif
