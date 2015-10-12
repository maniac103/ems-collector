/*
 * Buderus EMS data collector
 *
 * Copyright (C) 2011 Danny Baumann <dannybaumann@web.de>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __BYTE_ORDER_H__
#define __BYTE_ORDER_H__

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BE16_TO_CPU(x) ((uint16_t)((((uint16_t)(x) & 0x00ffU) << 8) | (((uint16_t)(x) & 0xff00U) >> 8)))
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define BE16_TO_CPU(x) (x)
#else
#error Unknown byte order
#endif

#endif /* __BYTE_ORDER_H__ */
